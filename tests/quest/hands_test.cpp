#include "pddi/quest/QuestHands.h"
#include <cassert>
#include <cstring>
#include <cstdio>
namespace {
bool supported=true,active=true,tracked=true,systemGesture=false,inferred=false,failCreate=false;
float strength=0,position=0;
int created=0,destroyed=0,resumed=0;
bool sourceRequested[2]{};
XrResult XRAPI_CALL Create(XrSession,const XrHandTrackerCreateInfoEXT* info,XrHandTrackerEXT* out){
    if(failCreate&&created==1)return XR_ERROR_RUNTIME_FAILURE;
    sourceRequested[created]=false;
    if(info->next){
        const auto* source=static_cast<const XrHandTrackingDataSourceInfoEXT*>(info->next);
        assert(source->type==XR_TYPE_HAND_TRACKING_DATA_SOURCE_INFO_EXT);
        assert(source->requestedDataSourceCount==1&&source->requestedDataSources[0]==XR_HAND_TRACKING_DATA_SOURCE_UNOBSTRUCTED_EXT);
        sourceRequested[created]=true;
    }
    *out=reinterpret_cast<XrHandTrackerEXT>(uintptr_t(++created));return XR_SUCCESS;
}
XrResult XRAPI_CALL Destroy(XrHandTrackerEXT){++destroyed;return XR_SUCCESS;}
XrResult XRAPI_CALL Resume(XrSession,const XrSimultaneousHandsAndControllersTrackingResumeInfoMETA*){++resumed;return XR_SUCCESS;}
XrResult XRAPI_CALL Locate(XrHandTrackerEXT tracker,const XrHandJointsLocateInfoEXT*,XrHandJointLocationsEXT* out){
    out->isActive=active;
    for(unsigned j=0;j<out->jointCount;++j){auto& joint=out->jointLocations[j];joint.locationFlags=XR_SPACE_LOCATION_ORIENTATION_VALID_BIT|XR_SPACE_LOCATION_POSITION_VALID_BIT|(tracked?XR_SPACE_LOCATION_POSITION_TRACKED_BIT:0);joint.pose.orientation.w=1;joint.pose.position.x=position;}
    for(auto* extension=static_cast<XrBaseOutStructure*>(out->next);extension;extension=extension->next){
        if(extension->type==XR_TYPE_HAND_TRACKING_AIM_STATE_FB){auto* aim=reinterpret_cast<XrHandTrackingAimStateFB*>(extension);aim->status=XR_HAND_TRACKING_AIM_VALID_BIT_FB|(systemGesture?XR_HAND_TRACKING_AIM_SYSTEM_GESTURE_BIT_FB:0);aim->aimPose.orientation.w=1;aim->aimPose.position.x=position;aim->pinchStrengthIndex=strength;}
        if(extension->type==XR_TYPE_HAND_TRACKING_DATA_SOURCE_STATE_EXT){
            // XR_EXT_hand_tracking_data_source requires matching create-time opt-in.
            if(!sourceRequested[reinterpret_cast<uintptr_t>(tracker)-1])return XR_ERROR_VALIDATION_FAILURE;
            auto* source=reinterpret_cast<XrHandTrackingDataSourceStateEXT*>(extension);source->isActive=true;source->dataSource=inferred?XR_HAND_TRACKING_DATA_SOURCE_CONTROLLER_EXT:XR_HAND_TRACKING_DATA_SOURCE_UNOBSTRUCTED_EXT;
        }
    }
    return XR_SUCCESS;
}
}
extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrGetSystemProperties(XrInstance,XrSystemId,XrSystemProperties* p){
    auto* hands=static_cast<XrSystemHandTrackingPropertiesEXT*>(p->next);hands->supportsHandTracking=supported;
    if(hands->next)static_cast<XrSystemSimultaneousHandsAndControllersPropertiesMETA*>(hands->next)->supportsSimultaneousHandsAndControllers=true;
    return XR_SUCCESS;
}
extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance,const char* name,PFN_xrVoidFunction* out){
#define PROC(n,f) if(std::strcmp(name,#n)==0){*out=reinterpret_cast<PFN_xrVoidFunction>(f);return XR_SUCCESS;}
    PROC(xrCreateHandTrackerEXT,Create);PROC(xrDestroyHandTrackerEXT,Destroy);PROC(xrLocateHandJointsEXT,Locate);PROC(xrResumeSimultaneousHandsAndControllersTrackingMETA,Resume);
#undef PROC
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}
int main(){
    using namespace questxr;
    QuestHands hands;std::array<MRPointer,2> controllers{};controllers[0].valid=true;
    supported=false;hands.Initialize(XR_NULL_HANDLE,1,XR_NULL_HANDLE,true,true,true);
    auto p=hands.Locate(XR_NULL_HANDLE,1000000000,controllers);assert(!p[0].valid&&created==0);
    p=hands.Locate(XR_NULL_HANDLE,1000000000,controllers,{true,false});assert(p[0].valid&&p[0].source==0);
    supported=true;hands.Initialize(XR_NULL_HANDLE,1,XR_NULL_HANDLE,true,true,true);assert(created==2&&resumed==0);hands.Resume(XR_NULL_HANDLE);assert(resumed==1);
    p=hands.Locate(XR_NULL_HANDLE,1000000000,controllers);assert(p[0].valid&&p[0].source==1&&!p[0].down);
    assert(hands.Visuals()[0].valid&&hands.Visuals()[0].joints[XR_HAND_JOINT_INDEX_TIP_EXT].valid);
    strength=.8f;p=hands.Locate(XR_NULL_HANDLE,1014000000,controllers);assert(p[0].down);
    strength=.6f;p=hands.Locate(XR_NULL_HANDLE,1028000000,controllers);assert(p[0].down);
    strength=.5f;p=hands.Locate(XR_NULL_HANDLE,1042000000,controllers);assert(!p[0].down);
    position=.2f;p=hands.Locate(XR_NULL_HANDLE,1056000000,controllers);assert(p[0].grip(0,3)>0&&p[0].grip(0,3)<position);
    // Real tracking loss falls back to Touch, never to inferred controller hands.
    tracked=false;p=hands.Locate(XR_NULL_HANDLE,1070000000,controllers);assert(p[0].source==0);
    tracked=true;inferred=true;p=hands.Locate(XR_NULL_HANDLE,1084000000,controllers);assert(p[0].source==0);
    inferred=false;p=hands.Locate(XR_NULL_HANDLE,1098000000,controllers);assert(p[0].source==1&&p[0].grip(0,3)==position);
    systemGesture=true;p=hands.Locate(XR_NULL_HANDLE,1112000000,controllers);assert(!p[0].valid);
    systemGesture=false;hands.Shutdown();assert(destroyed==2);
    // Optical hands can remain fully valid around a held Touch. Contact
    // keeps that controller in charge, even during hand pinches.
    created=destroyed=0;hands.Initialize(XR_NULL_HANDLE,1,XR_NULL_HANDLE,true,true,true);
    strength=1;controllers[0].down=false;
    p=hands.Locate(XR_NULL_HANDLE,1150000000,controllers,{true,true});assert(p[0].valid&&p[0].source==0&&!p[0].down);
    assert(!hands.Visuals()[0].valid);
    controllers[0].down=true;p=hands.Locate(XR_NULL_HANDLE,1164000000,controllers,{true,true});assert(p[0].source==0&&p[0].down);
    // A gameplay trigger must not be interpreted as an optical frame pinch.
    p=hands.Locate(XR_NULL_HANDLE,1178000000,controllers,{});assert(p[0].source==0&&p[0].down);
    controllers[0].down=false;p=hands.Locate(XR_NULL_HANDLE,1192000000,controllers,{});assert(p[0].source==1&&p[0].down);
    controllers[0].valid=false;p=hands.Locate(XR_NULL_HANDLE,1206000000,controllers,{true,true});assert(!p[0].valid&&p[0].source==0);
    // Putting Touch on the table keeps its pose valid, but returns control to
    // the optical hand after contact ends. Picking it back up selects Touch.
    controllers[0].valid=true;
    p=hands.Locate(XR_NULL_HANDLE,1220000000,controllers,{});assert(p[0].source==1&&p[0].valid);
    p=hands.Locate(XR_NULL_HANDLE,1234000000,controllers,{true,false});assert(p[0].source==0&&p[0].valid);
    active=false;p=hands.Locate(XR_NULL_HANDLE,1248000000,controllers,{});assert(!p[0].valid);active=true;
    hands.ClearInput();strength=.6f;
    assert(!hands.Visuals()[0].valid);
    p=hands.Locate(XR_NULL_HANDLE,1262000000,controllers,{});assert(p[0].source==1&&!p[0].down);
    hands.Shutdown();assert(destroyed==2);controllers[0].valid=true;
    // Partial creation failure releases the already created tracker.
    created=destroyed=0;failCreate=true;hands.Initialize(XR_NULL_HANDLE,1,XR_NULL_HANDLE,true,true,false);assert(created==1&&destroyed==1);hands.Shutdown();assert(destroyed==1);
    // Runtimes without data-source support use the ordinary tracking path,
    // with Touch priority while a controller is actually tracked.
    created=destroyed=0;failCreate=false;hands.Initialize(XR_NULL_HANDLE,1,XR_NULL_HANDLE,true,false,false);
    assert(!sourceRequested[0]&&!sourceRequested[1]);
    p=hands.Locate(XR_NULL_HANDLE,1200000000,controllers,{true,false});assert(p[0].source==0&&p[0].valid);
    controllers[0].valid=false;p=hands.Locate(XR_NULL_HANDLE,1214000000,controllers);assert(p[0].source==1&&p[0].valid);
    hands.Shutdown();assert(destroyed==2);
    std::puts("Hands: optional support, real/inferred source, pinch hysteresis, smoothing, tracking loss, system gesture and teardown OK");
}
