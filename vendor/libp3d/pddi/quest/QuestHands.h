#pragma once
#include "pddi/quest/MRInteraction.h"
#include <openxr/openxr.h>

namespace questxr {
class QuestHands {
public:
    void Initialize(XrInstance instance,XrSystemId system,XrSession session,bool aimEnabled,bool dataEnabled,bool simultaneousEnabled);
    std::array<MRPointer,2> Locate(XrSpace space,XrTime time,const std::array<MRPointer,2>& controllers,const std::array<bool,2>& controllersHeld={});
    void ClearInput();
    const std::array<MRHandVisual,2>& Visuals()const{return visuals;}
    void Shutdown();
    void Resume(XrSession session);
private:
    std::array<XrHandTrackerEXT,2> trackers{};
    std::array<MRHandVisual,2> visuals{};
    std::array<bool,2> pinching{};
    std::array<int,2> lastSource{-1,-1};
    std::array<int,2> lastDiagnostic{-1,-1};
    std::array<XrTime,2> lastSample{};
    std::array<XrPosef,2> filteredAim{},filteredGrip{};
    bool aim=false,data=false;
    PFN_xrResumeSimultaneousHandsAndControllersTrackingMETA resumeSimultaneous=nullptr;
    PFN_xrPauseSimultaneousHandsAndControllersTrackingMETA pauseSimultaneous=nullptr;
    XrSession simultaneousSession=XR_NULL_HANDLE;
    bool simultaneousRunning=false;
    PFN_xrLocateHandJointsEXT locate=nullptr;
    PFN_xrDestroyHandTrackerEXT destroy=nullptr;
};
}
