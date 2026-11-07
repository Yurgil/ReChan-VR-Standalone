#pragma once
#include <openxr/openxr.h>

namespace questxr {
// Optional feature: errors fall back to opaque VR, never terminate gameplay.
class QuestPassthrough {
public:
    bool Initialize(XrInstance instance,XrSystemId system,XrSession session);
    bool SetEnabled(bool enabled);
    void Shutdown();
    bool Available() const{return layer!=XR_NULL_HANDLE;}
    bool Active() const{return active;}
    XrPassthroughLayerFB Layer() const{return layer;}
private:
    XrPassthroughFB passthrough=XR_NULL_HANDLE;
    XrPassthroughLayerFB layer=XR_NULL_HANDLE;
    bool active=false;
    PFN_xrCreatePassthroughFB create=nullptr;
    PFN_xrDestroyPassthroughFB destroy=nullptr;
    PFN_xrPassthroughStartFB start=nullptr;
    PFN_xrPassthroughPauseFB pause=nullptr;
    PFN_xrCreatePassthroughLayerFB createLayer=nullptr;
    PFN_xrDestroyPassthroughLayerFB destroyLayer=nullptr;
    PFN_xrPassthroughLayerResumeFB resumeLayer=nullptr;
    PFN_xrPassthroughLayerPauseFB pauseLayer=nullptr;
};
}
