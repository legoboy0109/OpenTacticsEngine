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
