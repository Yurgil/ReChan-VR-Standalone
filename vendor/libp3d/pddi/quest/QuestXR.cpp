#include "pddi/quest/QuestXR.h"
#include "pddi/quest/QuestMath.h"
#include "pddi/quest/QuestPortal.h"
#include "pddi/quest/PortalPlacement.h"
#include "pddi/quest/QuestPassthrough.h"
#include "pddi/quest/MRInteraction.h"
#include "pddi/quest/MRInteractionMode.h"
#include "pddi/quest/ControllerPresence.h"
#include "pddi/quest/ControllerAvailability.h"
#include "pddi/quest/GameplayInputGate.h"
#include "pddi/quest/MRTestObject.h"
#include "pddi/quest/MRPortalRenderer.h"
#include "pddi/quest/MRPortalSurfaceRenderer.h"
#include "pddi/quest/QuestRoomScene.h"
#include "pddi/quest/QuestScenePermission.h"
#include "pddi/quest/QuestHands.h"
#include "pddi/quest/MRPointerRenderer.h"
#include <unordered_map>
#include "pddi/gles/AndroidPlatform.h"
#include "pddi/pddidev.h"
#include <GLES3/gl3.h>
#include <GLES2/gl2ext.h>
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>
#include <android/log.h>
#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <thread>
#include <chrono>
#include <vector>

