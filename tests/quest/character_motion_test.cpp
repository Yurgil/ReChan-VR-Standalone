#include "pddi/quest/MRCharacter.h"
#include <cassert>
#include <iostream>
using namespace questxr;
namespace {
using V=std::array<float,3>;
PortalPlacement Window(){PortalPlacement w;w.SetCentered(Translation(0,1.2f,-.6f),-kPortalDepth,.3f);return w;}
MRRoomGeometry Room(){MRRoomGeometry r;r.ready=true;MRRoomSurface floor;floor.floor=floor.located=true;float q=std::sqrt(.5f);floor.pose=PoseMatrix(-q,0,0,q,0,0,0);floor.boundary={{-3,-3},{3,-3},{3,3},{-3,3}};r.surfaces.push_back(floor);MRRoomSurface table;table.volume=table.located=true;table.low={-1,0,-1};table.extent={2,.75f,2};r.surfaces.push_back(table);return r;}
V Foot(const MRCharacter& a){const auto& s=a.Snapshot();auto m=a.Transform()*Translation((s.standingLow[0]+s.standingHigh[0])*.5f,s.up>0?s.standingLow[1]:s.standingHigh[1],(s.standingLow[2]+s.standingHigh[2])*.5f);return {m(0,3),m(1,3),m(2,3)};}
void Run(MRCharacter& actor,const MRRoomGeometry& room,const MRCharacterInput& input,float seconds,float hz){for(int i=0;i<int(seconds*hz+.5f);++i)actor.Update({},Window(),{0,1.2f,0},room,false,1/hz,input);}
MRCharacter Placed(const MRRoomGeometry& room,float hz,bool inverted=false){
 MRCharacter actor;MRCharacterTarget t;t.valid=true;t.identity=7;t.low=t.standingLow={-25,0,-20};t.high=t.standingHigh={25,180,20};t.grip={0,115,0};t.standingHeight=180;t.modelToPortal=Translation(0,-.18f,-3.6f)*Scale(1.f/180);if(inverted){t.up=-1;t.modelToPortal(1,1)*=-1;t.modelToPortal(2,2)*=-1;t.modelToPortal(1,3)=.18f;}actor.SetTarget(t);
 std::array<MRPointer,2> hands{};auto& h=hands[0];h.valid=true;h.source=1;h.grip=Translation(0,1.2f,-.61f);actor.Update(hands,Window(),{0,1.2f,0},room,false,1/hz);h.down=true;actor.Update(hands,Window(),{0,1.2f,0},room,false,1/hz);assert(actor.Held());
 h.grip=Translation(0,1.1f,0);for(int i=0;i<int(hz/2);++i)actor.Update(hands,Window(),{0,1.2f,0},room,false,1/hz);h.down=false;actor.Update(hands,Window(),{0,1.2f,0},room,false,1/hz);Run(actor,room,{},2,hz);assert(actor.Grounded());assert(Foot(actor)[1]>=0);return actor;
}
MRCharacterInput Neutral(){MRCharacterInput c;c.valid=true;c.controllers=3;return c;}
}
int main(){
 float shortest=10,longest=0,lowestPeak=10,highestPeak=0;
 for(float hz:{30.f,72.f,90.f,120.f}){
  auto room=Room();auto actor=Placed(room,hz);auto input=Neutral();const auto start=Foot(actor);
  input.moveY=1;Run(actor,room,input,.5f,hz);assert(std::fabs(Foot(actor)[2]-start[2])<.001f); // Pickup while deflected requires neutral.
  input.moveY=0;Run(actor,room,input,.1f,hz);input.moveY=1;Run(actor,room,input,2,hz);auto moved=Foot(actor);
  shortest=std::min(shortest,start[2]-moved[2]);longest=std::max(longest,start[2]-moved[2]);
  assert(start[2]-moved[2]>.47f&&start[2]-moved[2]<.51f&&actor.Grounded()&&actor.Moving());assert(std::fabs(moved[1]-.751f)<.001f);
  input.moveY=0;Run(actor,room,input,.3f,hz);assert(!actor.Moving());auto still=Foot(actor);Run(actor,room,input,.4f,hz);assert(std::fabs(Foot(actor)[2]-still[2])<.00001f);
  // Early trigger and release hysteresis, no automatic repeated jumps on landing.
  input.jumpTrigger=.03f;Run(actor,room,input,.1f,hz);assert(actor.Grounded());input.jumpTrigger=.051f;float peak=0;
  for(int i=0;i<int(hz*1.5f);++i){actor.Update({},Window(),{0,1.2f,0},room,false,1/hz,input);peak=std::max(peak,Foot(actor)[1]);}
  lowestPeak=std::min(lowestPeak,peak);highestPeak=std::max(highestPeak,peak);
  assert(peak>.80f&&peak<.82f&&actor.Grounded());input.jumpTrigger=.01f;Run(actor,room,input,.05f,hz);input.jumpTrigger=.051f;Run(actor,room,input,.05f,hz);assert(!actor.Grounded());
  input.valid=false;Run(actor,room,input,1,hz);assert(actor.Grounded());input=Neutral();Run(actor,room,input,.1f,hz);
  input.punch=true;Run(actor,room,input,.05f,hz);assert(actor.Action()==MRCharacterAction::Punch);Run(actor,room,input,.6f,hz);assert(actor.Action()==MRCharacterAction::None);input.punch=false;Run(actor,room,input,.05f,hz);input.kick=true;Run(actor,room,input,.05f,hz);assert(actor.Action()==MRCharacterAction::Kick);Run(actor,room,input,.8f,hz);assert(actor.Action()==MRCharacterAction::None);
  auto rolling=Placed(room,hz);input=Neutral();Run(rolling,room,input,.1f,hz);input.roll=true;Run(rolling,room,input,.05f,hz);assert(rolling.Action()==MRCharacterAction::Roll);Run(rolling,room,input,1,hz);assert(rolling.Action()==MRCharacterAction::None&&rolling.Grounded());assert(std::hypot(Foot(rolling)[0],Foot(rolling)[2])>.17f);
  auto interrupted=Placed(room,hz);input=Neutral();Run(interrupted,room,input,.1f,hz);input.roll=true;Run(interrupted,room,input,.05f,hz);assert(interrupted.Action()==MRCharacterAction::Roll);input.controllers=1;Run(interrupted,room,input,.05f,hz);assert(interrupted.Action()==MRCharacterAction::None&&!interrupted.Moving());
  auto inverted=Placed(room,hz,true);input=Neutral();Run(inverted,room,input,.1f,hz);input.moveX=1;Run(inverted,room,input,1,hz);assert(Foot(inverted)[0]>.22f&&std::fabs(Foot(inverted)[1]-.751f)<.001f);input.moveX=0;Run(inverted,room,input,.3f,hz);input.jumpTrigger=.06f;Run(inverted,room,input,.1f,hz);assert(!inverted.Grounded()&&Foot(inverted)[1]>.78f);
  // Right-stick sizing: independent of refresh rate, feet stay on the table,
  // scaled body collision, clamped life-size/minimum, no reset when walking.
  auto sized=Placed(room,hz);input=Neutral();Run(sized,room,input,.1f,hz);
  const auto fixedFoot=Foot(sized);input.resize=1;Run(sized,room,input,4,hz);
  assert(std::fabs(sized.StandingHeight()-kMRJackieMaxHeight)<.0001f);
  auto actualHeight=[&](const MRCharacter& a){const auto& m=a.Transform();return std::sqrt(m(0,1)*m(0,1)+m(1,1)*m(1,1)+m(2,1)*m(2,1))*a.Snapshot().standingHeight;};
  assert(std::fabs(actualHeight(sized)-kMRJackieMaxHeight)<.0001f&&std::fabs(Foot(sized)[1]-fixedFoot[1])<.0001f);
  // A large torso stays pinchable; returning never leaves a scaled original.
  auto largeReturn=sized;std::array<MRPointer,2> largeHand{};
  largeHand[0].valid=true;largeHand[0].source=1;largeHand[0].grip=Translation(0,fixedFoot[1]+1.0f,0);
  largeReturn.Update(largeHand,Window(),{0,1.2f,0},room,false,1/hz);largeHand[0].down=true;
  largeReturn.Update(largeHand,Window(),{0,1.2f,0},room,false,1/hz);assert(largeReturn.Held());
  largeHand[0].grip=Translation(0,1.2f,-.64f);
  for(int i=0;i<int(hz/2);++i)largeReturn.Update(largeHand,Window(),{0,1.2f,0},room,false,1/hz);
  largeHand[0].down=false;largeReturn.Update(largeHand,Window(),{0,1.2f,0},room,false,1/hz);assert(largeReturn.Returning());
  Run(largeReturn,room,Neutral(),.5f,hz);assert(!largeReturn.Active());
  input.resize=0;input.moveX=1;Run(sized,room,input,.2f,hz);assert(std::fabs(actualHeight(sized)-kMRJackieMaxHeight)<.0001f);
  input.moveX=0;input.resize=-1;Run(sized,room,input,5,hz);assert(std::fabs(sized.StandingHeight()-.05f)<.0001f&&std::fabs(actualHeight(sized)-.05f)<.0001f);
  auto lowRoom=room;MRRoomSurface lowRoof;lowRoof.volume=lowRoof.located=true;lowRoof.low={-.5f,1.05f,-.5f};lowRoof.extent={1,.2f,1};lowRoom.surfaces.push_back(lowRoof);
  auto underRoof=Placed(room,hz);input=Neutral();Run(underRoof,lowRoom,input,.1f,hz);input.resize=1;Run(underRoof,lowRoom,input,4,hz);assert(underRoof.StandingHeight()<.34f&&underRoof.StandingHeight()>.20f);
  // Grabbing after controller travel suspends room commands; reverse transfer still restores ownership.
  std::array<MRPointer,2> hand{};hand[0].valid=true;hand[0].source=1;auto foot=Foot(actor);hand[0].grip=Translation(foot[0],foot[1]+.05f,foot[2]);actor.Update(hand,Window(),{0,1.2f,0},room,false,1/hz);hand[0].down=true;actor.Update(hand,Window(),{0,1.2f,0},room,false,1/hz);assert(actor.Held());
  hand[0].grip=Translation(0,1.2f,-.64f);for(int i=0;i<int(hz/2);++i)actor.Update(hand,Window(),{0,1.2f,0},room,false,1/hz);hand[0].down=false;actor.Update(hand,Window(),{0,1.2f,0},room,false,1/hz);assert(actor.Returning());Run(actor,room,input,.5f,hz);assert(!actor.Active());
  // Head-relative travel ignores head height/pitch; diagonals are normalized.
  auto facing=Placed(room,hz);input=Neutral();Run(facing,room,input,.1f,hz);input.headPose=PoseMatrix(0,std::sin(.78539816f),0,std::cos(.78539816f),0,0,0);input.moveY=1;Run(facing,room,input,1,hz);assert(Foot(facing)[0]<-.22f&&std::fabs(Foot(facing)[2])<.002f);
  auto diagonal=Placed(room,hz);input=Neutral();Run(diagonal,room,input,.1f,hz);input.moveX=input.moveY=1;Run(diagonal,room,input,1,hz);auto d=Foot(diagonal);assert(std::hypot(d[0],d[2])<.26f);
  // Source loss stops travel, and a controller handoff requires neutral again.
  input.valid=false;auto beforeLoss=Foot(diagonal);Run(diagonal,room,input,.5f,hz);assert(std::fabs(Foot(diagonal)[0]-beforeLoss[0])<.001f);input.valid=true;Run(diagonal,room,input,.5f,hz);assert(std::fabs(Foot(diagonal)[0]-beforeLoss[0])<.001f);
  // Move off the tabletop and land on the scanned floor, without snapping up.
  auto edge=Placed(room,hz);input=Neutral();Run(edge,room,input,.1f,hz);input.moveY=1;Run(edge,room,input,7,hz);assert(edge.Active()&&edge.Grounded());if(!(Foot(edge)[2]<-1.3f&&std::fabs(Foot(edge)[1]-.001f)<.002f))std::cerr<<"edge hz="<<hz<<" y="<<Foot(edge)[1]<<" z="<<Foot(edge)[2]<<"\n";assert(Foot(edge)[2]<-1.3f&&std::fabs(Foot(edge)[1]-.001f)<.002f);
  // Solid furniture sides and scanned walls stop motion; no tunnelling at 30 Hz.
  MRRoomSurface block;block.volume=block.located=true;block.low={-.15f,.75f,-.35f};block.extent={.3f,.25f,.1f};room.surfaces.push_back(block);
  auto blocked=Placed(room,hz);input=Neutral();Run(blocked,room,input,.1f,hz);input.moveY=1;Run(blocked,room,input,2,hz);assert(Foot(blocked)[2]>-.24f&&Foot(blocked)[2]<-.20f&&blocked.Grounded());
  // A small step is traversable; a jump cannot pass through a tabletop object overhead.
  auto stepRoom=Room();MRRoomSurface small;small.volume=small.located=true;small.low={-.3f,.75f,-.5f};small.extent={.6f,.007f,.25f};stepRoom.surfaces.push_back(small);
  auto stepActor=Placed(stepRoom,hz);input=Neutral();Run(stepActor,stepRoom,input,.1f,hz);input.moveY=1;Run(stepActor,stepRoom,input,1.5f,hz);if(!(Foot(stepActor)[2]<-.3f&&stepActor.Grounded()&&Foot(stepActor)[1]>.756f))std::cerr<<"step hz="<<hz<<" y="<<Foot(stepActor)[1]<<" z="<<Foot(stepActor)[2]<<"\n";assert(Foot(stepActor)[2]<-.3f&&stepActor.Grounded()&&Foot(stepActor)[1]>.756f);
  auto ceilingRoom=Room();MRRoomSurface overhead;overhead.volume=overhead.located=true;overhead.low={-.3f,.87f,-.3f};overhead.extent={.6f,.1f,.6f};ceilingRoom.surfaces.push_back(overhead);
  auto base=Placed(Room(),hz);input=Neutral();Run(base,ceilingRoom,input,.1f,hz);input.jumpTrigger=.06f;float ceilingPeak=Foot(base)[1];for(int i=0;i<int(hz);++i){base.Update({},Window(),{0,1.2f,0},ceilingRoom,false,1/hz,input);ceilingPeak=std::max(ceilingPeak,Foot(base)[1]);}assert(ceilingPeak<.78f&&base.Grounded());
  room.surfaces.pop_back();MRRoomSurface wall;wall.wall=wall.located=true;wall.pose=Translation(0,1,-.4f);wall.boundary={{-2,-1},{2,-1},{2,1},{-2,1}};room.surfaces.push_back(wall);
  auto atWall=Placed(room,hz);input=Neutral();Run(atWall,room,input,.1f,hz);input.moveY=1;Run(atWall,room,input,3,hz);assert(Foot(atWall)[2]>-.38f&&atWall.Grounded());
  // Keep the miniature in the selected room even if a wall boundary is incomplete.
  room=Room();room.surfaces.resize(1);auto bounds=Placed(room,hz); // No table: fixture waits until grounded floor.
  input=Neutral();Run(bounds,room,input,.1f,hz);input.moveX=1;Run(bounds,room,input,16,hz);assert(Foot(bounds)[0]<3&&bounds.Active());
  room.ready=false;Run(bounds,room,input,.1f,hz);assert(!bounds.Active());
 }
 assert(longest-shortest<.002f&&highestPeak-lowestPeak<.002f);
 std::cout<<"MR room control: 5 cm–1.566 m sizing, scaled collisions/feet/return, rates, pickup-neutral, head-relative travel, jump hysteresis, actions, tabletop edges/floor, walls/furniture, tracking loss and one-room bounds OK\n";
}
