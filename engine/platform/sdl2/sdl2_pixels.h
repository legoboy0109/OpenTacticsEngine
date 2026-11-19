// The pixel format SDL2 textures are made and uploaded in (docs/PORTING.md "Texture format").
//
// Everything above the backend hands over bytes R,G,B,A. By default textures are SDL_PIXELFORMAT_RGBA32 (the same
// bytes) and nothing is converted. The Direct3D 11 renderer SDL2 uses on UWP has no RGBA32: it converts every
// upload, every frame for the 3D picture, in SDL's generic path. Configure with -DOTE_SDL2_UPLOAD_ARGB8888=ON (defines
// OTE_SDL2_UPLOAD_ARGB8888) to create ARGB8888 textures (D3D11's native BGRA) and swizzle the bytes here, in one tight
// loop, so SDL has nothing left to convert. Pictures are identical either way (tested on Linux, see docs/PORTING.md).
#pragma once
#include <SDL.h>

#include <cstdint>
#include <vector>

namespace ote {

#ifdef OTE_SDL2_UPLOAD_ARGB8888
const Uint32 kUploadFormat = SDL_PIXELFORMAT_ARGB8888;

// Converts a w x h block of R,G,B,A bytes (rows `pitch` bytes apart) into 0xAARRGGBB words in `scratch`; returns the
// pointer and pitch to hand to SDL_UpdateTexture. `scratch` is reused between calls (no allocation once it is big enough).
inline const void* uploadPixels(const uint8_t* rgba, int w, int h, int pitch, std::vector<uint32_t>& scratch, int& outPitch) {
  scratch.resize((size_t)w * (size_t)h);
  uint32_t* out = scratch.data();
  for (int y = 0; y < h; ++y) {
    const uint8_t* p = rgba + (size_t)y * (size_t)pitch;
    for (int x = 0; x < w; ++x, p += 4)
      *out++ = ((uint32_t)p[3] << 24) | ((uint32_t)p[0] << 16) | ((uint32_t)p[1] << 8) | (uint32_t)p[2];
  }
  outPitch = w * 4;
  return scratch.data();
}
#else
const Uint32 kUploadFormat = SDL_PIXELFORMAT_RGBA32;  // bytes R,G,B,A on any endianness (SDL 2.0.5+)

inline const void* uploadPixels(const uint8_t* rgba, int, int, int pitch, std::vector<uint32_t>&, int& outPitch) {
  outPitch = pitch;
  return rgba;
}
#endif

}  // namespace ote
