#include "pddi/quest/QuestScenePermission.h"
namespace questxr {
namespace {
struct Environment {
    JavaVM* vm;JNIEnv* env=nullptr;bool attached=false;
    explicit Environment(JavaVM* value):vm(value){if(vm){attached=vm->GetEnv(reinterpret_cast<void**>(&env),JNI_VERSION_1_6)==JNI_EDETACHED;if(attached&&vm->AttachCurrentThread(&env,nullptr)!=JNI_OK)env=nullptr;}}
    ~Environment(){if(attached&&env)vm->DetachCurrentThread();}
};
}
int ScenePermissionState(JavaVM* vm,jobject activity){
    Environment e(vm);if(!e.env||!activity)return -1;
    auto type=e.env->GetObjectClass(activity);auto method=e.env->GetMethodID(type,"questScenePermissionState","()I");
    int state=-1;if(method)state=e.env->CallIntMethod(activity,method);
    if(e.env->ExceptionCheck()){e.env->ExceptionClear();state=-1;}e.env->DeleteLocalRef(type);return state;
}
void RequestScenePermission(JavaVM* vm,jobject activity){
    Environment e(vm);if(!e.env||!activity)return;
    auto type=e.env->GetObjectClass(activity);auto method=e.env->GetMethodID(type,"requestQuestScenePermission","()V");
    if(method)e.env->CallVoidMethod(activity,method);if(e.env->ExceptionCheck())e.env->ExceptionClear();e.env->DeleteLocalRef(type);
}
}
