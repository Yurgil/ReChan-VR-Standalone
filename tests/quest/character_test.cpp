#include "pddi/quest/MRCharacter.h"
#include "pddi/quest/MRTransferMath.h"
#include <cassert>
#include <iostream>
using namespace questxr;
namespace {
bool Near(float a,float b){return std::fabs(a-b)<.00015f;}
void Same(const Mat4& a,const Mat4& b){for(int i=0;i<16;++i)assert(Near(a.m[i],b.m[i]));}
MRRoomGeometry Room(){MRRoomGeometry room;room.ready=true;MRRoomSurface floor;floor.floor=floor.located=true;float q=std::sqrt(.5f);floor.pose=PoseMatrix(-q,0,0,q,0,0,0);floor.boundary={{-3,-3},{3,-3},{3,3},{-3,3}};room.surfaces.push_back(floor);MRRoomSurface table;table.located=table.volume=true;table.low={-1,0,-1};table.extent={2,.75f,2};room.surfaces.push_back(table);return room;}
MRCharacterTarget Target(float size,bool upsideDown){MRCharacterTarget t;t.valid=true;t.identity=1;t.low=t.standingLow={-25,0,-20};t.high=t.standingHigh={25,180,20};t.grip={0,115,0};t.standingHeight=180;t.up=upsideDown?-1:1;t.modelToPortal=Translation(0,-.18f,-3.6f)*Scale(size/180);if(upsideDown){t.modelToPortal(1,1)*=-1;t.modelToPortal(2,2)*=-1;t.modelToPortal(1,3)=.18f;}return t;}
}
int main(){
 for(float hz:{30.f,72.f,120.f})for(float width:{.2f,.3f,.65f})for(float apparent:{.15f,1.f,1.5f})for(bool inverted:{false,true}){
  const float dt=1/hz;auto room=Room();PortalPlacement window;window.SetCentered(Translation(0,1.2f,-.6f),-kPortalDepth,width);auto target=Target(apparent,inverted);MRCharacter actor;actor.SetTarget(target);std::array<MRPointer,2> hand{};auto eye=std::array<float,3>{0,1.2f,0};
  auto& h=hand[0];h.valid=true;h.source=1;h.grip=Translation(0,1.2f,-.61f);actor.Update(hand,window,eye,room,false,dt);h.down=true;actor.Update(hand,window,eye,room,true,dt);assert(!actor.Active());
  h.down=false;actor.Update(hand,window,eye,room,false,dt);h.down=true;actor.Update(hand,window,eye,room,false,dt);assert(actor.Held());Same(actor.Transform(),window.LocalToReference()*target.modelToPortal);
  h.grip=Translation(.1f,1.2f,-.4f);for(int i=0;i<int(hz/2);++i)actor.Update(hand,window,eye,room,false,dt);
  const auto m=actor.Transform();for(int c=0;c<3;++c){float length=0;for(int r=0;r<3;++r)length+=m(r,c)*m(r,c);assert(Near(std::sqrt(length)*target.standingHeight,.07f));}
  auto grip=m*Translation(target.grip[0],target.grip[1],target.grip[2]);for(int d=0;d<3;++d)assert(Near(grip(d,3),h.grip(d,3)));assert(m(1,1)*target.up>0);
  h.down=false;actor.Update(hand,window,eye,room,false,dt);for(int i=0;i<int(hz*2);++i)actor.Update(hand,window,eye,room,false,dt);assert(actor.Grounded());
  const float footY=target.up>0?0:180;const auto foot=actor.Transform()*Translation(0,footY,0);assert(Near(foot(1,3),.751f));
  h.grip=actor.Transform()*Translation(0,90,0);actor.Update(hand,window,eye,room,false,dt);h.down=true;actor.Update(hand,window,eye,room,false,dt);assert(actor.Held());
  h.grip=Translation(0,1.2f,-.64f);for(int i=0;i<int(hz/2);++i)actor.Update(hand,window,eye,room,false,dt);h.down=false;actor.Update(hand,window,eye,room,false,dt);assert(actor.Returning());for(int i=0;i<int(hz/2);++i)actor.Update(hand,window,eye,room,false,dt);assert(!actor.Active());
  h.down=false;h.grip=Translation(0,1.2f,-.61f);actor.Update(hand,window,eye,room,false,dt);h.down=true;actor.Update(hand,window,eye,room,false,dt);assert(actor.Held());h.valid=false;actor.Update(hand,window,eye,room,false,dt);assert(!actor.Active()&&actor.Consumed());h.valid=true;actor.Update(hand,window,eye,room,false,dt);assert(!actor.Active());
  h.down=false;actor.Update(hand,window,eye,room,false,dt);h.down=true;actor.Update(hand,window,eye,room,false,dt);assert(actor.Held());room.ready=false;actor.Update(hand,window,eye,room,false,dt);assert(!actor.Active());
 }
 // Controllers do not select the character; assets/source invalidation cancel ownership.
 auto room=Room();PortalPlacement window;window.SetCentered(Translation(0,1.2f,-.6f),-kPortalDepth,.3f);MRCharacter actor;actor.SetTarget(Target(1,false));std::array<MRPointer,2> hands{};hands[0].valid=true;hands[0].grip=Translation(0,1.2f,-.61f);actor.Update(hands,window,{0,1.2f,0},room,false,.02f);hands[0].down=true;actor.Update(hands,window,{0,1.2f,0},room,false,.02f);assert(!actor.Active());actor.Invalidate();assert(!actor.HasTarget());
 std::cout<<"MR Jackie: projected pinch, exclusive ownership, canonical 7 cm/upright, stable grip, table landing, return, tracking/room loss OK\n";
}
