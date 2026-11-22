#!/bin/sh
# Builds the engine core + ote_tests for big-endian PowerPC (64-bit, Linux) and runs them under qemu-user.
#   tools/xbox360/bigendian_check.sh --toolchain DIR --qemu FILE [--jobs N] [--out DIR] [-- test args]
#   --toolchain DIR  an unpacked bootlin powerpc64-power8 glibc toolchain (the folder holding bin/powerpc64-linux-gcc)
#   --qemu FILE      a qemu-ppc64 (user mode) binary
# The result is ote_tests' own "N test(s) run, M failed" line. Silent: the tests never open an audio device.
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
TC=""; QEMU=""; JOBS=4; OUT="$REPO/build-ppc64be"
die() { echo "bigendian_check.sh: $*" >&2; exit 1; }
while [ $# -gt 0 ]; do
  case "$1" in
    --toolchain) TC="$2"; shift 2 ;;
    --qemu) QEMU="$2"; shift 2 ;;
    --jobs) JOBS="$2"; shift 2 ;;
    --out) OUT="$2"; shift 2 ;;
    --) shift; break ;;
    -h|--help) sed -n '2,7p' "$0"; exit 0 ;;
    *) die "unknown option $1" ;;
  esac
done
[ -x "$TC/bin/powerpc64-linux-gcc" ] || die "--toolchain DIR must hold bin/powerpc64-linux-gcc"
[ -x "$QEMU" ] || die "--qemu FILE must be an executable qemu-ppc64"
mkdir -p "$OUT"
cmake -S "$REPO" -B "$OUT" -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_TOOLCHAIN_FILE="$HERE/ppc64be-linux.cmake" -DOTE_BIGENDIAN_TC="$TC" \
  -DOTE_BUILD_PLAYER=OFF -DOTE_BUILD_EDITOR=OFF -DOTE_BUILD_TESTS=ON >"$OUT/configure.log" 2>&1 ||
  { tail -20 "$OUT/configure.log"; die "configure failed"; }
nice -n 19 cmake --build "$OUT" -j"$JOBS" >"$OUT/build.log" 2>&1 || { tail -30 "$OUT/build.log"; die "build failed"; }
cd "$REPO"
# -cpu power8: the toolchain targets POWER8. The tests find games/ and assets/ relative to the working directory.
exec "$QEMU" -cpu power8 "$OUT/ote_tests" "$@"
