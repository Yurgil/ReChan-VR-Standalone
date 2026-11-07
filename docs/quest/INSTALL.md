# Install ReChan VR Standalone on Quest 3

[Русская инструкция](INSTALL_RU.md)

## Requirements

- Meta Quest 3, Touch controllers, a USB data cable and a computer for installation.
- Developer Mode enabled and USB debugging authorized in the headset.
- Android Platform Tools (`adb`), available from the official Android SDK.
- A Bash shell (macOS/Linux, or a configured Git Bash/WSL environment on Windows).
- A **debug APK** built from this source, or provided by this project's maintainer.
- Your own legally obtained *Jackie Chan Stuntmaster*, NTSC-U **SLUS-00684**, disc image.

No disc image or extracted original game data is provided. PAL and other disc
variants are not supported targets; modified images are not generally validated.
Extract ZIP/7z archives yourself. Use the actual BIN/ISO, not a CUE file. Keep any
original multi-track image intact; do not concatenate tracks or rename an archive
to `.bin`.

## Install and import your image

Extract this source package, or clone the published repository, and open a shell
in its root directory. Check your connection:

```bash
adb devices
```

The Quest must appear as `device`, not `unauthorized`. Put on the headset to
approve debugging if needed. With multiple devices, set `ANDROID_SERIAL` to your
Quest's serial in your local shell; do not post it in public issue reports.

Install and copy your image using the supplied script:

```bash
ADB=/absolute/path/to/adb APK=/absolute/path/to/app-debug.apk \
  scripts/run_quest.sh game '/absolute/path/to/your/game.bin'
```

The script checks for Quest 3, installs with `-r`, stops the previous instance,
copies the supplied image to app-private storage, checks its byte count, and launches
the game. The engine checks the disc version and extracts the necessary data on the
headset. Confirm extraction if prompted and let it finish. If the Quest asks for
controllers, wake both Touch controllers.

With the repository's local toolchain and default build output, the command is:

```bash
scripts/run_quest.sh game '/absolute/path/to/your/game.bin'
```

The importer uses `run-as` and therefore requires a debuggable APK. Installing a
non-debuggable release APK alone does not provide a supported first-run disc import.
The inherited 2D Android folder picker is not validated on Horizon OS.

## Play and update

After setup, open **ReChan Quest** in the headset's library / Unknown Sources.
No PC, cable, streaming server or PCVR runtime is needed to play.

For an update, use the same APK signing key and install over the existing app.
With the script, omit the image to preserve imported data and saves:

```bash
ADB=/absolute/path/to/adb APK=/absolute/path/to/new-app-debug.apk \
  scripts/run_quest.sh game
```

Uninstalling the app or clearing its storage removes its private saves and imported
data. Copying a different disc image does not automatically replace previously
extracted resources; switching data sets requires a separate migration/reset.

## VR, MR and character interaction

VR is the startup mode. In Options, enable Mixed Reality for passthrough. Both stick
clicks toggle editing; a cyan outline indicates that it is active. Grab the frame
with controllers, or set Touch down, wait 1.25 seconds and pinch with tracked hands.
Editing the window does not create a test cube. There is no size selector in
Options; resize with hands or controllers. Optical hand trackers are inactive
during normal gameplay and while Touch is held, and activate only after both
controllers have been put down in editing mode.

Room interaction is optional. Enable **Interact with Jackie** on the Room interaction
page, then enter MR editing. Only this opt-in requests room data. A saved Quest scan
is reused; if absent, use the system scanning setup. A room scan is unnecessary
for ordinary VR/MR gameplay or window placement.

In a playable level, pinch near Jackie's projected body at the opening and pull
him out. Outside he is a miniature of 10 cm reference standing height. Put him on a
scanned surface, or take him back behind the opening and release to resume the
original game. Keep interaction mode enabled after placing him, pick up Touch,
and centre the sticks/release all buttons once. The left stick moves him relative
to your head direction; right trigger jumps (5% travel), A punches, B kicks and
right grip rolls. Right stick up/down adjusts standing height between 5 cm and
1.566 m, starting at 10 cm. Feet stay on the surface and growth stops at scanned
obstacles. The scan provides floor/table support, wall/furniture collisions
and the boundary of the selected room. Mode exit returns the original player.
The level keeps running after extraction, while the hidden original player is
isolated from movement and combat. Before extraction, window editing or sustained loss of
both Touch controllers pauses the level to protect against damage during input handoff. Movement resumes on mode exit;
a held button/trigger needs its own release and never blocks the rest of the pad.
Room locomotion is experimental in 0.10.3; no enemies from the original level are
spawned into the scanned room.

## Troubleshooting

- No device: check USB data connection, Developer Mode and the in-headset prompt.
- `unauthorized`: approve USB debugging while wearing the headset.
- `run-as: package not debuggable`: use this project's debug APK for the current importer.
- Signature mismatch on update: use an APK signed with the original installation's key.
- Disc rejected: confirm NTSC-U SLUS-00684 and an actual supported image, not an archive.
- System controller screen: wake both Touch controllers before diagnosing game launch.
- Cannot move the window: enable MR and both-stick editing first; use a fresh pinch
  after changing from Touch to hands.

For bugs, include app version, level, steps, VR/MR mode and relevant log excerpts.
Do not attach disc images, extracted resources, saves, room scans or passthrough
recordings to public reports. Build and log commands are in [BUILDING.md](BUILDING.md).


The clapperboard SCENE label shows the current stage, or MAIN MENU. TAKE starts
at 01, increments after a death/restart in that stage, and resets on main-menu or
game-over exit. Opening the pause/options menu does not reset it.


Download preview APKs and their SHA256 checksums from
[GitHub Releases](https://github.com/Yurgil/ReChan-VR-Standalone/releases).
A temporary loss of a single Touch action/controller does not stop the level or
suppress the other controller. Full input loss is debounced for 0.5 seconds before
automatic protection; normal pause and headset focus handling remain available.
