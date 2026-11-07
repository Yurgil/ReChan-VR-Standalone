#include "pddi/quest/MRTestObject.h"
#include "pddi/quest/MRTransferMath.h"
#include <cassert>
#include <iostream>
using namespace questxr;
namespace {
bool Near(float a,float b){return std::fabs(a-b)<.0001f;}
void Same(const Mat4& a,const Mat4& b){for(int i=0;i<16;++i)assert(Near(a.m[i],b.m[i]));}
MRPointer Hand(const Mat4& object){MRPointer p;p.valid=true;p.source=1;p.grip=Translation(object(0,3),object(1,3),object(2,3));return p;}
}
int main(){
    PortalPlacement window;window.SetCentered(Translation(0,0,-.6f),-kPortalDepth,.3f);
    MRTestObject cube;std::array<MRPointer,2> p{};
    cube.Update(p,window,false);assert(cube.Visible()&&!cube.Held());
    const auto initial=cube.Transform();p[0]=Hand(initial);cube.Update(p,window,false);
    p[0].down=true;cube.Update(p,window,false);assert(cube.Held()&&cube.Consumed());Same(initial,cube.Transform());
    // Pull through the portal: normalize smoothly to 7 cm.
    p[0].grip=Translation(.2f,0,-.4f);cube.Update(p,window,false);
    for(int i=0;i<12;++i)cube.Update(p,window,false);
    assert(Near(cube.Transform()(2,3),-.4f)&&Near(ObjectHalf(cube.Transform())*2,.07f));
    const auto outside=cube.Transform();p[0].down=false;cube.Update(p,window,false);assert(!cube.Held());Same(outside,cube.Transform());
    window.pose=Translation(1,0,0)*window.pose;cube.Update(p,window,false);Same(outside,cube.Transform());
    // Re-grab and return through the aperture; an inside object follows window placement.
    window.pose=Translation(-1,0,0)*window.pose;p[0]=Hand(cube.Transform());cube.Update(p,window,false);p[0].down=true;cube.Update(p,window,false);assert(cube.Held());
    p[0].grip=Translation(0,0,-.7f);for(int i=0;i<12;++i)cube.Update(p,window,false);p[0].down=false;cube.Update(p,window,false);
    auto inside=cube.Transform();window.pose=Translation(.2f,0,0)*window.pose;cube.Update(p,window,false);assert(Near(cube.Transform()(0,3),inside(0,3)+.2f));
    // Frame ownership prevents a competing cube grab. A held pinch on tracking
    // reacquisition cannot capture a newly spawned object without release.
    cube.Reset();cube.Update({},window,false);p[0]=Hand(cube.Transform());cube.Update(p,window,false);p[0].down=true;cube.Update(p,window,true);assert(!cube.Held());
    p[0].down=false;cube.Update(p,window,false);p[0].down=true;cube.Update(p,window,false);assert(cube.Held());
    p[0].valid=false;cube.Update(p,window,false);assert(!cube.Visible()&&!cube.Held()&&cube.Consumed());
    p[0].valid=true;cube.Update(p,window,false);assert(!cube.Held());
    // Controllers never grab the transfer probe; mode cancellation removes it.
    cube.Reset();cube.Update({},window,false);p[0]=Hand(cube.Transform());p[0].source=0;cube.Update(p,window,false);p[0].down=true;cube.Update(p,window,false);assert(!cube.Held());
    cube.Reset();assert(!cube.Visible()&&!cube.Consumed());
    // Small, large and 30 cm source objects all reach 7 cm. Scaling about an
    // off-centre model point keeps that point exactly under the moving fingers.
    for(float scale:{.2f,.3f,.65f,1.6666667f})for(float hz:{30.f,72.f,90.f,120.f}){
        PortalPlacement portal;portal.SetCentered(Translation(0,0,-.6f),-kPortalDepth,scale);
        MRTestObject probe;std::array<MRPointer,2> hand{};probe.Update(hand,portal,false,1/hz);
        auto original=probe.Transform();const float originalHalf=ObjectHalf(original);
        hand[0]=Hand(original);hand[0].grip(0,3)+=.015f;probe.Update(hand,portal,false,1/hz);
        const Mat4 inv=Scale(1/originalHalf)*RigidInverse(original*Scale(1/originalHalf));
        const auto localGrip=inv*hand[0].grip;
        hand[0].down=true;probe.Update(hand,portal,false,1/hz);Same(original,probe.Transform());
        for(int i=0;i<int(hz);++i){
            hand[0].grip=PoseMatrix(0,std::sin(.15f),0,std::cos(.15f),.08f,0,-.4f);
            probe.Update(hand,portal,false,1/hz);
            const auto point=probe.Transform()*Translation(localGrip(0,3),localGrip(1,3),localGrip(2,3));
            for(int d=0;d<3;++d)assert(Near(point(d,3),hand[0].grip(d,3)));
        }
        assert(Near(ObjectHalf(probe.Transform())*2,.07f));
        // Moving back restores the source scale; reverse direction is continuous.
        hand[0].grip=Translation(0,0,-1);for(int i=0;i<int(hz/2);++i)probe.Update(hand,portal,false,1/hz);
        assert(Near(ObjectHalf(probe.Transform()),originalHalf));
        // Releasing part-way into the room finishes normalization over time.
        hand[0].grip=Translation(0,0,-.57f);probe.Update(hand,portal,false,1/hz);
        hand[0].down=false;probe.Update(hand,portal,false,1/hz);assert(probe.InRoom());
        for(int i=0;i<int(hz/2);++i)probe.Update(hand,portal,false,1/hz);
        assert(Near(ObjectHalf(probe.Transform())*2,.07f));
    }
    std::cout<<"MR transfer: no-jump pinch, 7 cm normalization, stable grip pivot, release/return, frame ownership, tracking loss and mode reset OK\n";
}
