#include "core/data/graphics.h"

#include <cstdio>
#include <cstring>

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

SpriteCell pickCell(const Look& look, int x, int y, int joinMask, uint32_t timeMs) {
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
      int n = joinMask & 15;
      SpriteCell c;
      c.col = (int16_t)(first.col + n % 4);
      c.row = (int16_t)(first.row + n / 4);
      return c;
    }
    default:
      return first;
  }
}

int joinMaskAt(const Look& look, int x, int y, int gridW, int gridH,
               const std::function<int(int, int)>& terrainAt) {
  static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
  static const int bit[4] = {JoinN, JoinE, JoinS, JoinW};
  int mask = 0;
  for (int i = 0; i < 4; ++i) {
    int nx = x + dx[i], ny = y + dy[i];
    if (nx < 0 || ny < 0 || nx >= gridW || ny >= gridH) {
      if (look.joinEdge) mask |= bit[i];
      continue;
    }
    int t = terrainAt(nx, ny);
    if (t >= 0 && (size_t)t < look.joinWith.size() && look.joinWith[(size_t)t]) mask |= bit[i];
  }
  return mask;
}

bool readPngSize(const std::string& path, int& width, int& height, std::string& err) {
  FILE* f = std::fopen(path.c_str(), "rb");
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
