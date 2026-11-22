#!/bin/sh
# Builds the Xbox 360 player with xsdk + SDL-xenon and stages the APP FOLDER to copy onto an RGH / JTAG console (docs/XBOX360.md).
#
#   tools/xbox360/build.sh --xsdk DIR --sdl DIR [--toolchain DIR] [--jobs N] [--out DIR]
#
#   --xsdk DIR       a built xsdk checkout (https://github.com/xenosworks/xsdk): `make` has produced build/libxsdk.a, using
#                    the toolchain below. Required.
#   --sdl DIR        SDL-xenon (https://github.com/xenosworks/SDL-xenon, branch `xenon`). Its build-xsdk/ folder is built
#                    here when missing (docs/README-xsdk.md in that repo). Required.
#   --toolchain DIR  where xsdk's tools/toolchain/build.sh installed powerpc-xsdk-elf-gcc (default: $HOME/xsdk-gcc; the
#                    glslang binary xsdk's shaders need sits in its bin/ too)
#   --jobs N         parallel compiler jobs (default 4)
#   --out DIR        where objects and the result go (default <repo>/build-xbox360, which .gitignore covers)
#
# Result: <out>/OpenTacticsEngine/ = OpenTacticsEngine.xex, assets/, games/ (index.json + the three demo games), licenses/.
# Copy that folder to the console (FTP, e.g. Hdd1/Apps/OpenTacticsEngine/) and start the .xex from the dashboard; it runs
# from where it is and writes OpenTacticsEngine.log, settings and saves beside it.
#
# Only the demo games ever go into the folder (same rule as tools/xbox-og/build.sh, tools/windows/package.ps1): the script
# refuses to run when games/index.json lists anything else and checks the result for private games.
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
XSDK=""
SDL=""
TC="${XSDK_TOOLCHAIN:-$HOME/xsdk-gcc}"
JOBS=4
OUT="$REPO/build-xbox360"
GAMES="demo-srpg demo-economy demo-hex"

die() { echo "build.sh: $*" >&2; exit 1; }

while [ $# -gt 0 ]; do
  case "$1" in
    --xsdk) [ $# -ge 2 ] || die "--xsdk needs a folder"; XSDK="$2"; shift 2 ;;
    --sdl) [ $# -ge 2 ] || die "--sdl needs a folder"; SDL="$2"; shift 2 ;;
    --toolchain) [ $# -ge 2 ] || die "--toolchain needs a folder"; TC="$2"; shift 2 ;;
    --jobs) [ $# -ge 2 ] || die "--jobs needs a number"; JOBS="$2"; shift 2 ;;
    --out) [ $# -ge 2 ] || die "--out needs a folder"; OUT="$2"; shift 2 ;;
    -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
    *) die "unknown option $1 (see --help)" ;;
  esac
done
[ -n "$XSDK" ] || die "give --xsdk DIR (a built xsdk); see --help"
[ -n "$SDL" ] || die "give --sdl DIR (SDL-xenon); see --help"
XSDK="$(cd "$XSDK" && pwd)" || die "no such xsdk folder"
SDL="$(cd "$SDL" && pwd)" || die "no such SDL-xenon folder"
TC="$(cd "$TC" && pwd)" || die "no such toolchain folder (--toolchain)"
case "$JOBS" in ''|*[!0-9]*) die "--jobs must be a number" ;; esac
[ -x "$TC/bin/powerpc-xsdk-elf-gcc" ] || die "$TC has no bin/powerpc-xsdk-elf-gcc (xsdk's tools/toolchain/build.sh makes it)"
[ -x "$TC/bin/glslang" ] || die "$TC/bin/glslang is missing (xsdk docs/GETTING_STARTED.md section 4 builds glslang-xenon)"
[ -f "$XSDK/build/libxsdk.a" ] || die "$XSDK/build/libxsdk.a is missing: run 'make -j8 TOOLCHAIN=$TC/bin/powerpc-xsdk-elf-' there first"
[ -f "$SDL/CMakeLists.txt" ] || die "$SDL is not an SDL checkout"

