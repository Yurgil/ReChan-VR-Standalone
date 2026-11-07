#include "gen/time.h"
#include <cstdio>
#include <cmath>
int main(){
    bool ok=true;
    for(const int presentationHz:{30,60,72,90,120}){
        Time time;time.targetFPS=30;time.ResetPlayPresentationState();
        int ticks=0;
        for(int frame=0;frame<presentationHz*10;++frame){time.Tick(1.f/presentationHz);ticks+=time.BeginPlayFixedStep();}
        std::printf("XR %d Hz with saved desktop cap 30: %d logic ticks in 10 seconds\n",presentationHz,ticks);
        ok &= std::abs(ticks-300)<=1;
    }
    // Room interaction can hold the live level for minutes. The next frame
    // advances only its current delta, without collecting paused wall time.
    Time frozen;frozen.ResetPlayPresentationState();frozen.Tick(1.f/30);frozen.BeginPlayFixedStep();
    const float alpha=frozen.GetPlayPresentationAlpha();
    for(int i=0;i<7200;++i){frozen.Tick(1.f/72);frozen.HoldPlayPresentation();ok &= !frozen.DidPlayLogicStepThisFrame()&&frozen.GetPlayPresentationAlpha()==alpha;}
    frozen.Tick(1.f/72);ok &= frozen.BeginPlayFixedStep()<=1;
    // A stall is bounded, rather than advancing hundreds of physics ticks at once.
    Time stalled;stalled.targetFPS=0;stalled.ResetPlayPresentationState();stalled.Tick(1);
    ok &= stalled.BeginPlayFixedStep()<=4;
    return ok?0:1;
}
