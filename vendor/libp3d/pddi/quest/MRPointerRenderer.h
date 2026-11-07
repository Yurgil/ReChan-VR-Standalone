#pragma once
#include "pddi/quest/MRInteraction.h"
namespace questxr {
bool InitializeMRPointers();
void ShutdownMRPointers();
void DrawMRPointers(const std::array<Mat4,2>& referenceVP,const std::array<unsigned,2>& fbos,int width,int height,const std::array<MRPointerVisual,2>& pointers);
}
