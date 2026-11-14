#include "core/view/free3d.h"

#include <algorithm>

namespace ote {

namespace {

// The camera's sideways (r), upward (u) and forward (f) directions.
struct Basis {
  float ex, ey, ez, rx, rz, ux, uy, uz, fx, fy, fz;
  explicit Basis(const Free3DGeometry& g) {
    float s = std::sin(g.yaw), c = std::cos(g.yaw), sp = std::sin(g.pitch), cp = std::cos(g.pitch);
    g.eye(ex, ey, ez);
    fx = -s * cp; fy = -sp; fz = -c * cp;
    rx = c; rz = -s;
    ux = -s * sp; uy = cp; uz = -c * sp;  // u = r x f
  }
};

// Walks a line over the grid square by square (the squares it passes over,
// in order). Positions are in squares: x = X + gridW/2, z = Z + gridH/2.
struct Walk {
  int x = 0, z = 0;       // the square now
  float t = 0, tExit = 0; // the line's parameter where it enters / leaves the square
  bool ok = false;

  Walk(float ox, float oz, float dx, float dz, float tFrom, float tTo, int w, int h) : ox_(ox), oz_(oz), dx_(dx), dz_(dz), w_(w), h_(h) {
    // Cut the line to the map's outline.
    float t0 = tFrom, t1 = tTo;
    if (!cut(ox, dx, (float)w, t0, t1) || !cut(oz, dz, (float)h, t0, t1) || t0 > t1) return;
    t = t0;
    end_ = t1;
    x = std::max(0, std::min((int)std::floor(ox + dx * t0), w - 1));
    z = std::max(0, std::min((int)std::floor(oz + dz * t0), h - 1));
    const float inf = 1e30f;
    nextX_ = dx > 0 ? (x + 1 - ox) / dx : dx < 0 ? (x - ox) / dx : inf;
    nextZ_ = dz > 0 ? (z + 1 - oz) / dz : dz < 0 ? (z - oz) / dz : inf;
    stepX_ = dx != 0 ? std::fabs(1.0f / dx) : inf;
    stepZ_ = dz != 0 ? std::fabs(1.0f / dz) : inf;
    tExit = std::min(std::min(nextX_, nextZ_), end_);
    ok = true;
  }
  bool next() {
    if (tExit >= end_) return ok = false;
    t = tExit;
    if (nextX_ < nextZ_) { x += dx_ > 0 ? 1 : -1; nextX_ += stepX_; }
    else { z += dz_ > 0 ? 1 : -1; nextZ_ += stepZ_; }
    if (x < 0 || z < 0 || x >= w_ || z >= h_) return ok = false;
    tExit = std::min(std::min(nextX_, nextZ_), end_);
    return true;
  }

