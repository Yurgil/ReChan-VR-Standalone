# Provenance and credits

ReChan VR Standalone uses the **PC ReChan codebase as its game-code baseline**.
It was not made by replacing that baseline with the older Android fork.

| Source | Baseline | Role |
|---|---|---|
| [SilverwireGames/ReChan](https://github.com/SilverwireGames/ReChan) | `ecd39935559d024992b2049576ea0ecc3b204b8c` | Reimplemented game logic, data loaders, native extractor, authored-camera behavior, Pure3D/PDDI integration, support files and the existing GLES backend |
| [deivid22srk/ReChan-Android](https://github.com/deivid22srk/ReChan-Android) | `a8986853b74fc718e49fdb9f6c1c5f8a01d35aba` | Selected Gradle/CMake/NDK setup, Android activities/shell, platform bridge, input, audio/lifecycle and mobile support adaptations |
| Quest integration in this project | Current source | OpenXR native stereo, head tracking, Touch actions, Quest frame pacing/lifecycle fixes, clapperboard presentation, passthrough, controller/hand window manipulation, system room scene access and miniature-character transfer |

The Android fork is based on an earlier upstream snapshot. Selected platform changes
were adapted to the newer PC baseline instead of merging the entire older fork.
The Java namespace `com.deivid22srk.rechan` reflects that inherited Android shell;
the Quest installation package is `org.rechan.quest`.

The project also uses Khronos OpenXR, Android native_app_glue and the libraries
vendored with upstream ReChan. Keep their copyright and license notices with any
source or binary redistribution. The public development keystore inherited from
the Android fork is included for reproducible sideload updates; it is not a private
production credential.

The upstream [MIT copyright notice](../../LICENSE) is preserved. No ownership of
the original game's assets or intellectual property is claimed by this port.
