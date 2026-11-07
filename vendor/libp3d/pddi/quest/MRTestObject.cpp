#include "pddi/quest/MRTestObject.h"
#include "pddi/quest/MRTransferMath.h"
namespace questxr {
void MRTestObject::Reset(){initialized=inRoom=consumed=finishing=false;owner=-1;ready={};previousDown={};holdTime=finishTime=fallSpeed=0;}
void MRTestObject::Update(const std::array<MRPointer,2>& pointers,const PortalPlacement& window,bool windowBusy,float dt){
    dt=std::clamp(dt,0.f,.05f);
    consumed=owner>=0;
    if(!initialized){localTransform=Translation(0,0,-kPortalDepth-.18f)*Scale(.09f);initialized=true;}
    if(owner<0&&finishing){
        finishTime+=dt;const float amount=TransferEase(finishTime/.16f);
        const float target=inRoom?kRoomObjectHeight*.5f:.09f;
        if(inRoom)transform=ResizeAtPoint(transform,finishPivot,finishStart+(target-finishStart)*amount);
        else localTransform=ResizeAtPoint(localTransform,finishPivot,finishStart+(target-finishStart)*amount);
        if(finishTime>=.16f)finishing=false;
    }
    FollowWindow(window);
    if(owner>=0){
        const auto& hand=pointers[owner];
        if(!hand.valid||hand.source!=1){Reset();consumed=true;return;}
        transform=hand.grip*gripOffset;
        holdTime+=dt;
        const float target=TransferHalf(insideHalf,TransferDistance(window,hand.grip));
        const float half=grabHalf+(target-grabHalf)*TransferEase(holdTime/.12f);
        transform=ResizeAtPoint(transform,{hand.grip(0,3),hand.grip(1,3),hand.grip(2,3)},half);
        if(!hand.down){
            owner=-1;const auto local=window.ReferenceToLocal()*transform;
            inRoom=local(2,3)>-kPortalDepth;
            if(!inRoom&&(std::fabs(local(0,3))>kPortalHalfWidth||std::fabs(local(1,3))>kPortalHalfHeight)){Reset();consumed=true;return;}
            if(!inRoom)localTransform=local;
            finishing=true;finishTime=0;
            const auto pivot=inRoom?hand.grip:window.ReferenceToLocal()*hand.grip;
            finishPivot={pivot(0,3),pivot(1,3),pivot(2,3)};
            finishStart=ObjectHalf(inRoom?transform:localTransform);
        }
    }
    for(int i=0;i<2;++i){
        const auto& hand=pointers[i];const bool valid=hand.valid&&hand.source==1;
        if(!valid)ready[i]=false;else if(!hand.down)ready[i]=true;
        if(owner<0&&!windowBusy&&valid&&ready[i]&&hand.down&&!previousDown[i]){
            const float dx=hand.grip(0,3)-transform(0,3),dy=hand.grip(1,3)-transform(1,3),dz=hand.grip(2,3)-transform(2,3);
            const float half=ObjectHalf(transform);
            if(std::sqrt(dx*dx+dy*dy+dz*dz)<half*1.75f+.025f){owner=i;gripOffset=RigidInverse(hand.grip)*transform;grabHalf=half;insideHalf=.09f*window.scale;holdTime=0;finishing=false;consumed=true;}
        }
        previousDown[i]=hand.down;
    }
}
void MRTestObject::ApplyRoom(const MRRoomGeometry& room,float dt){
    if(!initialized||owner>=0||!inRoom||finishing||!room.ready){fallSpeed=0;return;}
    dt=std::clamp(dt,0.f,.05f);fallSpeed=std::max(fallSpeed-9.81f*dt,-3.f);
    const float movement=fallSpeed*dt;
    const float halfY=std::fabs(transform(1,0))+std::fabs(transform(1,1))+std::fabs(transform(1,2));
    MRRoomSupport support;
    if(room.Support({transform(0,3),transform(1,3)-halfY+.01f,transform(2,3)},-movement+.011f,ObjectHalf(transform)*.7f,support)){
        transform(1,3)=support.point[1]+halfY+.001f;fallSpeed=0;
    }else transform(1,3)+=movement;
}
}
