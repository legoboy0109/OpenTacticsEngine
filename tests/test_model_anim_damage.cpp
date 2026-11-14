// Damaged bend.glb files must never crash the loader, posing or skinning, and the bent model must draw differently
// (see test_model_anim.cpp for the model).
#include "model_anim_fixture.h"

namespace {

// Poses and skins everything a loaded model has, with times and indices that are in range and not.
void exercise(const Model3D& m) {
  ModelPose pose;
  MeshData out;
  const float times[] = {-1.0f, 0.0f, 0.3f, 1.0f, 10.0f, 1e9f};
  for (int c = -2; c <= (int)m.clips.size() + 1; ++c)
    for (float t : times)
      for (int loop = 0; loop < 2; ++loop) {
        poseModel(m, c, t, loop != 0, pose);
        for (size_t n = 0; n < m.nodes.size(); ++n)
          for (int part : m.nodes[n].parts) skinPart(m, pose, (int)n, part, out);
        skinPart(m, pose, -1, 0, out);
        skinPart(m, pose, 1000, 1000, out);
      }
}

struct Lcg {
  uint32_t s;
  uint32_t next() { return s = s * 1664525u + 1013904223u; }
};

}  // namespace

TEST(anim_damaged_files_never_crash) {
  std::vector<uint8_t> good = readFixture();
  CHECK(good.size() > 1000);
  Model3D m;
  std::string err;
  int loaded = 0;
  // Cut off at every length.
  for (size_t len = 0; len < good.size(); len += (len < 600 ? 1 : 7)) {
    std::vector<uint8_t> cut(good.begin(), good.begin() + (std::ptrdiff_t)len);
    if (loadBytes(cut, m, err)) {
      ++loaded;
      exercise(m);
    }
  }
  // Random bytes changed.
  Lcg rng{12345};
  int refused = 0;
  for (int round = 0; round < 400; ++round) {
    std::vector<uint8_t> b = good;
    int hits = 1 + (int)(rng.next() % 8);
    for (int h = 0; h < hits; ++h) {
      // Half the hits land in the first 900 bytes (the JSON), the rest in the data.
      size_t at = (rng.next() & 1) ? rng.next() % 900 : rng.next() % b.size();
      b[at] = (uint8_t)rng.next();
    }
    if (loadBytes(b, m, err)) {
      ++loaded;
      exercise(m);
    } else {
      ++refused;
    }
  }
  CHECK(loaded > 0 && refused > 0);
}

TEST(anim_bent_model_draws_differently) {
  Posed p;
  CHECK(p.ok);
  SoftRenderer3D r;
  MeshId mesh = r.createMesh(p.m.parts[0].mesh);
  CHECK(mesh != 0);
  Frame3D f;
  f.width = f.height = 64;
  f.clear = Rgba3D(0, 0, 0.5f, 1);
  f.view = Mat4::lookAt(Vec3(1, 1.5f, 8), Vec3(1, 1.5f, 2), Vec3(0, 1, 0));
  f.proj = Mat4::ortho(2.2f, 1.0f, 0.5f, 30.0f);
  auto render = [&](const MeshData& data) {
    r.updateMesh(mesh, data);
    r.begin(f);
    r.draw(mesh, Mat4(), Material3D());  // skinned vertices are in model space: no node matrix
    r.end();
    int w, h;
    const uint8_t* px = r.framePixels(w, h);
    return std::vector<uint8_t>(px, px + (size_t)w * h * 4);
  };
  std::vector<uint8_t> rest = render(p.m.parts[0].mesh);
  CHECK(r.trianglesDrawn() > 10);
  p.at("Bend", 1);
  MeshData bent;
  skinPart(p.m, p.pose, p.column, 0, bent);
  std::vector<uint8_t> posed = render(bent);
  CHECK(r.trianglesDrawn() > 10);
  size_t different = 0;
  for (size_t i = 0; i + 3 < rest.size(); i += 4)
    if (rest[i] != posed[i] || rest[i + 1] != posed[i + 1] || rest[i + 2] != posed[i + 2]) ++different;
  CHECK(different > 50);
}
