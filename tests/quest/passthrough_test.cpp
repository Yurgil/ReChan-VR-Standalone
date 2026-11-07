#include "pddi/quest/QuestPassthrough.h"
#include <cassert>
#include <cstring>
#include <string>
#include <vector>
#include <cstdio>

namespace {
std::vector<std::string> calls;
bool supported=true,failStart=false,failResume=false,failLayer=false;
XrResult XRAPI_CALL Create(XrSession,const XrPassthroughCreateInfoFB*,XrPassthroughFB* out){calls.push_back("create");*out=reinterpret_cast<XrPassthroughFB>(3);return XR_SUCCESS;}
XrResult XRAPI_CALL Destroy(XrPassthroughFB){calls.push_back("destroy");return XR_SUCCESS;}
XrResult XRAPI_CALL Start(XrPassthroughFB){calls.push_back("start");return failStart?XR_ERROR_RUNTIME_FAILURE:XR_SUCCESS;}
XrResult XRAPI_CALL Pause(XrPassthroughFB){calls.push_back("pause");return XR_SUCCESS;}
XrResult XRAPI_CALL CreateLayer(XrSession,const XrPassthroughLayerCreateInfoFB* info,XrPassthroughLayerFB* out){
    calls.push_back("createLayer");assert(info->purpose==XR_PASSTHROUGH_LAYER_PURPOSE_RECONSTRUCTION_FB);
    if(failLayer)return XR_ERROR_RUNTIME_FAILURE;*out=reinterpret_cast<XrPassthroughLayerFB>(4);return XR_SUCCESS;
}
XrResult XRAPI_CALL DestroyLayer(XrPassthroughLayerFB){calls.push_back("destroyLayer");return XR_SUCCESS;}
XrResult XRAPI_CALL ResumeLayer(XrPassthroughLayerFB){calls.push_back("resumeLayer");return failResume?XR_ERROR_RUNTIME_FAILURE:XR_SUCCESS;}
XrResult XRAPI_CALL PauseLayer(XrPassthroughLayerFB){calls.push_back("pauseLayer");return XR_SUCCESS;}
}
extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrGetSystemProperties(XrInstance,XrSystemId,XrSystemProperties* p){
    static_cast<XrSystemPassthroughPropertiesFB*>(p->next)->supportsPassthrough=supported;return XR_SUCCESS;
}
extern "C" XRAPI_ATTR XrResult XRAPI_CALL xrGetInstanceProcAddr(XrInstance,const char* name,PFN_xrVoidFunction* out){
#define PROC(xrName,fake) if(std::strcmp(name,#xrName)==0){*out=reinterpret_cast<PFN_xrVoidFunction>(fake);return XR_SUCCESS;}
    PROC(xrCreatePassthroughFB,Create);PROC(xrDestroyPassthroughFB,Destroy);PROC(xrPassthroughStartFB,Start);PROC(xrPassthroughPauseFB,Pause);
    PROC(xrCreatePassthroughLayerFB,CreateLayer);PROC(xrDestroyPassthroughLayerFB,DestroyLayer);PROC(xrPassthroughLayerResumeFB,ResumeLayer);PROC(xrPassthroughLayerPauseFB,PauseLayer);
#undef PROC
    return XR_ERROR_FUNCTION_UNSUPPORTED;
}
int main(){
    questxr::QuestPassthrough mr;
    supported=false;assert(!mr.Initialize(XR_NULL_HANDLE,1,XR_NULL_HANDLE));assert(!mr.Available());assert(!mr.SetEnabled(true));assert(calls.empty());
    supported=true;assert(mr.Initialize(XR_NULL_HANDLE,1,XR_NULL_HANDLE));assert(!mr.Active());
    assert(mr.SetEnabled(true));assert(mr.Active());const auto size=calls.size();assert(mr.SetEnabled(true));assert(calls.size()==size);
    assert(mr.SetEnabled(false));assert(!mr.Active());assert(mr.SetEnabled(true));mr.Shutdown();assert(!mr.Available());
    const std::vector<std::string> expected={"create","createLayer","start","resumeLayer","pauseLayer","pause","start","resumeLayer","pauseLayer","pause","destroyLayer","destroy"};
    assert(calls==expected);
    calls.clear();failResume=true;assert(mr.Initialize(XR_NULL_HANDLE,1,XR_NULL_HANDLE));assert(!mr.SetEnabled(true));assert(!mr.Active());assert(calls.back()=="pause");mr.Shutdown();
    calls.clear();failResume=false;failStart=true;assert(mr.Initialize(XR_NULL_HANDLE,1,XR_NULL_HANDLE));assert(!mr.SetEnabled(true));assert(!mr.Active());mr.Shutdown();
    calls.clear();failStart=false;failLayer=true;assert(!mr.Initialize(XR_NULL_HANDLE,1,XR_NULL_HANDLE));assert(!mr.Available());assert(calls.back()=="destroy");
    std::puts("Passthrough: optional capability, VR/MR/stop/resume/teardown and failed-start/layer fallback OK");
}
