#include "pddi/quest/QuestPassthrough.h"
#include <android/log.h>

namespace questxr {
namespace {
bool Okay(XrResult result,const char* operation){
    if(XR_SUCCEEDED(result))return true;
    __android_log_print(ANDROID_LOG_WARN,"ReChanXR","MR: %s failed (%d); VR remains available",operation,result);
    return false;
}
}
bool QuestPassthrough::Initialize(XrInstance instance,XrSystemId system,XrSession session){
    XrSystemPassthroughPropertiesFB passthroughProperties{XR_TYPE_SYSTEM_PASSTHROUGH_PROPERTIES_FB};
    XrSystemProperties properties{XR_TYPE_SYSTEM_PROPERTIES};properties.next=&passthroughProperties;
    if(!Okay(xrGetSystemProperties(instance,system,&properties),"get passthrough properties")||!passthroughProperties.supportsPassthrough)return false;
#define LOAD(member,name) if(!Okay(xrGetInstanceProcAddr(instance,#name,reinterpret_cast<PFN_xrVoidFunction*>(&member)),#name))return false
    LOAD(create,xrCreatePassthroughFB);LOAD(destroy,xrDestroyPassthroughFB);
    LOAD(start,xrPassthroughStartFB);LOAD(pause,xrPassthroughPauseFB);
    LOAD(createLayer,xrCreatePassthroughLayerFB);LOAD(destroyLayer,xrDestroyPassthroughLayerFB);
    LOAD(resumeLayer,xrPassthroughLayerResumeFB);LOAD(pauseLayer,xrPassthroughLayerPauseFB);
#undef LOAD
    XrPassthroughCreateInfoFB createInfo{XR_TYPE_PASSTHROUGH_CREATE_INFO_FB};
    if(!Okay(create(session,&createInfo,&passthrough),"create passthrough"))return false;
    XrPassthroughLayerCreateInfoFB layerInfo{XR_TYPE_PASSTHROUGH_LAYER_CREATE_INFO_FB};
    layerInfo.passthrough=passthrough;layerInfo.purpose=XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB;
    if(!Okay(createLayer(session,&layerInfo,&layer),"create passthrough layer")){Shutdown();return false;}
    __android_log_print(ANDROID_LOG_INFO,"ReChanXR","MR: passthrough available; default mode VR");return true;
}
bool QuestPassthrough::SetEnabled(bool enabled){
    if(enabled==active)return true;
    if(!Available())return !enabled;
    if(enabled){
        if(!Okay(start(passthrough),"start passthrough"))return false;
        if(!Okay(resumeLayer(layer),"resume passthrough layer")){pause(passthrough);return false;}
        active=true;
    }else{
        Okay(pauseLayer(layer),"pause passthrough layer");
        Okay(pause(passthrough),"pause passthrough");active=false;
    }
    __android_log_print(ANDROID_LOG_INFO,"ReChanXR","MR: mode %s",active?"MR":"VR");return true;
}
void QuestPassthrough::Shutdown(){
    if(active)SetEnabled(false);
    if(layer&&destroyLayer)destroyLayer(layer);
    if(passthrough&&destroy)destroy(passthrough);
    layer=XR_NULL_HANDLE;passthrough=XR_NULL_HANDLE;active=false;
}
}
