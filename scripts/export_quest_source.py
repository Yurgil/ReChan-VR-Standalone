#!/usr/bin/env python3
"""Export the current Quest source, without Git history or local game/device data.

Reads tracked and nonignored source paths from Git; does not change the checkout.
Only the documented public roots/docs can enter the new destination directory.
"""
import argparse
from pathlib import Path, PurePosixPath
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]
PUBLIC_ROOTS = {".github", "src", "vendor", "res", "scripts", "tools", "switch", "android", "tests"}
PUBLIC_FILES = {".gitignore", "LICENSE", "README.md", "premake5.lua", "premake5.cmd"}
PUBLIC_DOCS = {
    "docs/BUILDING.md", "docs/MODDING.md", "docs/REFERENCE.md",
    "docs/quest/BUILDING.md", "docs/quest/INSTALL.md", "docs/quest/INSTALL_RU.md",
    "docs/quest/PROVENANCE.md", "docs/quest/PUBLISHING.md",
}
BLOCKED_DIRS = {
    ".git", ".toolchain", ".gradle", ".cxx", ".idea", ".vscode", "__pycache__",
    "build", "bin", "discimage", "rchars", "rtarget", "userfiles", "minidumps",
    "device-checks", "screenshots", "screenshots_psx",
}
BLOCKED_SUFFIXES = {
    ".bin", ".iso", ".cue", ".chd", ".img", ".7z", ".zip", ".rar",
    ".rr", ".p3d", ".tim", ".str", ".xa", ".vag", ".sav",
    ".apk", ".aab", ".exe", ".dll", ".so", ".dylib", ".a", ".lib",
    ".obj", ".pdb", ".dmp", ".log", ".mp4", ".webm", ".mov",
    ".pem", ".key", ".p12", ".pfx", ".jks", ".keystore", ".jar",
}
# Public development inputs, required for the reproducible inherited build.
PUBLIC_BINARIES = {
    "android/app/rechan.keystore",
    "android/gradle/wrapper/gradle-wrapper.jar",
    "vendor/libp3d/vendor/sdl2/android-project/gradle/wrapper/gradle-wrapper.jar",
}
REQUIRED = {
    "README.md", "LICENSE", "docs/quest/INSTALL.md", "docs/quest/PROVENANCE.md",
    "android/app/src/main/cpp/CMakeLists.txt", "android/app/src/main/AndroidManifest.xml",
    "android/gradlew", "android/gradle/wrapper/gradle-wrapper.jar", "android/app/rechan.keystore",
    "scripts/build_quest.sh", "scripts/run_quest.sh", "scripts/export_quest_source.py",
    "vendor/libp3d/pddi/quest/QuestXR.cpp", "src/extra/questcharacter.cpp",
    "res/pc/text/english.txt",
}


def included(name):
    p = PurePosixPath(name)
    if p.is_absolute() or ".." in p.parts:
        return False
    if name not in PUBLIC_FILES and name not in PUBLIC_DOCS and p.parts[0] not in PUBLIC_ROOTS:
        return False
    if p.parts[0] == "docs" and name not in PUBLIC_DOCS:
        return False
    if any(part.lower() in BLOCKED_DIRS for part in p.parts[:-1]):
        return False
    if p.name.startswith(".env") or p.name in {"local.properties", ".DS_Store"}:
        return False
    return p.suffix.lower() not in BLOCKED_SUFFIXES or name in PUBLIC_BINARIES


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("destination", nargs="?", type=Path,
                        help="new, nonexistent directory for public source")
    parser.add_argument("--check", action="store_true", help="audit selection without writing")
    args = parser.parse_args()
    if not args.check and args.destination is None:
        parser.error("provide a destination or --check")
    raw = subprocess.check_output(
        ["git", "ls-files", "-z", "--cached", "--others", "--exclude-standard"], cwd=ROOT)
    names = sorted(set(raw.decode("utf-8").rstrip("\0").split("\0")))
    selected = [n for n in names if n and included(n) and (ROOT / n).exists()]
    missing = REQUIRED - set(selected)
    if missing:
        raise RuntimeError("Required source missing: " + ", ".join(sorted(missing)))
    for name in selected:
        source = ROOT / name
        if source.is_symlink() or ROOT not in source.resolve().parents:
            raise RuntimeError("Refusing a symlink or path outside source: " + name)
        if not source.is_file():
            raise RuntimeError("Expected a source file: " + name)
    size = sum((ROOT / n).stat().st_size for n in selected)
    print(f"Selected {len(selected)} files, {size / 1024 / 1024:.1f} MiB; "
          "game images/data, history, caches, saves and device captures excluded.")
    if args.check:
        return
    dest = args.destination.expanduser().resolve()
    # Refuse replacing data or recursively exporting a tree into itself.
    if dest.exists() or dest == ROOT or dest in ROOT.parents:
        raise RuntimeError("Destination must be a new directory")
    if ROOT in dest.parents and not (ROOT / ".toolchain") in dest.parents:
        raise RuntimeError("Use a sibling directory or .toolchain export directory")
    dest.mkdir(parents=True)
    try:
        for name in selected:
            target = dest / name
            target.parent.mkdir(parents=True, exist_ok=True)
            shutil.copy2(ROOT / name, target)
    except Exception:
        print(f"Partial export retained for inspection: {dest}", file=sys.stderr)
        raise
    print(f"Exported public source: {dest}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, RuntimeError, subprocess.CalledProcessError) as error:
        sys.exit(str(error))
