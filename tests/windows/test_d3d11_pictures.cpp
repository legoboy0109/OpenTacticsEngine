// The Direct3D 11 backend draws what the software backend (the reference) draws, feature by feature, on a device of
// its own (on a machine without a graphics chip that is WARP, Microsoft's complete software rasteriser): at the best
// feature level the machine has and capped at 10_1 (what an Xbox One gives UWP apps). The numbers are printed for
// docs/RENDER3D.md. Windows only.
#include <functional>

#include "d3d11_test_util.h"
#include "gfx3d/soft3d.h"
#include "test.h"

using namespace d3d11test;

namespace {

const float kPi = 3.14159265f;

// A small texture: `cells` x `cells` squares of two colours, `size` texels a side; alpha 0 in the second colour when
// `holes`.
std::vector<uint8_t> checker(int size, int cells, bool holes, uint8_t r1 = 230, uint8_t g1 = 200, uint8_t b1 = 40,
                             uint8_t r2 = 40, uint8_t g2 = 90, uint8_t b2 = 220) {
  std::vector<uint8_t> px((size_t)size * size * 4);
  const int cell = size / cells;
  for (int y = 0; y < size; ++y)
    for (int x = 0; x < size; ++x) {
      bool first = ((x / cell) + (y / cell)) % 2 == 0;
      uint8_t* p = &px[((size_t)y * size + x) * 4];
      p[0] = first ? r1 : r2;
      p[1] = first ? g1 : g2;
      p[2] = first ? b1 : b2;
      p[3] = (first || !holes) ? 255 : 0;
    }
  return px;
}

typedef std::function<void(Renderer3D&)> Scene;

void litBlocks(Renderer3D& r) {
  MeshId m = r.createMesh(quad(0.35f, 0, 220, 120, 60));
  Frame3D f = frame();
  r.begin(f);
  for (int i = 0; i < 3; ++i) {
    Mat4 at = Mat4::translate(Vec3(-0.6f + 0.6f * i, 0.3f * (i - 1), 0)) *
              Mat4::rotate(Quat::axisAngle(Vec3(0.3f, 1, 0.2f), -0.7f + 0.7f * i));
    r.draw(m, at, Material3D());
  }
  r.end();
}

void cutout(Renderer3D& r) {
  std::vector<uint8_t> t = checker(8, 4, true);
  Texture3DId tex = r.createTexture(t.data(), 8, 8, false);
  MeshId back = r.createMesh(quad(0.9f, -1, 90, 160, 90));
  MeshId front = r.createMesh(quad(0.6f, 0));
  Material3D m;
  m.texture = tex;
  m.blend = Blend3D::Cutout;
  m.lit = false;
  r.begin(frame());
  r.draw(back, Mat4(), Material3D());
  r.draw(front, Mat4::rotate(Quat::axisAngle(Vec3(0, 0, 1), 0.3f)), m);
  r.end();
}

// The see-through squares below are turned a little: the software backend's triangles include their edges, so a pixel
// whose centre lies exactly on the diagonal two triangles share is blended twice there (once by each); Direct3D and
// OpenGL fill such a pixel once. Untouched, a square whose corners sit on pixel corners shows that as a line of 32
// pixels; that is the reference's quirk, not something to copy.
const Mat4 kNudge = Mat4::rotate(Quat::axisAngle(Vec3(0, 0, 1), 0.05f));

// Three see-through squares handed over near-first: they must still be blended far to near.
void blendOrder(Renderer3D& r) {
  MeshId red = r.createMesh(quad(0.5f, 0, 255, 40, 40, 140));
  MeshId green = r.createMesh(quad(0.5f, 0, 40, 255, 40, 140));
  MeshId blue = r.createMesh(quad(0.5f, 0, 40, 40, 255, 140));
  MeshId solid = r.createMesh(quad(0.3f, -0.5f, 250, 250, 250));
  Material3D m;
  m.blend = Blend3D::Blend;
  m.lit = false;
  Material3D plain;
  plain.lit = false;
  r.begin(frame(64, 64, true));
  r.draw(red, Mat4::translate(Vec3(-0.3f, 0.2f, 1.0f)) * kNudge, m);
  r.draw(green, kNudge, m);
  r.draw(blue, Mat4::translate(Vec3(0.3f, -0.2f, -1.0f)) * kNudge, m);
  r.draw(solid, Mat4(), plain);  // solid, after the blended ones: drawn first anyway, and hides what lies behind it
  r.end();
}

void fog(Renderer3D& r) {
  MeshId m = r.createMesh(quad(0.4f, 0, 200, 200, 200));
  Frame3D f = frame(64, 64, true);
  f.fog = true;
  f.fogColor = Rgba3D(0.6f, 0.7f, 0.8f);
  f.fogStart = 4.5f;
  f.fogEnd = 9.0f;
  r.begin(f);
  for (int i = 0; i < 4; ++i) r.draw(m, Mat4::translate(Vec3(-0.9f + 0.6f * i, 0, -1.5f * i)), Material3D());
  r.end();
}

// Both squares are turned away from the camera; only the two-sided one is drawn.
void twoSided(Renderer3D& r) {
  MeshId m = r.createMesh(quad(0.35f, 0, 255, 160, 60));
  Material3D both;
  both.twoSided = true;
  both.lit = false;
  Material3D one = both;
  one.twoSided = false;
  Mat4 turned = Mat4::rotate(Quat::axisAngle(Vec3(0, 1, 0), kPi * 0.9f));
  r.begin(frame());
  r.draw(m, Mat4::translate(Vec3(-0.45f, 0, 0)) * turned, both);
  r.draw(m, Mat4::translate(Vec3(0.45f, 0, 0)) * turned, one);
  r.end();
}

// A wall, and a marker behind it shown through it (depth test off).
void depthOff(Renderer3D& r) {
  MeshId wall = r.createMesh(quad(0.8f, 1, 120, 120, 120));
  MeshId marker = r.createMesh(quad(0.3f, -1, 255, 255, 0));
  Material3D m;
  m.depthTest = false;
  m.lit = false;
  r.begin(frame());
  r.draw(wall, Mat4(), Material3D());
  r.draw(marker, Mat4::translate(Vec3(0.2f, 0.1f, 0)), m);
  r.end();
}

// Magnified textures: smooth (bilinear, repeating twice) beside hard pixels (clamped).
void sampling(Renderer3D& r) {
  std::vector<uint8_t> t = checker(4, 2, false);
  t[0] = 255;  // a texel of its own colour in a corner, to see which way up the texture lies
  t[1] = t[2] = 0;
  Texture3DId rep = r.createTexture(t.data(), 4, 4, true);
  Texture3DId clamp = r.createTexture(t.data(), 4, 4, false);
  MeshId twice = r.createMesh(quad(0.45f, 0, 255, 255, 255, 255, 2));
  MeshId once = r.createMesh(quad(0.45f, 0));
  Material3D smooth;
  smooth.texture = rep;
  smooth.smooth = true;
  smooth.lit = false;
  Material3D hard;
  hard.texture = clamp;
  hard.lit = false;
  r.begin(frame());
  r.draw(twice, Mat4::translate(Vec3(-0.5f, 0, 0)), smooth);
  r.draw(once, Mat4::translate(Vec3(0.5f, 0, 0)), hard);
  r.end();
}

// A posed character: the mesh is replaced between frames (and once between two draws of the same frame).
void updates(Renderer3D& r) {
  MeshId m = r.createMesh(quad(0.3f, 0, 255, 0, 0));
  MeshId other = r.createMesh(quad(0.2f, 0.5f, 0, 0, 255));
  Material3D plain;
  plain.lit = false;
  r.begin(frame());
  r.draw(m, Mat4(), plain);
  r.end();
  r.updateMesh(m, quad(0.5f, 0, 0, 255, 0));
  r.begin(frame());
  r.draw(m, Mat4::translate(Vec3(-0.3f, 0, 0)), plain);  // drawn with the green square...
  r.draw(other, Mat4(), plain);
  r.updateMesh(m, quad(0.15f, 0.8f, 255, 255, 255));    // ...then changed: the draw above keeps the old shape
  r.draw(m, Mat4::translate(Vec3(0.5f, 0.5f, 0)), plain);
  r.end();
}

// The picture changes size between frames (a window resized): the last one counts.
void resize(Renderer3D& r) {
  MeshId m = r.createMesh(quad(0.5f, 0, 60, 200, 255));
  r.begin(frame(64, 64));
  r.draw(m, Mat4(), Material3D());
  r.end();
  r.begin(frame(96, 40));
  r.draw(m, Mat4::rotate(Quat::axisAngle(Vec3(0, 0, 1), 0.4f)), Material3D());
  r.end();
}

// Cleared to alpha 0 (the 2D drawing under the picture shows), a see-through square and a solid one: the picture's
// alpha must be the software backend's too.
void clearAlpha(Renderer3D& r) {
  MeshId glass = r.createMesh(quad(0.5f, 0, 255, 128, 0, 100));
  MeshId solid = r.createMesh(quad(0.25f, -0.5f, 0, 128, 255));
  Material3D see;
  see.blend = Blend3D::Blend;
  see.lit = false;
  Frame3D f = frame();
  f.clear = Rgba3D(0, 0, 0, 0);
  r.begin(f);
  r.draw(solid, Mat4::translate(Vec3(0.4f, 0.4f, 0)), Material3D());
  r.draw(glass, kNudge, see);
  r.end();
}

struct Case {
  const char* name;
  Scene scene;
  bool alpha;  // compare the alpha channel too
};

const Case kCases[] = {
    {"lit, turned squares", litBlocks, false}, {"cut-out texture", cutout, false},
    {"blended, sorted far to near", blendOrder, false}, {"fog", fog, false},
    {"two-sided and culled", twoSided, false}, {"depth test off", depthOff, false},
    {"smooth repeat / hard clamp", sampling, false}, {"updateMesh", updates, false},
    {"resize 64x64 -> 96x40", resize, false}, {"clear alpha 0 (RGBA)", clearAlpha, true},
};

void compareAll(const char* level) {
  std::unique_ptr<Renderer3D> probe = makeD3D11(level);
  if (!probe) {
    std::printf("  SKIP d3d11 pictures at level %s: no device\n", *level ? level : "best");
    return;
  }
  for (const Case& c : kCases) {
    SoftRenderer3D soft;
    std::unique_ptr<Renderer3D> d3d = makeD3D11(level);
    c.scene(soft);
    c.scene(*d3d);
    Diff d = compare(grab(soft), grab(*d3d), c.alpha);
    report(c.name, level, d);
    CHECK(d.sameSize);
    CHECK(d.over <= 8);  // the player tests' 0.2 % of 64 x 64: a few edge pixels
  }
}

}  // namespace

