#!/usr/bin/env bash
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
export JAVA_HOME="${JAVA_HOME:-$ROOT/.toolchain/jdk/Contents/Home}"
export ANDROID_HOME="${ANDROID_HOME:-$ROOT/.toolchain/sdk}"
export GRADLE_USER_HOME="${GRADLE_USER_HOME:-$ROOT/.toolchain/gradle}"
if [[ ! -x "$JAVA_HOME/bin/java" || ! -d "$ANDROID_HOME/platforms/android-35" ]]; then
    echo 'JDK 17 and Android SDK 35 required; see docs/quest/BUILDING.md.' >&2
    exit 1
fi
exec "$ROOT/android/gradlew" -p "$ROOT/android" "${@:-assembleDebug}" --console=plain
