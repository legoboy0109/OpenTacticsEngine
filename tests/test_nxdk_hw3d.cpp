// The parts of the original Xbox's hardware 3D backend that need no console (platform/nxdk/hw3d_texture.h, hw3d_clip.h).
#include <cmath>
#include <set>
#include <vector>

#include "platform/nxdk/hw3d_clip.h"
#include "platform/nxdk/hw3d_texture.h"
#include "test.h"

using namespace ote;

TEST(hw3d_mip_levels_go_down_to_one_pixel) {
  CHECK_EQ(hw3d::mipLevelCount(1, 1), 1);
  CHECK_EQ(hw3d::mipLevelCount(8, 8), 4);
  CHECK_EQ(hw3d::mipLevelCount(1024, 512), 11);
  CHECK_EQ((int)hw3d::swizzledBytes(4, 2, 3), (4 * 2 + 2 * 1 + 1 * 1) * 4);
}

TEST(hw3d_swizzle_interleaves_x_first_and_is_a_permutation) {
  CHECK_EQ((int)hw3d::swizzleIndex(1, 0, 8, 8), 1);
  CHECK_EQ((int)hw3d::swizzleIndex(0, 1, 8, 8), 2);
  CHECK_EQ((int)hw3d::swizzleIndex(1, 1, 8, 8), 3);
  CHECK_EQ((int)hw3d::swizzleIndex(2, 0, 8, 8), 4);
  CHECK_EQ((int)hw3d::swizzleIndex(7, 7, 8, 8), 63);
  // A rectangle: the longer side's bits above the shorter one's run on in the high bits.
  CHECK_EQ((int)hw3d::swizzleIndex(2, 0, 8, 2), 4);
  CHECK_EQ((int)hw3d::swizzleIndex(4, 0, 8, 2), 8);
  CHECK_EQ((int)hw3d::swizzleIndex(0, 1, 8, 2), 2);
  CHECK_EQ((int)hw3d::swizzleIndex(1, 7, 2, 8), 15);  // tall: the three bits of y: the first after x, the others run on
  const int dims[][2] = {{8, 8}, {16, 4}, {2, 32}, {1, 8}, {8, 1}, {1, 1}};
  for (const auto& d : dims) {
    std::set<uint32_t> seen;
    for (int y = 0; y < d[1]; ++y)
      for (int x = 0; x < d[0]; ++x) seen.insert(hw3d::swizzleIndex(x, y, d[0], d[1]));
    CHECK_EQ((int)seen.size(), d[0] * d[1]);
    CHECK_EQ((int)*seen.rbegin(), d[0] * d[1] - 1);
  }
}

TEST(hw3d_build_swizzled_converts_to_argb_and_averages_the_mip_level) {
  // 2 x 2: red, green / blue, white (R,G,B,A bytes)
  const uint8_t px[16] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 255};
  uint32_t out[5] = {};
  hw3d::buildSwizzled(px, 2, 2, 2, out);
  CHECK_EQ((unsigned)out[hw3d::swizzleIndex(0, 0, 2, 2)], 0xFFFF0000u);
  CHECK_EQ((unsigned)out[hw3d::swizzleIndex(1, 0, 2, 2)], 0xFF00FF00u);
  CHECK_EQ((unsigned)out[hw3d::swizzleIndex(0, 1, 2, 2)], 0xFF0000FFu);
  CHECK_EQ((unsigned)out[hw3d::swizzleIndex(1, 1, 2, 2)], 0xFFFFFFFFu);
  CHECK_EQ((unsigned)out[4], 0xFF808080u);  // (255+0+0+255)/4 = 128 in each colour
}

namespace {
// A camera at the origin looking down -Z, 90 degrees, near 1, far 100: the matrix rows as the vertex program takes them.
void camera(float clip[16]) {
  Mat4 m = Mat4::perspective(1.5707964f, 1.0f, 1.0f, 100.0f);
  for (int r = 0; r < 4; ++r)
    for (int k = 0; k < 4; ++k) clip[r * 4 + k] = m.m[k * 4 + r];
}
Vertex3D at(float x, float y, float z) {
  Vertex3D v;
  v.x = x;
  v.y = y;
  v.z = z;
  return v;
}
}  // namespace