 private:
  static bool cut(float o, float d, float size, float& t0, float& t1) {
    if (std::fabs(d) < 1e-9f) return o >= 0 && o <= size;
    float a = (0 - o) / d, b = (size - o) / d;
    if (a > b) std::swap(a, b);
    t0 = std::max(t0, a);
    t1 = std::min(t1, b);
    return true;
  }
  float ox_, oz_, dx_, dz_, end_ = 0, nextX_ = 0, nextZ_ = 0, stepX_ = 0, stepZ_ = 0;
  int w_, h_;
};

}  // namespace

bool Free3DGeometry::projectPoint(float X, float Y, float Z, float& sx, float& sy, float* depth) const {
  Basis b(*this);
  float dx = X - b.ex, dy = Y - b.ey, dz = Z - b.ez;
  float zc = dx * b.fx + dy * b.fy + dz * b.fz;
  if (depth) *depth = zc;
  if (zc < kNear) {
    sx = sy = -100000.0f;
    return false;
  }
  float k = focal() / zc;
  sx = viewW * 0.5f + (dx * b.rx + dz * b.rz) * k;
  sy = viewH * 0.5f - (dx * b.ux + dy * b.uy + dz * b.uz) * k;
  return true;
}

float Free3DGeometry::pixelsAt(float X, float Y, float Z) const {
  float sx, sy, depth;
  return projectPoint(X, Y, Z, sx, sy, &depth) ? focal() / depth : 0.0f;
}

void Free3DGeometry::ray(float sx, float sy, float& dx, float& dy, float& dz) const {
  Basis b(*this);
  float a = (sx - viewW * 0.5f) / focal(), c = (viewH * 0.5f - sy) / focal();
  dx = b.fx + b.rx * a + b.ux * c;
  dy = b.fy + b.uy * c;
  dz = b.fz + b.rz * a + b.uz * c;
}

bool Free3DGeometry::groundAt(float sx, float sy, float Y, float& X, float& Z) const {
  float ex, ey, ez, dx, dy, dz;
  eye(ex, ey, ez);
  ray(sx, sy, dx, dy, dz);
  if (dy > -1e-5f) return false;  // level or upwards: never comes down
  float t = (Y - ey) / dy;
  if (t <= 0) return false;
  X = ex + dx * t;
  Z = ez + dz * t;
  return true;
}

int Free3DGeometry::pick(float sx, float sy, const std::vector<int>& elev) const {
  if (gridW <= 0 || gridH <= 0 || elev.size() < (size_t)(gridW * gridH)) return -1;
  float ex, ey, ez, dx, dy, dz;
  eye(ex, ey, ez);
  ray(sx, sy, dx, dy, dz);
  const float bottom = -stepY;
  for (Walk w(ex + gridW * 0.5f, ez + gridH * 0.5f, dx, dz, 0.0f, 1e6f, gridW, gridH); w.ok; w.next()) {
    float y0 = ey + dy * w.t, y1 = ey + dy * w.tExit;
    float top = elev[(size_t)(w.z * gridW + w.x)] * stepY;
    if (std::max(y0, y1) < bottom) return -1;          // under the map: nothing further on can be seen
    if (std::min(y0, y1) <= top + 1e-4f) return w.z * gridW + w.x;
  }
  return -1;
}

bool Free3DGeometry::hidden(int x, int y, const std::vector<int>& elev) const {
  if (x < 0 || y < 0 || x >= gridW || y >= gridH || elev.size() < (size_t)(gridW * gridH)) return false;
  // From just above the middle of the top face to the eye.
  float ox = x + 0.5f, oz = y + 0.5f, oy = elev[(size_t)(y * gridW + x)] * stepY + 0.02f;
  float ex, ey, ez;
  eye(ex, ey, ez);
  float dx = ex + gridW * 0.5f - ox, dz = ez + gridH * 0.5f - oz, dy = ey - oy;
  for (Walk w(ox, oz, dx, dz, 0.0f, 1.0f, gridW, gridH); w.ok; w.next()) {
    if (w.x == x && w.z == y) continue;
    float low = std::min(oy + dy * w.t, oy + dy * w.tExit);
    if (low < elev[(size_t)(w.z * gridW + w.x)] * stepY - 1e-3f) return true;
    if (dy > 0 && low > levels * stepY) break;  // above everything from here on
  }
  return false;
}

void Free3DGeometry::shiftIntoView(float X, float Y, float Z, float marginX, float marginY, float& dX, float& dZ) const {
  dX = dZ = 0;
  marginX = std::min(marginX, viewW * 0.5f - 1);
  marginY = std::min(marginY, viewH * 0.5f - 1);
  float sx, sy;
  if (!projectPoint(X, Y, Z, sx, sy)) {  // behind the camera: go straight there
    dX = X - focusX;
    dZ = Z - focusZ;
    return;
  }
  float qx = std::max(marginX, std::min(sx, viewW - marginX)), qy = std::max(marginY, std::min(sy, viewH - marginY));
  if (qx == sx && qy == sy) return;
  // Moving the focus moves the whole camera with it, so the point lands on
  // pixel q exactly when the focus moves by (the point - what q shows now).
  float gX, gZ;
  if (!groundAt(qx, qy, Y, gX, gZ)) {
    dX = X - focusX;
    dZ = Z - focusZ;
    return;
  }
  dX = X - gX;
  dZ = Z - gZ;
}

void Free3DGeometry::clamp() {
  pitch = std::max(kMinPitch, std::min(pitch, kMaxPitch));
  distance = std::max(kMinDistance, std::min(distance, kMaxDistance));
  focusX = std::max(-gridW * 0.5f, std::min(focusX, gridW * 0.5f));
  focusZ = std::max(-gridH * 0.5f, std::min(focusZ, gridH * 0.5f));
}

int Free3DGeometry::quarter() const {
  int q = (int)std::lround(yaw / 1.5707963f);
  return ((q % 4) + 4) % 4;
}

}  // namespace ote
