// glTF loader: accessors and primitives -> MeshData.
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <utility>

#include "core/base/log.h"
#include "gfx3d/model_internal.h"

namespace ote {
namespace gltf {

namespace {
uint8_t toByte(float f) {
  f = f < 0 ? 0 : (f > 1 ? 1 : f);
  return (uint8_t)(f * 255.0f + 0.5f);
}

// Finds an optional attribute accessor and checks it matches the vertex count. found=false: not present.
bool optionalAttribute(const Context& c, const Json& attrs, const char* name, size_t vertexCount, int minN, int maxN,
                       Accessor& a, bool& found, std::string& err) {
  found = false;
  if (!attrs.has(name)) return true;
  size_t idx;
  if (!asIndex(attrs[name], idx)) {
    err = std::string("a mesh has a damaged ") + name + " entry";
    return false;
  }
  if (!openAccessor(c, idx, a, err)) return false;
  if (a.n < minN || a.n > maxN || a.count != vertexCount || a.comp == kUInt) {
    err = std::string("a mesh has ") + name + " data of the wrong kind or length";
    return false;
  }
  found = true;
  return true;
}

// JOINTS_0 / WEIGHTS_0 -> four joints and four weights per vertex (weights normalised to sum 1; all zero = joint 0).
// found = false (and a warning): the primitive has no usable pair.
bool readSkinAttributes(const Context& c, const Json& attrs, size_t vcount, size_t jointCount, SkinWeights& sw, bool& found,
                        std::string& err) {
  found = false;
  Accessor ja, wa;
  bool hasJ, hasW;
  if (!optionalAttribute(c, attrs, "JOINTS_0", vcount, 4, 4, ja, hasJ, err) ||
      !optionalAttribute(c, attrs, "WEIGHTS_0", vcount, 4, 4, wa, hasW, err))
    return false;
  if (attrs.has("JOINTS_1") || attrs.has("WEIGHTS_1"))
    OTE_WARN("model: more than four joints per vertex are not supported; only the first four count");
  if (!hasJ || !hasW) {
    OTE_WARN("model: a skinned mesh part has no JOINTS_0 / WEIGHTS_0; it is drawn rigid");
    return true;
  }
  if (ja.comp != kUByte && ja.comp != kUShort) {
    err = "a mesh has JOINTS_0 data that is not whole numbers";
    return false;
  }
  if (wa.comp != kFloat && wa.comp != kUByte && wa.comp != kUShort) {
    err = "a mesh has WEIGHTS_0 data of a type that is not supported";
    return false;
  }
  wa.norm = wa.comp != kFloat;  // whole-number weights are always 0..max = 0..1
  sw.joints.resize(vcount * 4);
  sw.weights.resize(vcount * 4);
  for (size_t i = 0; i < vcount; ++i) {
    float sum = 0;
    for (int k = 0; k < 4; ++k) {
      size_t at = i * 4 + (size_t)k;
      const uint8_t* p = ja.data + i * ja.stride + (size_t)k * (ja.comp == kUByte ? 1 : 2);
      uint32_t j = ja.comp == kUByte ? p[0] : (uint32_t)(p[0] | (p[1] << 8));
      if (j >= jointCount) {
        err = "a mesh part refers to a joint its skin does not have";
        sw = SkinWeights();
        return false;
      }
      sw.joints[at] = (uint16_t)j;
      float w = readComp(wa, i, k);
      if (!(w >= 0) || w > 1e9f) w = 0;  // negative, NaN and absurd weights count as nothing
      sw.weights[at] = w;
      sum += w;
    }
    if (sum > 1e-8f) {
      for (int k = 0; k < 4; ++k) sw.weights[i * 4 + (size_t)k] /= sum;
    } else {
      for (int k = 0; k < 4; ++k) sw.weights[i * 4 + (size_t)k] = k == 0 ? 1.0f : 0.0f;
    }
  }
  found = true;
  return true;
}

}  // namespace

bool readPrimitive(const Context& c, const Json& prim, MeshData& out, bool& skip, std::string& err, size_t jointCount,
                   SkinWeights* skin) {
  skip = false;
  out = MeshData();
  if (skin) *skin = SkinWeights();
  size_t mode = 4;
  if (prim.has("mode") && !asIndex(prim["mode"], mode)) {
    err = "a mesh has a damaged drawing mode";
    return false;
  }
  if (mode != 4) {
    OTE_WARN("model: skipping a mesh part drawn as lines, points, strips or fans (only triangles are supported)");
    skip = true;
    return true;
  }
  if (prim.has("targets")) OTE_WARN("model: morph targets are ignored (the model is loaded in its base shape)");

  const Json& attrs = prim["attributes"];
  size_t idx;
  if (!asIndex(attrs["POSITION"], idx)) {
    err = "a mesh part has no POSITION data";
    return false;
  }
  Accessor pos;
  if (!openAccessor(c, idx, pos, err)) return false;
  if (pos.n != 3 || pos.comp == kUInt) {
    err = "a mesh part has POSITION data that is not 3D points";
    return false;
  }
  const size_t vcount = pos.count;
  if (vcount == 0) {
    OTE_WARN("model: skipping an empty mesh part");
    skip = true;
    return true;
  }

  Accessor nrm, uv, col;
  bool hasN, hasUv, hasCol;
  if (!optionalAttribute(c, attrs, "NORMAL", vcount, 3, 3, nrm, hasN, err) ||
      !optionalAttribute(c, attrs, "TEXCOORD_0", vcount, 2, 2, uv, hasUv, err) ||
      !optionalAttribute(c, attrs, "COLOR_0", vcount, 3, 4, col, hasCol, err))
    return false;

  // Triangle list: indices from the file or sequential.
  std::vector<uint32_t> tri;
  if (prim.has("indices")) {
    Accessor ia;
    if (!asIndex(prim["indices"], idx)) {
      err = "a mesh part has a damaged indices entry";
      return false;
    }
    if (!openAccessor(c, idx, ia, err)) return false;
    if (ia.n != 1 || (ia.comp != kUByte && ia.comp != kUShort && ia.comp != kUInt)) {
      err = "a mesh part has indices that are not whole numbers";
      return false;
    }
    tri.resize(ia.count);
    for (size_t i = 0; i < ia.count; ++i) {
      tri[i] = readIndex(ia, i);
      if (tri[i] >= vcount) {
        err = "a mesh part has an index pointing past its last vertex";
        return false;
      }
    }
  } else {
    tri.resize(vcount);
    for (size_t i = 0; i < vcount; ++i) tri[i] = (uint32_t)i;
  }
  if (tri.size() % 3 != 0) {
    OTE_WARN("model: a mesh part's index count is not a multiple of 3; the leftover is dropped");
    tri.resize(tri.size() / 3 * 3);
  }
  if (tri.empty()) {
    OTE_WARN("model: skipping a mesh part without triangles");
    skip = true;
    return true;
  }

  SkinWeights sw;
  bool hasSkin = false;
  if (jointCount > 0 && skin && !readSkinAttributes(c, attrs, vcount, jointCount, sw, hasSkin, err)) return false;

  std::vector<Vertex3D> v(vcount);
  for (size_t i = 0; i < vcount; ++i) {
    Vertex3D& o = v[i];
    o.x = readComp(pos, i, 0);
    o.y = readComp(pos, i, 1);
    o.z = readComp(pos, i, 2);
    if (hasN) {
      o.nx = readComp(nrm, i, 0);
      o.ny = readComp(nrm, i, 1);
      o.nz = readComp(nrm, i, 2);
    }
    if (hasUv) {
      o.u = readComp(uv, i, 0);
      o.v = readComp(uv, i, 1);
    }
    if (hasCol) {
      o.r = toByte(readComp(col, i, 0));
      o.g = toByte(readComp(col, i, 1));
      o.b = toByte(readComp(col, i, 2));
      o.a = col.n == 4 ? toByte(readComp(col, i, 3)) : 255;
    }
  }

  if (hasN) {
    out.vertices.swap(v);
    out.indices = tri;
    if (hasSkin) *skin = std::move(sw);
    return true;
  }
  // No normals: give every triangle its own three vertices carrying the flat face normal.
  out.vertices.reserve(tri.size());
  out.indices.reserve(tri.size());
  for (size_t t = 0; t < tri.size(); t += 3) {
    Vertex3D a = v[tri[t]], b = v[tri[t + 1]], d = v[tri[t + 2]];
    Vec3 n = normalize(cross(Vec3(b.x - a.x, b.y - a.y, b.z - a.z), Vec3(d.x - a.x, d.y - a.y, d.z - a.z)));
    Vertex3D* vs[3] = {&a, &b, &d};
    for (Vertex3D* p : vs) {
      p->nx = n.x;
      p->ny = n.y;
      p->nz = n.z;
      out.indices.push_back((uint32_t)out.vertices.size());
      out.vertices.push_back(*p);
    }
    if (hasSkin)
      for (size_t k = 0; k < 3; ++k) {
        size_t o = tri[t + k] * 4;
        skin->joints.insert(skin->joints.end(), sw.joints.begin() + o, sw.joints.begin() + o + 4);
        skin->weights.insert(skin->weights.end(), sw.weights.begin() + o, sw.weights.begin() + o + 4);
      }
  }
  return true;
}

}  // namespace gltf
}  // namespace ote
