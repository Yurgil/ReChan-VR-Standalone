#pragma once
#if defined(RC_PLATFORM_QUEST)
#include "p3d/matrix.h"
class Thing;
class Model;
class SModel;
struct OriginalSTree;
namespace questcharacter {
void Capture(SModel* model,const Mat4& world);
bool Hide(const Model* model);
// The original player is absent from simulation while its isolated MR clone is out.
bool IsDetached(const Thing* thing);
void ModelDestroyed(const Model* model);
void TreeDestroyed(const OriginalSTree* tree);
}
#endif
