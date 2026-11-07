# Build ReChan VR Standalone for Quest 3

Development version: `0.9.1-quest-dev` (versionCode 26).
App ID: `org.rechan.quest`; activity: `com.deivid22srk.rechan.GameActivity`.
The launcher label remains **ReChan Quest** for existing installations.

## Toolchain

The tested Android build uses:

| Component | Version |
|---|---|
| Java | JDK 17 |
| Android SDK / build tools | 35 / 35.0.0 |
| Android NDK | 27.0.12077973 |
| CMake | 3.22.1 |
| Gradle / Android Gradle Plugin | 8.12 / 8.9.2 |
| Khronos OpenXR Android loader | 1.1.49, Maven AAR + Prefab |
| ABI / C++ | arm64-v8a / C++20 |

Install the Android command-line tools and required packages using `sdkmanager`:

```bash
sdkmanager 'platform-tools' 'platforms;android-35' 'build-tools;35.0.0' \
  'ndk;27.0.12077973' 'cmake;3.22.1'
```

Set tool paths for your installation:

```bash
export JAVA_HOME=/absolute/path/to/jdk-17
export ANDROID_HOME=/absolute/path/to/android-sdk
```

Alternatively the helper defaults to `.toolchain/jdk/Contents/Home` and
`.toolchain/sdk` inside this repository. These local tools and all caches are
excluded from the source export. `GRADLE_USER_HOME` defaults to `.toolchain/gradle`.
Initial dependency resolution requires internet access.

From a Bash shell in the repository root:

```bash
scripts/build_quest.sh
```

Output: `android/app/build/outputs/apk/debug/app-debug.apk`.
No game image is needed to compile. The APK packages the engine, launcher and
ReChan support files from `res/pc`, not a user's disc image or extracted game data.

The inherited `android/app/rechan.keystore` is a **public development key**, retained
for reproducible sideload updates. It is not a private production credential.
The current ADB importer requires a debuggable APK; use `assembleDebug` for the
supported installation path. A non-debuggable release build needs a different,
validated importer before it can be offered as a self-contained installation route.

## Tests

Host C++ tests require a C++20 compiler. On macOS, install Xcode Command Line Tools.
Run after the Android dependency resolution so OpenXR headers are available:

```bash
scripts/test_quest_math.sh
scripts/test_quest_mr.sh
```

`OPENXR_HEADERS` can point to an include directory containing `openxr/openxr.h`
if it is not in the repository's Gradle cache. Tests cover stereo/placement,
frame interaction, character transfer, room support, surface effects, fixed game
timing, input, lifecycle and real MR adapters with a fake OpenXR runtime.
They do not substitute for binocular, gesture, performance or whole-game checks
on a headset.

## Install and diagnose

Follow [INSTALL.md](INSTALL.md) ([русский](INSTALL_RU.md)). Supply your own lawful
SLUS-00684 disc image. With local tools:

```bash
scripts/run_quest.sh game '/absolute/path/to/your/game.bin'
```

An explicit diagnostic bypasses gameplay and needs no disc image:

```bash
scripts/run_quest.sh diagnostic
```

This intentionally shows three stereo cubes. They are separate from ordinary
MR window editing, which no longer creates a diagnostic cube.

To read logs with your SDK's ADB:

```bash
adb logcat -v threadtime ReChanXR:I ReChan:I rechan-gles:I rechan:I AndroidRuntime:E DEBUG:E '*:S'
adb exec-out run-as org.rechan.quest cat files/rechan.log
```

Logs can include prior process runs; match the current PID and timestamps.
Redact local paths, device identifiers and personal information before sharing.
Do not publish passthrough recordings or room scans as generic diagnostics.
A submitted-frame count alone does not establish sustained 72 Hz or GPU headroom.

## Current runtime behavior

The simulation runs at 30 Hz independently of XR presentation. Author-camera
projection frames a stereo diorama inside a procedural clapperboard; menus/movies
use a 4:3 canvas. Texture filtering uses bilinear interpolation after palette
lookup. Dynamic environment shadows use two 1024×1024 cascades; character models
keep authored lighting and do not receive the additional shadow-map pass. MSAA
is off. Unsupported desktop display settings are hidden.

MR window/hand controls, optional one-room scene loading and miniature-character
transfer are described in the installation guide. Room loading and transfer were
confirmed on Quest 3 in 0.8.1 and 0.9.0. Movement control for the external miniature,
environment-depth occlusion and full-game/performance validation remain unfinished.
No persistent window anchor is created or saved.