namespace questxr {
namespace {
JavaVM* javaVM=nullptr;
jobject javaActivity=nullptr; // NativeActivity global reference, valid until game thread joins.
XrInstance instance=XR_NULL_HANDLE;
XrSession session=XR_NULL_HANDLE;
XrSystemId system=XR_NULL_SYSTEM_ID;
XrSpace localSpace=XR_NULL_HANDLE, headSpace=XR_NULL_HANDLE;
XrSwapchain swapchain=XR_NULL_HANDLE;
XrActionSet actionSet=XR_NULL_HANDLE;
XrAction stickAction=XR_NULL_HANDLE, triggerAction=XR_NULL_HANDLE, gripAction=XR_NULL_HANDLE;
XrAction aAction=XR_NULL_HANDLE,bAction=XR_NULL_HANDLE,xAction=XR_NULL_HANDLE,yAction=XR_NULL_HANDLE;
XrAction menuAction=XR_NULL_HANDLE,thumbAction=XR_NULL_HANDLE;
XrAction aimAction=XR_NULL_HANDLE,poseAction=XR_NULL_HANDLE;
XrAction contactAction=XR_NULL_HANDLE;
std::array<XrSpace,2> aimSpaces{},gripSpaces{};
std::array<bool,2> controllerDown{};
QuestHands trackedHands;
bool opticalHandsActive=false;
bool handSupported=false,handAim=false,handData=false,handSimultaneous=false;
MRInteraction interaction;
MRInteractionMode interactionMode;
ControllerPresence controllerPresence;
ControllerAvailability controllerAvailability;
// Empty legacy probe for shared render/surface interfaces; gameplay never spawns it.
MRTestObject testObject;
MRCharacter character;
void (*characterDraw)()=nullptr;
bool characterPublished=false,characterPass=false;
Mat4 characterTransform;
int characterClip=0;
MRPortalSurface portalSurface;
QuestRoomScene roomScene;
bool jackieInteraction=false,roomStartPending=false,roomForceScan=false,scenePermissionAsked=false,scenePermissionDenied=false;
XrTime interactionTime=0;
GameplayInputGate gameplayGate;
bool gameplayInputBlocked=true;
std::array<Mat4,2> referenceVP;
std::array<XrPath,2> hands{};
std::vector<XrSwapchainImageOpenGLESKHR> images;
std::vector<GLuint> framebuffers;
std::vector<std::array<GLuint,2>> eyeFramebuffers;
GLuint depth=0;
PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC framebufferMultiview=nullptr;
std::array<XrView,2> views{{{XR_TYPE_VIEW},{XR_TYPE_VIEW}}};
std::array<Mat4,2> eyeVP,screenMVP,portalFromEye;
std::array<std::array<float,4>,2> eyeTangents;
QuestPassthrough passthrough;
bool mrRequested=false;
std::array<float,6> portalEyes{};
bool portalEnabled=true;
struct PortalUniforms { GLint model, eyes, bounds, enabled, transfer; };
std::unordered_map<GLuint,PortalUniforms> portalUniforms;
PortalPlacement vrPlacement,mrPlacement;
constexpr float kDefaultMRScale=.3f;

Mat4 cameraOptics;
std::array<float,4> screenClip{-1,-1,1,1};
XrFrameState frameState{XR_TYPE_FRAME_STATE};
XrSessionState sessionState=XR_SESSION_STATE_UNKNOWN;
bool running=false,closing=false,frameBegun=false,imageAcquired=false,renderable=false;
bool recenterPending=true,wasRecenter=false;
uint32_t imageIndex=0;
int width=1440,height=1440;
std::string error;
uint64_t submittedFrames=0;

void Log(const char* message){__android_log_print(ANDROID_LOG_INFO,"ReChanXR","%s",message);}
void Check(XrResult result,const char* operation){
    if(XR_FAILED(result)){
        char message[256];std::snprintf(message,sizeof(message),"%s failed: XrResult %d",operation,result);
        throw std::runtime_error(message);
    }
}
#define XR_CHECK(expr) Check((expr),#expr)
void Fail(const std::exception& e){error=e.what();closing=true;__android_log_print(ANDROID_LOG_ERROR,"ReChanXR","%s",error.c_str());}
XrPath Path(const char* value){XrPath path;XR_CHECK(xrStringToPath(instance,value,&path));return path;}
Mat4 Pose(const XrPosef& p){return PoseMatrix(p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w,p.position.x,p.position.y,p.position.z);}
void RefreshMRPlacement(){mrPlacement.SetCentered(vrPlacement.pose*Translation(0,0,-1.2f),-kPortalDepth,kDefaultMRScale);}
void SetOpticalHands(bool enabled){
    enabled=enabled&&handSupported;
    if(enabled==opticalHandsActive)return;
    if(enabled){trackedHands.Initialize(instance,system,session,handAim,handData,handSimultaneous);trackedHands.Resume(session);}
    else trackedHands.Shutdown();
    opticalHandsActive=enabled;
    Log(enabled?"MR hands: tracking ON (editing, Touch put down)":"MR hands: tracking OFF");
}
void CancelInteraction(){
    interaction.Cancel();interactionTime=0;
    testObject.Reset();character.Cancel();
    portalSurface.Reset();
    SetOpticalHands(false);trackedHands.ClearInput();controllerPresence.Reset();controllerDown={};
}
void ResetInput(){
    CancelInteraction();
    interactionMode.Reset();gameplayGate.Reset();controllerAvailability.Reset();gameplayInputBlocked=true;
    for(int b=0;b<GamepadButton::COUNT;++b)androidbridge::PostGamepadButton(b,false);
    for(int a=0;a<GamepadAxis::COUNT;++a)androidbridge::PostGamepadAxis(a,a>=GamepadAxis::LeftTrigger?-1.f:0.f);
    androidbridge::PostGamepadConnected(false);
}
XrAction Action(const char* name,XrActionType type,bool handed=false){
    XrActionCreateInfo info{XR_TYPE_ACTION_CREATE_INFO};
    std::snprintf(info.actionName,sizeof(info.actionName),"%s",name);
    std::snprintf(info.localizedActionName,sizeof(info.localizedActionName),"%s",name);
    info.actionType=type;
    if(handed){info.countSubactionPaths=2;info.subactionPaths=hands.data();}
    XrAction action;XR_CHECK(xrCreateAction(actionSet,&info,&action));return action;
}
void InitializeInput(){
    hands={Path("/user/hand/left"),Path("/user/hand/right")};
    XrActionSetCreateInfo setInfo{XR_TYPE_ACTION_SET_CREATE_INFO};
    std::strcpy(setInfo.actionSetName,"gameplay");std::strcpy(setInfo.localizedActionSetName,"ReChan controls");
    XR_CHECK(xrCreateActionSet(instance,&setInfo,&actionSet));
    aimAction=Action("window_aim",XR_ACTION_TYPE_POSE_INPUT,true);
    poseAction=Action("window_grip_pose",XR_ACTION_TYPE_POSE_INPUT,true);
    contactAction=Action("controller_contact",XR_ACTION_TYPE_BOOLEAN_INPUT,true);
    stickAction=Action("stick",XR_ACTION_TYPE_VECTOR2F_INPUT,true);
    triggerAction=Action("trigger",XR_ACTION_TYPE_FLOAT_INPUT,true);
    gripAction=Action("grip",XR_ACTION_TYPE_FLOAT_INPUT,true);
    thumbAction=Action("stick_click",XR_ACTION_TYPE_BOOLEAN_INPUT,true);
    aAction=Action("punch_confirm",XR_ACTION_TYPE_BOOLEAN_INPUT);
    bAction=Action("kick_back",XR_ACTION_TYPE_BOOLEAN_INPUT);
    xAction=Action("left_lower",XR_ACTION_TYPE_BOOLEAN_INPUT);
    yAction=Action("counter_clear",XR_ACTION_TYPE_BOOLEAN_INPUT);
    menuAction=Action("pause",XR_ACTION_TYPE_BOOLEAN_INPUT);
    const std::vector<XrActionSuggestedBinding> bindings={
        {contactAction,Path("/user/hand/left/input/trigger/touch")},{contactAction,Path("/user/hand/right/input/trigger/touch")},
        {contactAction,Path("/user/hand/left/input/thumbstick/touch")},{contactAction,Path("/user/hand/right/input/thumbstick/touch")},
        {contactAction,Path("/user/hand/left/input/thumbrest/touch")},{contactAction,Path("/user/hand/right/input/thumbrest/touch")},
        {contactAction,Path("/user/hand/left/input/x/touch")},{contactAction,Path("/user/hand/left/input/y/touch")},
        {contactAction,Path("/user/hand/right/input/a/touch")},{contactAction,Path("/user/hand/right/input/b/touch")},
        {aimAction,Path("/user/hand/left/input/aim/pose")},{aimAction,Path("/user/hand/right/input/aim/pose")},
        {poseAction,Path("/user/hand/left/input/grip/pose")},{poseAction,Path("/user/hand/right/input/grip/pose")},
        {stickAction,Path("/user/hand/left/input/thumbstick")},{stickAction,Path("/user/hand/right/input/thumbstick")},
        {triggerAction,Path("/user/hand/left/input/trigger/value")},{triggerAction,Path("/user/hand/right/input/trigger/value")},
        {gripAction,Path("/user/hand/left/input/squeeze/value")},{gripAction,Path("/user/hand/right/input/squeeze/value")},
        {thumbAction,Path("/user/hand/left/input/thumbstick/click")},{thumbAction,Path("/user/hand/right/input/thumbstick/click")},
        {aAction,Path("/user/hand/right/input/a/click")},{bAction,Path("/user/hand/right/input/b/click")},
        {xAction,Path("/user/hand/left/input/x/click")},{yAction,Path("/user/hand/left/input/y/click")},
        {menuAction,Path("/user/hand/left/input/menu/click")}
    };
    XrInteractionProfileSuggestedBinding suggested{XR_TYPE_INTERACTION_PROFILE_SUGGESTED_BINDING};
    suggested.interactionProfile=Path("/interaction_profiles/oculus/touch_controller");
    suggested.countSuggestedBindings=static_cast<uint32_t>(bindings.size());suggested.suggestedBindings=bindings.data();
    XR_CHECK(xrSuggestInteractionProfileBindings(instance,&suggested));
    for(int i=0;i<2;++i){
        XrActionSpaceCreateInfo info{XR_TYPE_ACTION_SPACE_CREATE_INFO};info.subactionPath=hands[i];info.poseInActionSpace.orientation.w=1;
        info.action=aimAction;XR_CHECK(xrCreateActionSpace(session,&info,&aimSpaces[i]));
        info.action=poseAction;XR_CHECK(xrCreateActionSpace(session,&info,&gripSpaces[i]));
    }
    XrSessionActionSetsAttachInfo attach{XR_TYPE_SESSION_ACTION_SETS_ATTACH_INFO};attach.countActionSets=1;attach.actionSets=&actionSet;
    XR_CHECK(xrAttachSessionActionSets(session,&attach));
}
bool Button(XrAction action,XrPath hand=XR_NULL_PATH){
    XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.action=action;get.subactionPath=hand;
    XrActionStateBoolean value{XR_TYPE_ACTION_STATE_BOOLEAN};XR_CHECK(xrGetActionStateBoolean(session,&get,&value));
    return value.isActive&&value.currentState;
}
XrActionStateFloat FloatState(XrAction action,XrPath hand){
    XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.action=action;get.subactionPath=hand;
    XrActionStateFloat value{XR_TYPE_ACTION_STATE_FLOAT};XR_CHECK(xrGetActionStateFloat(session,&get,&value));
    return value;
}
float Axis(XrAction action,XrPath hand){const auto value=FloatState(action,hand);return value.isActive?value.currentState:0;}
XrActionStateVector2f Stick(XrPath hand){
    XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.action=stickAction;get.subactionPath=hand;
    XrActionStateVector2f value{XR_TYPE_ACTION_STATE_VECTOR2F};XR_CHECK(xrGetActionStateVector2f(session,&get,&value));return value;
}
bool LocateController(XrAction action,XrSpace space,XrPath hand,Mat4& result){
    XrActionStateGetInfo get{XR_TYPE_ACTION_STATE_GET_INFO};get.action=action;get.subactionPath=hand;
    XrActionStatePose state{XR_TYPE_ACTION_STATE_POSE};XR_CHECK(xrGetActionStatePose(session,&get,&state));
    if(!state.isActive)return false;
    XrSpaceLocation loc{XR_TYPE_SPACE_LOCATION};
    if(XR_FAILED(xrLocateSpace(space,localSpace,frameState.predictedDisplayTime,&loc)))return false;
    const auto flags=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
    if((loc.locationFlags&flags)!=flags)return false;result=Pose(loc.pose);return true;
}
void UpdateInteraction(){
    // One gate for frame manipulation, hand samples and future character grabs.
    // No optical hand queries or frame hit tests are performed during gameplay.
    if(!MRInteractionEnabled()){CancelInteraction();return;}
    if(!frameState.predictedDisplayTime||interactionTime==frameState.predictedDisplayTime)return;
    const float interactionDt=interactionTime?float(frameState.predictedDisplayTime-interactionTime)*1e-9f:0;
    interactionTime=frameState.predictedDisplayTime;
    std::array<MRPointer,2> pointers{};
    std::array<bool,2> held{};
    const auto move=Stick(hands[0]),resize=Stick(hands[1]);
    const double now=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    for(int i=0;i<2;++i){
        pointers[i].valid=LocateController(aimAction,aimSpaces[i],hands[i],pointers[i].aim)&&LocateController(poseAction,gripSpaces[i],hands[i],pointers[i].grip);
        const float trigger=Axis(triggerAction,hands[i]),grip=Axis(gripAction,hands[i]);
        controllerDown[i]=trigger>=(controllerDown[i]?.02f:.05f)||grip>=(controllerDown[i]?.35f:.55f);
        pointers[i].down=controllerDown[i];
        const bool buttons=Button(thumbAction,hands[i])||(i?(Button(aAction)||Button(bAction)):(Button(xAction)||Button(yAction)||Button(menuAction)));
        held[i]=controllerPresence.Update(i,Button(contactAction,hands[i]),trigger>.02f||grip>.1f||buttons||((i?resize:move).isActive&&std::hypot((i?resize:move).currentState.x,(i?resize:move).currentState.y)>.1f),now);
    }
    SetOpticalHands(!held[0]&&!held[1]);
    pointers=trackedHands.Locate(localSpace,interactionTime,pointers,held);
    XrSpaceLocation head{XR_TYPE_SPACE_LOCATION};
    if(XR_FAILED(xrLocateSpace(headSpace,localSpace,interactionTime,&head))||!(head.locationFlags&XR_SPACE_LOCATION_POSITION_VALID_BIT)){CancelInteraction();return;}
    const bool wasActive=interaction.Active();
    testObject.Reset(); // Editing the window must not create a diagnostic cube.
    const bool useCharacter=jackieInteraction&&character.HasTarget();
    if(useCharacter){
        const auto previous=character.State();const bool wasMoving=character.Moving();const auto oldAction=character.Action();
        MRCharacterInput input;
        for(int i=0;i<2;++i)if(held[i]&&pointers[i].valid&&pointers[i].source==0)input.controllers|=1u<<i;
        input.valid=input.controllers&&(head.locationFlags&XR_SPACE_LOCATION_ORIENTATION_VALID_BIT);
        input.headPose=Pose(head.pose);
        if((input.controllers&1)&&move.isActive){input.moveX=move.currentState.x;input.moveY=move.currentState.y;}
        if(input.controllers&2){input.jumpTrigger=Axis(triggerAction,hands[1]);input.punch=Button(aAction);input.kick=Button(bAction);input.roll=Axis(gripAction,hands[1])>.55f;input.resize=resize.isActive?resize.currentState.y:0;}
        character.Update(pointers,mrPlacement,{head.pose.position.x,head.pose.position.y,head.pose.position.z},roomScene.Geometry(),interaction.Active(),interactionDt,input);
        if(character.Consumed())interaction.Cancel();
        else interaction.Update(pointers,mrPlacement,{head.pose.position.x,head.pose.position.y,head.pose.position.z});
        if(wasMoving!=character.Moving())__android_log_print(ANDROID_LOG_INFO,"ReChanXR","MR Jackie locomotion: %s",character.Moving()?"moving":"stopped");
        if(oldAction!=character.Action())__android_log_print(ANDROID_LOG_INFO,"ReChanXR","MR Jackie action: %d",int(character.Action()));
        if(previous!=character.State())__android_log_print(ANDROID_LOG_INFO,"ReChanXR","MR Jackie state %d -> %d",int(previous),int(character.State()));
        portalSurface.Update(trackedHands.Visuals(),mrPlacement,testObject,interactionDt,character.Active()?&character.ContactPose():nullptr,character.Held());
    }else{
        character.Cancel();
        interaction.Update(pointers,mrPlacement,{head.pose.position.x,head.pose.position.y,head.pose.position.z});
        portalSurface.Update(trackedHands.Visuals(),mrPlacement,testObject,interactionDt);
    }
    if(wasActive!=interaction.Active())__android_log_print(ANDROID_LOG_INFO,"ReChanXR","MR window: %s",interaction.Active()?"grab":"release");
}
void SyncInput(){
    if(sessionState!=XR_SESSION_STATE_FOCUSED){ResetInput();return;}
    XrActiveActionSet active{actionSet,XR_NULL_PATH};XrActionsSyncInfo sync{XR_TYPE_ACTIONS_SYNC_INFO};sync.countActiveActionSets=1;sync.activeActionSets=&active;
    const auto result=xrSyncActions(session,&sync);
    if(result==XR_SESSION_NOT_FOCUSED){ResetInput();return;}
    XR_CHECK(result);
    const bool mrInput=passthrough.Active()&&portalEnabled;
    if(mrInput){
        const double now=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
        interactionMode.Update(Button(thumbAction,hands[0]),Button(thumbAction,hands[1]),now);
        if(interactionMode.Changed()){
            CancelInteraction();
            __android_log_print(ANDROID_LOG_INFO,"ReChanXR","MR interaction mode: %s",interactionMode.Editing()?"ON":"OFF");
            if(interactionMode.Editing()&&jackieInteraction&&roomScene.State()!=MRRoomState::Ready){roomStartPending=true;scenePermissionAsked=scenePermissionDenied=false;}
        }
        if(interactionMode.Recenter())recenterPending=true;
    }else interactionMode.Reset();
    if(Button(menuAction)&&character.Active())character.Cancel();
    UpdateInteraction();
    androidbridge::PostGamepadConnected(true);
    androidbridge::SetPhysicalPadConnected(true); // Suppress the imported phone touch overlay.
    const double inputNow=std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    const auto previousMask=controllerAvailability.Mask();
    std::array<bool,2> activeHands{};
    for(int hand=0;hand<2;++hand){
        // isActive on one stick is not a test of the entire Touch pair.
        // Trigger/grip actions can remain usable during a profile transition.
        activeHands[hand]=Stick(hands[hand]).isActive||FloatState(triggerAction,hands[hand]).isActive||FloatState(gripAction,hands[hand]).isActive;
    }
    controllerAvailability.Update(activeHands[0],activeHands[1],inputNow);
    const bool blocked=controllerAvailability.Blocked(interactionMode.Editing(),inputNow);
    if(previousMask!=controllerAvailability.Mask())
        __android_log_print(ANDROID_LOG_INFO,"ReChanXR","Touch action availability: mask=%u (left=1 right=2)",controllerAvailability.Mask());
    if(blocked!=gameplayInputBlocked){
        __android_log_print(ANDROID_LOG_INFO,"ReChanXR","Gameplay input: %s (editing=%d TouchMask=%u hands=%d)",blocked?"suspended":"ready",interactionMode.Editing(),controllerAvailability.Mask(),opticalHandsActive);
    }
    gameplayInputBlocked=blocked;
    const std::array<std::pair<XrAction,int>,5> buttons={{{aAction,GamepadButton::A},{bAction,GamepadButton::B},{xAction,GamepadButton::X},{yAction,GamepadButton::Y},{menuAction,GamepadButton::Start}}};
    uint32_t raw=0;
    for(const auto& [action,button]:buttons)if(Button(action))raw|=1u<<button;
    for(int hand=0;hand<2;++hand){
        if(Axis(gripAction,hands[hand])>.55f)raw|=1u<<(hand?GamepadButton::RightBumper:GamepadButton::LeftBumper);
        const bool click=mrInput?(hand==1&&interactionMode.Status()):Button(thumbAction,hands[hand]);
        if(click)raw|=1u<<(hand?GamepadButton::RightThumb:GamepadButton::LeftThumb);
    }
    const auto filtered=gameplayGate.FilterButtons(raw,blocked);
    for(int button=0;button<GamepadButton::COUNT;++button)
        androidbridge::PostGamepadButton(button,(filtered&(1u<<button))!=0);
    // The menu button remains available while the game world is suspended.
    androidbridge::PostGamepadButton(GamepadButton::Start,Button(menuAction));
    for(int hand=0;hand<2;++hand){
        const auto value=Stick(hands[hand]);
        float x=!blocked&&value.isActive?value.currentState.x:0,y=!blocked&&value.isActive?-value.currentState.y:0;
        const float length=std::sqrt(x*x+y*y);
        if(length<0.18f)x=y=0;
        else{const float scale=(std::min(length,1.f)-0.18f)/(0.82f*length);x*=scale;y*=scale;}
        androidbridge::PostGamepadAxis(hand?GamepadAxis::RightX:GamepadAxis::LeftX,x);
        androidbridge::PostGamepadAxis(hand?GamepadAxis::RightY:GamepadAxis::LeftY,y);
        const float trigger=gameplayGate.FilterTrigger(hand,Axis(triggerAction,hands[hand]),blocked);
        androidbridge::PostGamepadAxis(hand?GamepadAxis::RightTrigger:GamepadAxis::LeftTrigger,2*trigger-1);
    }
    const bool recenter=Button(thumbAction,hands[0]);
    if(!mrInput&&recenter&&!wasRecenter)recenterPending=true;
    wasRecenter=recenter;
}
void TickRoomScene(){
    if(!jackieInteraction||!passthrough.Active()||sessionState!=XR_SESSION_STATE_FOCUSED)return;
    if(roomStartPending){
        if(!roomScene.Available()){roomStartPending=false;return;}
        const int permission=ScenePermissionState(javaVM,javaActivity);
        if(permission==1){roomStartPending=false;scenePermissionAsked=scenePermissionDenied=false;roomScene.Request(roomForceScan);roomForceScan=false;Log("MR room: scene permission granted; preparing room");}
        else if(!scenePermissionAsked){RequestScenePermission(javaVM,javaActivity);scenePermissionAsked=true;}
        else if(permission<0){scenePermissionDenied=true;roomStartPending=scenePermissionAsked=false;Log("MR room: scene permission denied; basic MR unaffected");}
    }
    XrSpaceLocation head{XR_TYPE_SPACE_LOCATION};
    if(XR_SUCCEEDED(xrLocateSpace(headSpace,localSpace,frameState.predictedDisplayTime,&head))&&(head.locationFlags&XR_SPACE_LOCATION_POSITION_VALID_BIT))roomScene.Tick(localSpace,frameState.predictedDisplayTime,{head.pose.position.x,head.pose.position.y,head.pose.position.z});
}
void UpdateMatrices(){
    XrViewLocateInfo locate{XR_TYPE_VIEW_LOCATE_INFO};locate.viewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    locate.displayTime=frameState.predictedDisplayTime;locate.space=localSpace;
    XrViewState state{XR_TYPE_VIEW_STATE};uint32_t count=0;
    XR_CHECK(xrLocateViews(session,&locate,&state,2,&count,views.data()));
    const auto required=XR_VIEW_STATE_POSITION_VALID_BIT|XR_VIEW_STATE_ORIENTATION_VALID_BIT;
    renderable=renderable&&count==2&&(state.viewStateFlags&required)==required;
    if(!renderable)return;
    if(recenterPending){
        XrSpaceLocation head{XR_TYPE_SPACE_LOCATION};XR_CHECK(xrLocateSpace(headSpace,localSpace,frameState.predictedDisplayTime,&head));
        const auto valid=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_ORIENTATION_VALID_BIT;
        if((head.locationFlags&valid)!=valid){renderable=false;return;}
        const auto& q=head.pose.orientation;
        const float yaw=std::atan2(2*(q.w*q.y+q.x*q.z),1-2*(q.y*q.y+q.x*q.x));
        CancelInteraction();
        vrPlacement.Reset(PoseMatrix(0,std::sin(yaw/2),0,std::cos(yaw/2),head.pose.position.x,head.pose.position.y,head.pose.position.z));
        RefreshMRPlacement();
        recenterPending=false;Log("World origin recentered");
    }
    Mat4 panel;panel.m[0]=1.6f;panel.m[5]=1.2f;panel.m[10]=0;panel.m[14]=-2.4f;
    TickRoomScene();
    const PortalPlacement& placement=passthrough.Active()?mrPlacement:vrPlacement;
    for(int eye=0;eye<2;++eye){
        const auto& f=views[eye].fov;
        referenceVP[eye]=Projection(f.angleLeft,f.angleRight,f.angleDown,f.angleUp)*RigidInverse(Pose(views[eye].pose));
        eyeVP[eye]=referenceVP[eye]*placement.LocalToReference();
        screenMVP[eye]=eyeVP[eye]*panel;
        const Mat4 eyeInAnchor=placement.ReferenceToLocal()*Pose(views[eye].pose);
        portalFromEye[eye]=eyeInAnchor;
        eyeTangents[eye]={std::tan(f.angleLeft),std::tan(f.angleRight),std::tan(f.angleDown),std::tan(f.angleUp)};
        for(int component=0;component<3;++component)portalEyes[3*eye+component]=eyeInAnchor(component,3);
    }
}
} // namespace

void SetCharacterDrawCallback(void (*callback)()){characterDraw=callback;}
void PublishCharacter(MRCharacterTarget target,const Mat4& gameView,const Mat4& world){
    target.modelToPortal=cameraOptics*Scale(kDioramaWorldScale)*gameView*world;
    character.SetTarget(target);characterPublished=true;
}
void InvalidateCharacter(){character.Invalidate();}
bool CharacterActive(){return character.Active();}
bool WorldSimulationPaused(){
    // A detached character owns the controls, but not the original level clock.
    return sessionState!=XR_SESSION_STATE_FOCUSED||(!character.Active()&&gameplayInputBlocked);
}
const MRCharacter& Character(){return character;}
void SetGameProjection(const Mat4& projection){cameraOptics=CameraOptics(projection);}
bool MRInteractionEnabled(){return passthrough.Active()&&portalEnabled&&interactionMode.Editing();}
bool MixedRealityAvailable(){return passthrough.Available();}
bool MixedRealityRequested(){return mrRequested;}
bool JackieInteractionRequested(){return jackieInteraction;}
void SetJackieInteraction(bool enabled){
    if(!enabled)CancelInteraction();
    jackieInteraction=enabled;roomScene.SetEnabled(enabled);roomStartPending=enabled&&MRInteractionEnabled();roomForceScan=scenePermissionAsked=scenePermissionDenied=false;
}
void RescanRoom(){CancelInteraction();SetJackieInteraction(true);roomStartPending=true;roomForceScan=true;}
const char* RoomStatusToken(){
    if(!jackieInteraction)return "FE_QOFF";
    if(scenePermissionDenied)return "FE_QDEN";
    if(roomStartPending&&scenePermissionAsked)return "FE_QPER";
    switch(roomScene.State()){
        case MRRoomState::Off:return "FE_QOFF";case MRRoomState::Unavailable:return "FE_QNA";case MRRoomState::Idle:return "FE_QWAIT";
        case MRRoomState::Loading:return "FE_QLOAD";case MRRoomState::Scanning:return "FE_QSCAN";case MRRoomState::Localizing:return "FE_QLOC";
        case MRRoomState::Ready:return "FE_QREADY";case MRRoomState::Cancelled:return "FE_QCANCEL";case MRRoomState::Missing:return "FE_QMISS";default:return "FE_QERR";
    }
}
int RoomSurfaceCount(){return std::count_if(roomScene.Geometry().surfaces.begin(),roomScene.Geometry().surfaces.end(),[](const auto& s){return s.located;});}
void SetMixedReality(bool enabled){CancelInteraction();interactionMode.Reset();mrRequested=enabled&&portalEnabled&&MixedRealityAvailable();}
void SetPortalEnabled(bool enabled){portalEnabled=enabled;if(!enabled)mrRequested=false;}
void SetAndroidContext(JavaVM* vm,jobject activity){javaVM=vm;javaActivity=activity;}
const char* Error(){return error.c_str();}
int Width(){return width;}
int Height(){return height;}
bool ShouldClose(){return closing||androidbridge::ExitRequested();}
unsigned Framebuffer(){return imageAcquired?framebuffers[imageIndex]:0;}

bool Initialize(EGLDisplay display,EGLConfig config,EGLContext context){
    // NativeActivity can be recreated in the same Android process. These are
    // session-lifetime flags, not process-lifetime state.
    running=closing=frameBegun=imageAcquired=renderable=false;
    character.Invalidate();characterPass=characterPublished=false;
    sessionState=XR_SESSION_STATE_UNKNOWN;
    recenterPending=true;wasRecenter=false;submittedFrames=0;imageIndex=0;
    interaction.Cancel();interactionTime=0;gameplayGate.Reset();controllerAvailability.Reset();gameplayInputBlocked=true;controllerDown={};
    jackieInteraction=roomStartPending=roomForceScan=scenePermissionAsked=scenePermissionDenied=false;
    portalEnabled=true;mrRequested=false;vrPlacement.Reset(Mat4());RefreshMRPlacement();cameraOptics=Mat4();error.clear();
    try{
        if(!javaVM||!javaActivity)throw std::runtime_error("Android activity not provided to OpenXR");
        PFN_xrInitializeLoaderKHR initializeLoader=nullptr;
        XR_CHECK(xrGetInstanceProcAddr(XR_NULL_HANDLE,"xrInitializeLoaderKHR",reinterpret_cast<PFN_xrVoidFunction*>(&initializeLoader)));
        XrLoaderInitInfoAndroidKHR loader{XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR};loader.applicationVM=javaVM;loader.applicationContext=javaActivity;
        XR_CHECK(initializeLoader(reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(&loader)));
        uint32_t availableCount=0;
        XR_CHECK(xrEnumerateInstanceExtensionProperties(nullptr,0,&availableCount,nullptr));
        std::vector<XrExtensionProperties> available(availableCount,{XR_TYPE_EXTENSION_PROPERTIES});
        XR_CHECK(xrEnumerateInstanceExtensionProperties(nullptr,availableCount,&availableCount,available.data()));
        std::vector<const char*> extensions={XR_KHR_ANDROID_CREATE_INSTANCE_EXTENSION_NAME,XR_KHR_OPENGL_ES_ENABLE_EXTENSION_NAME};
        bool hasPassthrough=false,hasHands=false,hasHandAim=false,hasHandSource=false,hasSimultaneous=false;
        for(const auto& extension:available){
            if(std::strcmp(extension.extensionName,XR_FB_PASSTHROUGH_EXTENSION_NAME)==0)hasPassthrough=true;
            if(std::strcmp(extension.extensionName,XR_EXT_HAND_TRACKING_EXTENSION_NAME)==0)hasHands=true;
            if(std::strcmp(extension.extensionName,XR_FB_HAND_TRACKING_AIM_EXTENSION_NAME)==0)hasHandAim=true;
            if(std::strcmp(extension.extensionName,XR_EXT_HAND_TRACKING_DATA_SOURCE_EXTENSION_NAME)==0)hasHandSource=true;
            if(std::strcmp(extension.extensionName,XR_META_SIMULTANEOUS_HANDS_AND_CONTROLLERS_EXTENSION_NAME)==0)hasSimultaneous=true;
            __android_log_print(ANDROID_LOG_INFO,"ReChanXR","Capability: %s",extension.extensionName);
        }
        if(hasPassthrough)extensions.push_back(XR_FB_PASSTHROUGH_EXTENSION_NAME);
        const char* roomExtensions[]={XR_FB_SPATIAL_ENTITY_EXTENSION_NAME,XR_FB_SPATIAL_ENTITY_STORAGE_EXTENSION_NAME,XR_FB_SPATIAL_ENTITY_QUERY_EXTENSION_NAME,XR_FB_SPATIAL_ENTITY_CONTAINER_EXTENSION_NAME,XR_FB_SCENE_EXTENSION_NAME};
        auto supports=[&](const char* name){return std::any_of(available.begin(),available.end(),[&](const auto& e){return std::strcmp(e.extensionName,name)==0;});};
        bool hasRoom=true;for(const char* name:roomExtensions)hasRoom&=supports(name);
        const bool hasSceneCapture=hasRoom&&supports(XR_FB_SCENE_CAPTURE_EXTENSION_NAME);
        if(hasRoom){for(const char* name:roomExtensions)extensions.push_back(name);if(hasSceneCapture)extensions.push_back(XR_FB_SCENE_CAPTURE_EXTENSION_NAME);}
        if(hasHands){
            extensions.push_back(XR_EXT_HAND_TRACKING_EXTENSION_NAME);
            if(hasHandAim)extensions.push_back(XR_FB_HAND_TRACKING_AIM_EXTENSION_NAME);
            if(hasHandSource)extensions.push_back(XR_EXT_HAND_TRACKING_DATA_SOURCE_EXTENSION_NAME);
            if(hasSimultaneous)extensions.push_back(XR_META_SIMULTANEOUS_HANDS_AND_CONTROLLERS_EXTENSION_NAME);
        }
        XrInstanceCreateInfoAndroidKHR android{XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR};android.applicationVM=javaVM;android.applicationActivity=javaActivity;
        XrInstanceCreateInfo create{XR_TYPE_INSTANCE_CREATE_INFO};create.next=&android;
        std::strcpy(create.applicationInfo.applicationName,"ReChan Quest");create.applicationInfo.applicationVersion=1;
        std::strcpy(create.applicationInfo.engineName,"ReChan Pure3D");create.applicationInfo.apiVersion=XR_MAKE_VERSION(1,0,0);
        create.enabledExtensionCount=static_cast<uint32_t>(extensions.size());create.enabledExtensionNames=extensions.data();XR_CHECK(xrCreateInstance(&create,&instance));
        XrSystemGetInfo systemInfo{XR_TYPE_SYSTEM_GET_INFO};systemInfo.formFactor=XR_FORM_FACTOR_HEAD_MOUNTED_DISPLAY;
        XR_CHECK(xrGetSystem(instance,&systemInfo,&system));
        PFN_xrGetOpenGLESGraphicsRequirementsKHR requirementsFn=nullptr;
        XR_CHECK(xrGetInstanceProcAddr(instance,"xrGetOpenGLESGraphicsRequirementsKHR",reinterpret_cast<PFN_xrVoidFunction*>(&requirementsFn)));
        XrGraphicsRequirementsOpenGLESKHR requirements{XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};XR_CHECK(requirementsFn(instance,system,&requirements));
        GLint major=0,minor=0;glGetIntegerv(GL_MAJOR_VERSION,&major);glGetIntegerv(GL_MINOR_VERSION,&minor);
        const XrVersion glVersion=XR_MAKE_VERSION(major,minor,0);
        if(glVersion<requirements.minApiVersionSupported||glVersion>requirements.maxApiVersionSupported)throw std::runtime_error("EGL context version outside OpenXR graphics requirements");
        bool multiview=false;GLint extensionCount=0;glGetIntegerv(GL_NUM_EXTENSIONS,&extensionCount);
        for(GLint i=0;i<extensionCount;++i)if(std::strcmp(reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS,i)),"GL_OVR_multiview2")==0)multiview=true;
        framebufferMultiview=reinterpret_cast<PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC>(eglGetProcAddress("glFramebufferTextureMultiviewOVR"));
        if(!multiview||!framebufferMultiview)throw std::runtime_error("Quest renderer requires GL_OVR_multiview2");
        XrGraphicsBindingOpenGLESAndroidKHR graphics{XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR};graphics.display=display;graphics.config=config;graphics.context=context;
        XrSessionCreateInfo sessionInfo{XR_TYPE_SESSION_CREATE_INFO};sessionInfo.next=&graphics;sessionInfo.systemId=system;
        XR_CHECK(xrCreateSession(instance,&sessionInfo,&session));
        if(hasPassthrough)passthrough.Initialize(instance,system,session);
        XrReferenceSpaceCreateInfo spaceInfo{XR_TYPE_REFERENCE_SPACE_CREATE_INFO};spaceInfo.poseInReferenceSpace.orientation.w=1;
        spaceInfo.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_LOCAL;XR_CHECK(xrCreateReferenceSpace(session,&spaceInfo,&localSpace));
        spaceInfo.referenceSpaceType=XR_REFERENCE_SPACE_TYPE_VIEW;XR_CHECK(xrCreateReferenceSpace(session,&spaceInfo,&headSpace));
        InitializeInput();
        handSupported=hasHands;handAim=hasHandAim;handData=hasHandSource;handSimultaneous=hasSimultaneous;
        // Trackers are created only while editing with both Touch controllers put down.
        roomScene.Initialize(instance,session,hasRoom,hasSceneCapture);
        uint32_t count=0;XR_CHECK(xrEnumerateViewConfigurationViews(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,0,&count,nullptr));
        if(count!=2)throw std::runtime_error("Expected a stereo OpenXR view configuration");
        std::vector<XrViewConfigurationView> configViews(count,{XR_TYPE_VIEW_CONFIGURATION_VIEW});
        XR_CHECK(xrEnumerateViewConfigurationViews(instance,system,XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,count,&count,configViews.data()));
        width=std::max(configViews[0].recommendedImageRectWidth,configViews[1].recommendedImageRectWidth);
        height=std::max(configViews[0].recommendedImageRectHeight,configViews[1].recommendedImageRectHeight);
        XR_CHECK(xrEnumerateSwapchainFormats(session,0,&count,nullptr));std::vector<int64_t> formats(count);XR_CHECK(xrEnumerateSwapchainFormats(session,count,&count,formats.data()));
        // ReChan's colours are authored/display-encoded; linear RGBA8 preserves baseline appearance.
        if(std::find(formats.begin(),formats.end(),GL_RGBA8)==formats.end())throw std::runtime_error("OpenXR runtime has no RGBA8 swapchain format");
        XrSwapchainCreateInfo swapInfo{XR_TYPE_SWAPCHAIN_CREATE_INFO};swapInfo.usageFlags=XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT|XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        swapInfo.format=GL_RGBA8;swapInfo.sampleCount=1;swapInfo.width=width;swapInfo.height=height;swapInfo.faceCount=1;swapInfo.arraySize=2;swapInfo.mipCount=1;
        XR_CHECK(xrCreateSwapchain(session,&swapInfo,&swapchain));XR_CHECK(xrEnumerateSwapchainImages(swapchain,0,&count,nullptr));
        images.assign(count,{XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR});XR_CHECK(xrEnumerateSwapchainImages(swapchain,count,&count,reinterpret_cast<XrSwapchainImageBaseHeader*>(images.data())));
        glGenTextures(1,&depth);glBindTexture(GL_TEXTURE_2D_ARRAY,depth);glTexStorage3D(GL_TEXTURE_2D_ARRAY,1,GL_DEPTH_COMPONENT24,width,height,2);
        framebuffers.resize(count);eyeFramebuffers.resize(count);glGenFramebuffers(count,framebuffers.data());
        for(uint32_t i=0;i<count;++i){
            glBindFramebuffer(GL_FRAMEBUFFER,framebuffers[i]);
            framebufferMultiview(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,images[i].image,0,0,2);
            framebufferMultiview(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,depth,0,0,2);
            if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Incomplete multiview framebuffer");
            glGenFramebuffers(2,eyeFramebuffers[i].data());
            for(int eye=0;eye<2;++eye){
                glBindFramebuffer(GL_FRAMEBUFFER,eyeFramebuffers[i][eye]);
                glFramebufferTextureLayer(GL_FRAMEBUFFER,GL_COLOR_ATTACHMENT0,images[i].image,0,eye);
                glFramebufferTextureLayer(GL_FRAMEBUFFER,GL_DEPTH_ATTACHMENT,depth,0,eye);
                if(glCheckFramebufferStatus(GL_FRAMEBUFFER)!=GL_FRAMEBUFFER_COMPLETE)throw std::runtime_error("Incomplete eye framebuffer");
            }
        }
        glBindFramebuffer(GL_FRAMEBUFFER,0);
        if(!InitializeMRPointers())throw std::runtime_error("MR pointer renderer initialization failed");
        if(!InitializeMRPortalObjects())throw std::runtime_error("MR5 renderer initialization failed");
        if(!InitializeMRPortalSurface())throw std::runtime_error("MR surface initialization failed");
        if(!InitializePortalFrame())throw std::runtime_error("Clapperboard frame initialization failed");
        ResetInput();Log("OpenXR initialized; stereo array swapchain and Touch actions ready");return true;
    }catch(const std::exception& e){Fail(e);Shutdown();return false;}
}

