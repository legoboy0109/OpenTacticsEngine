#!/usr/bin/env bash
# Builds the debug APK of the Android player (docs/ANDROID.md). Debug signing only (Gradle's auto-generated debug
# keystore); there is no release build or signing here on purpose.
#   tools/android/build_apk.sh            # fetch SDL if missing, build, print the APK path and size
#   tools/android/build_apk.sh clean      # remove android/build and android/app/build first
set -euo pipefail

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
proj="$here/../../android"

export ANDROID_HOME="${ANDROID_HOME:-$HOME/Android/Sdk}"
export ANDROID_SDK_ROOT="$ANDROID_HOME"
# The default `java` on the dev machine is 1.8; Gradle/AGP need 17+.
if [ -z "${JAVA_HOME:-}" ]; then
  for j in /usr/lib/jvm/java-21-openjdk /usr/lib/jvm/java-17-openjdk; do
    [ -x "$j/bin/java" ] && JAVA_HOME="$j" && break
  done
  export JAVA_HOME
fi
[ -d "$ANDROID_HOME" ] || { echo "ANDROID_HOME not found: $ANDROID_HOME" >&2; exit 1; }
[ -n "${JAVA_HOME:-}" ] || { echo "no JDK 17+ found: set JAVA_HOME" >&2; exit 1; }

"$here/fetch_sdl.sh"
cd "$proj"
[ "${1:-}" = clean ] && rm -rf build app/build app/.cxx
./gradlew --console=plain :app:assembleDebug

apk="$proj/app/build/outputs/apk/debug/app-debug.apk"
[ -f "$apk" ] || { echo "build finished but $apk is missing" >&2; exit 1; }
echo "APK: $apk ($(du -h "$apk" | cut -f1))"
