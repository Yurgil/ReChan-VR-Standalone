#pragma once
#include "p3d/matrix.h"
#include "pddi/quest/MRCharacter.h"
#include <jni.h>
#include <EGL/egl.h>
#include <string>

// All functions except SetAndroidContext run on the engine/render thread.
namespace questxr {
void SetAndroidContext(JavaVM* vm, jobject activity);
bool Initialize(EGLDisplay display, EGLConfig config, EGLContext context);
void Shutdown();
int RunDiagnostic();
void SetPortalEnabled(bool enabled);
void SetPortalCaption(const char* scene,unsigned take);
// Shared gate for frame manipulation and future MR character interaction.
bool MRInteractionEnabled();
bool MixedRealityAvailable();
bool MixedRealityRequested();
void SetMixedReality(bool enabled);
bool JackieInteractionRequested();
void SetJackieInteraction(bool enabled);
void RescanRoom();
// Engine bridge: the original player remains frozen; only its isolated skeleton is drawn here.
void SetCharacterDrawCallback(void (*callback)());
void PublishCharacter(MRCharacterTarget target,const Mat4& gameView,const Mat4& world);
void InvalidateCharacter();
bool CharacterActive();
bool WorldSimulationPaused();
const MRCharacter& Character();
const char* RoomStatusToken();
int RoomSurfaceCount();
void SetGameProjection(const Mat4& projection);
void PollEvents();
bool ShouldClose();
void BeginFrame();
void EndFrame();
int Width();
int Height();
unsigned Framebuffer();
void UploadMeshMatrices(int location, const Mat4& gameView, const Mat4& model);
void DrawScreenArrays(unsigned mode, int first, int count, unsigned program);
std::string VertexShader(const char* source, const char* program);
std::string FragmentShader(const char* source, const char* program);
void SetScreenScissor(int x, int y, int w, int h);
bool IsBoundsVisible(const Mat4& gameView, const Mat4& model, const float* low, const float* high);
bool IsPolygonVisible(const Mat4& gameView, const Mat4& model, const float* xyz, int count);
const char* Error();
}