void PollEvents(){
    if(!instance||closing)return;
    try{
        do{
            XrEventDataBuffer event{XR_TYPE_EVENT_DATA_BUFFER};XrResult result;
            while((result=xrPollEvent(instance,&event))==XR_SUCCESS){
                roomScene.HandleEvent(event);
                if(event.type==XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED){
                    const auto& change=*reinterpret_cast<XrEventDataSessionStateChanged*>(&event);sessionState=change.state;
                    __android_log_print(ANDROID_LOG_INFO,"ReChanXR","Session state %d",sessionState);
                    if(sessionState==XR_SESSION_STATE_READY){XrSessionBeginInfo begin{XR_TYPE_SESSION_BEGIN_INFO};begin.primaryViewConfigurationType=XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;XR_CHECK(xrBeginSession(session,&begin));running=true;roomScene.Resume();}
                    else if(sessionState==XR_SESSION_STATE_STOPPING){running=false;ResetInput();passthrough.SetEnabled(false);XR_CHECK(xrEndSession(session));}
                    else if(sessionState==XR_SESSION_STATE_EXITING||sessionState==XR_SESSION_STATE_LOSS_PENDING){closing=true;ResetInput();}
                }else if(event.type==XR_TYPE_EVENT_DATA_PASSTHROUGH_STATE_CHANGED_FB){
                    const auto& change=*reinterpret_cast<XrEventDataPassthroughStateChangedFB*>(&event);
                    __android_log_print(ANDROID_LOG_INFO,"ReChanXR","MR: passthrough state flags %llu",static_cast<unsigned long long>(change.flags));
                    if(change.flags&(XR_PASSTHROUGH_STATE_CHANGED_NON_RECOVERABLE_ERROR_BIT_FB|XR_PASSTHROUGH_STATE_CHANGED_REINIT_REQUIRED_BIT_FB)){
                        mrRequested=false;passthrough.Shutdown();
                    }
                }else if(event.type==XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING){closing=true;ResetInput();}
                else if(event.type==XR_TYPE_EVENT_DATA_REFERENCE_SPACE_CHANGE_PENDING){recenterPending=true;}
                event={XR_TYPE_EVENT_DATA_BUFFER};
            }
            if(result!=XR_EVENT_UNAVAILABLE)XR_CHECK(result);
            if(!running&&!ShouldClose())std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }while(!running&&!ShouldClose());
        if(running&&!ShouldClose())SyncInput();
    }catch(const std::exception& e){Fail(e);ResetInput();}
}

