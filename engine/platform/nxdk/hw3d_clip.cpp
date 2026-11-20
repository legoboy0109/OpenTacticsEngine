#include "platform/nxdk/hw3d_clip.h"

namespace ote {
namespace hw3d {

namespace {

const int kPlanes = 5;

// Signed distances to the five boundaries from clip coordinates: >= 0 is inside.
void distances(const float c[4], float guard, float d[kPlanes]) {
  d[0] = c[2] + c[3];          // near: z >= -w
  d[1] = guard * c[3] - c[0];  // x <=  guard * w
  d[2] = guard * c[3] + c[0];  // x >= -guard * w
  d[3] = guard * c[3] - c[1];
  d[4] = guard * c[3] + c[1];
}

void transform(const float m[16], const Vertex3D& v, float out[4]) {
  for (int r = 0; r < 4; ++r) out[r] = m[r * 4] * v.x + m[r * 4 + 1] * v.y + m[r * 4 + 2] * v.z + m[r * 4 + 3];
}

struct Corner {
  Vertex3D v;
  float d[kPlanes];
};

uint8_t lerpByte(uint8_t a, uint8_t b, float t) {
  float r = (float)a + ((float)b - (float)a) * t + 0.5f;
  return (uint8_t)(r < 0 ? 0 : r > 255 ? 255 : r);
}

Corner between(const Corner& a, const Corner& b, float t) {
  Corner c;
  c.v.x = a.v.x + (b.v.x - a.v.x) * t;
  c.v.y = a.v.y + (b.v.y - a.v.y) * t;
  c.v.z = a.v.z + (b.v.z - a.v.z) * t;
  c.v.nx = a.v.nx + (b.v.nx - a.v.nx) * t;
  c.v.ny = a.v.ny + (b.v.ny - a.v.ny) * t;
  c.v.nz = a.v.nz + (b.v.nz - a.v.nz) * t;
  c.v.u = a.v.u + (b.v.u - a.v.u) * t;
  c.v.v = a.v.v + (b.v.v - a.v.v) * t;
  c.v.r = lerpByte(a.v.r, b.v.r, t);
  c.v.g = lerpByte(a.v.g, b.v.g, t);
  c.v.b = lerpByte(a.v.b, b.v.b, t);
  c.v.a = lerpByte(a.v.a, b.v.a, t);
  for (int k = 0; k < kPlanes; ++k) c.d[k] = a.d[k] + (b.d[k] - a.d[k]) * t;  // distances are linear in position too
  return c;
}

}  // namespace

Visibility classifyBox(const float clip[16], const float lo[3], const float hi[3], float guard) {
  bool allInside = true;
  bool outsideOne[kPlanes] = {true, true, true, true, true};  // every corner is outside plane k
  for (int i = 0; i < 8; ++i) {
    Vertex3D p;
    p.x = (i & 1) ? hi[0] : lo[0];
    p.y = (i & 2) ? hi[1] : lo[1];
    p.z = (i & 4) ? hi[2] : lo[2];
    float c[4], d[kPlanes];
    transform(clip, p, c);
    distances(c, guard, d);
    for (int k = 0; k < kPlanes; ++k) {
      if (d[k] < 0) allInside = false;
      else outsideOne[k] = false;
    }
  }
  if (allInside) return Visibility::Inside;
  for (int k = 0; k < kPlanes; ++k)
    if (outsideOne[k]) return Visibility::Outside;
  return Visibility::Straddles;
}

void clipMesh(const Vertex3D* vertices, size_t vertexCount, const uint16_t* indices, size_t indexCount, const float clip[16],
              float guard, ClipResult& out) {
  std::vector<float> dist(vertexCount * kPlanes);
  for (size_t i = 0; i < vertexCount; ++i) {
    float c[4];
    transform(clip, vertices[i], c);
    distances(c, guard, &dist[i * kPlanes]);
  }
  for (size_t t = 0; t + 2 < indexCount; t += 3) {
    const uint16_t i0 = indices[t], i1 = indices[t + 1], i2 = indices[t + 2];
    if (i0 >= vertexCount || i1 >= vertexCount || i2 >= vertexCount) continue;
    const float* d[3] = {&dist[i0 * kPlanes], &dist[i1 * kPlanes], &dist[i2 * kPlanes]};
    bool inside = true, outside = false;
    for (int k = 0; k < kPlanes; ++k) {
      const bool a = d[0][k] >= 0, b = d[1][k] >= 0, c = d[2][k] >= 0;
      if (!(a && b && c)) inside = false;
      if (!a && !b && !c) outside = true;
    }
    if (outside) continue;
    if (inside) {
      out.keep.push_back(i0);
      out.keep.push_back(i1);
      out.keep.push_back(i2);
      continue;
    }
    // Crosses a boundary: cut the polygon by each plane in turn (at most 3 + 5 corners), then fan it into triangles.
    Corner poly[16], next[16];
    int n = 3;
    const uint16_t ids[3] = {i0, i1, i2};
    for (int k = 0; k < 3; ++k) {
      poly[k].v = vertices[ids[k]];
      for (int p = 0; p < kPlanes; ++p) poly[k].d[p] = d[k][p];
    }
    for (int p = 0; p < kPlanes && n >= 3; ++p) {
      int m = 0;
      for (int k = 0; k < n; ++k) {
        const Corner& a = poly[k];
        const Corner& b = poly[(k + 1) % n];
        const bool ina = a.d[p] >= 0, inb = b.d[p] >= 0;
        if (ina) next[m++] = a;
        if (ina != inb) next[m++] = between(a, b, a.d[p] / (a.d[p] - b.d[p]));
      }
      for (int k = 0; k < m; ++k) poly[k] = next[k];
      n = m;
    }
    for (int k = 1; k + 1 < n; ++k) {
      out.loose.push_back(poly[0].v);
      out.loose.push_back(poly[k].v);
      out.loose.push_back(poly[k + 1].v);
    }
  }
}

}  // namespace hw3d
}  // namespace ote