TEST(d3d11_draws_what_the_software_backend_draws) { compareAll(""); }

TEST(d3d11_at_feature_level_10_1_draws_the_same) {
  compareAll("10_1");
  // And the same picture as at the best level, exactly.
  for (const Case& c : kCases) {
    std::unique_ptr<Renderer3D> best = makeD3D11(""), low = makeD3D11("10_1");
    if (!best || !low) return;
    c.scene(*best);
    c.scene(*low);
    Diff d = compare(grab(*best), grab(*low), true);
    if (d.max != 0) std::printf("  d3d11 best vs 10_1 %-30s: max %d, %d pixels > 8\n", c.name, d.max, d.over);
    CHECK(d.sameSize && d.max == 0);
  }
}

// Smooth materials read the smaller copies of a mipmapped texture: a fine checker shrunk eight times becomes an even
// grey instead of shimmering black and white; hard pixels never do.
TEST(d3d11_mipmaps_for_smooth_materials_only) {
  std::unique_ptr<Renderer3D> r = makeD3D11("");
  if (!r) return;
  std::vector<uint8_t> t = checker(128, 128, false, 0, 0, 0, 255, 255, 255);
  Texture3DId mip = r->createTexture(t.data(), 128, 128, false, true);
  MeshId m = r->createMesh(quad(0.25f, 0));  // 16 x 16 pixels on screen for 128 x 128 texels
  Material3D smooth;
  smooth.texture = mip;
  smooth.smooth = true;
  smooth.lit = false;
  Material3D hard = smooth;
  hard.smooth = false;
  r->begin(frame());
  r->draw(m, Mat4::translate(Vec3(-0.5f, 0, 0)), smooth);
  r->draw(m, Mat4::translate(Vec3(0.5f, 0, 0)), hard);
  r->end();
  Picture p = grab(*r);
  int greyOff = 0, hardMid = 0;
  for (int y = 28; y < 36; ++y)
    for (int x = 0; x < 64; ++x) {
      int v = p.at(x, y)[0];
      if (x >= 12 && x < 20 && (v < 110 || v > 145)) ++greyOff;  // middle of the smooth square
      if (x >= 44 && x < 52 && v > 10 && v < 245) ++hardMid;      // middle of the hard one
    }
  std::printf("  d3d11 mipmaps: smooth square %d pixels off grey, hard square %d pixels neither black nor white\n",
              greyOff, hardMid);
  CHECK_EQ(greyOff, 0);
  CHECK_EQ(hardMid, 0);
}