void BeginFrame(){
    if(frameBegun||ShouldClose())return;
    screenClip={-1,-1,1,1};characterPublished=false;
    try{
        PollEvents();if(!running||ShouldClose())return;
        if(!passthrough.SetEnabled(mrRequested))mrRequested=false;
        XrFrameWaitInfo wait{XR_TYPE_FRAME_WAIT_INFO};frameState={XR_TYPE_FRAME_STATE};XR_CHECK(xrWaitFrame(session,&wait,&frameState));
        XrFrameBeginInfo begin{XR_TYPE_FRAME_BEGIN_INFO};XR_CHECK(xrBeginFrame(session,&begin));frameBegun=true;
        renderable=frameState.shouldRender;UpdateMatrices();
        // Acquire even on shouldRender=false: engine draws can safely target app-owned swapchain image,
        // while EndFrame submits no layer. Never draw into framebuffer zero on a surfaceless context.
        XrSwapchainImageAcquireInfo acquire{XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO};XR_CHECK(xrAcquireSwapchainImage(swapchain,&acquire,&imageIndex));imageAcquired=true;
        XrSwapchainImageWaitInfo imageWait{XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO};imageWait.timeout=XR_INFINITE_DURATION;XR_CHECK(xrWaitSwapchainImage(swapchain,&imageWait));
        glBindFramebuffer(GL_FRAMEBUFFER,framebuffers[imageIndex]);glViewport(0,0,width,height);
    }catch(const std::exception& e){Fail(e);}
}