TEST(hw3d_clip_classifies_boxes) {
  float clip[16];
  camera(clip);
  const float guard = 2;
  float lo1[3] = {-1, -1, -20}, hi1[3] = {1, 1, -10};
  CHECK(hw3d::classifyBox(clip, lo1, hi1, guard) == hw3d::Visibility::Inside);
  float lo2[3] = {-1, -1, 5}, hi2[3] = {1, 1, 10};  // behind the camera
  CHECK(hw3d::classifyBox(clip, lo2, hi2, guard) == hw3d::Visibility::Outside);
  float lo3[3] = {-50, -1, -30}, hi3[3] = {50, 1, 5};  // a ground plane under the camera: crosses the near plane
  CHECK(hw3d::classifyBox(clip, lo3, hi3, guard) == hw3d::Visibility::Straddles);
  float lo4[3] = {-1, -1, -0.5f}, hi4[3] = {1, 1, -0.2f};  // in front but nearer than the near plane
  CHECK(hw3d::classifyBox(clip, lo4, hi4, guard) == hw3d::Visibility::Outside);
}

TEST(hw3d_clip_cuts_a_triangle_at_the_near_plane) {
  float clip[16];
  camera(clip);
  // One corner behind the camera, two in front: the part past z = -1 (the near plane) must remain, as two triangles at most.
  std::vector<Vertex3D> v = {at(-1, 0, -10), at(1, 0, -10), at(0, 0, 5)};
  uint16_t idx[3] = {0, 1, 2};
  hw3d::ClipResult r;
  hw3d::clipMesh(v.data(), v.size(), idx, 3, clip, 2.0f, r);
  CHECK(r.keep.empty());
  CHECK(r.loose.size() >= 3 && r.loose.size() % 3 == 0 && r.loose.size() <= 9);
  for (const Vertex3D& p : r.loose) CHECK(p.z <= -1.0f + 1e-3f);  // nothing nearer than the near plane
  // The cut keeps the winding: area sign (in the x-z plane) is the same as the original triangle's.
  float area0 = (v[1].x - v[0].x) * (v[2].z - v[0].z) - (v[2].x - v[0].x) * (v[1].z - v[0].z);
  float area1 = (r.loose[1].x - r.loose[0].x) * (r.loose[2].z - r.loose[0].z) - (r.loose[2].x - r.loose[0].x) * (r.loose[1].z - r.loose[0].z);
  CHECK(area0 * area1 > 0);
}

TEST(hw3d_clip_keeps_whole_triangles_and_drops_outside_ones) {
  float clip[16];
  camera(clip);
  std::vector<Vertex3D> v = {at(-1, 0, -10), at(1, 0, -10), at(0, 1, -10),  // inside
                             at(-1, 0, 10), at(1, 0, 10), at(0, 1, 10),     // behind the camera
                             at(500, 0, -10), at(502, 0, -10), at(501, 1, -10)};  // far to the right
  uint16_t idx[9] = {0, 1, 2, 3, 4, 5, 6, 7, 8};
  hw3d::ClipResult r;
  hw3d::clipMesh(v.data(), v.size(), idx, 9, clip, 2.0f, r);
  CHECK_EQ((int)r.keep.size(), 3);
  CHECK_EQ((int)r.keep[0], 0);
  CHECK(r.loose.empty());
}

TEST(hw3d_clip_interpolates_what_the_cut_vertices_carry) {
  float clip[16];
  camera(clip);
  Vertex3D a = at(0, 0, -5), b = at(0, 0, 3), c = at(1, 0, -5);
  a.u = 0;
  b.u = 1;  // runs from -5 to 3; the near plane z = -1 is half way along
  c.u = 0;
  a.r = 0;
  b.r = 200;
  c.r = 0;
  std::vector<Vertex3D> v = {a, b, c};
  uint16_t idx[3] = {0, 1, 2};
  hw3d::ClipResult r;
  hw3d::clipMesh(v.data(), v.size(), idx, 3, clip, 2.0f, r);
  bool found = false;
  for (const Vertex3D& p : r.loose)
    if (std::fabs(p.z + 1.0f) < 1e-3f && std::fabs(p.x) < 1e-3f) {
      found = true;
      CHECK(std::fabs(p.u - 0.5f) < 1e-3f);
      CHECK(std::abs((int)p.r - 100) <= 1);
    }
  CHECK(found);
}

TEST(hw3d_halve_weights_colours_by_alpha) {
  // 2 x 2: one opaque red pixel and three fully transparent black ones: the result is red, a quarter opaque.
  const uint8_t px[16] = {255, 0, 0, 255, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
  std::vector<uint8_t> out;
  hw3d::halveImage(px, 2, 2, out);
  CHECK_EQ((int)out.size(), 4);
  CHECK_EQ((int)out[0], 255);
  CHECK_EQ((int)out[1], 0);
  CHECK_EQ((int)out[3], 64);
  hw3d::halveImage(px, 1, 1, out);  // a 1 x 1 picture stays 1 x 1
  CHECK_EQ((int)out.size(), 4);
}
