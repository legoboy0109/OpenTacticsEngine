#!/bin/sh
# Builds the original-Xbox player with nxdk and stages the APP FOLDER to copy onto the console's hard disk (docs/XBOX_OG.md).
#
#   tools/xbox-og/build.sh --nxdk DIR [--jobs N] [--out DIR] [--test-iso]
#   tools/xbox-og/build.sh --check-game DIR       (only checks a game folder's file names, builds nothing)
#
#   --nxdk DIR     a built nxdk (https://github.com/XboxDev/nxdk, `git clone --recursive`, then its own README: make the
#                  toolchain and libraries, `make tools`). Required.
#   --jobs N       parallel compiler jobs (default 4)
#   --out DIR      where objects and the result go (default <repo>/build-xbox-og, which .gitignore covers)
#   --test-extra D with --test-iso: the files of folder D are merged into the test disc's app folder (e.g. a games/ folder
#                  of your own, to see it listed), test disc only
#   --test-iso     also build a small installer disc image for emulator tests (tools/xbox-og/installer): it copies the app
#                  folder from the disc to E:\Apps\OpenTacticsEngine and starts it from there. Not how users install.
#   --test-args F  with --test-iso: the file F goes onto the disc as the app's args.txt (a scripted run, see docs/XBOX_OG.md)
#
# Result: <out>/OpenTacticsEngine/  = default.xbe, assets/, games/ (index.json + the three demo games), licenses/.
# Copy that folder (FTP) to E:\Apps\ (or any folder or partition) and start default.xbe from the dashboard; it runs from where
# it is, keeps its settings, saves and log in <that folder>\userdata\.
#
# Only the demo games ever go into the folder (same rule as tools/windows/package.ps1 and tools/uwp/build.ps1): the script
# refuses to run when games/index.json lists anything else, and checks the result for private games. Everything staged is
# checked against the Xbox disk's file-name rules (tools/xbox-og/check_names.py) and the build fails with the list of
# offending paths.
set -eu

HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
NXDK=""
JOBS=4
OUT="$REPO/build-xbox-og"
TEST_ISO=0
TEST_ARGS=
TEST_EXTRA=
GAMES="demo-srpg demo-economy demo-hex"

die() { echo "build.sh: $*" >&2; exit 1; }

while [ $# -gt 0 ]; do
  case "$1" in
    --nxdk) [ $# -ge 2 ] || die "--nxdk needs a folder"; NXDK="$2"; shift 2 ;;
    --jobs) [ $# -ge 2 ] || die "--jobs needs a number"; JOBS="$2"; shift 2 ;;
    --out) [ $# -ge 2 ] || die "--out needs a folder"; OUT="$2"; shift 2 ;;
    --test-iso) TEST_ISO=1; shift ;;
    --test-args) [ $# -ge 2 ] || die "--test-args needs a file"
                 TEST_ARGS="$(cd "$(dirname "$2")" && pwd)/$(basename "$2")"; shift 2 ;;
    --test-extra) [ $# -ge 2 ] || die "--test-extra needs a folder"; TEST_EXTRA="$(cd "$2" && pwd)"; shift 2 ;;
    --check-game) [ $# -ge 2 ] || die "--check-game needs a game folder"
                  G="$(cd "$2" && pwd)" || die "no such folder: $2"
                  exec python3 -I "$HERE/check_names.py" --prefix "E:\\Apps\\OpenTacticsEngine\\games\\$(basename "$G")\\" "$G" ;;
    -h|--help) sed -n '2,26p' "$0"; exit 0 ;;
    *) die "unknown option $1 (see --help)" ;;
  esac
done
[ -n "$NXDK" ] || die "give --nxdk DIR (a built nxdk); see --help"
NXDK="$(cd "$NXDK" && pwd)" || die "no such nxdk folder"
[ -x "$NXDK/bin/nxdk-cc" ] && [ -f "$NXDK/lib/libnxdk.lib" ] && [ -x "$NXDK/tools/cxbe/cxbe" ] ||
  die "$NXDK is not a built nxdk (needs bin/nxdk-cc, lib/libnxdk.lib and tools/cxbe/cxbe: follow nxdk's README to build it)"