void EndFrame(){
    if(!frameBegun)return;
    try{
        if(imageAcquired&&renderable&&portalEnabled)
            DrawPortalFrame(eyeVP,eyeFramebuffers[imageIndex],width,height,passthrough.Active()&&interactionMode.Editing());
        if(imageAcquired&&renderable&&portalEnabled&&passthrough.Active())
            DrawPortalAlpha(portalFromEye,eyeTangents,eyeFramebuffers[imageIndex],width,height);
        if(imageAcquired&&renderable&&MRInteractionEnabled()&&character.Active()&&characterDraw){
            glBindFramebuffer(GL_FRAMEBUFFER,framebuffers[imageIndex]);glViewport(0,0,width,height);
            characterPass=true;characterTransform=character.Transform();characterClip=character.ClipKind();
            characterDraw();characterPass=false;
        }
        if(!characterPublished&&!character.Active())character.Invalidate();
        if(imageAcquired&&renderable&&MRInteractionEnabled())
            DrawMRPortalObjects(referenceVP,eyeFramebuffers[imageIndex],width,height,mrPlacement,portalEyes,trackedHands.Visuals(),testObject,jackieInteraction?&roomScene.Geometry():nullptr);
        if(imageAcquired&&renderable&&MRInteractionEnabled())
            DrawMRPortalSurface(eyeVP,eyeFramebuffers[imageIndex],width,height,mrPlacement,portalSurface);
        if(imageAcquired&&renderable&&passthrough.Active())
            DrawMRPointers(referenceVP,eyeFramebuffers[imageIndex],width,height,interaction.Visuals());
        if(imageAcquired&&renderable&&MRInteractionEnabled()&&character.HasTarget())
            DrawMRPointers(referenceVP,eyeFramebuffers[imageIndex],width,height,character.Visuals());
        glFlush();
        if(imageAcquired){XrSwapchainImageReleaseInfo release{XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO};XR_CHECK(xrReleaseSwapchainImage(swapchain,&release));imageAcquired=false;}
        std::array<XrCompositionLayerProjectionView,2> projectionViews;
        for(int eye=0;eye<2;++eye){auto& v=projectionViews[eye];v={XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};v.pose=views[eye].pose;v.fov=views[eye].fov;
            v.subImage.swapchain=swapchain;v.subImage.imageRect={{0,0},{width,height}};v.subImage.imageArrayIndex=eye;}
        XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};layer.space=localSpace;layer.viewCount=2;layer.views=projectionViews.data();
        XrCompositionLayerPassthroughFB room{XR_TYPE_COMPOSITION_LAYER_PASSTHROUGH_FB};
        room.layerHandle=passthrough.Layer();room.space=XR_NULL_HANDLE;
        std::array<const XrCompositionLayerBaseHeader*,2> layers{};
        uint32_t layerCount=0;
        if(passthrough.Active()&&portalEnabled){
            layer.layerFlags=XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT|XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT;
            layers[layerCount++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&room);
        }
        layers[layerCount++]=reinterpret_cast<const XrCompositionLayerBaseHeader*>(&layer);
        XrFrameEndInfo end{XR_TYPE_FRAME_END_INFO};end.displayTime=frameState.predictedDisplayTime;end.environmentBlendMode=XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
        end.layerCount=renderable?layerCount:0;end.layers=renderable?layers.data():nullptr;XR_CHECK(xrEndFrame(session,&end));frameBegun=false;
        if(++submittedFrames%720==0)__android_log_print(ANDROID_LOG_INFO,"ReChanXR","Submitted frames: %llu",static_cast<unsigned long long>(submittedFrames));
    }catch(const std::exception& e){frameBegun=false;Fail(e);}
}

