#include "pddi/quest/MRRoomGeometry.h"
#include "pddi/quest/MRTestObject.h"
#include "pddi/quest/MRTransferMath.h"
#include <cassert>
#include <iostream>
using namespace questxr;
int main(){
    const std::vector<std::array<float,2>> concave={{-2,-2},{2,-2},{2,0},{0,0},{0,2},{-2,2}};
    assert(MRRoomGeometry::Contains(concave,-1,1));assert(!MRRoomGeometry::Contains(concave,1,1));assert(!MRRoomGeometry::Contains(concave,-1.99f,1,.05f));
    MRRoomGeometry room;MRRoomSurface floor;floor.floor=floor.located=true;const float q=std::sqrt(.5f);
    floor.pose=PoseMatrix(-q,0,0,q,0,0,0);floor.boundary={{-2,-2},{2,-2},{2,2},{-2,2}};room.surfaces.push_back(floor);
    MRRoomSurface table;table.located=table.volume=true;table.labels="TABLE";table.low={-.5f,0,-.4f};table.extent={1,.75f,.8f};room.surfaces.push_back(table);
    MRRoomSurface wall;wall.located=wall.wall=true;wall.pose=Translation(0,1,-2);wall.boundary={{-2,-1},{2,-1},{2,1},{-2,1}};room.surfaces.push_back(wall);
    MRRoomSupport hit;assert(!room.Support({0,2,0},3,0,hit));room.ready=true;
    assert(room.Support({0,2,0},3,.03f,hit)&&hit.surface==1&&std::fabs(hit.point[1]-.75f)<.0001f);
    assert(room.Support({1.5f,2,0},3,.03f,hit)&&hit.surface==0&&std::fabs(hit.point[1])<.0001f);
    assert(!room.Support({2.5f,2,0},3,.03f,hit));assert(!room.Support({0,2,0},.5f,0,hit));
    assert(room.BlocksSphere({0,.5f,0},.03f));assert(room.BlocksSphere({0,1,-1.99f},.03f));assert(!room.BlocksSphere({1,1,0},.03f));
    room.surfaces[1].pose=PoseMatrix(0,std::sin(.35f),0,std::cos(.35f),0,0,0);
    assert(room.Support({0,2,0},3,.03f,hit)&&hit.surface==1);
    for(float hz:{30.f,72.f,120.f}){
        PortalPlacement window;window.SetCentered(Translation(0,1.2f,-.6f),-kPortalDepth,.3f);
        MRTestObject cube;std::array<MRPointer,2> p{};cube.Update(p,window,false);
        p[0].valid=true;p[0].source=1;p[0].grip=Translation(cube.Transform()(0,3),cube.Transform()(1,3),cube.Transform()(2,3));cube.Update(p,window,false);p[0].down=true;cube.Update(p,window,false);
        p[0].grip=Translation(0,1.2f,0);for(int i=0;i<20;++i)cube.Update(p,window,false);p[0].down=false;cube.Update(p,window,false);
        for(int i=0;i<int(hz*2);++i){cube.Update(p,window,false,1/hz);cube.ApplyRoom(room,1/hz);}
        assert(std::fabs(cube.Transform()(1,3)-.786f)<.002f&&std::fabs(ObjectHalf(cube.Transform())*2-.07f)<.0001f);
        room.ready=false;const float y=cube.Transform()(1,3);cube.ApplyRoom(room,1/hz);assert(cube.Transform()(1,3)==y);room.ready=true;
    }
    room.surfaces[0].located=false;assert(!room.Support({1.5f,2,0},3,.03f,hit));
    std::cout<<"Room geometry: concave boundaries, margins, rotated boxes, support/obstacles, cube drop and tracking-loss freeze OK\n";
}
