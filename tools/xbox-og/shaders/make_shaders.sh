#!/bin/sh
# Regenerates engine/platform/nxdk/hw3d_vp.inl and hw3d_ps.inl from the two shader sources next to this script, with nxdk's own
# vp20compiler and fp20compiler (no Cg, no Microsoft tools). The generated files are committed, so a normal build needs none of this.
#   tools/xbox-og/shaders/make_shaders.sh /path/to/nxdk
# The compilers are built in a temporary folder from the nxdk checkout's sources (nxdk itself is left as it is).
set -eu
HERE="$(cd "$(dirname "$0")" && pwd)"
REPO="$(cd "$HERE/../../.." && pwd)"
NXDK="${1:-}"
[ -n "$NXDK" ] && [ -f "$NXDK/tools/vp20compiler/main.c" ] || { echo "usage: $0 /path/to/nxdk" >&2; exit 1; }
T="$(mktemp -d)"
trap 'rm -rf "$T"' EXIT
cp -r "$NXDK/tools/vp20compiler" "$NXDK/tools/fp20compiler" "$T/"
make -C "$T/vp20compiler" >/dev/null 2>&1
make -C "$T/fp20compiler" >/dev/null 2>&1
OUT="$REPO/engine/platform/nxdk"
"$T/vp20compiler/vp20compiler" "$HERE/hw3d_vp.vsh" > "$OUT/hw3d_vp.inl"
"$T/fp20compiler/fp20compiler" "$HERE/hw3d_ps.rc" > "$OUT/hw3d_ps.inl"
echo "wrote $OUT/hw3d_vp.inl and $OUT/hw3d_ps.inl"
