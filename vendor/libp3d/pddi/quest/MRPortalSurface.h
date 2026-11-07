#pragma once
#include "pddi/quest/MRTestObject.h"
namespace questxr {
struct MRPortalRipple {float x=0,y=0,age=0,strength=0;int source=-1;};
class MRPortalSurface {
public:
    void Reset();
    void Update(const std::array<MRHandVisual,2>& hands,const PortalPlacement& window,const MRTestObject& object,float dt,const Mat4* characterContact=nullptr,bool characterHeld=false);
    const std::array<MRPortalRipple,12>& Ripples()const{return ripples;}
private:
    struct Contact{bool valid=false;std::array<float,3> local{};float lastTime=-10;std::array<float,2> lastPoint{};};
    void Sample(Contact& previous,std::array<float,3> local,bool valid,float scale,int source,float strength);
    void Emit(float x,float y,int source,float strength);
    std::array<MRPortalRipple,12> ripples{};
    std::array<Contact,5> contacts{};
    std::array<bool,2> handValid{};
    float time=0;
    unsigned next=0;
};
}
