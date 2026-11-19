#include "core/data/graphics.h"

#include <cstdio>
#include <cstring>

#include "core/base/fileio.h"

namespace ote {

int Graphics::findSheet(const std::string& id) const {
  for (size_t i = 0; i < sheets.size(); ++i)
    if (sheets[i].id == id) return (int)i;
  return -1;
}

// Small integer hash so neighbouring squares get unrelated variants, and the
// same square always gets the same one (no flicker, same on every platform).
static uint32_t cellHash(int x, int y) {
  uint32_t h = (uint32_t)x * 0x9E3779B1u ^ ((uint32_t)y * 0x85EBCA77u + 0x165667B1u);
  h ^= h >> 15;
  h *= 0x2C1B3C6Du;
  h ^= h >> 12;
  h *= 0x297A2D39u;
  h ^= h >> 15;
  return h;
}

int WalkLook::pick(float dx, float dy) const {
  static const float vx[kWalkDirs] = {0, 0, 1, -1}, vy[kWalkDirs] = {1, -1, 0, 0};
  int best = -1;
  float bestDot = 0;
  for (int d = 0; d < kWalkDirs; ++d) {
    if (frames[d].empty()) continue;
    const float dot = vx[d] * dx + vy[d] * dy;
    if (best < 0 || dot > bestDot) {
      best = d;
      bestDot = dot;
    }
  }
  return best;
}

SpriteCell pickCell(const Look& look, int x, int y, int joinMask, uint32_t timeMs, int blockSide) {
  if (look.cells.empty()) return SpriteCell();
  const SpriteCell first = look.cells[0];
  switch (look.kind) {
    case Look::Variants:
      return look.cells[cellHash(x, y) % look.cells.size()];
    case Look::Animated: {
      uint32_t ms = look.frameMs > 0 ? (uint32_t)look.frameMs : 400u;
      return look.cells[(timeMs / ms) % look.cells.size()];
    }
    case Look::Join: {
      // Several blocks = an animated join: time picks the block, the
      // neighbours pick the piece inside it.
      SpriteCell block = first;
      if (look.cells.size() > 1) {
        uint32_t ms = look.frameMs > 0 ? (uint32_t)look.frameMs : 400u;
        block = look.cells[(timeMs / ms) % look.cells.size()];
      }
      int n = joinMask & (blockSide * blockSide - 1);
      SpriteCell c;
      c.col = (int16_t)(block.col + n % blockSide);
      c.row = (int16_t)(block.row + n / blockSide);
      return c;
    }
    default:
      return first;
  }
}

SpriteCell pickStandCell(const StandLook& look, int x, int y, uint32_t timeMs) {
  return pickCell(look.picture, x, y, 0, timeMs);
}

int pickStandModel(const StandLook& look, int x, int y) {
  if (look.models.empty()) return 0;
  return (int)(cellHash(x, y) % look.models.size());
}

void standScatter(int x, int y, float& dx, float& dz, float& turnDeg) {
  // Different bits of one hash, so the three values are unrelated to each other and to the variant choice.
  uint32_t h = cellHash(x + 101, y - 37);
  dx = ((int)(h & 0xFF) - 128) / 128.0f * 0.18f;
  dz = ((int)((h >> 8) & 0xFF) - 128) / 128.0f * 0.18f;
  turnDeg = (float)((h >> 16) % 360u);
}

bool readPngSize(const std::string& path, int& width, int& height, std::string& err) {
  FILE* f = openFile(path, "rb");
  if (!f) {
    err = "the file is missing or cannot be read";
    return false;
  }
  unsigned char h[24];
  size_t n = std::fread(h, 1, sizeof h, f);
  std::fclose(f);
  static const unsigned char sig[8] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A};
  if (n < sizeof h || std::memcmp(h, sig, 8) != 0 || std::memcmp(h + 12, "IHDR", 4) != 0) {
    err = "the file is not a PNG image";
    return false;
  }
  uint32_t w = ((uint32_t)h[16] << 24) | ((uint32_t)h[17] << 16) | ((uint32_t)h[18] << 8) | h[19];
  uint32_t ht = ((uint32_t)h[20] << 24) | ((uint32_t)h[21] << 16) | ((uint32_t)h[22] << 8) | h[23];
  if (w == 0 || ht == 0 || w > 1000000u || ht > 1000000u) {
    err = "the PNG image has an invalid size";
    return false;
  }
  width = (int)w;
  height = (int)ht;
  return true;
}

}  // namespace ote
