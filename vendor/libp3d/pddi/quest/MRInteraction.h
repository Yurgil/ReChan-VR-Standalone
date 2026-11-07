#pragma once
#include "pddi/quest/PortalPlacement.h"
#include "pddi/quest/QuestPortal.h"
#include <array>

namespace questxr {
struct MRPointer {
    Mat4 aim,grip;
    bool valid=false,down=false;
    int source=0; // 0 = Touch, 1 = actual tracked hand
};
struct MRPointerVisual {
    std::array<float,3> start{},end{};
    bool visible=false,active=false,direct=false,resize=false;
};
struct MRHandJoint {std::array<float,3> position{};float radius=.006f;bool valid=false;};
struct MRHandVisual {std::array<MRHandJoint,26> joints{};bool valid=false;};
// No OpenXR/GL dependencies: deterministic manipulation shared by hands/Touch.
class MRInteraction {
public:
    void Update(const std::array<MRPointer,2>& input,PortalPlacement& window,const std::array<float,3>& head);
    void Cancel();
    bool Active()const{return mask!=0;}
    bool Consumed()const{return consumed;}
    const std::array<MRPointerVisual,2>& Visuals()const{return visuals;}
private:
    struct Grip {bool direct=false,resize=false;float distance=0;int source=0;};
    bool Hit(const MRPointer& input,const PortalPlacement& window,Grip& grip)const;
    Mat4 GripPose(const MRPointer& input,const Grip& grip)const;
    std::array<Grip,2> grips{};
    std::array<bool,2> ready{},wasDown{};
    std::array<int,2> previousSource{-1,-1};
    std::array<MRPointerVisual,2> visuals{};
    unsigned mask=0;
    bool consumed=false;
    PortalPlacement baseline;
    std::array<Mat4,2> startPose{};
};
}
