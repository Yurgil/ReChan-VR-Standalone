// AndroidShell.cpp — NativeActivity glue: lifecycle, filesystem setup, game
// thread. The game loop itself (src/gen/main.cpp GameMain) runs on a dedicated
// thread; this thread pumps the android_app command/input queues until the
// activity is destroyed.
#include <android/log.h>
#include <android/native_window.h>
#include <android_native_app_glue.h>

#include <filesystem>
#include <pthread.h>
#include <sys/stat.h>
#include <unistd.h>

#include <string>
#include <vector>

#include "pddi/gles/AndroidPlatform.h"
#include "pc/audio.h"
#include "pddi/quest/QuestXR.h"

extern int GameMain(int argc, char** argv);
bool RechanAndroidHandleInputEvent(AInputEvent* event);
void RechanAndroidResetTouch();

// Belt-and-suspenders anchor: referencing glue's entry symbol here guarantees
// the linker pulls native_app_glue's object out of the static archive (the
// -u link option does the same; this survives any flag reshuffling).
extern "C" void ANativeActivity_onCreate(ANativeActivity*, void*, std::size_t);
[[maybe_unused]] static const void* const kGlueAnchor =
    reinterpret_cast<const void*>(&ANativeActivity_onCreate);

namespace {

android_app* g_app = nullptr;
pthread_t g_gameThread = {};
bool g_gameThreadStarted = false;
bool g_diagnostic = false;

bool DiagnosticRequested(ANativeActivity* activity) {
    JNIEnv* env = nullptr;
    const bool attach = activity->vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6) == JNI_EDETACHED;
    if (attach && activity->vm->AttachCurrentThread(&env, nullptr) != JNI_OK) return false;
    if (!env) return false;
    const jclass activityClass = env->GetObjectClass(activity->clazz);
    const jmethodID getIntent = env->GetMethodID(activityClass, "getIntent", "()Landroid/content/Intent;");
    const jobject intent = env->CallObjectMethod(activity->clazz, getIntent);
    bool result = false;
    if (intent) {
        const jclass intentClass = env->GetObjectClass(intent);
        const jmethodID getBoolean = env->GetMethodID(intentClass, "getBooleanExtra", "(Ljava/lang/String;Z)Z");
        const jstring key = env->NewStringUTF("quest_diagnostic");
        result = env->CallBooleanMethod(intent, getBoolean, key, JNI_FALSE);
        env->DeleteLocalRef(key);
        env->DeleteLocalRef(intentClass);
        env->DeleteLocalRef(intent);
    }
    env->DeleteLocalRef(activityClass);
    if (env->ExceptionCheck()) { env->ExceptionClear(); result = false; }
    if (attach) activity->vm->DetachCurrentThread();
    return result;
}
ANativeWindow* g_acquiredWindow = nullptr;

struct AssetTask {
    std::string assetPath; // path inside the APK assets/
    std::string outPath;   // absolute output file
};

void ExtractManifestAssets(AAssetManager* mgr) {
    AAsset* manifest = AAssetManager_open(mgr, "pc_manifest.txt", AASSET_MODE_BUFFER);
    if (!manifest) {
        __android_log_print(ANDROID_LOG_ERROR, "rechan",
                            "pc_manifest.txt missing from APK assets");
        return;
    }
    const off_t len = AAsset_getLength64(manifest);
    const char* begin = static_cast<const char*>(AAsset_getBuffer(manifest));
    std::vector<char> lines(begin, begin + len);
    AAsset_close(manifest);

    std::string line;
    size_t pos = 0;
    while (pos <= lines.size()) {
        if (pos == lines.size() || lines[pos] == '\n') {
            // flush current line
            while (!line.empty() && (line.back() == '\r' || line.back() == '\n'))
                line.pop_back();
            if (!line.empty()) {
                AAsset* asset =
                    AAssetManager_open(mgr, line.c_str(), AASSET_MODE_STREAMING);
                if (!asset) {
                    __android_log_print(ANDROID_LOG_WARN, "rechan",
                                        "asset missing in APK: %s", line.c_str());
                    line.clear();
                    ++pos;
                    continue;
                }
                const std::string outPath = line; // relative to CWD (<files>)
                // Skip when already extracted with identical size.
                FILE* existing = fopen(outPath.c_str(), "rb");
                if (existing) {
                    fseek(existing, 0, SEEK_END);
                    const long sz = ftell(existing);
                    fclose(existing);
                    if (sz == static_cast<long>(AAsset_getLength64(asset))) {
                        AAsset_close(asset);
                        line.clear();
                        ++pos;
                        continue;
                    }
                }

                const std::filesystem::path parent =
                    std::filesystem::path(outPath).parent_path();
                if (!parent.empty()) {
                    std::filesystem::create_directories(parent);
                }
                FILE* out = fopen(outPath.c_str(), "wb");
                if (!out) {
                    __android_log_print(ANDROID_LOG_ERROR, "rechan",
                                        "cannot write %s", outPath.c_str());
                    AAsset_close(asset);
                    line.clear();
                    ++pos;
                    continue;
                }
                char buf[65536];
                int read;
                while ((read = AAsset_read(asset, buf, sizeof(buf))) > 0) {
                    fwrite(buf, 1, static_cast<size_t>(read), out);
                }
                fclose(out);
                AAsset_close(asset);
            }
            line.clear();
            ++pos;
        }
        else {
            line.push_back(lines[pos]);
            ++pos;
        }
    }
}

