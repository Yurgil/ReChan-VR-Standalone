#pragma once
#include <jni.h>
namespace questxr {
// 0 = not requested, 1 = granted, 2 = system prompt pending, -1 = denied/error.
int ScenePermissionState(JavaVM* vm,jobject activity);
void RequestScenePermission(JavaVM* vm,jobject activity);
}
