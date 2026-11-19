#!/usr/bin/env bash
# Downloads the pinned SDL3 release source into android/third_party/ (ignored by version control), checks its sha256, and
# unpacks it into its own folder. The Android build takes SDL's Java glue (SDLActivity & co.) and the native SDL3 from
# there. SDL is zlib-licensed; its LICENSE.txt stays in the unpacked tree. Nothing from the archive is executed.
set -euo pipefail

SDL_VERSION="3.4.20"
SDL_SHA256="f3bb06f55bed9557227a700af2ea62e49497ebdf58037732a731a5fb3ef8581f"
SDL_URL="https://github.com/libsdl-org/SDL/releases/download/release-${SDL_VERSION}/SDL3-${SDL_VERSION}.tar.gz"

here="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
dest="$here/../../android/third_party"
tree="$dest/SDL3-${SDL_VERSION}"

if [ -f "$tree/CMakeLists.txt" ]; then
  echo "SDL ${SDL_VERSION} already present: $tree"
  exit 0
fi

mkdir -p "$dest"
work="$(mktemp -d "$dest/.fetch.XXXXXX")"
trap 'rm -rf "$work"' EXIT
curl -fsSL -o "$work/sdl.tar.gz" "$SDL_URL"
echo "${SDL_SHA256}  $work/sdl.tar.gz" | sha256sum -c - >/dev/null || { echo "sha256 mismatch for $SDL_URL" >&2; exit 1; }
mkdir "$work/unpack"
tar -xzf "$work/sdl.tar.gz" -C "$work/unpack"
mv "$work/unpack/SDL3-${SDL_VERSION}" "$tree"
echo "SDL ${SDL_VERSION} unpacked to $tree"
