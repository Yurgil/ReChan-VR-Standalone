# ReChan VR Standalone

Native **Meta Quest 3** VR and mixed reality port of *Jackie Chan Stuntmaster*,
built on ReChan. The game runs on the headset: a PC is needed for installation,
not for streaming or playing.

**You must own a legally obtained copy of Jackie Chan Stuntmaster
(NTSC-U, SLUS-00684) and supply your own disc image. No game disc, extracted
PlayStation game data, levels, models, audio or movies are distributed here.**

Development build: **0.10.4-quest-dev**. Repository: [Yurgil/ReChan-VR-Standalone](https://github.com/Yurgil/ReChan-VR-Standalone).
**Download a ready-to-install APK:** [GitHub Releases](https://github.com/Yurgil/ReChan-VR-Standalone/releases).
The installed app is currently named **ReChan Quest** (`org.rechan.quest`).

**This project is still in active development. It has been tested only on Meta
Quest 3. Operation on other headsets is not guaranteed.**

## What works

- Native ARM64 execution with OpenXR stereo rendering and head tracking.
- Third-person gameplay in a clapperboard diorama, preserving the authored camera.
- Quest Touch controls; 4:3 menus and movies.
- Optional passthrough MR, with window movement and resizing using Touch or hands.
- A shared interaction mode, toggled by pressing both thumbsticks together.
- Optional use of one room scanned by Quest: take Jackie through the window,
  hold him as a miniature, place him on a surface, and return him to the game.
  His reference standing height outside the window is 10 cm; he uses an airborne
  animation while held. Once placed, he supports Touch movement, jumps, attacks
  and rolls. Right stick up/down changes his reference height from 5 to 156.6 cm.
- The original level continues while Jackie is outside; its hidden player is
  isolated from movement and combat until return.

This is an experimental port, not a fully validated release. Whole-game testing
and performance tuning are still in progress. MR room locomotion and direct height adjustment are experimental. Other headsets have not been tested.
Window editing no longer spawns the old diagnostic cube. Slate SCENE shows the
current stage (or MAIN MENU); TAKE starts at 01 and increases after death/restart.
Leaving for the main menu or game over resets it. Pausing preserves the count.

## Installation

Read the [installation guide](docs/quest/INSTALL.md)
([инструкция на русском](docs/quest/INSTALL_RU.md)). In brief:

1. Enable Quest Developer Mode, connect over USB and approve USB debugging.
2. Download the **APK from [Releases](https://github.com/Yurgil/ReChan-VR-Standalone/releases)**, or [build it](docs/quest/BUILDING.md).
   Preview APKs use the stable sideload key and are debuggable for the data importer.
3. Extract your own disc archive on your computer. Supply the **BIN/ISO image**,
   not the ZIP/7z archive or CUE descriptor.
4. From a Bash shell in this repository, install and copy your image:

   ```bash
   ADB=/absolute/path/to/adb APK=/absolute/path/to/app-debug.apk \
     scripts/run_quest.sh game '/absolute/path/to/your/game.bin'
   ```

   If using the local toolchain and freshly built APK, just run:

   ```bash
   scripts/run_quest.sh game '/absolute/path/to/your/game.bin'
   ```

5. Complete first-run extraction in the headset. Later, launch **ReChan Quest**
   from the headset's library / Unknown Sources without a computer or USB cable.

The current supported importer uses ADB and `run-as`, so it requires a debuggable
APK. The inherited Android folder-picker importer is not validated on Quest.
Updates preserve saves when installed with `adb install -r` using the same signing key.

## Controls

| Touch input | Action |
|---|---|
| Left stick | Move / menu navigation |
| A, lower right button | Punch / menu confirm |
| B, upper right button | Kick / menu back |
| Right trigger | Jump |
| Left trigger | Grab / open doors |
| Right grip | Roll |
| Left grip | Focus on an enemy |
| Y, upper left button | Counterattack |
| Right stick click | Status interface |
| Left menu button | Pause |
| Left stick click | Recenter the window |
| Both stick clicks together, in MR | Toggle interaction / window editing |

Triggers activate at 5% travel and release below 2%. Gameplay actions are blocked
while interaction mode is enabled. In MR, single stick clicks are resolved on
release to distinguish them from the two-stick combination.

## Mixed reality

Enable **Options → Mixed Reality**. Resize the window directly with hands or Touch.
Press both thumbsticks together: the frame outline turns cyan. Grab a frame strip
with a Touch trigger/grip to move it, a corner to resize, or use two controllers
to scale. Put the controllers down, wait 1.25 seconds, then use a hand pinch on
the frame. Press both sticks again to return to gameplay.

For character transfer, enable **Options → Room interaction → Interact with Jackie**.
Enter interaction mode and allow the room permission. The app reuses a saved Quest
room scan or requests the system setup if needed. Basic MR window editing needs
no room scan. In a playable level, move the window within reach, put down Touch,
reach into the opening opposite Jackie's body and pinch to take him out. Release
him over a scanned surface; pinch again and carry him behind the window surface
to return him. The original level keeps running during transfer and while Jackie is in the room;
its hidden player is isolated from movement, combat and damage until return. After placing
Jackie on a scanned surface, pick up Touch and centre the sticks/release the
buttons once. Hand trackers are active only in interaction mode after both Touch controllers
are put down; ordinary gameplay and active Touch input do not track optical hands.
Keep interaction mode enabled: the left stick moves him relative
to your head direction, the right trigger jumps, A/B punch/kick and right grip
rolls. Push the right stick up/down to change his standing height from 5 cm
to 1.566 m (default 10 cm); his feet stay on the support surface and room
geometry limits growth. His movement uses the scanned floor/furniture/walls.
Before extraction, the level pauses during window editing or sustained loss of
both Touch controllers. A missing stick or a single controller no longer pauses
the level or disables the other hand; leaving editing restores movement immediately, with release required only
for each button/trigger that was held during editing. Press both sticks to leave interaction and return to the level.
Turning off interaction or losing tracking while holding Jackie returns him to
the original level. Scanned surface outlines are currently visible in interaction mode.

## Origin and credits

The game-code baseline is the **PC ReChan project**,
[SilverwireGames/ReChan](https://github.com/SilverwireGames/ReChan), commit `ecd3993`.
Selected Android platform/build/input files were adapted from
[deivid22srk/ReChan-Android](https://github.com/deivid22srk/ReChan-Android),
commit `a898685`. This project adds the Quest OpenXR, stereo, controller and MR
integration. See [provenance and credits](docs/quest/PROVENANCE.md).

## Building and publishing

- [Build from source](docs/quest/BUILDING.md)
- [Prepare a source-only GitHub repository and APK](docs/quest/PUBLISHING.md)

The source export excludes local disc data, extracted resources, saves, room
captures, development notes, build caches and Git history. An APK contains the
engine and ReChan support files, not the user's game image or extracted disc data.

## Legal and license

This is an unofficial, independent port. It is not affiliated with or endorsed by
the original game's developers, publisher or rights holders, or by Meta.
*Jackie Chan Stuntmaster* and its original game content belong to their respective
rights holders. Obtain your own lawful disc image; this project provides no download
links or game data.

The [MIT license](LICENSE) covers the project code as specified by its copyright
notice; it grants no rights to the original game. Preserve upstream notices.
ReChan-authored support files from `res/pc` are included; third-party libraries,
fonts and other support components retain their applicable licenses and notices.
