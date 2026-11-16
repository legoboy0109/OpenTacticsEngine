#include "gfx3d/shapes.h"

namespace ote {

void addQuad(MeshData& m, Vec3 bl, Vec3 br, Vec3 tr, Vec3 tl, Tint3D t, float u0, float v0, float u1, float v1) {
  Vec3 n = normalize(cross(br - bl, tl - bl));
  uint32_t base = (uint32_t)m.vertices.size();
  const Vec3 p[4] = {bl, br, tr, tl};
  const float uv[4][2] = {{u0, v1}, {u1, v1}, {u1, v0}, {u0, v0}};
  for (int i = 0; i < 4; ++i) {
    Vertex3D v;
    v.x = p[i].x;
    v.y = p[i].y;
    v.z = p[i].z;
    v.nx = n.x;
    v.ny = n.y;
    v.nz = n.z;
    v.u = uv[i][0];
    v.v = uv[i][1];
    v.r = t.r;
    v.g = t.g;
    v.b = t.b;
    v.a = t.a;
    m.vertices.push_back(v);
  }
  const uint32_t idx[6] = {0, 1, 2, 0, 2, 3};
  for (uint32_t i : idx) m.indices.push_back(base + i);
}

void addFan(MeshData& m, const Vec3* ring, int n, Tint3D t, const float* uv) {
  if (n < 3) return;
  Vec3 nrm = normalize(cross(ring[1] - ring[0], ring[2] - ring[0])), mid(0, 0, 0);
  float mu = 0, mv = 0;
  for (int i = 0; i < n; ++i) {
    mid = mid + ring[i];
    if (uv) {
      mu += uv[2 * i];
      mv += uv[2 * i + 1];
    }
  }
  mid = mid * (1.0f / (float)n);
  auto push = [&](Vec3 p, float u, float v) {
    Vertex3D x;
    x.x = p.x;
    x.y = p.y;
    x.z = p.z;
    x.nx = nrm.x;
    x.ny = nrm.y;
    x.nz = nrm.z;
    x.u = u;
    x.v = v;
    x.r = t.r;
    x.g = t.g;
    x.b = t.b;
    x.a = t.a;
    m.vertices.push_back(x);
  };
  const uint32_t base = (uint32_t)m.vertices.size();
  push(mid, mu / (float)n, mv / (float)n);
  for (int i = 0; i < n; ++i) push(ring[i], uv ? uv[2 * i] : 0, uv ? uv[2 * i + 1] : 0);
  for (int i = 0; i < n; ++i) {
    m.indices.push_back(base);
    m.indices.push_back(base + 1 + (uint32_t)i);
    m.indices.push_back(base + 1 + (uint32_t)((i + 1) % n));
  }
}

void addBlock(MeshData& m, Vec3 lo, Vec3 hi, Tint3D top, Tint3D side) {
  float h = hi.y - lo.y;
  addQuad(m, Vec3(lo.x, hi.y, hi.z), Vec3(hi.x, hi.y, hi.z), Vec3(hi.x, hi.y, lo.z), Vec3(lo.x, hi.y, lo.z), top);
  if (h <= 0) return;
  addQuad(m, Vec3(lo.x, lo.y, hi.z), Vec3(hi.x, lo.y, hi.z), Vec3(hi.x, hi.y, hi.z), Vec3(lo.x, hi.y, hi.z), side, 0, 0, 1, h);  // +Z
  addQuad(m, Vec3(hi.x, lo.y, lo.z), Vec3(lo.x, lo.y, lo.z), Vec3(lo.x, hi.y, lo.z), Vec3(hi.x, hi.y, lo.z), side, 0, 0, 1, h);  // -Z
  addQuad(m, Vec3(hi.x, lo.y, hi.z), Vec3(hi.x, lo.y, lo.z), Vec3(hi.x, hi.y, lo.z), Vec3(hi.x, hi.y, hi.z), side, 0, 0, 1, h);  // +X
  addQuad(m, Vec3(lo.x, lo.y, lo.z), Vec3(lo.x, lo.y, hi.z), Vec3(lo.x, hi.y, hi.z), Vec3(lo.x, hi.y, lo.z), side, 0, 0, 1, h);  // -X
}

}  // namespace ote
