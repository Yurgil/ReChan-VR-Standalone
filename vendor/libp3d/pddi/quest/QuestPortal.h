#pragma once
#include "p3d/matrix.h"
#include <array>
#include <cmath>

namespace questxr {
inline constexpr float kPortalHalfWidth = 1.6f;
inline constexpr float kPortalHalfHeight = 1.2f;
inline constexpr float kPortalDepth = 2.4f;
inline constexpr float kSlateHalfWidth=1.82f,kSlateBottom=-1.51f,kSlateTop=1.62f;
// Magnify the authored camera image into the physical window. At the recenter
// eye, every point maps to exactly the original game NDC, including scripted zoom.
inline Mat4 CameraOptics(const Mat4& gameProjection) {
    Mat4 optics;
    optics.m[0] = kPortalHalfWidth / kPortalDepth * gameProjection.m[0];
    optics.m[5] = kPortalHalfHeight / kPortalDepth * gameProjection.m[5];
    return optics;
}

// A fragment is visible only behind the slate and through the inner aperture.
// Eye and surface coordinates are relative to the recenter anchor.
inline bool PortalRayVisible(const float* eye, const float* surface) {
    const float dz=surface[2]-eye[2];
    if (std::fabs(dz)<0.000001f) return false;
    const float t=(-kPortalDepth-eye[2])/dz;
    if (t<0 || t>1) return false;
    const float x=eye[0]+t*(surface[0]-eye[0]);
    const float y=eye[1]+t*(surface[1]-eye[1]);
    return std::fabs(x)<=kPortalHalfWidth && std::fabs(y)<=kPortalHalfHeight;
}

bool InitializePortalFrame();
void ShutdownPortalFrame();
void DrawPortalAlpha(const std::array<Mat4,2>& portalFromEye,
                     const std::array<std::array<float,4>,2>& tangents,
                     const std::array<unsigned,2>& eyeFramebuffers,int width,int height);
void DrawPortalFrame(const std::array<Mat4, 2>& eyeVP,
                     const std::array<unsigned, 2>& eyeFramebuffers,
                     int width, int height,bool editing=false);
}