void Shutdown(){
    if(frameBegun)EndFrame();ResetInput();
    trackedHands.Shutdown();ShutdownMRPointers();
    ShutdownMRPortalObjects();
    ShutdownMRPortalSurface();
    roomScene.Shutdown();
    for(auto& space:aimSpaces){if(space)xrDestroySpace(space);space=XR_NULL_HANDLE;}
    for(auto& space:gripSpaces){if(space)xrDestroySpace(space);space=XR_NULL_HANDLE;}
    aimAction=poseAction=contactAction=XR_NULL_HANDLE;
    passthrough.Shutdown();mrRequested=false;
    ShutdownPortalFrame();portalUniforms.clear();
    for(auto& pair:eyeFramebuffers)glDeleteFramebuffers(2,pair.data());eyeFramebuffers.clear();
    if(!framebuffers.empty())glDeleteFramebuffers(framebuffers.size(),framebuffers.data());framebuffers.clear();
    if(depth){glDeleteTextures(1,&depth);depth=0;}
    if(swapchain){xrDestroySwapchain(swapchain);swapchain=XR_NULL_HANDLE;}
    if(headSpace){xrDestroySpace(headSpace);headSpace=XR_NULL_HANDLE;}
    if(localSpace){xrDestroySpace(localSpace);localSpace=XR_NULL_HANDLE;}
    if(session){xrDestroySession(session);session=XR_NULL_HANDLE;}
    if(actionSet){xrDestroyActionSet(actionSet);actionSet=XR_NULL_HANDLE;}
    if(instance){xrDestroyInstance(instance);instance=XR_NULL_HANDLE;}
    running=false;images.clear();
}

