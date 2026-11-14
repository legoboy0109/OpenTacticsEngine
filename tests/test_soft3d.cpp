// The software backend of the 3D drawing layer (docs/RENDER3D.md): what ends
// up in the picture for simple scenes.
#include <cmath>

#include "gfx3d/soft3d.h"
#include "test.h"

using namespace ote;

namespace {

Vertex3D vert(float x, float y, float z, float u = 0, float v = 0, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255,
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

// A square facing +Z, `half` wide from its middle, at depth z.
MeshData quad(float half, float z, uint8_t r = 255, uint8_t g = 255, uint8_t b = 255, uint8_t a = 255) {
  MeshData m;
  m.vertices = {vert(-half, -half, z, 0, 1, r, g, b, a), vert(half, -half, z, 1, 1, r, g, b, a),
                vert(half, half, z, 1, 0, r, g, b, a), vert(-half, half, z, 0, 0, r, g, b, a)};
  m.indices = {0, 1, 2, 0, 2, 3};
  return m;
}

// 64 x 64 picture, camera at (0,0,5) looking at the origin, 2 x 2 world units visible at the origin.
Frame3D frame(bool perspective = false) {
  Frame3D f;
  f.width = f.height = 64;
  f.clear = Rgba3D(0, 0, 0.5f, 1);
  f.view = Mat4::lookAt(Vec3(0, 0, 5), Vec3(0, 0, 0), Vec3(0, 1, 0));
  f.proj = perspective ? Mat4::perspective(2.0f * std::atan(1.0f / 5.0f), 1.0f, 0.5f, 50.0f)
                       : Mat4::ortho(1.0f, 1.0f, 0.5f, 50.0f);
  f.sunDir = Vec3(0, 0, -1);  // straight at things facing the camera
  f.sun = Rgba3D(1, 1, 1);
  f.ambient = Rgba3D(0, 0, 0);
  return f;
}

struct Px { int r, g, b, a; };
Px at(const SoftRenderer3D& s, int x, int y) {
  int w, h;
  const uint8_t* p = s.framePixels(w, h);
  const uint8_t* q = p + ((size_t)y * w + x) * 4;
  return Px{q[0], q[1], q[2], q[3]};
}
bool near(Px p, int r, int g, int b, int tol = 2) {
  return std::abs(p.r - r) <= tol && std::abs(p.g - g) <= tol && std::abs(p.b - b) <= tol;
}

}  // namespace

TEST(math3d_basics) {
  Mat4 t = Mat4::translate(Vec3(1, 2, 3)) * Mat4::scale(Vec3(2, 2, 2));
  Vec3 p = t.point(Vec3(1, 1, 1));
  CHECK(std::fabs(p.x - 3) < 1e-5f && std::fabs(p.y - 4) < 1e-5f && std::fabs(p.z - 5) < 1e-5f);
  // A quarter turn about Y takes +X to -Z.
  Vec3 d = Mat4::rotate(Quat::axisAngle(Vec3(0, 1, 0), 3.14159265f / 2)).direction(Vec3(1, 0, 0));
  CHECK(std::fabs(d.x) < 1e-5f && std::fabs(d.z + 1) < 1e-5f);
  // A camera sees what is in front of it at negative z.
  Vec3 v = Mat4::lookAt(Vec3(0, 0, 5), Vec3(0, 0, 0), Vec3(0, 1, 0)).point(Vec3(0, 0, 0));
  CHECK(std::fabs(v.z + 5) < 1e-5f);
  Quat h = nlerp(Quat(), Quat::axisAngle(Vec3(0, 0, 1), 1.0f), 0.5f);
  CHECK(std::fabs(h.z - std::sin(0.25f)) < 1e-3f);
}

TEST(soft3d_draws_a_lit_square_where_it_should_be) {
  SoftRenderer3D s;
  MeshId m = s.createMesh(quad(0.5f, 0, 255, 0, 0));
  CHECK(m != 0);
  s.begin(frame());
  s.draw(m, Mat4(), Material3D());
  s.end();
  int w, h;
  CHECK(s.framePixels(w, h) != nullptr);
  CHECK_EQ(w, 64);
  CHECK_EQ(h, 64);
  CHECK(near(at(s, 32, 32), 255, 0, 0));  // the middle: lit red
  CHECK(near(at(s, 17, 17), 255, 0, 0));  // just inside its corner (it covers pixels 16..47)
  CHECK(near(at(s, 46, 46), 255, 0, 0));
  CHECK(near(at(s, 14, 32), 0, 0, 128));  // just outside: the clear colour
  CHECK(near(at(s, 49, 32), 0, 0, 128));
  CHECK(near(at(s, 2, 2), 0, 0, 128));
  CHECK_EQ(s.trianglesDrawn(), 2);

  // Moved by the model matrix: one unit up is 32 pixels up.
  s.begin(frame());
  s.draw(m, Mat4::translate(Vec3(0, 0.5f, 0)), Material3D());
  s.end();
  CHECK(near(at(s, 32, 4), 255, 0, 0));
  CHECK(near(at(s, 32, 60), 0, 0, 128));

  // Perspective: the same square further away is smaller.
  s.begin(frame(true));
  s.draw(m, Mat4::translate(Vec3(0, 0, -5)), Material3D());
  s.end();
  CHECK(near(at(s, 32, 32), 255, 0, 0));
  CHECK(near(at(s, 20, 32), 0, 0, 128));  // at depth 0 this pixel was inside
}

TEST(soft3d_lighting_culling_and_depth) {
  SoftRenderer3D s;
  MeshId m = s.createMesh(quad(0.5f, 0));
  Frame3D f = frame();
  f.ambient = Rgba3D(0.25f, 0.25f, 0.25f);
  f.sun = Rgba3D(0.5f, 0.5f, 0.5f);
  f.sunDir = Vec3(0, -1, -1);  // 45 degrees onto the square
  s.begin(f);
  s.draw(m, Mat4(), Material3D());
  s.end();
  int expect = (int)((0.25f + 0.5f * 0.70710678f) * 255.0f + 0.5f);
  CHECK(near(at(s, 32, 32), expect, expect, expect, 3));

  // Unlit: the colour as it is. Material colour multiplies.
  Material3D flat;
  flat.lit = false;
  flat.color = Rgba3D(0.5f, 1, 0);
  s.begin(f);
  s.draw(m, Mat4(), flat);
  s.end();
  CHECK(near(at(s, 32, 32), 128, 255, 0));

  // Seen from behind it is not drawn, unless the material is two-sided.
  Mat4 turned = Mat4::rotate(Quat::axisAngle(Vec3(0, 1, 0), 3.14159265f));
  s.begin(f);
  s.draw(m, turned, flat);
  s.end();
  CHECK(near(at(s, 32, 32), 0, 0, 128));
  flat.twoSided = true;
  s.begin(f);
  s.draw(m, turned, flat);
  s.end();
  CHECK(near(at(s, 32, 32), 128, 255, 0));

  // Depth: the nearer square wins whatever the drawing order.
  MeshId nearQuad = s.createMesh(quad(0.25f, 1, 0, 255, 0)), farQuad = s.createMesh(quad(0.5f, -1, 255, 0, 0));
  Material3D plain;
  plain.lit = false;
  for (int order = 0; order < 2; ++order) {
    s.begin(f);
    s.draw(order ? nearQuad : farQuad, Mat4(), plain);
    s.draw(order ? farQuad : nearQuad, Mat4(), plain);
    s.end();
    CHECK(near(at(s, 32, 32), 0, 255, 0));   // near one in the middle
    CHECK(near(at(s, 20, 32), 255, 0, 0));   // far one around it
  }
}

TEST(soft3d_textures_cutout_and_blending) {
  SoftRenderer3D s;
  // 2 x 2 texture: red, green / blue, transparent white.
  const uint8_t px[16] = {255, 0, 0, 255, 0, 255, 0, 255, 0, 0, 255, 255, 255, 255, 255, 0};
  Texture3DId t = s.createTexture(px, 2, 2, false);
  CHECK(t != 0);
  CHECK_EQ(s.createTexture(px, 3, 2, false), 0);  // not a power of two
  MeshId m = s.createMesh(quad(0.5f, 0));
  Material3D mat;
  mat.lit = false;
  mat.texture = t;
  Frame3D f = frame();
  s.begin(f);
  s.draw(m, Mat4(), mat);
  s.end();
  CHECK(near(at(s, 24, 24), 255, 0, 0));   // top-left of the picture = (0,0) of the texture
  CHECK(near(at(s, 40, 24), 0, 255, 0));
  CHECK(near(at(s, 24, 40), 0, 0, 255));
  CHECK(near(at(s, 40, 40), 255, 255, 255));  // opaque material: alpha ignored

  mat.blend = Blend3D::Cutout;
  s.begin(f);
  s.draw(m, Mat4(), mat);
  s.end();
  CHECK(near(at(s, 24, 24), 255, 0, 0));
  CHECK(near(at(s, 40, 40), 0, 0, 128));  // the transparent texel is left out

  // Blending: half see-through white over the red far square, whatever the order;
  // and it does not hide a solid thing drawn later behind it.
  MeshId glass = s.createMesh(quad(0.5f, 1, 255, 255, 255, 128)), wall = s.createMesh(quad(0.5f, -1, 255, 0, 0));
  Material3D see, solid;
  see.lit = solid.lit = false;
  see.blend = Blend3D::Blend;
  for (int order = 0; order < 2; ++order) {
    s.begin(f);
    s.draw(order ? glass : wall, Mat4(), order ? see : solid);
    s.draw(order ? wall : glass, Mat4(), order ? solid : see);
    s.end();
    CHECK(near(at(s, 32, 32), 255, 128, 128, 3));
  }
}

TEST(soft3d_survives_geometry_behind_the_camera_and_bad_input) {
  SoftRenderer3D s;
  // A long floor strip running from far in front of the camera to behind it.
  MeshData floor;
  floor.vertices = {vert(-1, -1, -40), vert(1, -1, -40), vert(1, -1, 40), vert(-1, -1, 40)};
  for (Vertex3D& v : floor.vertices) { v.nx = 0; v.ny = 1; v.nz = 0; }
  floor.indices = {0, 2, 1, 0, 3, 2};
  MeshId m = s.createMesh(floor);
  Material3D mat;
  mat.lit = false;
  mat.twoSided = true;
  s.begin(frame(true));
  s.draw(m, Mat4(), mat);
  s.end();
  CHECK(near(at(s, 32, 60), 255, 255, 255));  // the floor fills the bottom of the picture
  CHECK(near(at(s, 32, 4), 0, 0, 128));       // and nothing leaks into the sky
  CHECK(s.trianglesDrawn() >= 2);

  MeshData bad;
  CHECK_EQ(s.createMesh(bad), 0);
  bad = quad(1, 0);
  bad.indices.push_back(0);
  CHECK_EQ(s.createMesh(bad), 0);  // not whole triangles
  bad = quad(1, 0);
  bad.indices[0] = 99;
  CHECK_EQ(s.createMesh(bad), 0);  // index out of range
  s.begin(frame());
  s.draw(0, Mat4(), mat);          // no mesh: nothing happens
  s.draw(12345, Mat4(), mat);
  s.end();
  s.destroyMesh(m);
  s.begin(frame());
  s.draw(m, Mat4(), mat);          // destroyed: nothing happens
  s.end();
  CHECK(near(at(s, 32, 60), 0, 0, 128));

  Frame3D fog = frame(true);
  fog.fog = true;
  fog.fogColor = Rgba3D(1, 0, 0);
  fog.fogStart = 1;
  fog.fogEnd = 6;
  MeshId q = s.createMesh(quad(0.5f, 0));
  s.begin(fog);
  s.draw(q, Mat4(), mat);  // 5 units away: 80% fog
  s.end();
  Px p = at(s, 32, 32);
  CHECK(p.r > 240 && p.g > 35 && p.g < 70);
}

TEST(soft3d_smooth_textures_blend_texels_and_hard_ones_do_not) {
  SoftRenderer3D s;
  // 2 x 1 texture: black, white. The quad shows it across 64 pixels; the seam between the texels is at x = 32.
  const uint8_t px[8] = {0, 0, 0, 255, 255, 255, 255, 255};
  Texture3DId t = s.createTexture(px, 2, 1, false, true);  // mipmaps asked for: the software backend just ignores that
  CHECK(t != 0);
  MeshData m;
  m.vertices = {vert(-1, -1, 0, 0, 1), vert(1, -1, 0, 1, 1), vert(1, 1, 0, 1, 0), vert(-1, 1, 0, 0, 0)};
  m.indices = {0, 1, 2, 0, 2, 3};
  MeshId q = s.createMesh(m);
  Material3D mat;
  mat.lit = false;
  mat.texture = t;
  s.begin(frame());
  s.draw(q, Mat4(), mat);
  s.end();
  CHECK(near(at(s, 20, 32), 0, 0, 0));  // hard pixels: a jump at the seam
  CHECK(near(at(s, 44, 32), 255, 255, 255));
  mat.smooth = true;
  s.begin(frame());
  s.draw(q, Mat4(), mat);
  s.end();
  Px a = at(s, 24, 32), b = at(s, 32, 32), c = at(s, 40, 32);  // a ramp: dark, grey, light
  CHECK(a.r < b.r && b.r < c.r);
  CHECK(near(b, 128, 128, 128, 12));
  CHECK(near(at(s, 4, 32), 0, 0, 0));  // clamped at the edge: no wrap-around bleed
}

TEST(soft3d_draws_at_the_requested_picture_size_and_caps_it) {
  SoftRenderer3D s;
  MeshId q = s.createMesh(quad(0.5f, 0));
  Frame3D f = frame();
  f.renderWidth = f.renderHeight = 256;  // 4 times the logical 64: same picture, more pixels
  s.begin(f);
  s.draw(q, Mat4(), Material3D());
  s.end();
  int w = 0, h = 0;
  CHECK(s.framePixels(w, h) != nullptr);
  CHECK_EQ(w, 256);
  CHECK_EQ(h, 256);
  CHECK(near(at(s, 128, 128), 255, 255, 255));  // the square is still in the middle, half as wide as the picture
  CHECK(near(at(s, 20, 20), 0, 0, 128));
  f.renderWidth = 8192;  // longer side capped at 4096, aspect kept
  f.renderHeight = 4096;
  s.begin(f);
  s.end();
  s.framePixels(w, h);
  CHECK_EQ(w, 4096);
  CHECK_EQ(h, 2048);
  CHECK_EQ(s.caps().maxTextureSize, 2048);
}
