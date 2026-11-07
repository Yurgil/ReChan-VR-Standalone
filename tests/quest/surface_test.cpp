#include "pddi/quest/MRPortalSurface.h"
#include <cassert>
#include <iostream>
using namespace questxr;
namespace {
int Count(const MRPortalSurface& s){int n=0;for(const auto& r:s.Ripples())n+=r.strength>0;return n;}
void Finger(std::array<MRHandVisual,2>& hands,const PortalPlacement& w,float distance,float x=0){
    hands[0].valid=true;auto& tip=hands[0].joints[10];tip.valid=true;
    const auto p=w.LocalToReference()*Translation(x,0,-kPortalDepth+distance/w.scale);
    tip.position={p(0,3),p(1,3),p(2,3)};
}
}
int main(){
    for(float scale:{.2f,.3f,.65f})for(float hz:{30.f,72.f,120.f}){
        PortalPlacement window;window.SetCentered(PoseMatrix(0,.3f,0,std::sqrt(1-.09f),.1f,.1f,-.6f),-kPortalDepth,scale);
        MRPortalSurface surface;MRTestObject object;std::array<MRHandVisual,2> hands{};
        Finger(hands,window,-.05f);surface.Update(hands,window,object,1/hz);assert(Count(surface)==0);
        Finger(hands,window,.03f);surface.Update(hands,window,object,1/hz);assert(Count(surface)==1);
        // Frame-driven expiry is stable across XR rates and window scales.
        for(int i=0;i<int(hz*1.3f);++i)surface.Update(hands,window,object,1/hz);assert(Count(surface)==0);
        surface.Reset();Finger(hands,window,-.001f);surface.Update(hands,window,object,1/hz);assert(Count(surface)==1);
        for(int i=0;i<int(hz*.8f);++i)surface.Update(hands,window,object,1/hz);assert(Count(surface)==1);
        // A stationary near-surface hand does not emit a wave every frame.
        for(int i=0;i<int(hz*.6f);++i)surface.Update(hands,window,object,1/hz);assert(Count(surface)==0);
        surface.Reset();Finger(hands,window,-.05f,2);surface.Update(hands,window,object,1/hz);Finger(hands,window,.03f,2);surface.Update(hands,window,object,1/hz);assert(Count(surface)==0);
        surface.Reset();Finger(hands,window,-.05f);surface.Update(hands,window,object,1/hz);Finger(hands,window,.03f);surface.Update(hands,window,object,1/hz);assert(Count(surface)==1);
        hands[0].valid=false;surface.Update(hands,window,object,1/hz);assert(Count(surface)==0);
        // Reacquisition on the opposite side is not a new crossing.
        Finger(hands,window,-.03f);surface.Update(hands,window,object,1/hz);assert(Count(surface)==0);
        surface.Reset();assert(Count(surface)==0);
    }
    std::cout<<"MR surface: physical plane crossing, rotated/scaled window, rate-independent decay, stationary suppression, aperture limits and loss/reset OK\n";
}