void UploadMeshMatrices(int location,const Mat4& gameView,const Mat4& model){
    std::array<Mat4,2> mvp;
    // Initial presentation scale; calibrated on-device in Q5 before acceptance.
    for(int eye=0;eye<2;++eye)mvp[eye]=characterPass?referenceVP[eye]*characterTransform*model:eyeVP[eye]*(portalEnabled?cameraOptics:Mat4())*Scale(kDioramaWorldScale)*gameView*model;
    glUniformMatrix4fv(location,2,GL_FALSE,mvp[0].Data());
    GLint program=0;glGetIntegerv(GL_CURRENT_PROGRAM,&program);
    auto found=portalUniforms.find(program);
    if(found==portalUniforms.end()){
        PortalUniforms uniforms{
            glGetUniformLocation(program,"uQuestModelToPortal"),
            glGetUniformLocation(program,"uQuestEyePosition[0]"),
            glGetUniformLocation(program,"uQuestPortalBounds"),
            glGetUniformLocation(program,"uQuestPortalEnabled"),
            glGetUniformLocation(program,"uQuestTransferModel")};
        found=portalUniforms.emplace(program,uniforms).first;
    }
    const auto& u=found->second;
    const Mat4 modelToPortal=characterPass?mrPlacement.ReferenceToLocal()*characterTransform*model:(portalEnabled?cameraOptics:Mat4())*Scale(kDioramaWorldScale)*gameView*model;
    glUniformMatrix4fv(u.model,1,GL_FALSE,modelToPortal.Data());
    glUniform3fv(u.eyes,2,portalEyes.data());
    glUniform3f(u.bounds,kPortalHalfWidth,kPortalHalfHeight,-kPortalDepth);
    glUniform1i(u.enabled,portalEnabled?1:0);
    glUniform1i(u.transfer,characterPass?characterClip:0);
}

