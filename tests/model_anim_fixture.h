// Shared by test_model_anim.cpp and test_model_anim_damage.cpp: the bend.glb fixture and hand-calculation helpers.
#pragma once
#include <cmath>
#include <cstdio>
#include <cstring>

#include "gfx3d/model.h"
#include "gfx3d/soft3d.h"
#include "test.h"

using namespace ote;

namespace anim_fixture {

inline const float kPi = 3.14159265358979f;
inline float rad(float deg) { return deg * kPi / 180.0f; }
inline bool near(float a, float b, float tol = 1e-4f) { return std::fabs(a - b) <= tol; }

inline Vec3 toModel(float x, float y, float z) { return Vec3(1 + z, y, 2 - x); }
inline Vec3 toLocal(const Vec3& m) { return Vec3(2 - m.z, m.y, m.x - 1); }
inline Vec3 rotZ(const Vec3& p, float a) { return Vec3(p.x * std::cos(a) - p.y * std::sin(a), p.x * std::sin(a) + p.y * std::cos(a), p.z); }

inline std::string path() { return ote_test::sourceDir() + "/tests/fixtures/models/bend.glb"; }

inline std::vector<uint8_t> readFixture() {
  std::vector<uint8_t> bytes;
  FILE* f = std::fopen(path().c_str(), "rb");
  if (!f) return bytes;
  uint8_t buf[4096];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) bytes.insert(bytes.end(), buf, buf + n);
  std::fclose(f);
  return bytes;
}

inline bool loadBend(Model3D& m, const Caps3D& caps = Caps3D()) {
  std::string err;
  return loadModel(path(), caps, nullptr, m, err);
}

inline bool loadBytes(const std::vector<uint8_t>& b, Model3D& m, std::string& err) {
  return loadModelBytes(b.data(), b.size(), ote_test::sourceDir() + "/tests/fixtures/models", Caps3D(), nullptr, m, err);
}

inline int nodeNamed(const Model3D& m, const char* name) {
  for (size_t i = 0; i < m.nodes.size(); ++i)
    if (m.nodes[i].name == name) return (int)i;
  return -1;
}

// Replaces the first occurrence of `from` in the file's JSON by `to` (same length, so the file stays valid).
inline bool patch(std::vector<uint8_t>& b, const char* from, const char* to) {
  size_t n = std::strlen(from);
  for (size_t i = 0; i + n <= b.size(); ++i)
    if (std::memcmp(&b[i], from, n) == 0) {
      std::memcpy(&b[i], to, n);
      return true;
    }
  return false;
}

// World position of a joint at `time` of clip `name`.
struct Posed {
  Model3D m;
  ModelPose pose;
  int root, middle, tip, column;
  bool ok;
  explicit Posed() {
    ok = loadBend(m);
    root = nodeNamed(m, "root");
    middle = nodeNamed(m, "middle");
    tip = nodeNamed(m, "tip");
    column = nodeNamed(m, "column");
  }
  void at(const char* clip, float t, bool loop = false) { poseModel(m, m.findClip(clip), t, loop, pose); }
  Vec3 tipPos() const { return Vec3(pose.world[(size_t)tip].m[12], pose.world[(size_t)tip].m[13], pose.world[(size_t)tip].m[14]); }
  // World x axis of a joint, expected for a joint turned `a` about base-local Z: base-local (cos, sin, 0) -> (0, sin, -cos).
  bool xAxisIs(int node, float a) const {
    const Mat4& w = pose.world[(size_t)node];
    return near(w.m[0], 0, 1e-4f) && near(w.m[1], std::sin(a), 1e-4f) && near(w.m[2], -std::cos(a), 1e-4f);
  }
};

// Tip position for the "Bend" clip and the root lifted by h, joints turned a1 (middle) and a2 (tip, unused here).
inline Vec3 expectedTip(float a1, float h = 0) {
  return toModel(-std::sin(a1), h + 1 + std::cos(a1), 0);
}

}  // namespace anim_fixture
using namespace anim_fixture;
