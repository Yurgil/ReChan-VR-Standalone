#pragma once
#include "pddi/quest/QuestMath.h"
#include <algorithm>

namespace questxr {
// One placement for meshes, slate, UI, clipping and future interaction.
// pose is local-window -> OpenXR reference space; scale never changes game units.
struct PortalPlacement {
    Mat4 pose;
    float scale=1;
    void Reset(const Mat4& referencePose){pose=referencePose;scale=1;}
    void SetScale(float value){if(std::isfinite(value))scale=std::clamp(value,.2f,2.f);}
    // Scale around the aperture centre, keeping its reference-space position fixed.
    void SetCentered(const Mat4& centrePose,float localCentreZ,float value){
        SetScale(value);pose=centrePose*Translation(0,0,-localCentreZ*scale);
    }
    Mat4 LocalToReference() const{return pose*Scale(scale);}
    Mat4 ReferenceToLocal() const{return Scale(1/scale)*RigidInverse(pose);}
};
}
