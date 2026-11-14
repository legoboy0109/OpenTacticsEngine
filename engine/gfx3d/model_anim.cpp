// glTF loader: skins and animations (see model.h ModelSkin / ModelClip).
#include <cmath>
#include <cstdio>

#include "core/base/log.h"
#include "gfx3d/model_internal.h"

namespace ote {
namespace gltf {

bool buildSkins(const Context& c, const std::vector<int>& nodeMap, const std::vector<size_t>& used, Model3D& out,
                std::string& err) {
  char msg[160];
  for (size_t si : used) {
    const Json& j = c.json["skins"][si];
    const Json& joints = j["joints"];
    ModelSkin skin;
    for (size_t k = 0; k < joints.size(); ++k) {
      size_t node;
      if (!asIndex(joints[k], node) || node >= nodeMap.size()) {
        std::snprintf(msg, sizeof msg, "its skin %zu names a joint that does not exist", si);
        err = msg;
        return false;
      }
      if (nodeMap[node] < 0) {
        std::snprintf(msg, sizeof msg, "its skin %zu uses a joint (node %zu) that is not part of the scene", si, node);
        err = msg;
        return false;
      }
      skin.joints.push_back(nodeMap[node]);
    }
    skin.inverseBind.assign(skin.joints.size(), Mat4());  // absent matrices = identity
    if (j.has("inverseBindMatrices")) {
      size_t idx;
      Accessor a;
      if (!asIndex(j["inverseBindMatrices"], idx)) {
        err = "a skin has a damaged inverseBindMatrices entry";
        return false;
      }
      if (!openAccessor(c, idx, a, err)) return false;
      if (a.n != 16 || a.comp != kFloat || a.count < skin.joints.size()) {
        std::snprintf(msg, sizeof msg, "its skin %zu has inverse bind matrices of the wrong kind or too few", si);
        err = msg;
        return false;
      }
      for (size_t k = 0; k < skin.joints.size(); ++k)
        for (int e = 0; e < 16; ++e) skin.inverseBind[k].m[e] = readComp(a, k, e);  // glTF is column-major too
    }
    out.skins.push_back(std::move(skin));
  }
  return true;
}

namespace {

// One sampler -> a track body (times / values / step). false: skipped, `why` says why.
bool readSampler(const Context& c, const Json& s, ModelTrack::Path path, ModelTrack& t, std::string& why) {
  size_t inIdx, outIdx;
  if (!asIndex(s["input"], inIdx) || !asIndex(s["output"], outIdx)) {
    why = "a sampler has no input or output";
    return false;
  }
  Accessor in, o;
  if (!openAccessor(c, inIdx, in, why) || !openAccessor(c, outIdx, o, why)) return false;
  const bool rot = path == ModelTrack::Rotation;
  const int width = rot ? 4 : 3;
  if (in.n != 1 || in.comp != kFloat || o.n != width || o.comp == kUInt || (rot && (o.comp == kUByte || o.comp == kUShort))) {
    why = "a sampler's data is of the wrong kind";
    return false;
  }
  if (rot && o.comp != kFloat) o.norm = true;  // signed whole-number rotations are always normalised
  const std::string mode = s["interpolation"].isString() ? s["interpolation"].asString() : std::string("LINEAR");
  if (mode != "LINEAR" && mode != "STEP" && mode != "CUBICSPLINE") {
    why = "a sampler has an unknown interpolation";
    return false;
  }
  const bool cubic = mode == "CUBICSPLINE";
  if (in.count == 0) {
    why = "a sampler has no keys";
    return false;
  }
  if (o.count != in.count * (cubic ? 3 : 1)) {
    why = "a sampler's input and output counts do not match";
    return false;
  }
  t.step = mode == "STEP";
  t.times.resize(in.count);
  t.values.resize(in.count * (size_t)width);
  for (size_t i = 0; i < in.count; ++i) {
    float time = readComp(in, i, 0);
    if (!std::isfinite(time) || (i > 0 && !(time > t.times[i - 1]))) {
      why = "a sampler's key times are not rising";
      return false;
    }
    t.times[i] = time;
    size_t src = cubic ? i * 3 + 1 : i;  // cubic: (in-tangent, value, out-tangent); only the value is kept
    for (int k = 0; k < width; ++k) {
      float v = readComp(o, src, k);
      if (!std::isfinite(v)) {
        why = "a sampler has values that are not numbers";
        return false;
      }
      t.values[i * (size_t)width + (size_t)k] = v;
    }
  }
  return true;
}

}  // namespace

void buildClips(const Context& c, const std::vector<int>& nodeMap, Model3D& out) {
  const Json& anims = c.json["animations"];
  for (size_t ai = 0; ai < anims.size(); ++ai) {
    const Json& a = anims[ai];
    ModelClip clip;
    if (a["name"].isString() && !a["name"].asString().empty()) {
      clip.name = a["name"].asString();
    } else {
      char nm[32];
      std::snprintf(nm, sizeof nm, "Animation %zu", ai + 1);
      clip.name = nm;
    }
    const Json& chans = a["channels"];
    for (size_t ci = 0; ci < chans.size(); ++ci) {
      const Json& target = chans[ci]["target"];
      size_t node, sampler;
      if (!asIndex(target["node"], node) || node >= nodeMap.size() || nodeMap[node] < 0) continue;  // not in the scene
      const std::string path = target["path"].asString();
      ModelTrack t;
      if (path == "translation") {
        t.path = ModelTrack::Translation;
      } else if (path == "rotation") {
        t.path = ModelTrack::Rotation;
      } else if (path == "scale") {
        t.path = ModelTrack::Scale;
      } else {
        if (path == "weights") OTE_WARN("model: morph target animation is ignored");
        continue;
      }
      if (!asIndex(chans[ci]["sampler"], sampler) || sampler >= a["samplers"].size()) {
        OTE_WARN("model: animation %s: a channel uses a sampler that does not exist; skipped", clip.name.c_str());
        continue;
      }
      std::string why;
      if (!readSampler(c, a["samplers"][sampler], t.path, t, why)) {
        OTE_WARN("model: animation %s: %s; the channel is skipped", clip.name.c_str(), why.c_str());
        continue;
      }
      t.node = nodeMap[node];
      clip.duration = std::fmax(clip.duration, t.times.back());
      clip.tracks.push_back(std::move(t));
    }
    if (clip.tracks.empty()) {
      OTE_WARN("model: animation %s has nothing usable and is dropped", clip.name.c_str());
      continue;
    }
    out.clips.push_back(std::move(clip));
  }
}

}  // namespace gltf
}  // namespace ote