# --- the demo-only rule -------------------------------------------------------------------------------------------------
LISTED="$(python3 -I -c 'import json,sys; print(" ".join(json.load(open(sys.argv[1]))["games"]))' "$REPO/games/index.json")"
[ "$LISTED" = "$GAMES" ] || die "games/index.json lists '$LISTED' but this script ships '$GAMES'. Update both on purpose."
for g in $GAMES; do [ -f "$REPO/games/$g/game.json" ] || die "games/$g has no game.json"; done

mkdir -p "$OUT"

# --- SDL-xenon ------------------------------------------------------------------------------------------------------------
if [ ! -f "$SDL/build-xsdk/SDL3Config.cmake" ] && [ ! -f "$SDL/build-xsdk/cmake/SDL3Config.cmake" ]; then
  echo "== building SDL-xenon (build-xsdk)"
  cmake -S "$SDL" -B "$SDL/build-xsdk" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$XSDK/cmake/xsdk.cmake" -DXSDK_TOOLCHAIN="$TC" \
    -DCMAKE_BUILD_TYPE=Release >"$OUT/sdl-configure.log" 2>&1 || { tail -20 "$OUT/sdl-configure.log"; die "SDL configure failed"; }
  nice -n 19 cmake --build "$SDL/build-xsdk" -j"$JOBS" >"$OUT/sdl-build.log" 2>&1 || { tail -20 "$OUT/sdl-build.log"; die "SDL build failed"; }
fi

# --- the player -----------------------------------------------------------------------------------------------------------
echo "== building the XEX (-j$JOBS)"
BUILD="$OUT/obj-build"
cmake -S "$REPO" -B "$BUILD" -G Ninja -DCMAKE_TOOLCHAIN_FILE="$XSDK/cmake/xsdk.cmake" -DXSDK_TOOLCHAIN="$TC" \
  -DCMAKE_BUILD_TYPE=Release -DOTE_BUILD_TESTS=OFF -DOTE_BUILD_EDITOR=OFF -DOTE_BUILD_PLAYER=ON -DOTE_PLATFORM=sdl3 \
  -DSDL3_DIR="$SDL/build-xsdk" >"$OUT/configure.log" 2>&1 || { tail -30 "$OUT/configure.log"; die "configure failed (see $OUT/configure.log)"; }
nice -n 19 cmake --build "$BUILD" -j"$JOBS" >"$OUT/make.log" 2>&1 || { tail -30 "$OUT/make.log"; die "the build failed (full log: $OUT/make.log)"; }
if grep -E "warning:" "$OUT/make.log" | grep -q "engine/"; then
  echo "note: compiler warnings in our code, see $OUT/make.log"
fi
XEX="$BUILD/engine/player/ote-player.xex"
[ -f "$XEX" ] || die "no .xex was produced (see $OUT/make.log)"

# --- stage the app folder -------------------------------------------------------------------------------------------------
APP="$OUT/OpenTacticsEngine"
rm -rf "$APP"
mkdir -p "$APP/games" "$APP/licenses"
cp "$XEX" "$APP/OpenTacticsEngine.xex"
cp -R "$REPO/assets" "$APP/assets"
cp "$REPO/games/index.json" "$APP/games/index.json"
for g in $GAMES; do cp -R "$REPO/games/$g" "$APP/games/$g"; done
cp "$REPO/LICENSE" "$APP/licenses/GPL-3.0.txt"
cp "$REPO/games/demo-srpg/CREDITS.md" "$APP/licenses/demo-srpg-CREDITS.md"
cp "$XSDK/LICENSE" "$APP/licenses/xsdk-MIT.txt"
cp "$SDL/LICENSE.txt" "$APP/licenses/SDL-zlib.txt"

# nothing private may be in there (belt and braces: the copy above only names the demo games)
if find "$APP" -iname 'vision-quest*' -o -iname 'practice-*' | grep -q .; then die "a private game ended up in the app folder"; fi
for d in "$APP/games"/*/; do
  case "$(basename "$d")" in demo-srpg|demo-economy|demo-hex) ;; *) die "unexpected game folder in the app folder: $d" ;; esac
done

SIZE="$(du -sm "$APP" | cut -f1)"
echo "== done: $APP ($SIZE MB; OpenTacticsEngine.xex $(du -k "$APP/OpenTacticsEngine.xex" | cut -f1) KB)"
echo "   copy the folder to the console (e.g. Hdd1/Apps/OpenTacticsEngine) and start OpenTacticsEngine.xex"
