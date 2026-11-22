#!/bin/sh
# Boots the built .xex in Xenia Canary (Linux build) on a private virtual X display and takes a screenshot: a crash / smoke
# check only, never evidence about a real console (docs/XBOX360.md). Silent: Xenia runs with the audio backend "nop", so no
# sound device is touched, and in its own empty HOME and working folder so nobody's Xenia settings are read or written.
#
#   tools/xbox360/xenia_smoke.sh --xenia FILE --app DIR [--seconds N] [--out DIR] [--display :N]
#   --xenia FILE   the xenia_canary binary (https://github.com/xenia-canary/xenia-canary-releases, the Linux build)
#   --app DIR      the staged app folder (tools/xbox360/build.sh's <out>/OpenTacticsEngine)
#   --seconds N    how long to let the title run before the screenshot (default 40)
#   --out DIR      screenshots, Xenia's log and the title's log end up here (default <repo>/build-xbox360/smoke)
# Needs Xvnc (TigerVNC) and ImageMagick's `import`. The title is started with the app folder's args.txt copied from
# --args FILE when given (e.g. "--frames 600" style options); without one it runs the launcher until stopped.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
XENIA=""; APP=""; SECONDS_RUN=40; OUT="$REPO/build-xbox360/smoke"; DISP=":71"; ARGS=""
die() { echo "xenia_smoke.sh: $*" >&2; exit 1; }
while [ $# -gt 0 ]; do
  case "$1" in
    --xenia) XENIA="$2"; shift 2 ;;
    --app) APP="$2"; shift 2 ;;
    --seconds) SECONDS_RUN="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    --display) DISP="$2"; shift 2 ;;
    --args) ARGS="$2"; shift 2 ;;
    -h|--help) sed -n '2,12p' "$0"; exit 0 ;;
    *) die "unknown option $1" ;;
  esac
done
[ -x "$XENIA" ] || die "--xenia FILE must be the xenia_canary binary"
[ -f "$APP/OpenTacticsEngine.xex" ] || die "--app DIR must hold OpenTacticsEngine.xex"
command -v Xvnc >/dev/null || die "Xvnc (TigerVNC) is needed"
command -v import >/dev/null || die "ImageMagick's import is needed"
mkdir -p "$OUT"
RUN="$(mktemp -d)"
trap 'kill $XPID $EPID 2>/dev/null || true; rm -rf "$RUN"' EXIT
# Xenia runs from a copy of the app folder: its title log and the title's saves must not dirty the staged folder.
cp -R "$APP" "$RUN/app"
[ -n "$ARGS" ] && cp "$ARGS" "$RUN/app/args.txt"
: > "$RUN/app/dbgprint.txt"  # xsdk: the title's log lines also go to DbgPrint, which Xenia writes to xenia.log
mkdir -p "$RUN/home" "$RUN/cwd"
Xvnc "$DISP" -geometry 1280x720 -depth 24 -rfbport -1 -rfbunixpath "$RUN/vnc.sock" -SecurityTypes None >"$OUT/xvnc.log" 2>&1 &
XPID=$!
sleep 3
cd "$RUN/cwd"
# Settings go on the command line (Xenia's config flags); --apu=nop is the silent audio backend. Vulkan on whatever the host has.
HOME="$RUN/home" XDG_CONFIG_HOME="$RUN/home/.config" XDG_DATA_HOME="$RUN/home/.local/share" DISPLAY="$DISP" \
  "$XENIA" --apu=nop --headless=false --readback_resolve=full --async_shader_compilation=false \
  --log_file="$OUT/xenia.log" "$RUN/app/OpenTacticsEngine.xex" >"$OUT/xenia.stdout.log" 2>&1 &
EPID=$!
# One screenshot every 10 seconds, so a stuck first frame and a later one can be told apart.
n=0
while [ $n -lt "$SECONDS_RUN" ]; do
  sleep 10; n=$((n + 10))
  kill -0 "$EPID" 2>/dev/null || { echo "xenia exited after about $n s"; break; }
  DISPLAY="$DISP" import -window root "$OUT/shot-$n.png" 2>/dev/null || true
done
cp "$RUN/app"/*.log "$OUT/" 2>/dev/null || true
# Xenia mounts game:\ read-only, so the title falls back to its cache: folder for the log (and --screenshot cache:/x.bmp goes
# there): collect whatever it wrote anywhere under the run folder.
find "$RUN" \( -name '*.log' -o -name '*.bmp' \) -not -path "$RUN/app/*" -exec cp {} "$OUT/" \; 2>/dev/null || true
cp -R "$RUN/app/userdata" "$OUT/userdata" 2>/dev/null || true
echo "results in $OUT"
