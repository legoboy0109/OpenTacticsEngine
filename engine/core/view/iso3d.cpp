#include "core/view/iso3d.h"

#include <algorithm>

namespace ote {

namespace {
// How far the sight line moves over the ground per sample, in squares. Small
// enough that it cannot jump over a block's corner unnoticed in practice.
const float kStride = 0.05f;
}

int Iso3DGeometry::pick(float wx, float wy, const std::vector<int>& elev) const {
  if (hex.on) {  // the same walk down the line of sight, over hexagon blocks
    if (elev.size() < (size_t)(hex.geo.cols * hex.geo.rows)) return -1;
    float top = levels * stepY, bottom = -stepY;
    int n = std::max(1, (int)std::ceil((top - bottom) * cosPitch() / sinPitch / kStride));
    for (int i = 0; i <= n; ++i) {
      float Y = top - (top - bottom) * (float)i / (float)n, gx, gy;
      unprojectAt(wx, wy, Y, gx, gy);
      int c = hex.cellAt(gx - spaceW() * 0.5f, gy - spaceH() * 0.5f);
      if (c >= 0 && Y <= elev[(size_t)c] * stepY + 1e-4f) return c;
    }
    return -1;
  }
  if (gridW <= 0 || gridH <= 0 || elev.size() < (size_t)(gridW * gridH)) return -1;
  // Follow the line of sight through the pixel from above the highest block
  // down to the underside; lower on the line is further from the camera, so
  // the first block met is the one seen.
  float top = levels * stepY, bottom = -stepY;
  float ground = (top - bottom) * cosPitch() / sinPitch;  // ground distance covered on the way down
  int n = std::max(1, (int)std::ceil(ground / kStride));
  for (int i = 0; i <= n; ++i) {
    float Y = top - (top - bottom) * (float)i / (float)n, gx, gy;
    unprojectAt(wx, wy, Y, gx, gy);
    int x = (int)std::floor(gx), y = (int)std::floor(gy);
    if (x < 0 || y < 0 || x >= gridW || y >= gridH) continue;
    if (Y <= elev[(size_t)(y * gridW + x)] * stepY + 1e-4f) return y * gridW + x;
  }
  return -1;
}

bool Iso3DGeometry::hidden(int x, int y, const std::vector<int>& elev) const {
  if (hex.on) {
    if (x < 0 || y < 0 || x >= hex.geo.cols || y >= hex.geo.rows || elev.size() < (size_t)(hex.geo.cols * hex.geo.rows)) return false;
    float s = std::sin(yaw), c = std::cos(yaw), rise = sinPitch / cosPitch(), X, Z;
    hex.middle(x, y, X, Z);
    const int self = y * hex.geo.cols + x;
    const float base = elev[(size_t)self] * stepY, top = levels * stepY;
    for (int i = 1; base + rise * kStride * i <= top; ++i) {
      float Y = base + rise * kStride * i;
      int o = hex.cellAt(X + s * kStride * i, Z + c * kStride * i);
      if (o < 0) return false;
      if (o != self && elev[(size_t)o] * stepY > Y + 1e-3f) return true;
    }
    return false;
  }
  if (x < 0 || y < 0 || x >= gridW || y >= gridH || elev.size() < (size_t)(gridW * gridH)) return false;
  // Walk from the middle of the top face towards the camera.
  float s = std::sin(yaw), c = std::cos(yaw), rise = sinPitch / cosPitch();
  float gx = x + 0.5f, gy = y + 0.5f, Y = elev[(size_t)(y * gridW + x)] * stepY, top = levels * stepY;
  for (int i = 1; Y <= top; ++i) {
    float px = gx + s * kStride * i, py = gy + c * kStride * i;
    Y = elev[(size_t)(y * gridW + x)] * stepY + rise * kStride * i;
    int cx = (int)std::floor(px), cy = (int)std::floor(py);
    if (cx < 0 || cy < 0 || cx >= gridW || cy >= gridH) return false;
    if ((cx != x || cy != y) && elev[(size_t)(cy * gridW + cx)] * stepY > Y + 1e-3f) return true;
  }
  return false;
}

}  // namespace ote
