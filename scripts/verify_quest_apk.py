#!/usr/bin/env python3
"""Check that a distributable Quest APK contains only approved support assets."""
import argparse
from pathlib import Path
import zipfile

ROOT = Path(__file__).resolve().parents[1]


def verify(apk):
    support = ROOT / "res/pc"
    expected = {"pc/" + p.relative_to(support).as_posix(): p.read_bytes()
                for p in support.rglob("*") if p.is_file()}
    manifest = "".join(name + "\n" for name in sorted(expected)).encode()
    expected["pc_manifest.txt"] = manifest
    with zipfile.ZipFile(apk) as archive:
        names = archive.namelist()
        if len(names) != len(set(names)):
            raise ValueError("Duplicate APK entries")
        assets = {n.removeprefix("assets/"): n for n in names
                  if n.startswith("assets/") and not n.endswith("/")}
        if set(assets) != set(expected):
            raise ValueError(f"Unexpected/missing assets: {set(assets) ^ set(expected)}")
        for name, data in expected.items():
            if archive.read(assets[name]) != data:
                raise ValueError(f"Support asset differs from source: {name}")
        if "lib/arm64-v8a/librechan.so" not in names:
            raise ValueError("Quest ARM64 game library missing")
        if any(n.startswith("lib/") and not n.startswith("lib/arm64-v8a/") for n in names):
            raise ValueError("Unexpected native architecture")
        blocked = {".bin", ".iso", ".cue", ".chd", ".rr", ".tim", ".str", ".xa", ".vag", ".sav"}
        if any(Path(n).suffix.lower() in blocked for n in names):
            raise ValueError("Original game/save format present in APK")
    print(f"APK verified: ARM64, {len(expected)} exact support assets; no original game data")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("apk", type=Path)
    verify(parser.parse_args().apk)