void PrepareFileSystem(android_app* app) {
    const char* filesDir = app->activity->internalDataPath;
    if (!filesDir || chdir(filesDir) != 0) {
        __android_log_print(ANDROID_LOG_ERROR, "rechan",
                            "chdir to internalDataPath failed (%s)",
                            filesDir ? filesDir : "<null>");
    }
    mkdir("discimage", 0755);
    mkdir("userfiles", 0755);
    mkdir("~mods", 0755);
    mkdir("minidumps", 0755);
    ExtractManifestAssets(app->activity->assetManager);
    __android_log_print(ANDROID_LOG_INFO, "rechan", "CWD=%s", getcwd(nullptr, 0));
}

void HandleCommand(android_app* app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW:
            if (app->window) {
                ANativeWindow_acquire(app->window);
                g_acquiredWindow = app->window;
                androidbridge::SetNativeWindow(app->window);
            }
            break;
        case APP_CMD_TERM_WINDOW:
            // Fingers still down when the surface goes away will never get
            // their UP events — drop them or the virtual joystick stays
            // deflected and ghost fingers hog the touch slots forever.
            RechanAndroidResetTouch();
            androidbridge::SetNativeWindow(nullptr);
            // The EGL surface keeps its own strong reference to the window, so
            // dropping ours here is safe; it only guards the handoff window.
            if (g_acquiredWindow) {
                ANativeWindow_release(g_acquiredWindow);
                g_acquiredWindow = nullptr;
            }
            break;
        case APP_CMD_PAUSE:
        case APP_CMD_STOP:
        case APP_CMD_LOST_FOCUS:
            // Same story as TERM_WINDOW: the system takes the fingers away
            // (shade pulled, home gesture, incoming call) without per-pointer
            // UPs. Reset the touch state so the HUD starts clean on resume.
            RechanAndroidResetTouch();
            // Backgrounded: silence the output device (the game loop parks on
            // its own once the surface goes away).
            if (cmd == APP_CMD_PAUSE) {
                AudioEngine::Suspend();
            }
            break;
        case APP_CMD_RESUME:
            AudioEngine::Resume();
            break;
        case APP_CMD_DESTROY:
            androidbridge::RequestExit();
            if (g_acquiredWindow) {
                ANativeWindow_release(g_acquiredWindow);
                g_acquiredWindow = nullptr;
            }
            break;
        default:
            break;
    }
}

int HandleInput(android_app*, AInputEvent* event) {
    return RechanAndroidHandleInputEvent(event) ? 1 : 0;
}

void PumpEventsOnce(android_app* app) {
    int events = 0;
    android_poll_source* source = nullptr;
    const int ident =
        ALooper_pollOnce(-1, nullptr, &events, reinterpret_cast<void**>(&source));
    if (ident >= 0 && source) {
        source->process(app, source);
    }
}

void PumpEventsUntilWindowOrDestroy(android_app* app) {
    while (!app->destroyRequested && !androidbridge::PeekCurrentWindow()) {
        PumpEventsOnce(app);
    }
}

void* GameThreadFunc(void*) {
    char argv0[] = "rechan";
    char diagnosticArg[] = "--quest-diagnostic";
    char* argv[] = {argv0, g_diagnostic ? diagnosticArg : nullptr, nullptr};
    GameMain(g_diagnostic ? 2 : 1, argv);

    // The engine loop ended (in-game quit or exit request): close the app,
    // unless the system already started tearing it down.
    if (!g_app->destroyRequested) {
        ANativeActivity_finish(g_app->activity);
    }
    return nullptr;
}

void StartGameThread() {
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setstacksize(&attr, 4 * 1024 * 1024); // deep recursion in FE code
    g_gameThreadStarted = true;
    pthread_create(&g_gameThread, &attr, GameThreadFunc, nullptr);
    pthread_attr_destroy(&attr);
}

} // namespace

void android_main(android_app* state) {
    __android_log_print(ANDROID_LOG_INFO, "rechan", "New activity; clearing previous exit=%d", androidbridge::ExitRequested());
    androidbridge::BeginActivity();
    g_gameThreadStarted = false;
    RechanAndroidResetTouch();
    g_app = state;
    g_diagnostic = DiagnosticRequested(state->activity);
    questxr::SetAndroidContext(state->activity->vm, state->activity->clazz);
    state->onAppCmd = HandleCommand;
    state->onInputEvent = HandleInput;

    PrepareFileSystem(state);

    // Wait for a window (or destruction) before touching EGL.
    PumpEventsUntilWindowOrDestroy(state);

    if (!state->destroyRequested && androidbridge::PeekCurrentWindow()) {
        StartGameThread();
    }

    // Keep servicing the OS queues for as long as we live.
    while (!state->destroyRequested) {
        PumpEventsOnce(state);
    }

    if (g_gameThreadStarted) {
        pthread_join(g_gameThread, nullptr);
        g_gameThreadStarted = false;
    }
    androidbridge::SetNativeWindow(nullptr);
    g_app = nullptr;
}
