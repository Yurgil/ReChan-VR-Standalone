# Publish ReChan VR Standalone

Public repository: [Yurgil/ReChan-VR-Standalone](https://github.com/Yurgil/ReChan-VR-Standalone), a fork of SilverwireGames/ReChan.
Suggested GitHub description:

> Native Quest 3 VR/MR port of ReChan (Jackie Chan Stuntmaster), with stereo diorama,
> Touch and hand interaction. Requires your own SLUS-00684 disc. No game data included.

This guide describes publication; the commands below do not create a remote repository
or upload anything by themselves. The public README links the [English](INSTALL.md) and [Russian](INSTALL_RU.md)
installation guides and [upstream credits](PROVENANCE.md).

## Update the GitHub fork

Keep the inherited ReChan Git history when publishing port changes to the fork.
Use a separate publishing checkout, apply the reviewed Quest source/documentation
changes, and commit them on top of the upstream baseline. Fetch the fork before
pushing and use a normal fast-forward update; do not force-push over remote changes.
Keep upstream LICENSE/copyright notices and the Android port credits.

Use the source export below to choose public files, excluding local game data and
development evidence. Its lack of `.git` history is a packaging property; do not
replace the fork's existing history with a new root commit. APKs belong in Releases,
not in the source tree.

## Prepare a clean source snapshot

The development checkout contains private local tools, game data and device evidence.
Export the current source into a **new directory** instead of copying the checkout:

```bash
python3 scripts/export_quest_source.py --check
python3 scripts/export_quest_source.py ../rechan-vr-standalone
```

The exporter includes the current tracked and nonignored source changes, so Quest
files need not already be committed in the development checkout. It uses a curated
set of public source roots and documents. It excludes:

- `.git` history, local tools/caches and build outputs;
- disc images/archives and extracted PS1 resource formats;
- saves, crash dumps, device screenshots/videos and room captures;
- internal task plans/handoffs, machine-specific configuration and private key formats;
- inherited gameplay screenshots, which are unnecessary for building the port.

ReChan support files in `res/pc`, vendored dependencies and license notices remain.
The Gradle wrapper JARs and inherited **public development keystore** are explicit
exceptions to the binary exclusion rule: they are build inputs, not game data.
The tool checks file/path selection; it is not a copyright or licensing certification
for every upstream support file. Preserve their applicable notices.

The export contains no Git history. This intentionally avoids publishing local data
or inherited binary history; provenance is documented in PROVENANCE.md. For a separate new repository, check the exported tree before initializing it.
For the existing fork, apply the selected files on top of its preserved Git history.
Keep the original development checkout and its upstream remotes intact.

## APK releases

Build using [BUILDING.md](BUILDING.md). For the currently supported disc import route,
provide the **debuggable** `app-debug.apk`; `run-as` cannot import the disc into a
non-debuggable APK. Give a downloadable artifact an unambiguous name such as:

```text
rechan-vr-standalone-0.10.4-quest3.apk
```

Record its SHA-256 and attach the source snapshot/installation instructions as useful.
Publish only the APK and reviewed source — never the development `.toolchain` folder,
app-private backups, disc image, extracted resource folders, saves or passthrough footage.
The Gradle assets task packages only the ReChan support directory `res/pc`; your image
is imported separately into private device storage after installation.

Suggested release description:

> Experimental native Quest 3 build. Requires your own legally obtained Jackie Chan
> Stuntmaster NTSC-U (SLUS-00684) disc image; no original game data is included.
> Install the debug APK and import your image using the repository's ADB script.
> VR stereo, Touch controls, MR window manipulation and miniature-character transfer
> and Touch-controlled external character locomotion are available. Full-game validation is ongoing.

Keep the legal/copyright notice and upstream/Android credits visible. Do not present
this project as an official release or promise support for untested headsets/disc variants.


## Automated preview releases

The `Quest preview release` workflow builds a signed, debuggable ARM64 APK from an
immutable `v*` tag. It checks the tag against `questVersion`, verifies the actual
Quest Gradle Wrapper checksum, runs native regression tests, builds without game
data, and audits the APK using `scripts/verify_quest_apk.py`. It then attaches the
APK and `SHA256SUMS` to a GitHub prerelease using the workflow's repository token;
no personal API token or private signing secret is needed for this development key.

For a new preview:

1. Update `questVersion`/`versionCode`, README and installation notes.
2. Add `.github/releases/vVERSION.md` with changes and the own-copy requirement.
3. Complete local tests/build/APK audit, then push the source commit normally.
4. Create and push an annotated `vVERSION` tag. Watch Actions until the release
   contains both APK and checksum; triggering a build alone is not publication.
5. Download the public APK, check the checksum and contents, and smoke-test it on
   Quest 3. Preserve existing tags; fix build infrastructure on the main branch
   and use workflow dispatch with the existing release tag when retrying a build.

Existing preview installations can be updated in place because all preview APKs
use the same development sideload signature. Game data is supplied separately by
the user and must never be attached to the release.
