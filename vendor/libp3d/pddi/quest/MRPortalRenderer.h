#pragma once
#include "pddi/quest/MRTestObject.h"
namespace questxr {
bool InitializeMRPortalObjects();
void ShutdownMRPortalObjects();
void DrawMRPortalObjects(const std::array<Mat4,2>& vp,const std::array<unsigned,2>& fbos,int width,int height,const PortalPlacement& window,const std::array<float,6>& eyes,const std::array<MRHandVisual,2>& hands,const MRTestObject& object,const MRRoomGeometry* room=nullptr);
}
