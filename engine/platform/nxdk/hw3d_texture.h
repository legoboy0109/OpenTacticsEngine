// 3D textures the way the NV2A wants them (docs/RENDER3D.md "Original Xbox backend"): 32-bit A8R8G8B8, each mipmap level
// "swizzled" (pixels stored in an interleaved, Morton-like order the chip reads faster), levels one after the other.
// Plain C++ with no console headers, so the host's unit tests cover it (tests/test_nxdk_hw3d.cpp).
#pragma once
#include <cstddef>
#include <cstdint>

namespace ote {
namespace hw3d {

// Levels of a full mipmap chain down to 1 x 1 (a 1024 x 512 texture has 11).
int mipLevelCount(int width, int height);
// Bytes of `levels` levels of a width x height texture, 4 bytes a pixel.
size_t swizzledBytes(int width, int height, int levels);
// Where pixel (x, y) of a width x height level lies, in 4-byte units from the level's start. Width and height are powers of
// two; for a rectangle the longer side's extra bits are the high bits of the address.
uint32_t swizzleIndex(int x, int y, int width, int height);
// Fills `dst` (swizzledBytes(width, height, levels) bytes) from R,G,B,A bytes (top row first): converts to A8R8G8B8 words,
// makes the smaller levels by averaging 2 x 2 pixels, swizzles each level.
void buildSwizzled(const uint8_t* rgba, int width, int height, int levels, uint32_t* dst);

}  // namespace hw3d
}  // namespace ote