bool IsBoundsVisible(const Mat4& gameView,const Mat4& model,const float* low,const float* high){
    if(!frameBegun||!renderable)return true;
    for(int eye=0;eye<2;++eye)if(BoxInFrustum(eyeVP[eye]*(portalEnabled?cameraOptics:Mat4())*Scale(kDioramaWorldScale)*gameView*model,low,high))return true;
    return false;
}
bool IsPolygonVisible(const Mat4& gameView,const Mat4& model,const float* xyz,int count){
    if(!frameBegun||!renderable)return true;
    for(int eye=0;eye<2;++eye)if(PolygonInFrustum(eyeVP[eye]*(portalEnabled?cameraOptics:Mat4())*Scale(kDioramaWorldScale)*gameView*model,xyz,count,true,cameraOptics.m[0]<0))return true;
    return false;
}

std::string VertexShader(const char* source,const char* program){
    std::string result=source;
    if(std::strcmp(program,"mesh")==0){
        const std::string uniform="uniform mat4 uMVP;";
        auto pos=result.find(uniform);if(pos==std::string::npos)throw std::runtime_error("Mesh shader MVP declaration missing");
        result.replace(pos,uniform.size(),"uniform mat4 uQuestMVP[2];\n#define uMVP uQuestMVP[gl_ViewID_OVR]\n");
        const std::string main="void main()";
        pos=result.find(main);
        if(pos==std::string::npos)throw std::runtime_error("Mesh shader main missing");
        result.replace(pos,main.size(),"void RechanWorldMain()");
        return "layout(num_views=2) in;\n"
            "uniform mat4 uQuestModelToPortal;\nuniform vec3 uQuestEyePosition[2];\n"
            "out vec3 vQuestPortalPosition;\nflat out vec3 vQuestPortalEye;\n"+result+
            "\nvoid main(){RechanWorldMain();vQuestPortalPosition=(uQuestModelToPortal*vec4(aPos,1.0)).xyz;"
            "vQuestPortalEye=uQuestEyePosition[gl_ViewID_OVR];}\n";
    }
    if(std::strcmp(program,"shadow_depth")==0)return result;
    const std::string main="void main()";
    auto pos=result.find(main);if(pos==std::string::npos)throw std::runtime_error("Screen shader main missing");
    result.replace(pos,main.size(),"void RechanScreenMain()");
    return "uniform mat4 uQuestScreen;\nout vec2 vQuestScreenNdc;\n"+result+
        "\nvoid main(){RechanScreenMain();vQuestScreenNdc=gl_Position.xy/gl_Position.w;gl_Position=uQuestScreen*gl_Position;}\n";
}

std::string FragmentShader(const char* source,const char* program){
    std::string result=source;
    if(std::strcmp(program,"shadow_depth")==0)return result;
    if(std::strcmp(program,"mesh")==0){
        const std::string main="void main()";auto pos=result.find(main);
        if(pos==std::string::npos)throw std::runtime_error("Mesh fragment main missing");
        result.replace(pos,main.size(),"void RechanWorldFragment()");
        return "in vec3 vQuestPortalPosition;\nflat in vec3 vQuestPortalEye;\n"
            "uniform vec3 uQuestPortalBounds;\nuniform int uQuestPortalEnabled;\nuniform int uQuestTransferModel;\n"+result+R"(
void main(){
    // Only the explicitly grabbed clone may cross the window. Ordinary level
    // geometry retains strict 0.3.0 clipping, including the original player.
    if(uQuestPortalEnabled!=0){
        vec3 ray=vQuestPortalPosition-vQuestPortalEye;
        if(abs(ray.z)<0.000001)discard;
        float t=(uQuestPortalBounds.z-vQuestPortalEye.z)/ray.z;
        vec2 aperture=vQuestPortalEye.xy+t*ray.xy;
        bool behind=vQuestPortalPosition.z<=uQuestPortalBounds.z;
        if(uQuestTransferModel==2){
            if(behind&&t>=0.0&&t<=1.0&&abs(aperture.x)<=1.82&&aperture.y>=-1.51&&aperture.y<=1.62)discard;
        }else if(uQuestTransferModel==0||behind){
            if(t<0.0||t>1.0||any(greaterThan(abs(aperture),uQuestPortalBounds.xy)))discard;
        }
    }
    RechanWorldFragment();
}
)";
    }
    const std::string main="void main()";auto pos=result.find(main);
    if(pos==std::string::npos)throw std::runtime_error("Screen fragment main missing");
    result.replace(pos,main.size(),"void RechanScreenFragment()");
    return "in vec2 vQuestScreenNdc;\nuniform vec4 uQuestClip;\n"+result+
        "\nvoid main(){if(any(lessThan(vQuestScreenNdc,uQuestClip.xy))||any(greaterThan(vQuestScreenNdc,uQuestClip.zw)))discard;RechanScreenFragment();}\n";
}

void SetScreenScissor(int x,int y,int w,int h){
    // Preserve PDDI's GL-bottom-left scissor convention in logical screen NDC.
    // Fragment clipping remains correct when head motion projects the UI plane obliquely.
    screenClip={2.f*x/1280-1,2.f*y/960-1,2.f*(x+std::max(w,0))/1280-1,2.f*(y+std::max(h,0))/960-1};
}

void DrawScreenArrays(unsigned mode,int first,int count,unsigned program){
    GLint previous=0;glGetIntegerv(GL_FRAMEBUFFER_BINDING,&previous);
    const GLint location=glGetUniformLocation(program,"uQuestScreen");
    const GLint clipLocation=glGetUniformLocation(program,"uQuestClip");
    if(imageAcquired&&static_cast<GLuint>(previous)==framebuffers[imageIndex]){
        glUniform4fv(clipLocation,1,screenClip.data());
        for(int eye=0;eye<2;++eye){
            glBindFramebuffer(GL_FRAMEBUFFER,eyeFramebuffers[imageIndex][eye]);
            glUniformMatrix4fv(location,1,GL_FALSE,screenMVP[eye].Data());glDrawArrays(mode,first,count);
        }
        glBindFramebuffer(GL_FRAMEBUFFER,previous);
    }else{
        const Mat4 identity;glUniform4f(clipLocation,-1,-1,1,1);
        glUniformMatrix4fv(location,1,GL_FALSE,identity.Data());glDrawArrays(mode,first,count);
    }
}
} // namespace questxr