case "$JOBS" in ''|*[!0-9]*) die "--jobs must be a number" ;; esac

# --- the demo-only rule -------------------------------------------------------------------------------------------------
LISTED="$(python3 -I -c 'import json,sys; print(" ".join(json.load(open(sys.argv[1]))["games"]))' "$REPO/games/index.json")"
[ "$LISTED" = "$GAMES" ] || die "games/index.json lists '$LISTED' but this script ships '$GAMES'. Update both on purpose."
for g in $GAMES; do [ -f "$REPO/games/$g/game.json" ] || die "games/$g has no game.json"; done

# --- build ----------------------------------------------------------------------------------------------------------------
mkdir -p "$OUT/obj-build"
export NXDK_DIR="$NXDK"
export PATH="$NXDK/bin:$PATH"
cd "$OUT/obj-build"
# Dependency files (obj/**/*.d) hold absolute paths, so objects built from another checkout would confuse make: start clean
# when the repository folder or the nxdk folder changed since the last build here, keep them (header changes rebuild) otherwise.
STAMP="$REPO|$NXDK"
if [ "$(cat "$OUT/obj-build/.build-stamp" 2>/dev/null || true)" != "$STAMP" ]; then rm -rf obj; printf '%s' "$STAMP" > "$OUT/obj-build/.build-stamp"; fi
echo "== building the XBE (-j$JOBS)"
make -f "$HERE/Makefile" -j"$JOBS" >"$OUT/make.log" 2>&1 || { tail -30 "$OUT/make.log"; die "the nxdk build failed (full log: $OUT/make.log)"; }
if grep -E "warning:" "$OUT/make.log" | grep -q "engine/\|tools/xbox-og"; then
  echo "note: compiler warnings in our code, see $OUT/make.log"
fi
XBE="$OUT/obj-build/bin/default.xbe"
[ -f "$XBE" ] || die "no default.xbe was produced (see $OUT/make.log)"

# --- stage the app folder ---------------------------------------------------------------------------------------------------
APP="$OUT/OpenTacticsEngine"
rm -rf "$APP"
mkdir -p "$APP/games" "$APP/licenses"
cp "$XBE" "$APP/default.xbe"
cp -R "$REPO/assets" "$APP/assets"
cp "$REPO/games/index.json" "$APP/games/index.json"
for g in $GAMES; do cp -R "$REPO/games/$g" "$APP/games/$g"; done
cp "$REPO/LICENSE" "$APP/licenses/GPL-3.0.txt"
cp "$REPO/games/demo-srpg/CREDITS.md" "$APP/licenses/demo-srpg-CREDITS.md"
cp "$HERE/NOTICE.txt" "$APP/licenses/NOTICE.txt"

# nothing private may be in there (belt and braces: the copy above only names the demo games)
if find "$APP" -iname 'vision-quest*' -o -iname 'practice-*' | grep -q .; then die "a private game ended up in the app folder"; fi
for d in "$APP/games"/*/; do
  case "$(basename "$d")" in demo-srpg|demo-economy|demo-hex) ;; *) die "unexpected game folder in the app folder: $d" ;; esac
done

echo "== checking file names against the Xbox disk's rules"
python3 -I "$HERE/check_names.py" "$APP" || die "some names cannot exist on the Xbox hard disk (listed above)"

if [ "$TEST_ISO" = 1 ]; then
  echo "== building the installer test disc"
  mkdir -p "$OUT/obj-installer"
  ( cd "$OUT/obj-installer" && make -f "$HERE/installer/Makefile" -j"$JOBS" APP_DIR="$APP" ARGS_FILE="$TEST_ARGS" EXTRA_DIR="$TEST_EXTRA" >"$OUT/make-installer.log" 2>&1 ) ||
    { tail -30 "$OUT/make-installer.log"; die "the installer build failed (see $OUT/make-installer.log)"; }
  echo "installer disc: $OUT/obj-installer/ote-installer.iso"
fi

SIZE="$(du -sm "$APP" | cut -f1)"
echo "== done: $APP ($SIZE MB; default.xbe $(du -k "$APP/default.xbe" | cut -f1) KB)"
echo "   copy the folder to the console's hard disk (e.g. E:\\Apps\\OpenTacticsEngine) and start default.xbe"
