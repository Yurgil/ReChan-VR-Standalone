#pragma once
#if defined(RC_PLATFORM_QUEST)
#include "p3d/matrix.h"
class Model;
class SModel;
struct OriginalSTree;
namespace questcharacter {
void Capture(SModel* model,const Mat4& world);
bool Hide(const Model* model);
void ModelDestroyed(const Model* model);
void TreeDestroyed(const OriginalSTree* tree);
}
#endif
