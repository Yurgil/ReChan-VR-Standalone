#pragma once
#include "pddi/quest/MRInteraction.h"
#include "pddi/quest/MRRoomGeometry.h"
#include <cstdint>
namespace questxr {
struct MRCharacterTarget {
    bool valid=false;
    uint64_t identity=0;
    Mat4 modelToPortal;
    std::array<float,3> low{},high{},standingLow{},standingHigh{},grip{};
    float standingHeight=1,up=1;
};
inline constexpr float kMRJackieStandingHeight=.10f;
inline constexpr float kMRJackieMinHeight=.05f,kMRJackieMaxHeight=1.74f*.9f;
struct MRCharacterInput {
    bool valid=false;
    unsigned controllers=0;
    float moveX=0,moveY=0,jumpTrigger=0,resize=0;
    bool punch=false,kick=false,roll=false;
    Mat4 headPose;
};
enum class MRCharacterAction{None,Punch,Kick,Roll};
enum class MRCharacterState{InGame,Held,Falling,Grounded,Returning};
class MRCharacter {
public:
    void SetTarget(const MRCharacterTarget& value);
    void Invalidate();
    void Cancel();
    void Update(const std::array<MRPointer,2>& hands,const PortalPlacement& window,const std::array<float,3>& eye,const MRRoomGeometry& room,bool windowBusy,float dt,const MRCharacterInput& input={});
    bool Active()const{return state!=MRCharacterState::InGame;}
    bool Held()const{return state==MRCharacterState::Held;}
    bool Grounded()const{return state==MRCharacterState::Grounded;}
    bool Consumed()const{return consumed;}
    bool HasTarget()const{return target.valid;}
    bool InRoom()const{return state==MRCharacterState::Grounded||state==MRCharacterState::Falling;}
    bool Moving()const{return Grounded()&&moveSpeed>.01f&&action==MRCharacterAction::None;}
    float MoveSpeed()const{return moveSpeed;}
    float StandingHeight()const{return standingHeight;}
    MRCharacterAction Action()const{return action;}
    float ActionProgress()const;
    bool Returning()const{return state==MRCharacterState::Returning;}
    float ReturnProgress()const{return returnTime/.3f;}
    float Elapsed()const{return elapsed;}
    int ClipKind()const{return extracted&&!crossing&&!Returning()?2:1;}
    MRCharacterState State()const{return state;}
    const Mat4& Transform()const{return transform;}
    const Mat4& ContactPose()const{return contact;}
    const MRCharacterTarget& Snapshot()const{return snapshot;}
    const std::array<MRPointerVisual,2>& Visuals()const{return visuals;}
private:
    bool PickPortal(const MRPointer& hand,const PortalPlacement& window,const std::array<float,3>& eye,std::array<float,3>& contactPoint)const;
    bool PickRoom(const MRPointer& hand)const;
    void Grab(int hand,const MRPointer& pointer,const PortalPlacement& window,bool fromGame);
    void BeginReturn();
    void UpdateRoom(const MRRoomGeometry& room,float dt,const MRCharacterInput& input);
    std::array<float,3> ModelFoot()const;
    void Face(float yaw);
    MRCharacterTarget target,snapshot;
    MRCharacterState state=MRCharacterState::InGame;
    Mat4 transform,sourceTransform,canonical,grabTransform,returnFrom,contact;
    std::array<float,3> anchorOffset{},gripPoint{};
    std::array<bool,2> ready{},previousDown{};
    std::array<MRPointerVisual,2> visuals{};
    MRCharacterAction action=MRCharacterAction::None;
    bool controlsReady=false,jumpDown=false,previousPunch=false,previousKick=false,previousRoll=false;
    float velocityX=0,velocityZ=0,moveSpeed=0,actionTime=0;
    float standingHeight=kMRJackieStandingHeight;
    unsigned inputSources=0;
    int owner=-1;
    bool consumed=false,extracted=false,crossing=true;
    float age=0,elapsed=0,returnTime=0,settleTime=0,fallSpeed=0,initialHandYaw=0;
};
}
