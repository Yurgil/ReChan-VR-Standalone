#pragma once
#include "pddi/quest/MRInteraction.h"
#include "pddi/quest/MRRoomGeometry.h"
namespace questxr {
// Transfer probe: fixed 7 cm outside, eased scale at the portal surface.
// It deliberately has no level scripts or room physics.
class MRTestObject {
public:
    void Reset();
    void Update(const std::array<MRPointer,2>& pointers,const PortalPlacement& window,bool windowBusy,float dt=1.f/72.f);
    void FollowWindow(const PortalPlacement& window){if(initialized&&owner<0&&!inRoom)transform=window.LocalToReference()*localTransform;}
    void ApplyRoom(const MRRoomGeometry& room,float dt);
    const Mat4& Transform()const{return transform;}
    bool Visible()const{return initialized;}
    bool Held()const{return owner>=0;}
    bool Consumed()const{return consumed;}
    bool InRoom()const{return inRoom;}
private:
    bool initialized=false,inRoom=false,consumed=false;
    int owner=-1;
    std::array<bool,2> ready{},previousDown{};
    Mat4 transform,localTransform,gripOffset;
    bool finishing=false;
    float grabHalf=.035f,insideHalf=.027f,holdTime=0,finishTime=0,finishStart=0;
    std::array<float,3> finishPivot{};
    float fallSpeed=0;
};
}
