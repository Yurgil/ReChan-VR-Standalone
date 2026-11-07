#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rechan-quest-mr.XXXXXX")"
trap 'rm -f "$OUT/passthrough-test" "$OUT/hands-test" "$OUT/scene-test"; rmdir "$OUT"' EXIT
if [[ -z "${OPENXR_HEADERS:-}" ]]; then
    OPENXR_HEADERS="$(python3 -c 'from pathlib import Path; import sys; print(next(Path(sys.argv[1]).rglob("include/openxr/openxr.h")).parent.parent)' "$ROOT/.toolchain/gradle/caches")"
fi
FLAGS=()
if [[ "$(uname -s)" == Darwin ]]; then
    SDK="$(xcrun --show-sdk-path)"
    FLAGS+=(-isysroot "$SDK" -isystem "$SDK/usr/include/c++/v1")
fi
"${CXX:-c++}" -std=c++20 "${FLAGS[@]}" -I "$ROOT/tests/quest/stubs" -I "$ROOT/vendor/libp3d" -I "$OPENXR_HEADERS" "$ROOT/tests/quest/passthrough_test.cpp" "$ROOT/vendor/libp3d/pddi/quest/QuestPassthrough.cpp" -o "$OUT/passthrough-test"
"$OUT/passthrough-test"

"${CXX:-c++}" -std=c++20 "${FLAGS[@]}" -I "$ROOT/tests/quest/stubs" -I "$ROOT/vendor/libp3d" -I "$OPENXR_HEADERS" "$ROOT/tests/quest/hands_test.cpp" "$ROOT/vendor/libp3d/pddi/quest/QuestHands.cpp" -o "$OUT/hands-test"
"$OUT/hands-test"

"${CXX:-c++}" -std=c++20 "${FLAGS[@]}" -I "$ROOT/tests/quest/stubs" -I "$ROOT/vendor/libp3d" -I "$OPENXR_HEADERS" "$ROOT/tests/quest/room_scene_test.cpp" "$ROOT/vendor/libp3d/pddi/quest/QuestRoomScene.cpp" "$ROOT/vendor/libp3d/pddi/quest/MRRoomGeometry.cpp" -o "$OUT/scene-test"
"$OUT/scene-test"
