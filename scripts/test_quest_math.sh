#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
OUT="$(mktemp -d "${TMPDIR:-/tmp}/rechan-quest-math.XXXXXX")"
trap 'rm -f "$OUT/math-test" "$OUT/timing-test" "$OUT/input-test" "$OUT/lifecycle-test" "$OUT/interaction-test" "$OUT/transfer-test" "$OUT/surface-test" "$OUT/room-test" "$OUT/character-test"; rmdir "$OUT"' EXIT
FLAGS=()
if [[ "$(uname -s)" == Darwin ]]; then
    SDK="$(xcrun --show-sdk-path)"
    FLAGS+=(-isysroot "$SDK" -isystem "$SDK/usr/include/c++/v1")
fi
"${CXX:-c++}" -std=c++20 "${FLAGS[@]}" -I "$ROOT/vendor/libp3d" "$ROOT/tests/quest/math_test.cpp" -o "$OUT/math-test"
"$OUT/math-test"

"${CXX:-c++}" -std=c++20 "${FLAGS[@]}" -I "$ROOT/vendor/libp3d" "$ROOT/tests/quest/interaction_test.cpp" "$ROOT/vendor/libp3d/pddi/quest/MRInteraction.cpp" -o "$OUT/interaction-test"
"$OUT/interaction-test"

"${CXX:-c++}" -std=c++20 "${FLAGS[@]}" -I "$ROOT/vendor/libp3d" "$ROOT/tests/quest/transfer_test.cpp" "$ROOT/vendor/libp3d/pddi/quest/MRTestObject.cpp" "$ROOT/vendor/libp3d/pddi/quest/MRRoomGeometry.cpp" -o "$OUT/transfer-test"
"$OUT/transfer-test"

"${CXX:-c++}" -std=c++20 "${FLAGS[@]}" -I "$ROOT/vendor/libp3d" "$ROOT/tests/quest/surface_test.cpp" "$ROOT/vendor/libp3d/pddi/quest/MRPortalSurface.cpp" "$ROOT/vendor/libp3d/pddi/quest/MRTestObject.cpp" "$ROOT/vendor/libp3d/pddi/quest/MRRoomGeometry.cpp" -o "$OUT/surface-test"
"$OUT/surface-test"

"${CXX:-c++}" -std=c++20 "${FLAGS[@]}" -I "$ROOT/vendor/libp3d" "$ROOT/tests/quest/room_geometry_test.cpp" "$ROOT/vendor/libp3d/pddi/quest/MRRoomGeometry.cpp" "$ROOT/vendor/libp3d/pddi/quest/MRTestObject.cpp" -o "$OUT/room-test"
"$OUT/room-test"

"${CXX:-c++}" -std=c++20 "${FLAGS[@]}" -I "$ROOT/vendor/libp3d" "$ROOT/tests/quest/character_test.cpp" "$ROOT/vendor/libp3d/pddi/quest/MRCharacter.cpp" "$ROOT/vendor/libp3d/pddi/quest/MRRoomGeometry.cpp" -o "$OUT/character-test"
"$OUT/character-test"

"${CXX:-c++}" -std=c++20 -DRC_PLATFORM_QUEST "${FLAGS[@]}" -I "$ROOT/src" -I "$ROOT/vendor/libp3d" "$ROOT/tests/quest/timing_test.cpp" "$ROOT/src/gen/time.cpp" -o "$OUT/timing-test"
"$OUT/timing-test"

"${CXX:-c++}" -std=c++20 -DRC_PLATFORM_QUEST -DRC_PLATFORM_ANDROID "${FLAGS[@]}" -I "$ROOT/src" -I "$ROOT/vendor/libp3d" "$ROOT/tests/quest/input_test.cpp" "$ROOT/src/pc/inputaction.cpp" "$ROOT/vendor/libp3d/p3d/input.cpp" "$ROOT/vendor/libp3d/pddi/null/nullrender.cpp" -o "$OUT/input-test"
"$OUT/input-test"

"${CXX:-c++}" -std=c++20 "${FLAGS[@]}" -I "$ROOT/vendor/libp3d" "$ROOT/tests/quest/lifecycle_test.cpp" "$ROOT/vendor/libp3d/pddi/gles/AndroidPlatform.cpp" -o "$OUT/lifecycle-test"
"$OUT/lifecycle-test"
