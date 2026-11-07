# Build ReChan VR Standalone for Quest 3

Development version: `0.10.4-quest-dev` (versionCode 31).
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
confirmed on Quest 3 in 0.8.1 and 0.9.0. 0.10.0 adds Touch locomotion/jump/actions for a 10 cm miniature with independent
room collision/support, neutral-input handoff, swept substeps and room-floor bounds;
headset acceptance is pending. Environment-depth occlusion and full-game/performance
validation remain unfinished.
No persistent window anchor is created or saved.


0.10.1 removes the MR window-size menu/binding; resizing is direct manipulation.
Optical trackers are lazily created only in shared editing mode with both Touch
controllers put down, and destroyed on Touch pickup, mode exit or focus loss.
The standalone roll now uses the same clip24 as Player AS_DIVE_ROLL, with correct
sequence-part sampling. SCENE shows the current area/stage or MAIN MENU; TAKE is
counted once per confirmed nonfatal death and reset by main-menu/game-over exit.
Label geometry is uploaded on content change only, keeping the existing stable
ink/frame layering. Procedural lettering supports A–Z and every take digit.


0.10.2 pairs simultaneous-hand tracking resume with pause before destroying trackers.
OpenXR is the exclusive Quest input writer; Android/mobile touch paths are disabled.
A per-control release latch replaces the global wait-for-neutral pad lock. Before extraction, gameplay
suspends during window editing and when Touch actions are unavailable; XR focus
loss always suspends gameplay.
Rendering and input polling continue, and the pause menu remains accessible.
The extracted Jackie supports right-stick height adjustment from 5 cm to 1.566 m,
with a fixed foot pivot, scaled room collisions and obstacle-limited growth.


0.10.3 lowers maximum MR standing-reference height to 90% of the former limit
(1.566 m). The original level now runs while the MR clone is extracted: only the
hidden player is skipped by movement, Think, animation and collision/combat paths;
its animation tick baseline is maintained to avoid fast-forward on return.
World reconstruction uses a common .002 scale in render, portal clipping, stereo
culling and character picking. This pushes close camera geometry behind the frame
without widening the authored camera or enabling foreground spill. Reference-eye
projection and depth ordering remain unchanged; head-motion parallax is reduced.


0.10.4 fixes false level pauses when the runtime temporarily deactivates one Touch
profile/stick. Availability uses each hand's stick/trigger/grip actions independently;
the remaining controller keeps working. Only sustained absence of both controllers
(0.5 seconds), explicit window editing or focus loss suspends the source level.
Tag pushes matching `v*` build and audit a signed ARM64 preview APK on GitHub Actions,
then publish it and SHA256SUMS to a GitHub prerelease. Original game assets are not
required by this build and are rejected by `scripts/verify_quest_apk.py`.
