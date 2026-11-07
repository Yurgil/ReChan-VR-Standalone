#include "pddi/quest/MRInteraction.h"
#include "pddi/quest/MRInteractionMode.h"
#include "pddi/quest/ControllerPresence.h"
#include <cassert>
#include <iostream>
using namespace questxr;
namespace {
bool Near(float a,float b){return std::fabs(a-b)<.0001f;}
void Same(const Mat4& a,const Mat4& b){for(int i=0;i<16;++i)assert(Near(a.m[i],b.m[i]));}
PortalPlacement Window(){PortalPlacement w;w.SetCentered(Translation(0,0,-1.2f),-kPortalDepth,.3f);return w;}
MRPointer Ray(float x,float y){MRPointer p;p.valid=true;p.aim=Translation(x,y,0);p.grip=p.aim;return p;}
MRPointer Direct(float x,float y){auto p=Ray(x,y);p.grip=Translation(x,y,-1.2f);return p;}
std::array<float,3> head{0,0,0};
}
int main(){
    ControllerPresence presence;
    assert(!presence.Update(0,false,false,1));
    assert(presence.Update(0,true,false,2));
    assert(presence.Update(0,false,false,2.1));
    assert(presence.Update(0,false,false,3.2));
    assert(!presence.Update(0,false,false,3.26));
    assert(presence.Update(1,false,true,3));
    assert(!presence.Update(0,false,false,3.3));
    presence.Reset();assert(!presence.Update(1,false,false,3.01));
    MRInteractionMode mode;
    mode.Update(false,false,0);assert(!mode.Editing());
    // Either order of the chord toggles exactly once, with no solo action.
    mode.Update(true,false,1);assert(!mode.Recenter());
    mode.Update(true,true,1.04);assert(mode.Editing()&&mode.Changed()&&!mode.Recenter()&&!mode.Status());
    mode.Update(true,true,2);assert(mode.Editing()&&!mode.Changed());
    mode.Update(false,true,2.1);mode.Update(true,true,2.2);assert(mode.Editing()&&!mode.Changed());
    mode.Update(false,false,2.3);mode.Update(false,true,3);mode.Update(true,true,3.04);
    assert(!mode.Editing()&&mode.Changed()&&!mode.Recenter()&&!mode.Status());
    mode.Update(false,false,3.1);assert(!mode.Recenter()&&!mode.Status());
    // Single clicks retain recenter/HUD on release; the HUD pulse spans logic ticks.
    mode.Update(true,false,4);mode.Update(false,false,4.1);assert(mode.Recenter());
    mode.Update(false,false,4.2);assert(!mode.Recenter());
    mode.Update(false,true,5);assert(!mode.Status());mode.Update(false,false,5.1);assert(mode.Status());
    mode.Update(false,false,5.15);assert(mode.Status());mode.Update(false,false,5.21);assert(!mode.Status());
    // Focus/MR reset needs neutral thumbs before rearming, never restores edit.
    mode.Reset();mode.Update(true,true,6);assert(!mode.Editing());mode.Update(false,false,6.1);
    mode.Update(true,true,6.2);assert(mode.Editing());mode.Update(false,false,6.3);
    mode.Update(true,false,6.4);mode.Update(false,false,6.5);assert(!mode.Recenter());
    // A gameplay press through the aperture must never seize the window.
    MRInteraction edit;auto w=Window();std::array<MRPointer,2> p{Ray(0,0),{}};
    edit.Update(p,w,head);p[0].down=true;edit.Update(p,w,head);assert(!edit.Active()&&!edit.Consumed());
    p[0]=Ray(.52f,0);edit.Update(p,w,head);auto initial=w.pose;
    p[0].down=true;edit.Update(p,w,head);assert(edit.Active()&&edit.Consumed());Same(initial,w.pose);
    p[0].aim=Translation(.72f,.1f,0);edit.Update(p,w,head);assert(Near(w.pose(0,3),.2f)&&Near(w.pose(1,3),.1f));
    auto released=w.pose;p[0].down=false;edit.Update(p,w,head);assert(!edit.Active()&&edit.Consumed());Same(released,w.pose);
    edit.Update(p,w,head);assert(!edit.Consumed());

    // A near pinch uses the physical pinch point, independent of its aim ray.
    edit.Cancel();w=Window();p={Direct(.52f,0),{}};edit.Update(p,w,head);
    assert(edit.Visuals()[0].direct);p[0].down=true;edit.Update(p,w,head);
    p[0].grip=Translation(.62f,0,-1.2f);edit.Update(p,w,head);assert(Near(w.pose(0,3),.1f));

    // Joining/leaving a two point grip rebases without a jump. Scaling follows
    // the midpoint, and the non-scaled pose remains a rigid transform.
    edit.Cancel();w=Window();p={Ray(-.52f,0),Ray(.52f,0)};edit.Update(p,w,head);
    p[0].down=true;edit.Update(p,w,head);initial=w.pose;
    p[1].down=true;edit.Update(p,w,head);Same(initial,w.pose);
    p[0].aim=Translation(-.78f,0,0);p[1].aim=Translation(.78f,0,0);edit.Update(p,w,head);
    assert(Near(w.scale,.45f));assert(Near(w.pose(0,0),1));
    const auto centre=w.pose*Translation(0,0,-kPortalDepth*w.scale);assert(Near(centre(2,3),-1.2f));
    released=w.pose;p[1].down=false;edit.Update(p,w,head);Same(released,w.pose);
    p[0].aim=Translation(-.68f,0,0);edit.Update(p,w,head);assert(Near(w.pose(0,3),.1f));

    // A corner is a resize handle about the centre, with a bounded scale.
    edit.Cancel();w=Window();p={Ray(.52f,.4f),{}};edit.Update(p,w,head);assert(edit.Visuals()[0].resize);
    p[0].down=true;edit.Update(p,w,head);p[0].aim=Translation(1.04f,.8f,0);edit.Update(p,w,head);
    assert(Near(w.scale,.6f));auto c=w.pose*Translation(0,0,-kPortalDepth*w.scale);assert(Near(c(0,3),0)&&Near(c(2,3),-1.2f));
    p[0].aim=Translation(10,10,0);edit.Update(p,w,head);assert(Near(w.scale,.65f));

    // Tracking loss freezes placement; a held trigger cannot reacquire it.
    edit.Cancel();w=Window();p={Ray(.52f,0),{}};edit.Update(p,w,head);p[0].down=true;edit.Update(p,w,head);initial=w.pose;
    p[0].valid=false;edit.Update(p,w,head);assert(!edit.Active());Same(initial,w.pose);
    p[0].valid=true;edit.Update(p,w,head);assert(!edit.Active());
    p[0].down=false;edit.Update(p,w,head);p[0].down=true;edit.Update(p,w,head);assert(edit.Active());
    p[0].source=1;edit.Update(p,w,head);assert(!edit.Active());
    edit.Update(p,w,head);assert(!edit.Active());

    // Rotation follows one controller; extreme depth stays within reach limits.
    edit.Cancel();w=Window();p={Ray(.52f,0),{}};edit.Update(p,w,head);p[0].down=true;edit.Update(p,w,head);
    p[0].aim=PoseMatrix(0,0,std::sin(.2f),std::cos(.2f),.52f,0,0);edit.Update(p,w,head);assert(Near(w.pose(0,0),std::cos(.4f)));
    p[0].aim=Translation(.52f,0,10);edit.Update(p,w,head);c=w.pose*Translation(0,0,-kPortalDepth*w.scale);
    assert(Near(std::sqrt(c(0,3)*c(0,3)+c(1,3)*c(1,3)+c(2,3)*c(2,3)),3.f));

    // Coincident two point poses never produce NaN or a singular window.
    edit.Cancel();w=Window();p={Ray(-.52f,0),Ray(.52f,0)};edit.Update(p,w,head);p[0].down=p[1].down=true;edit.Update(p,w,head);initial=w.pose;
    p[0].aim=p[1].aim=Translation(0,0,0);edit.Update(p,w,head);Same(initial,w.pose);assert(Near(w.scale,.3f));
    std::cout<<"MR frame interaction tests passed\n";
}
