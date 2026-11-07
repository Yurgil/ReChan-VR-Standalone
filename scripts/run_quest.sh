#!/usr/bin/env bash
# Development deployment; installs the local debug APK and preserves app saves.
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
ADB="${ADB:-$ROOT/.toolchain/sdk/platform-tools/adb}"
APK="${APK:-$ROOT/android/app/build/outputs/apk/debug/app-debug.apk}"
MODE="${1:-diagnostic}"
IMAGE="${2:-}"
PACKAGE=org.rechan.quest
ACTIVITY=com.deivid22srk.rechan.GameActivity
if [[ "$MODE" != diagnostic && "$MODE" != game ]]; then
    echo 'Usage: scripts/run_quest.sh diagnostic | game [path/to/image.bin]' >&2; exit 2
fi
if [[ ! -x "$ADB" || ! -f "$APK" ]]; then
    echo 'Build the APK first; set ADB/APK if using external paths.' >&2; exit 1
fi
if [[ -n "$IMAGE" && ! -f "$IMAGE" ]]; then echo "Image not found: $IMAGE" >&2; exit 1; fi
# adb itself rejects ambiguous/unauthorised devices. ANDROID_SERIAL can select one explicitly.
"$ADB" get-state >/dev/null
MODEL="$("$ADB" shell getprop ro.product.model | tr -d '\r')"
if [[ "$MODEL" != 'Quest 3' ]]; then
    echo "Expected Quest 3; connected device reports: $MODEL" >&2; exit 1
fi
"$ADB" install -r "$APK"
"$ADB" shell am force-stop "$PACKAGE"
if [[ -n "$IMAGE" ]]; then
    echo 'Copying user disc image to app-private storage…'
    "$ADB" shell run-as "$PACKAGE" mkdir -p files/discimage
    "$ADB" shell -T "run-as $PACKAGE sh -c 'cat > files/discimage/game.bin.part'" < "$IMAGE"
    EXPECTED="$(wc -c < "$IMAGE" | tr -d ' ')"
    ACTUAL="$("$ADB" shell run-as "$PACKAGE" stat -c %s files/discimage/game.bin.part | tr -d '\r')"
    if [[ "$ACTUAL" != "$EXPECTED" ]]; then echo 'Incomplete image transfer; retaining .part for diagnosis.' >&2; exit 1; fi
    "$ADB" shell run-as "$PACKAGE" mv files/discimage/game.bin.part files/discimage/game.bin
fi
if [[ "$MODE" == diagnostic ]]; then
    "$ADB" shell am start -W -n "$PACKAGE/$ACTIVITY" --ez quest_diagnostic true
else
    "$ADB" shell am start -W -n "$PACKAGE/$ACTIVITY" --ez quest_diagnostic false
fi
printf '\nLog capture: %s logcat -v threadtime ReChanXR:I rechan-gles:I rechan:I AndroidRuntime:E DEBUG:E "*:S"\n' "$ADB"