// 4x multisampling at twice the logical size (what the player asks at the display's resolution): the inside of shapes
// is the software picture's, their edges get in-between colours.
TEST(d3d11_multisampling_smooths_edges_only) {
  for (const char* level : {"", "10_1"}) {
    std::unique_ptr<Renderer3D> d3d = makeD3D11(level);
    if (!d3d) return;
    SoftRenderer3D soft;
    MeshId ms = soft.createMesh(quad(0.5f, 0, 255, 255, 255)), md = d3d->createMesh(quad(0.5f, 0, 255, 255, 255));
    Frame3D f = frame();
    f.renderWidth = f.renderHeight = 128;
    f.samples = 4;
    f.clear = Rgba3D(0, 0, 0, 1);
    Material3D plain;
    plain.lit = false;
    const Mat4 turn = Mat4::rotate(Quat::axisAngle(Vec3(0, 0, 1), 0.3f));
    soft.begin(f);
    soft.draw(ms, turn, plain);
    soft.end();
    d3d->begin(f);
    d3d->draw(md, turn, plain);
    d3d->end();
    Picture a = grab(soft), b = grab(*d3d);
    Diff d = compare(a, b);
    int between = 0;
    for (int i = 0; i < b.w * b.h; ++i)
      if (b.px[(size_t)i * 4] > 20 && b.px[(size_t)i * 4] < 235) ++between;
    report("4x multisampling, 128 x 128", level, d);
    std::printf("  d3d11 multisampling (level %s): %d edge pixels in between black and white\n", *level ? level : "best",
                between);
    CHECK(d.sameSize);
    CHECK(between > 100);              // the edges of a turned square, about 4 x 128 pixels long, are smoothed...
    CHECK(d.over <= between + 8);      // ...and that is all that differs
  }
}
