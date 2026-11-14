// glTF 2.0 loader (.glb and .gltf). Builds a Model3D for a fixed-function renderer; see model.h.
//
// Supported: nodes of the default scene (TRS or matrix), triangle meshes (POSITION, NORMAL, TEXCOORD_0, COLOR_0,
// any index type, byteStride), materials (base colour, base colour texture, alpha mode, double-sided, unlit,
// sampler wrap/filter), PNG/JPEG images from files, data: URIs or buffer views, KHR_mesh_quantization,
// skins (four joints per vertex) and animations (translation / rotation / scale; see model_anim.cpp).
// Not supported: sparse accessors (refused), other primitive modes (skipped), morph targets (ignored),
// more than four joints per vertex (the extra ones ignored), second texture coordinates, all PBR beyond the above;
// Draco, meshopt and any other extension listed in extensionsRequired are refused.
// Every offset, length and index in the file is checked against the real sizes before it is used.
#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/base/log.h"
#include "gfx3d/model_internal.h"

namespace ote {

namespace {

using gltf::asIndex;
using gltf::Context;

bool failf(std::string& err, const char* fmt, size_t a, size_t b = 0) {
  char msg[160];
  std::snprintf(msg, sizeof msg, fmt, a, b);
  err = msg;
  return false;
}

uint32_t le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

// Splits a .glb into its JSON text and (optional) BIN chunk.
bool splitGlb(const uint8_t* d, size_t size, std::string& json, const uint8_t*& bin, size_t& binSize, std::string& err) {
  bin = nullptr;
  binSize = 0;
  if (size < 20) {
    err = "the file is too short to be a glTF binary";
    return false;
  }
  if (le32(d + 4) != 2) {
    err = "it is not glTF version 2";
    return false;
  }
  if (le32(d + 8) > size) {
    err = "the file is shorter than its header says (it is cut off)";
    return false;
  }
  size_t end = le32(d + 8), pos = 12;
  bool haveJson = false;
  while (pos + 8 <= end) {
    size_t len = le32(d + pos);
    uint32_t type = le32(d + pos + 4);
    pos += 8;
    if (len > end - pos) {
      err = "one of its data chunks is longer than the file";
      return false;
    }
    if (type == 0x4E4F534A && !haveJson) {  // "JSON", must come first
      json.assign((const char*)d + pos, len);
      haveJson = true;
    } else if (!haveJson) {
      err = "its first chunk is not the JSON description";
      return false;
    } else if (type == 0x004E4942 && !bin) {  // "BIN"
      bin = d + pos;
      binSize = len;
    }
    pos += len;
  }
  if (!haveJson) {
    err = "it has no JSON description";
    return false;
  }
  return true;
}

bool supportedExtension(const std::string& e) { return e == "KHR_materials_unlit" || e == "KHR_mesh_quantization"; }

bool loadBuffers(Context& c, const uint8_t* bin, size_t binSize, std::string& err) {
  const Json& bufs = c.json["buffers"];
  c.buffers.resize(bufs.size());
  for (size_t i = 0; i < bufs.size(); ++i) {
    gltf::Buffer& b = c.buffers[i];
    size_t len;
    if (!asIndex(bufs[i]["byteLength"], len)) return failf(err, "its buffer %zu has no valid length", i);
    if (bufs[i]["uri"].isString()) {
      if (!gltf::loadUri(c.baseDir, bufs[i]["uri"].asString(), b.own, err)) return false;
      b.data = b.own.data();
      b.size = b.own.size();
    } else if (i == 0 && bin) {
      b.data = bin;
      b.size = binSize;
    } else {
      return failf(err, "its buffer %zu has no data", i);
    }
    if (b.size < len) return failf(err, "its buffer %zu is shorter than the file says", i);
  }
  return true;
}

// Turns a node's matrix into translation / rotation / scale.
void decompose(const Json& m, ModelNode& n) {
  float a[16];
  for (int i = 0; i < 16; ++i) a[i] = (float)m[i].asNumber(i % 5 == 0 ? 1 : 0);
  n.translation = Vec3(a[12], a[13], a[14]);
  Vec3 cx(a[0], a[1], a[2]), cy(a[4], a[5], a[6]), cz(a[8], a[9], a[10]);
  float sx = length(cx), sy = length(cy), sz = length(cz);
  if (dot(cross(cx, cy), cz) < 0) sx = -sx;  // a mirror: put it on x
  n.scale = Vec3(sx, sy, sz);
  if (std::fabs(sx) < 1e-12f || sy < 1e-12f || sz < 1e-12f) return;  // degenerate: keep identity rotation
  float r[9] = {a[0] / sx, a[1] / sx, a[2] / sx, a[4] / sy, a[5] / sy, a[6] / sy, a[8] / sz, a[9] / sz, a[10] / sz};
  // r is column-major: r[col*3 + row].
  float tr = r[0] + r[4] + r[8], x, y, z, w;
  if (tr > 0) {
    float s = std::sqrt(tr + 1) * 2;
    w = s / 4; x = (r[5] - r[7]) / s; y = (r[6] - r[2]) / s; z = (r[1] - r[3]) / s;
  } else if (r[0] > r[4] && r[0] > r[8]) {
    float s = std::sqrt(1 + r[0] - r[4] - r[8]) * 2;
    w = (r[5] - r[7]) / s; x = s / 4; y = (r[3] + r[1]) / s; z = (r[6] + r[2]) / s;
  } else if (r[4] > r[8]) {
    float s = std::sqrt(1 + r[4] - r[0] - r[8]) * 2;
    w = (r[6] - r[2]) / s; x = (r[3] + r[1]) / s; y = s / 4; z = (r[7] + r[5]) / s;
  } else {
    float s = std::sqrt(1 + r[8] - r[0] - r[4]) * 2;
    w = (r[1] - r[3]) / s; x = (r[6] + r[2]) / s; y = (r[7] + r[5]) / s; z = s / 4;
  }
  n.rotation = normalize(Quat(x, y, z, w));
}

// Converts glTF mesh `mesh` into parts. skinJoints > 0: drawn by a node whose skin has that many joints (the parts then
// carry joints and weights, so they cannot be shared with unskinned nodes).
bool buildMesh(const Context& c, size_t mesh, size_t skinJoints, Model3D& out, std::vector<int>& result, std::string& err) {
  const Json& m = c.json["meshes"][mesh];
  if (!m.isObject()) return failf(err, "it uses mesh %zu, which does not exist", mesh);
  const Json& prims = m["primitives"];
  for (size_t p = 0; p < prims.size(); ++p) {
    MeshData md;
    gltf::SkinWeights sw;
    bool skip;
    if (!gltf::readPrimitive(c, prims[p], md, skip, err, skinJoints, &sw)) return false;
    if (skip) continue;
    int material = -1;
    if (prims[p].has("material")) {
      size_t mi;
      if (!asIndex(prims[p]["material"], mi) || mi >= out.materials.size())
        return failf(err, "a part of its mesh %zu uses a material that does not exist", mesh);
      material = (int)mi;
    }
    std::vector<MeshData> pieces;
    std::vector<gltf::SkinWeights> skinPieces;
    gltf::splitMesh(md, c.caps.maxMeshVertices, pieces, &sw, &skinPieces);
    for (size_t k = 0; k < pieces.size(); ++k) {
      ModelPart part;
      part.mesh.vertices.swap(pieces[k].vertices);
      part.mesh.indices.swap(pieces[k].indices);
      part.joints.swap(skinPieces[k].joints);
      part.weights.swap(skinPieces[k].weights);
      part.material = material;
      result.push_back((int)out.parts.size());
      out.parts.push_back(std::move(part));
    }
  }
  return true;
}

// nodeMap: glTF node index -> index in out.nodes (-1: not in the scene). usedSkins: the glTF skins that kept nodes use;
// ModelNode::skin is the position in this list.
bool buildNodes(const Context& c, Model3D& out, std::vector<int>& nodeMap, std::vector<size_t>& usedSkins, std::string& err) {
  const Json& nodes = c.json["nodes"];
  // Start nodes: the default scene, else scene 0, else every node that is nobody's child.
  std::vector<size_t> roots;
  size_t sceneIdx = 0;
  const Json& scenes = c.json["scenes"];
  if (c.json.has("scene") && !asIndex(c.json["scene"], sceneIdx)) {
    err = "its default scene number is damaged";
    return false;
  }
  if (scenes.size() > 0) {
    if (sceneIdx >= scenes.size()) return failf(err, "its default scene %zu does not exist", sceneIdx);
    const Json& list = scenes[sceneIdx]["nodes"];
    for (size_t i = 0; i < list.size(); ++i) {
      size_t n;
      if (!asIndex(list[i], n) || n >= nodes.size()) return failf(err, "its scene %zu names a node that does not exist", sceneIdx);
      roots.push_back(n);
    }
  } else {
    std::vector<bool> isChild(nodes.size(), false);
    for (size_t i = 0; i < nodes.size(); ++i)
      for (size_t k = 0; k < nodes[i]["children"].size(); ++k) {
        size_t ch;
        if (asIndex(nodes[i]["children"][k], ch) && ch < nodes.size()) isChild[ch] = true;
      }
    for (size_t i = 0; i < nodes.size(); ++i)
      if (!isChild[i]) roots.push_back(i);
  }

  std::vector<bool> visited(nodes.size(), false);
  nodeMap.assign(nodes.size(), -1);
  const size_t meshCount = c.json["meshes"].size(), skinCount = c.json["skins"].size();
  struct Built { size_t mesh; int skin; std::vector<int> parts; };
  std::vector<Built> built;  // parts made so far per (mesh, skin): nodes using the same pair share them
  struct Item { size_t node; int parent; };
  std::vector<Item> stack;  // explicit stack: a damaged file with a very long chain must not overflow the real one
  for (size_t i = roots.size(); i-- > 0;) stack.push_back({roots[i], -1});
  while (!stack.empty()) {
    Item it = stack.back();
    stack.pop_back();
    if (visited[it.node]) return failf(err, "its node %zu is used more than once in the scene", it.node);
    visited[it.node] = true;
    const Json& j = nodes[it.node];
    ModelNode n;
    n.name = j["name"].asString();
    n.parent = it.parent;
    if (j["matrix"].size() == 16) {
      decompose(j["matrix"], n);
    } else {
      const Json &t = j["translation"], &r = j["rotation"], &s = j["scale"];
      if (t.size() == 3) n.translation = Vec3((float)t[0].asNumber(), (float)t[1].asNumber(), (float)t[2].asNumber());
      if (r.size() == 4)
        n.rotation = normalize(Quat((float)r[0].asNumber(), (float)r[1].asNumber(), (float)r[2].asNumber(), (float)r[3].asNumber(1)));
      if (s.size() == 3) n.scale = Vec3((float)s[0].asNumber(1), (float)s[1].asNumber(1), (float)s[2].asNumber(1));
    }
    size_t mesh;
    if (j.has("mesh")) {
      if (!asIndex(j["mesh"], mesh) || mesh >= meshCount) return failf(err, "its node %zu uses a mesh that does not exist", it.node);
      size_t skin = 0;
      int skinKey = -1;  // the glTF skin, or -1
      if (j.has("skin")) {
        if (!asIndex(j["skin"], skin) || skin >= skinCount) return failf(err, "its node %zu uses a skin that does not exist", it.node);
        skinKey = (int)skin;
        size_t at = 0;
        while (at < usedSkins.size() && usedSkins[at] != skin) ++at;
        if (at == usedSkins.size()) usedSkins.push_back(skin);
        n.skin = (int)at;
      }
      size_t b = 0;
      while (b < built.size() && !(built[b].mesh == mesh && built[b].skin == skinKey)) ++b;
      if (b == built.size()) {
        size_t jointCount = skinKey >= 0 ? c.json["skins"][skin]["joints"].size() : 0;
        if (skinKey >= 0 && (jointCount == 0 || jointCount > 65535)) return failf(err, "its skin %zu has no joints or too many", skin);
        built.push_back({mesh, skinKey, {}});
        if (!buildMesh(c, mesh, jointCount, out, built.back().parts, err)) return false;
      }
      n.parts = built[b].parts;
    }
    int self = (int)out.nodes.size();
    nodeMap[it.node] = self;
    out.nodes.push_back(n);
    const Json& kids = j["children"];
    for (size_t k = kids.size(); k-- > 0;) {
      size_t ch;
      if (!asIndex(kids[k], ch) || ch >= nodes.size()) return failf(err, "its node %zu has a child that does not exist", it.node);
      stack.push_back({ch, self});
    }
  }
  return true;
}

void computeBounds(Model3D& m) {
  bool any = false;
  Vec3 lo, hi;
  for (size_t n = 0; n < m.nodes.size(); ++n) {
    if (m.nodes[n].parts.empty()) continue;
    Mat4 mat = m.nodeMatrix((int)n);
    for (int pi : m.nodes[n].parts) {
      // A skinned part's vertices are already in model space (glTF ignores the node's own transform for them).
      const Mat4 pm = m.parts[(size_t)pi].skinned() ? Mat4() : mat;
      for (const Vertex3D& v : m.parts[(size_t)pi].mesh.vertices) {
        Vec3 p = pm.point(Vec3(v.x, v.y, v.z));
        if (!any) {
          lo = hi = p;
          any = true;
        }
        lo = Vec3(std::fmin(lo.x, p.x), std::fmin(lo.y, p.y), std::fmin(lo.z, p.z));
        hi = Vec3(std::fmax(hi.x, p.x), std::fmax(hi.y, p.y), std::fmax(hi.z, p.z));
      }
    }
  }
  m.boundsMin = lo;
  m.boundsMax = hi;
}

}  // namespace

bool loadModelBytes(const uint8_t* data, size_t size, const std::string& baseDir, const Caps3D& caps,
                    ModelImageDecoder decode, Model3D& out, std::string& err) {
  out = Model3D();
  err.clear();
  Context c;
  c.baseDir = baseDir;
  c.caps = caps;
  c.decode = decode;

  std::string text;
  const uint8_t* bin = nullptr;
  size_t binSize = 0;
  if (data && size >= 4 && std::memcmp(data, "glTF", 4) == 0) {
    if (!splitGlb(data, size, text, bin, binSize, err)) return false;
  } else {
    size_t i = 0;
    while (data && i < size && (data[i] == ' ' || data[i] == '\t' || data[i] == '\n' || data[i] == '\r')) ++i;
    if (!data || i >= size || data[i] != '{') {
      err = "it is not a glTF file";
      return false;
    }
    text.assign((const char*)data, size);
  }
  std::string jerr;
  if (!Json::parse(text, c.json, jerr)) {
    err = "its description is damaged (" + jerr + ")";
    return false;
  }
  if (!c.json.isObject() || !c.json["asset"]["version"].isString()) {
    err = "it is not a glTF file";
    return false;
  }
  if (c.json["asset"]["version"].asString().compare(0, 2, "2.") != 0) {
    err = "it is not glTF version 2";
    return false;
  }
  const Json& req = c.json["extensionsRequired"];
  for (size_t i = 0; i < req.size(); ++i)
    if (!supportedExtension(req[i].asString())) {
      err = "it needs the extension " + req[i].asString() + ", which is not supported";
      return false;
    }

  std::vector<int> nodeMap;
  std::vector<size_t> usedSkins;
  if (!loadBuffers(c, bin, binSize, err) || !gltf::buildMaterials(c, out, err) ||
      !buildNodes(c, out, nodeMap, usedSkins, err) || !gltf::buildSkins(c, nodeMap, usedSkins, out, err)) {
    out = Model3D();
    return false;
  }
  if (out.parts.empty()) {
    out = Model3D();
    err = "it contains no triangle meshes";
    return false;
  }
  gltf::buildClips(c, nodeMap, out);
  computeBounds(out);
  return true;
}

bool loadModel(const std::string& path, const Caps3D& caps, ModelImageDecoder decode, Model3D& out, std::string& err) {
  out = Model3D();
  std::vector<uint8_t> bytes;
  if (!gltf::readFileBytes(path, bytes)) {
    err = "the file is missing or cannot be read";
    return false;
  }
  size_t slash = path.find_last_of("/\\");
  std::string dir = slash == std::string::npos ? std::string() : slash == 0 ? std::string("/") : path.substr(0, slash);
  if (!loadModelBytes(bytes.data(), bytes.size(), dir, caps, decode, out, err)) {
    OTE_ERROR("model %s: %s", path.c_str(), err.c_str());
    return false;
  }
  return true;
}

}  // namespace ote
