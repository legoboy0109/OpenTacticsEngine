// glTF loader: fitting content to what a renderer can take (Caps3D).
#include <algorithm>

#include "gfx3d/model_internal.h"

namespace ote {
namespace gltf {

void splitMesh(const MeshData& in, int maxVertices, std::vector<MeshData>& out) {
  out.clear();
  const size_t limit = (size_t)std::max(maxVertices, 3);  // a triangle needs three vertices
  if (in.vertices.size() <= limit) {
    out.push_back(in);
    return;
  }
  const uint32_t kNone = 0xFFFFFFFFu;
  std::vector<uint32_t> remap(in.vertices.size(), kNone);  // old vertex -> index in the current part
  std::vector<uint32_t> used;                              // old vertices in the current part (to reset remap)
  MeshData cur;
  auto flush = [&]() {
    if (!cur.indices.empty()) out.push_back(cur);
    cur = MeshData();
    for (uint32_t o : used) remap[o] = kNone;
    used.clear();
  };
  for (size_t t = 0; t + 2 < in.indices.size(); t += 3) {
    size_t fresh = 0;
    for (int k = 0; k < 3; ++k) {
      uint32_t o = in.indices[t + k];
      bool dup = false;
      for (int q = 0; q < k; ++q) dup = dup || in.indices[t + q] == o;  // the same new vertex twice counts once
      if (remap[o] == kNone && !dup) ++fresh;
    }
    if (cur.vertices.size() + fresh > limit) flush();
    for (int k = 0; k < 3; ++k) {
      uint32_t o = in.indices[t + k];
      if (remap[o] == kNone) {
        remap[o] = (uint32_t)cur.vertices.size();
        cur.vertices.push_back(in.vertices[o]);
        used.push_back(o);
      }
      cur.indices.push_back(remap[o]);
    }
  }
  flush();
}

namespace {
int floorPow2(int v) {
  int p = 1;
  while (p * 2 <= v && p < (1 << 30)) p *= 2;
  return p;
}
}  // namespace

void fitImage(ModelImage& img, int maxSide) {
  const int w = img.width, h = img.height;
  int cap = floorPow2(std::max(maxSide, 1));
  int nw = std::min(floorPow2(w), cap), nh = std::min(floorPow2(h), cap);
  if (nw == w && nh == h) return;
  // Each new pixel averages the block of old pixels it covers (new <= old, so a box filter is enough). Colours are
  // weighted by alpha so transparent pixels do not darken the edges of cut-outs.
  std::vector<uint8_t> px((size_t)nw * nh * 4);
  for (int y = 0; y < nh; ++y) {
    int y0 = (int)((long long)y * h / nh), y1 = std::max(y0 + 1, (int)((long long)(y + 1) * h / nh));
    for (int x = 0; x < nw; ++x) {
      int x0 = (int)((long long)x * w / nw), x1 = std::max(x0 + 1, (int)((long long)(x + 1) * w / nw));
      double sr = 0, sg = 0, sb = 0, sa = 0;
      int n = 0;
      for (int sy = y0; sy < y1; ++sy)
        for (int sx = x0; sx < x1; ++sx) {
          const uint8_t* s = &img.rgba[((size_t)sy * w + sx) * 4];
          sr += s[0] * (double)s[3];
          sg += s[1] * (double)s[3];
          sb += s[2] * (double)s[3];
          sa += s[3];
          ++n;
        }
      uint8_t* d = &px[((size_t)y * nw + x) * 4];
      if (sa > 0) {
        d[0] = (uint8_t)(sr / sa + 0.5);
        d[1] = (uint8_t)(sg / sa + 0.5);
        d[2] = (uint8_t)(sb / sa + 0.5);
      } else {
        d[0] = d[1] = d[2] = 0;
      }
      d[3] = (uint8_t)(sa / n + 0.5);
    }
  }
  img.width = nw;
  img.height = nh;
  img.rgba.swap(px);
}

}  // namespace gltf
}  // namespace ote
