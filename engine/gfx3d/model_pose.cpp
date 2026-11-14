// Posing and skinning of loaded models (model.h): sampling a clip, world matrices, CPU skinning.
#include <algorithm>
#include <cmath>

#include "gfx3d/model.h"

namespace ote {

namespace {

// Where `t` falls among a track's keys: key index `a`, and the blend `f` (0..1) towards key a + 1.
// n = usable keys (>= 1).
void locate(const std::vector<float>& times, size_t n, float t, size_t& a, float& f) {
  f = 0;
  if (t <= times[0]) {
    a = 0;
    return;
  }
  if (t >= times[n - 1]) {
    a = n - 1;
    return;
  }
  // First key later than t, minus one.
  a = (size_t)(std::upper_bound(times.begin(), times.begin() + (std::ptrdiff_t)n, t) - times.begin()) - 1;
  float span = times[a + 1] - times[a];
  f = span > 0 ? (t - times[a]) / span : 0;
}

float timeInClip(const ModelClip& clip, float time, bool loop) {
  if (!(time == time)) return 0;  // NaN
  if (loop) {
    if (!(clip.duration > 0)) return 0;
    float t = std::fmod(time, clip.duration);
    return t < 0 ? t + clip.duration : t;
  }
  return time;
}

// Scratch for pass 1 of poseModel: a node's local translation, rotation and scale, kept in the node's own world
// matrix slot (16 floats) until pass 2 turns them into the real matrix. Saves a per-call allocation.
struct LocalTRS {
  static void setT(Mat4& m, Vec3 v) { m.m[0] = v.x; m.m[1] = v.y; m.m[2] = v.z; }
  static void setR(Mat4& m, Quat q) { m.m[4] = q.x; m.m[5] = q.y; m.m[6] = q.z; m.m[7] = q.w; }
  static void setS(Mat4& m, Vec3 v) { m.m[8] = v.x; m.m[9] = v.y; m.m[10] = v.z; }
  static Vec3 t(const Mat4& m) { return Vec3(m.m[0], m.m[1], m.m[2]); }
  static Quat r(const Mat4& m) { return Quat(m.m[4], m.m[5], m.m[6], m.m[7]); }
  static Vec3 s(const Mat4& m) { return Vec3(m.m[8], m.m[9], m.m[10]); }
};

}  // namespace

void poseModel(const Model3D& model, int clip, float time, bool loop, ModelPose& out) {
  const size_t n = model.nodes.size();
  out.world.resize(n);
  for (size_t i = 0; i < n; ++i) {
    const ModelNode& nd = model.nodes[i];
    LocalTRS::setT(out.world[i], nd.translation);
    LocalTRS::setR(out.world[i], nd.rotation);
    LocalTRS::setS(out.world[i], nd.scale);
  }
  if (clip >= 0 && (size_t)clip < model.clips.size()) {
    const ModelClip& c = model.clips[(size_t)clip];
    const float t = timeInClip(c, time, loop);
    for (const ModelTrack& tr : c.tracks) {
      if (tr.node < 0 || (size_t)tr.node >= n || tr.times.empty()) continue;
      const size_t width = tr.path == ModelTrack::Rotation ? 4 : 3;
      const size_t keys = std::min(tr.times.size(), tr.values.size() / width);
      if (keys == 0) continue;
      size_t a;
      float f;
      locate(tr.times, keys, t, a, f);
      const float* v0 = &tr.values[a * width];
      const float* v1 = (!tr.step && f > 0 && a + 1 < keys) ? &tr.values[(a + 1) * width] : v0;
      Mat4& slot = out.world[(size_t)tr.node];
      if (tr.path == ModelTrack::Rotation) {
        Quat q0(v0[0], v0[1], v0[2], v0[3]), q1(v1[0], v1[1], v1[2], v1[3]);
        LocalTRS::setR(slot, v1 == v0 ? normalize(q0) : nlerp(q0, q1, f));
      } else {
        Vec3 v(v0[0] + (v1[0] - v0[0]) * f, v0[1] + (v1[1] - v0[1]) * f, v0[2] + (v1[2] - v0[2]) * f);
        if (tr.path == ModelTrack::Translation) LocalTRS::setT(slot, v);
        else LocalTRS::setS(slot, v);
      }
    }
  }
  // Pass 2: parents come before children, so a parent's slot already holds its final world matrix.
  for (size_t i = 0; i < n; ++i) {
    Mat4& slot = out.world[i];
    Mat4 local = Mat4::trs(LocalTRS::t(slot), LocalTRS::r(slot), LocalTRS::s(slot));
    const int p = model.nodes[i].parent;
    slot = (p >= 0 && (size_t)p < i) ? out.world[(size_t)p] * local : local;
  }
}

void skinPart(const Model3D& model, const ModelPose& pose, int node, int part, MeshData& out) {
  if (part < 0 || (size_t)part >= model.parts.size()) {
    out = MeshData();
    return;
  }
  const ModelPart& pt = model.parts[(size_t)part];
  out.indices = pt.mesh.indices;
  out.vertices = pt.mesh.vertices;  // everything else about a vertex stays as it is
  const size_t vn = pt.mesh.vertices.size();
  if (!pt.skinned() || pt.joints.size() != vn * 4 || pt.weights.size() != vn * 4) return;
  if (node < 0 || (size_t)node >= model.nodes.size() || pose.world.size() != model.nodes.size()) return;
  const int si = model.nodes[(size_t)node].skin;
  if (si < 0 || (size_t)si >= model.skins.size()) return;
  const ModelSkin& skin = model.skins[(size_t)si];
  const size_t jn = skin.joints.size();

  Mat4 stackJoints[48];
  std::vector<Mat4> heapJoints;
  Mat4* jm = stackJoints;
  if (jn > 48) {
    heapJoints.resize(jn);
    jm = heapJoints.data();
  }
  for (size_t j = 0; j < jn; ++j) {
    const int nd = skin.joints[j];
    const Mat4 world = (nd >= 0 && (size_t)nd < pose.world.size()) ? pose.world[(size_t)nd] : Mat4();
    jm[j] = j < skin.inverseBind.size() ? world * skin.inverseBind[j] : world;
  }

  for (size_t v = 0; v < vn; ++v) {
    const Vertex3D& src = pt.mesh.vertices[v];
    Vec3 p, nrm;
    float used = 0;
    for (size_t k = 0; k < 4; ++k) {
      const size_t j = pt.joints[v * 4 + k];
      const float w = pt.weights[v * 4 + k];
      if (j >= jn || w == 0) continue;
      p = p + jm[j].point(Vec3(src.x, src.y, src.z)) * w;
      nrm = nrm + jm[j].direction(Vec3(src.nx, src.ny, src.nz)) * w;
      used += w;
    }
    if (used <= 0) continue;  // no usable influence: the vertex stays at rest
    Vertex3D& dst = out.vertices[v];
    dst.x = p.x;
    dst.y = p.y;
    dst.z = p.z;
    Vec3 nn = normalize(nrm);
    if (dot(nn, nn) > 0) {
      dst.nx = nn.x;
      dst.ny = nn.y;
      dst.nz = nn.z;
    }
  }
}

}  // namespace ote
