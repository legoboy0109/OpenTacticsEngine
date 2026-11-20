#include "platform/nxdk/hw3d_texture.h"

#include <algorithm>
#include <vector>

namespace ote {
namespace hw3d {

int mipLevelCount(int width, int height) {
  int n = 1;
  for (int s = std::max(width, height); s > 1; s >>= 1) ++n;
  return n;
}

namespace {
int levelSize(int base, int level) { return std::max(1, base >> level); }

// The bits of x and y are interleaved, x first, up to the shorter side's width; the longer side's remaining bits follow.
// (The order the hardware's address logic uses; the same masks xemu builds.)
void swizzleMasks(int width, int height, uint32_t& maskX, uint32_t& maskY) {
  maskX = maskY = 0;
  uint32_t bit = 1, maskBit = 1;
  bool more;
  do {
    more = false;
    if (bit < (uint32_t)width) {
      maskX |= maskBit;
      maskBit <<= 1;
      more = true;
    }
    if (bit < (uint32_t)height) {
      maskY |= maskBit;
      maskBit <<= 1;
      more = true;
    }
    bit <<= 1;
  } while (more);
}

// Puts the low bits of v into the positions the mask has set.
uint32_t deposit(uint32_t v, uint32_t mask) {
  uint32_t out = 0;
  for (uint32_t bit = 1; mask; bit <<= 1) {
    uint32_t low = mask & (~mask + 1);
    if (v & bit) out |= low;
    mask &= mask - 1;
  }
  return out;
}
}  // namespace

size_t swizzledBytes(int width, int height, int levels) {
  size_t total = 0;
  for (int l = 0; l < levels; ++l) total += (size_t)levelSize(width, l) * (size_t)levelSize(height, l) * 4;
  return total;
}

uint32_t swizzleIndex(int x, int y, int width, int height) {
  uint32_t mx, my;
  swizzleMasks(width, height, mx, my);
  return deposit((uint32_t)x, mx) | deposit((uint32_t)y, my);
}

void halveImage(const uint8_t* rgba, int width, int height, std::vector<uint8_t>& out) {
  const int nw = std::max(1, width / 2), nh = std::max(1, height / 2);
  out.assign((size_t)nw * nh * 4, 0);
  for (int y = 0; y < nh; ++y)
    for (int x = 0; x < nw; ++x) {
      int sum[3] = {0, 0, 0}, alpha = 0, count = 0;
      for (int k = 0; k < 4; ++k) {
        int sx = std::min(width - 1, x * 2 + (k & 1)), sy = std::min(height - 1, y * 2 + (k >> 1));
        const uint8_t* p = &rgba[((size_t)sy * width + sx) * 4];
        for (int c = 0; c < 3; ++c) sum[c] += p[c] * p[3];
        alpha += p[3];
        ++count;
      }
      uint8_t* o = &out[((size_t)y * nw + x) * 4];
      for (int c = 0; c < 3; ++c) o[c] = alpha ? (uint8_t)((sum[c] + alpha / 2) / alpha) : 0;
      o[3] = (uint8_t)((alpha + count / 2) / count);
    }
}

void buildSwizzled(const uint8_t* rgba, int width, int height, int levels, uint32_t* dst) {
  std::vector<uint8_t> cur(rgba, rgba + (size_t)width * height * 4), next;
  int w = width, h = height;
  for (int l = 0; l < levels; ++l) {
    uint32_t mx, my;
    swizzleMasks(w, h, mx, my);
    std::vector<uint32_t> yOff((size_t)h), xOff((size_t)w);
    for (int y = 0; y < h; ++y) yOff[(size_t)y] = deposit((uint32_t)y, my);
    for (int x = 0; x < w; ++x) xOff[(size_t)x] = deposit((uint32_t)x, mx);
    for (int y = 0; y < h; ++y) {
      const uint8_t* s = &cur[(size_t)y * w * 4];
      for (int x = 0; x < w; ++x, s += 4)
        dst[yOff[(size_t)y] | xOff[(size_t)x]] = ((uint32_t)s[3] << 24) | ((uint32_t)s[0] << 16) | ((uint32_t)s[1] << 8) | s[2];
    }
    dst += (size_t)w * h;
    if (l + 1 == levels) break;
    int nw = std::max(1, w / 2), nh = std::max(1, h / 2);
    next.assign((size_t)nw * nh * 4, 0);
    for (int y = 0; y < nh; ++y)
      for (int x = 0; x < nw; ++x) {
        int x0 = std::min(w - 1, x * 2), x1 = std::min(w - 1, x * 2 + 1), y0 = std::min(h - 1, y * 2), y1 = std::min(h - 1, y * 2 + 1);
        for (int c = 0; c < 4; ++c) {
          int sum = cur[((size_t)y0 * w + x0) * 4 + c] + cur[((size_t)y0 * w + x1) * 4 + c] + cur[((size_t)y1 * w + x0) * 4 + c] +
                    cur[((size_t)y1 * w + x1) * 4 + c];
          next[((size_t)y * nw + x) * 4 + c] = (uint8_t)((sum + 2) / 4);
        }
      }
    cur.swap(next);
    w = nw;
    h = nh;
  }
}

}  // namespace hw3d
}  // namespace ote
