#include "pddi/quest/QuestHands.h"
#include <android/log.h>

namespace questxr {
namespace {
Mat4 Matrix(const XrPosef& p){return PoseMatrix(p.orientation.x,p.orientation.y,p.orientation.z,p.orientation.w,p.position.x,p.position.y,p.position.z);}
XrPosef Filter(const XrPosef& previous,const XrPosef& target,float amount){
    XrPosef result=target;
    result.position={previous.position.x+(target.position.x-previous.position.x)*amount,previous.position.y+(target.position.y-previous.position.y)*amount,previous.position.z+(target.position.z-previous.position.z)*amount};
    auto a=previous.orientation,b=target.orientation;
    const float sign=a.x*b.x+a.y*b.y+a.z*b.z+a.w*b.w<0?-1.f:1.f;
    XrQuaternionf q={a.x+(sign*b.x-a.x)*amount,a.y+(sign*b.y-a.y)*amount,a.z+(sign*b.z-a.z)*amount,a.w+(sign*b.w-a.w)*amount};
    const float length=std::sqrt(q.x*q.x+q.y*q.y+q.z*q.z+q.w*q.w);
    if(length>.00001f)result.orientation={q.x/length,q.y/length,q.z/length,q.w/length};
    return result;
}
}
void QuestHands::Initialize(XrInstance instance,XrSystemId system,XrSession session,bool aimEnabled,bool dataEnabled,bool simultaneousEnabled){
    aim=aimEnabled;data=dataEnabled;
    XrSystemHandTrackingPropertiesEXT hands{XR_TYPE_SYSTEM_HAND_TRACKING_PROPERTIES_EXT};
    XrSystemSimultaneousHandsAndControllersPropertiesMETA simultaneous{XR_TYPE_SYSTEM_SIMULTANEOUS_HANDS_AND_CONTROLLERS_PROPERTIES_META};
    if(simultaneousEnabled)hands.next=&simultaneous;
    XrSystemProperties props{XR_TYPE_SYSTEM_PROPERTIES};props.next=&hands;
    if(XR_FAILED(xrGetSystemProperties(instance,system,&props))||!hands.supportsHandTracking)return;
    PFN_xrCreateHandTrackerEXT create=nullptr;
    if(XR_FAILED(xrGetInstanceProcAddr(instance,"xrCreateHandTrackerEXT",reinterpret_cast<PFN_xrVoidFunction*>(&create)))||
       XR_FAILED(xrGetInstanceProcAddr(instance,"xrLocateHandJointsEXT",reinterpret_cast<PFN_xrVoidFunction*>(&locate)))||
       XR_FAILED(xrGetInstanceProcAddr(instance,"xrDestroyHandTrackerEXT",reinterpret_cast<PFN_xrVoidFunction*>(&destroy))))return;
    for(int i=0;i<2;++i){
        XrHandTrackerCreateInfoEXT info{XR_TYPE_HAND_TRACKER_CREATE_INFO_EXT};info.hand=i?XR_HAND_RIGHT_EXT:XR_HAND_LEFT_EXT;info.handJointSet=XR_HAND_JOINT_SET_DEFAULT_EXT;
        // Querying DataSourceState on locate is only legal if the tracker opted
        // into DataSourceInfo at creation (XR_EXT_hand_tracking_data_source).
        XrHandTrackingDataSourceEXT requested=XR_HAND_TRACKING_DATA_SOURCE_UNOBSTRUCTED_EXT;
        XrHandTrackingDataSourceInfoEXT sourceInfo{XR_TYPE_HAND_TRACKING_DATA_SOURCE_INFO_EXT};
        sourceInfo.requestedDataSourceCount=1;sourceInfo.requestedDataSources=&requested;
        if(data)info.next=&sourceInfo;
        const auto result=create(session,&info,&trackers[i]);
        if(XR_FAILED(result)){__android_log_print(ANDROID_LOG_ERROR,"ReChanXR","MR hands: create %s failed %d",i?"right":"left",result);Shutdown();return;}
    }
    if(simultaneousEnabled&&simultaneous.supportsSimultaneousHandsAndControllers){
        xrGetInstanceProcAddr(instance,"xrResumeSimultaneousHandsAndControllersTrackingMETA",reinterpret_cast<PFN_xrVoidFunction*>(&resumeSimultaneous));
    }
    __android_log_print(ANDROID_LOG_INFO,"ReChanXR","MR hands: simultaneous supported=%d",simultaneous.supportsSimultaneousHandsAndControllers);
    __android_log_print(ANDROID_LOG_INFO,"ReChanXR","MR input: hand trackers ready; aim=%d source=%d",aim,data);
}
std::array<MRPointer,2> QuestHands::Locate(XrSpace space,XrTime time,const std::array<MRPointer,2>& controllers,const std::array<bool,2>& controllersHeld){
    auto result=controllers;
    visuals={};
    for(int i=0;i<2;++i){
        const XrTime previousTime=lastSample[i];lastSample[i]=0;
        // Contact sensors/buttons keep a held Touch in charge. Once it is put
        // down, ignore its still-valid pose and allow the actual optical hand.
        if(controllersHeld[i]||controllers[i].down){pinching[i]=false;continue;}
        result[i]={};
        if(!trackers[i])continue;
        std::array<XrHandJointLocationEXT,XR_HAND_JOINT_COUNT_EXT> joints{};
        XrHandTrackingAimStateFB aimState{XR_TYPE_HAND_TRACKING_AIM_STATE_FB};
        XrHandTrackingDataSourceStateEXT source{XR_TYPE_HAND_TRACKING_DATA_SOURCE_STATE_EXT};
        XrHandJointLocationsEXT locations{XR_TYPE_HAND_JOINT_LOCATIONS_EXT};locations.jointCount=joints.size();locations.jointLocations=joints.data();
        if(aim){locations.next=&aimState;if(data)aimState.next=&source;}else if(data)locations.next=&source;
        XrHandJointsLocateInfoEXT info{XR_TYPE_HAND_JOINTS_LOCATE_INFO_EXT};info.baseSpace=space;info.time=time;
        const auto required=XR_SPACE_LOCATION_POSITION_VALID_BIT|XR_SPACE_LOCATION_POSITION_TRACKED_BIT;
        const auto resultCode=locate(trackers[i],&info,&locations);
        const auto& thumb=joints[XR_HAND_JOINT_THUMB_TIP_EXT];const auto& index=joints[XR_HAND_JOINT_INDEX_TIP_EXT];const auto& palm=joints[XR_HAND_JOINT_PALM_EXT];
        auto diagnostic=[&](int code){
            if(lastDiagnostic[i]==code)return;
            lastDiagnostic[i]=code;
            const char* labels[]={"tracked","inactive","source rejected","joints invalid","system gesture","Touch preferred","locate error"};
            __android_log_print(ANDROID_LOG_INFO,"ReChanXR","MR hands %s: %s; result=%d active=%d source=%d/%d joints=%llu/%llu aim=%llu",i?"right":"left",labels[code],resultCode,locations.isActive,source.isActive,source.dataSource,static_cast<unsigned long long>(thumb.locationFlags),static_cast<unsigned long long>(index.locationFlags),static_cast<unsigned long long>(aimState.status));
        };
        if(XR_FAILED(resultCode)||!locations.isActive){diagnostic(XR_FAILED(resultCode)?6:1);pinching[i]=false;continue;}
        // Controller-inferred hand joints must not generate a second pinch source.
        if(data&&(!source.isActive||source.dataSource!=XR_HAND_TRACKING_DATA_SOURCE_UNOBSTRUCTED_EXT)){diagnostic(2);pinching[i]=false;continue;}
        if((thumb.locationFlags&required)!=required||(index.locationFlags&required)!=required||!(palm.locationFlags&XR_SPACE_LOCATION_ORIENTATION_VALID_BIT)){diagnostic(3);pinching[i]=false;continue;}
        if(aim&&(aimState.status&XR_HAND_TRACKING_AIM_SYSTEM_GESTURE_BIT_FB)){diagnostic(4);pinching[i]=false;result[i]={};continue;}
        diagnostic(0);
        auto pinch=palm.pose;
        pinch.position={(thumb.pose.position.x+index.pose.position.x)*.5f,(thumb.pose.position.y+index.pose.position.y)*.5f,(thumb.pose.position.z+index.pose.position.z)*.5f};
        MRPointer p;p.valid=true;p.source=1;
        XrPosef pointerPose=pinch;
        if(aim&&(aimState.status&XR_HAND_TRACKING_AIM_VALID_BIT_FB)){
            pointerPose=aimState.aimPose;
            pinching[i]=aimState.pinchStrengthIndex>=(pinching[i]?.55f:.75f);
        }else{
            const float dx=thumb.pose.position.x-index.pose.position.x,dy=thumb.pose.position.y-index.pose.position.y,dz=thumb.pose.position.z-index.pose.position.z;
            pinching[i]=std::sqrt(dx*dx+dy*dy+dz*dz)<(pinching[i]?.04f:.025f);
        }
        const double elapsed=double(time-previousTime)*1e-9;
        const bool reset=!previousTime||elapsed<=0||elapsed>.15;
        filteredGrip[i]=reset?pinch:Filter(filteredGrip[i],pinch,1-std::exp(-elapsed/.025));
        filteredAim[i]=reset?pointerPose:Filter(filteredAim[i],pointerPose,1-std::exp(-elapsed/.04));
        lastSample[i]=time;p.grip=Matrix(filteredGrip[i]);p.aim=Matrix(filteredAim[i]);
        static_assert(XR_HAND_JOINT_COUNT_EXT==26);
        visuals[i].valid=true;
        const Mat4 smooth=p.grip*RigidInverse(Matrix(pinch));
        for(int j=0;j<XR_HAND_JOINT_COUNT_EXT;++j){
            auto& visual=visuals[i].joints[j];const auto& joint=joints[j];
            visual.valid=(joint.locationFlags&XR_SPACE_LOCATION_POSITION_VALID_BIT)!=0;
            const auto point=smooth*Translation(joint.pose.position.x,joint.pose.position.y,joint.pose.position.z);
            visual.position={point(0,3),point(1,3),point(2,3)};
            visual.radius=std::clamp(joint.radius,.004f,.015f);
        }
        p.down=pinching[i];result[i]=p;
    }
    for(int i=0;i<2;++i)if(result[i].source!=lastSource[i]){
        __android_log_print(ANDROID_LOG_INFO,"ReChanXR","MR input %s: %s",i?"right":"left",result[i].source?"hand":"Touch");lastSource[i]=result[i].source;
    }
    return result;
}
void QuestHands::Resume(XrSession session){
    if(resumeSimultaneous){XrSimultaneousHandsAndControllersTrackingResumeInfoMETA info{XR_TYPE_SIMULTANEOUS_HANDS_AND_CONTROLLERS_TRACKING_RESUME_INFO_META};__android_log_print(ANDROID_LOG_INFO,"ReChanXR","MR input: simultaneous tracking result %d",resumeSimultaneous(session,&info));}
}
void QuestHands::Shutdown(){
    for(auto& tracker:trackers){if(tracker&&destroy)destroy(tracker);tracker=XR_NULL_HANDLE;}
    ClearInput();resumeSimultaneous=nullptr;locate=nullptr;destroy=nullptr;
}
void QuestHands::ClearInput(){pinching={};lastSample={};lastSource={-1,-1};lastDiagnostic={-1,-1};visuals={};}
}
