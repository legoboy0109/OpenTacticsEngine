// ResilientRenderer3D (gfx3d/resilient3d.h): a backend whose device goes away is rebuilt with everything the game made,
// or replaced by the software backend. Checked with a stand-in "hardware" backend (the software one, which can be told
// it lost its device), so this runs everywhere; the real Direct3D 11 and EGL keepers are checked on their platforms.
#include <cstring>

#include "gfx3d/resilient3d.h"
#include "gfx3d/soft3d.h"
#include "test.h"

using namespace ote;

namespace {

struct Device {
  bool lost = false;
  int made = 0;  // backends made on it (the first one and every rebuild)
  bool refuse = false;
};

// The software backend, drawing nothing while its "device" is lost (as a real one does).
class FlakyRenderer3D : public SoftRenderer3D {
 public:
  explicit FlakyRenderer3D(Device& d) : d_(d) {}
  MeshId createMesh(const MeshData& data) override { return d_.lost ? 0 : SoftRenderer3D::createMesh(data); }
  Texture3DId createTexture(const uint8_t* rgba, int w, int h, bool repeat, bool mipmaps) override {
    return d_.lost ? 0 : SoftRenderer3D::createTexture(rgba, w, h, repeat, mipmaps);
  }
  void draw(MeshId mesh, const Mat4& model, const Material3D& material) override {
    if (!d_.lost) SoftRenderer3D::draw(mesh, model, material);
  }

 private:
  Device& d_;
};

class TestKeeper : public DeviceKeeper {
 public:
  explicit TestKeeper(Device& d) : d_(d) {}
  const char* name() const override { return "3D (test)"; }
  const char* lossText() const override { return "the test device was lost"; }
  bool lost() override { return d_.lost; }
  std::unique_ptr<Renderer3D> rebuild(std::string& why) override {
    if (d_.refuse) {
      why = "refused by the test";
      return nullptr;
    }
    d_.lost = false;
    ++d_.made;
    return std::unique_ptr<Renderer3D>(new FlakyRenderer3D(d_));
  }
  void simulateLoss() override { d_.lost = true; }
  void release() override { ++released; }
  int released = 0;

 private:
  Device& d_;
};

MeshData square(uint8_t r, uint8_t g, uint8_t b) {
  MeshData m;
  for (int i = 0; i < 4; ++i) {
    Vertex3D v;
    v.x = (i == 1 || i == 2) ? 0.5f : -0.5f;
    v.y = (i >= 2) ? 0.5f : -0.5f;
    v.nz = 1;
    v.ny = 0;
    v.u = (i == 1 || i == 2) ? 1.0f : 0.0f;
    v.v = (i >= 2) ? 0.0f : 1.0f;
    v.r = r;
    v.g = g;
    v.b = b;
    m.vertices.push_back(v);
  }
  m.indices = {0, 1, 2, 0, 2, 3};
  return m;
}

Frame3D frame32() {
  Frame3D f;
  f.width = f.height = 32;
  f.view = Mat4::lookAt(Vec3(0, 0, 5), Vec3(0, 0, 0), Vec3(0, 1, 0));
  f.proj = Mat4::ortho(1.0f, 1.0f, 0.5f, 50.0f);
  return f;
}

// The middle pixel of the last picture.
uint32_t middle(const Renderer3D& r) {
  int w = 0, h = 0;
  const uint8_t* p = r.framePixels(w, h);
  if (!p) return 0;
  uint32_t v;
  std::memcpy(&v, p + ((size_t)(h / 2) * w + w / 2) * 4, 4);
  return v;
}

void drawOnce(Renderer3D& r, MeshId m, Texture3DId t) {
  Material3D mat;
  mat.lit = false;
  mat.texture = t;
  r.begin(frame32());
  r.draw(m, Mat4(), mat);
  r.end();
}

}  // namespace

TEST(resilient3d_rebuilds_after_a_loss_and_keeps_the_ids) {
  Device dev;
  TestKeeper* keeper = new TestKeeper(dev);
  ResilientRenderer3D r(std::unique_ptr<Renderer3D>(new FlakyRenderer3D(dev)), std::unique_ptr<DeviceKeeper>(keeper), 2);
  const uint8_t texel[4] = {255, 255, 255, 255};
  MeshId m = r.createMesh(square(200, 40, 10));
  Texture3DId t = r.createTexture(texel, 1, 1, false, false);
  CHECK(m != 0 && t != 0);
  drawOnce(r, m, t);
  const uint32_t before = middle(r);
  CHECK(before != 0);
  drawOnce(r, m, t);  // frame 2: the simulated loss, then the rebuild, which repeats the frame
  CHECK_EQ(dev.made, 1);
  CHECK(!r.usingSoftware());
  CHECK_EQ(middle(r), before);
  drawOnce(r, m, t);  // and on: the same ids still draw the same thing
  CHECK_EQ(middle(r), before);

  // A loss noticed when making something: rebuilt, and the new thing is made on the new device.
  dev.lost = true;
  MeshId m2 = r.createMesh(square(10, 200, 10));
  CHECK(m2 != 0 && m2 != m);
  CHECK_EQ(dev.made, 2);
  drawOnce(r, m2, 0);
  CHECK(middle(r) != before);
  drawOnce(r, m, t);
  CHECK_EQ(middle(r), before);
}

TEST(resilient3d_falls_back_to_software_when_no_rebuild_works) {
  Device dev;
  dev.refuse = true;
  TestKeeper* keeper = new TestKeeper(dev);
  ResilientRenderer3D r(std::unique_ptr<Renderer3D>(new FlakyRenderer3D(dev)), std::unique_ptr<DeviceKeeper>(keeper));
  MeshId m = r.createMesh(square(200, 40, 10));
  drawOnce(r, m, 0);
  const uint32_t before = middle(r);
  dev.lost = true;
  drawOnce(r, m, 0);  // the frame is lost, the rebuild refused: the software backend repeats it
  CHECK(r.usingSoftware());
  CHECK_EQ(keeper->released, 1);
  CHECK_EQ(middle(r), before);
  // The refusal option of the platforms (OTE_3D_SIMULATE_NO_REBUILD) does the same without asking the keeper.
  Device dev2;
  ResilientRenderer3D r2(std::unique_ptr<Renderer3D>(new FlakyRenderer3D(dev2)),
                         std::unique_ptr<DeviceKeeper>(new TestKeeper(dev2)), 1, true);
  MeshId m2 = r2.createMesh(square(200, 40, 10));
  drawOnce(r2, m2, 0);
  CHECK(r2.usingSoftware());
  CHECK_EQ(dev2.made, 0);
  CHECK_EQ(middle(r2), before);
}
