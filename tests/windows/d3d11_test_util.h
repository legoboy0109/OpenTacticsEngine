// Helpers of the Direct3D 11 backend tests (Windows only; built into ote_tests there, see CMakeLists.txt).
#pragma once
#include <cmath>
#include <cstdio>
#include <memory>
#include <string>
#include <vector>

#include "gfx3d/d3d11/d3d11_device.h"
#include "gfx3d/d3d11/d3d11_renderer3d.h"
#include "gfx3d/render3d.h"

namespace d3d11test {

using namespace ote;

struct Picture {
  int w = 0, h = 0;
  std::vector<uint8_t> px;  // RGBA, top row first
  const uint8_t* at(int x, int y) const { return &px[((size_t)y * w + x) * 4]; }
};

inline Picture grab(const Renderer3D& r) {
  Picture p;
  const uint8_t* src = r.framePixels(p.w, p.h);
  if (src) p.px.assign(src, src + (size_t)p.w * p.h * 4);
  return p;
}

struct Diff {
  int max = 0;          // largest channel difference
  double mean = 0;      // mean channel difference
  int over = 0;         // pixels with a channel more than 8 apart (the player tests' tolerance)
  int total = 0;
  bool sameSize = false;
  std::string where;  // the first few pixels more than 8 apart: "x,y a->b" (red channel)
};

// RGB, and alpha too when `alpha`.
inline Diff compare(const Picture& a, const Picture& b, bool alpha = false) {
  Diff d;
  d.sameSize = a.w == b.w && a.h == b.h && !a.px.empty() && a.px.size() == b.px.size();
  if (!d.sameSize) return d;
  const int ch = alpha ? 4 : 3;
  double sum = 0;
  d.total = a.w * a.h;
  for (int i = 0; i < d.total; ++i) {
    int worst = 0;
    for (int c = 0; c < ch; ++c) {
      int v = std::abs((int)a.px[(size_t)i * 4 + c] - (int)b.px[(size_t)i * 4 + c]);
      sum += v;
      if (v > worst) worst = v;
    }
    if (worst > d.max) d.max = worst;
    if (worst > 8 && ++d.over <= 6) {
      char buf[64];
      std::snprintf(buf, sizeof(buf), " %d,%d %d->%d", i % a.w, i / a.w, a.px[(size_t)i * 4], b.px[(size_t)i * 4]);
      d.where += buf;
    }
  }
  d.mean = sum / ((double)d.total * ch);
  return d;
}

inline void report(const char* what, const char* level, const Diff& d) {
  std::printf("  d3d11 vs software  %-30s level %-5s: max %3d, mean %.3f, %d of %d pixels > 8%s%s\n", what,
              *level ? level : "best", d.max, d.mean, d.over, d.total, d.over ? ", at" : "", d.where.c_str());
}

// A backend on a device of its own (null and a printed reason when this Windows has no Direct3D 11 at all).
inline std::unique_ptr<Renderer3D> makeD3D11(const char* level, std::shared_ptr<D3D11LossFlag> loss = nullptr) {
  D3D11DeviceChoice c;
  c.maxLevel = level;
  std::string why;
  Microsoft::WRL::ComPtr<ID3D11Device> dev = makeD3D11Device(c, why);
  std::unique_ptr<Renderer3D> r;
  if (dev)
    r = D3D11Renderer3D::create(
        std::unique_ptr<D3D11Host>(new OwnD3D11Host(dev, loss ? loss : std::make_shared<D3D11LossFlag>())), why);
  if (!r) std::printf("  (no Direct3D 11 backend at level %s: %s)\n", *level ? level : "best", why.c_str());
  return r;
}

inline Vertex3D vert(float x, float y, float z, float u, float v, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255,
                     uint8_t a = 255) {
  Vertex3D o;
  o.x = x;
  o.y = y;
  o.z = z;
  o.nx = 0;
  o.ny = 0;
  o.nz = 1;
  o.u = u;
  o.v = v;
  o.r = r;
  o.g = g;
  o.b = b;
  o.a = a;
  return o;
}

// A square facing +Z, `half` wide from its middle, at depth z; texture (0,0) at its top-left.
inline MeshData quad(float half, float z, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255, uint8_t a = 255,
                     float uvScale = 1) {
  MeshData m;
  m.vertices = {vert(-half, -half, z, 0, uvScale, r, g, b, a), vert(half, -half, z, uvScale, uvScale, r, g, b, a),
                vert(half, half, z, uvScale, 0, r, g, b, a), vert(-half, half, z, 0, 0, r, g, b, a)};
  m.indices = {0, 1, 2, 0, 2, 3};
  return m;
}

// w x h picture, camera at (0,0,5) looking at the origin, 2 world units visible from top to bottom at the origin.
inline Frame3D frame(int w = 64, int h = 64, bool perspective = false) {
  Frame3D f;
  f.width = w;
  f.height = h;
  f.clear = Rgba3D(0, 0, 0.5f, 1);
  f.view = Mat4::lookAt(Vec3(0, 0, 5), Vec3(0, 0, 0), Vec3(0, 1, 0));
  const float aspect = (float)w / (float)h;
  f.proj = perspective ? Mat4::perspective(2.0f * std::atan(1.0f / 5.0f), aspect, 0.5f, 50.0f)
                       : Mat4::ortho(1.0f, aspect, 0.5f, 50.0f);
  f.sunDir = Vec3(-0.3f, -0.5f, -1);
  f.sun = Rgba3D(0.7f, 0.7f, 0.65f);
  f.ambient = Rgba3D(0.3f, 0.3f, 0.35f);
  return f;
}

}  // namespace d3d11test
