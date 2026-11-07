#pragma once
#include "pddi/quest/MRPortalSurface.h"
namespace questxr {
bool InitializeMRPortalSurface();
void ShutdownMRPortalSurface();
void DrawMRPortalSurface(const std::array<Mat4,2>& windowVP,const std::array<unsigned,2>& fbos,int width,int height,const PortalPlacement& window,const MRPortalSurface& surface);
}
