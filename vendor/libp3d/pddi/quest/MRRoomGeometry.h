#pragma once
#include "pddi/quest/QuestMath.h"
#include <array>
#include <string>
#include <vector>
namespace questxr {
struct MRRoomSurface {
    Mat4 pose;
    std::string labels;
    std::vector<std::array<float,2>> boundary;
    std::array<float,3> low{},extent{};
    bool located=false,volume=false,floor=false,wall=false;
};
struct MRRoomSupport {std::array<float,3> point{},normal{};float distance=0;int surface=-1;};
struct MRRoomGeometry {
    std::vector<MRRoomSurface> surfaces;
    bool ready=false;
    static bool Contains(const std::vector<std::array<float,2>>& polygon,float x,float y,float margin=0);
    bool Support(const std::array<float,3>& origin,float maxDrop,float radius,MRRoomSupport& result)const;
    bool ContainsFloorProjection(const std::array<float,3>& point,float margin=0)const;
    bool BlocksSphere(const std::array<float,3>& centre,float radius)const;
};
}
