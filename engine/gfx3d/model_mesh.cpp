// glTF loader: accessors and primitives -> MeshData.
#include <algorithm>
#include <cstdio>
#include <cstring>

#include "core/base/log.h"
#include "gfx3d/model_internal.h"

namespace ote {
namespace gltf {

namespace {

enum { kByte = 5120, kUByte = 5121, kShort = 5122, kUShort = 5123, kUInt = 5125, kFloat = 5126 };

struct Accessor {
  const uint8_t* data = nullptr;
  size_t stride = 0, count = 0;
  int comp = 0, n = 0;
  bool norm = false;
};

int compSize(int t) {
  switch (t) {
    case kByte: case kUByte: return 1;
    case kShort: case kUShort: return 2;
    case kUInt: case kFloat: return 4;
  }
  return 0;
}

int typeCount(const std::string& t) {
  if (t == "SCALAR") return 1;
  if (t == "VEC2") return 2;
  if (t == "VEC3") return 3;
  if (t == "VEC4") return 4;
  return 0;  // matrices: not used by anything this loader reads
}

bool fail(std::string& err, const char* fmt, size_t a) {
  char msg[160];
  std::snprintf(msg, sizeof msg, fmt, a);
  err = msg;
  return false;
}

// Checks everything about accessor `idx` against the real buffer sizes, so reads below cannot go out of range.
bool openAccessor(const Context& c, size_t idx, Accessor& a, std::string& err) {
  const Json& j = c.json["accessors"][idx];
  if (!j.isObject()) return fail(err, "it uses accessor %zu, which does not exist", idx);
  if (j.has("sparse")) return fail(err, "its accessor %zu uses sparse storage, which is not supported", idx);
  size_t count = 0, off = 0, ct = 0;
  if (!asIndex(j["componentType"], ct) || !asIndex(j["count"], count) || (j.has("byteOffset") && !asIndex(j["byteOffset"], off)))
    return fail(err, "its accessor %zu is damaged (bad type, count or offset)", idx);
  int es = compSize((int)ct), n = typeCount(j["type"].asString());
  if (es == 0 || n == 0) return fail(err, "its accessor %zu has a data type that is not supported", idx);
  size_t view = 0;
  if (!asIndex(j["bufferView"], view)) return fail(err, "its accessor %zu has no data", idx);
  const uint8_t* base;
  size_t size, vstride;
  if (!viewData(c, view, base, size, vstride, err)) return false;
  size_t elem = (size_t)es * (size_t)n;
  size_t stride = vstride ? vstride : elem;
  if (stride < elem) return fail(err, "its accessor %zu has a byte stride smaller than its elements", idx);
  if (count > 0) {
    size_t need = off + (count - 1) * stride + elem;  // every factor is <= 1e9: no overflow
    if (need > size) return fail(err, "its accessor %zu reads past the end of its data", idx);
  }
  a.data = base + off;
  a.stride = stride;
  a.count = count;
  a.comp = (int)ct;
  a.n = n;
  a.norm = j["normalized"].asBool(false);
  return true;
}

// Component k of element i as a float (normalised integers mapped to 0..1 / -1..1). Little-endian host assumed
// (every target of the engine is).
float readComp(const Accessor& a, size_t i, int k) {
  const uint8_t* p = a.data + i * a.stride + (size_t)k * (size_t)compSize(a.comp);
  switch (a.comp) {
    case kFloat: { float f; std::memcpy(&f, p, 4); return f; }
    case kUByte: return a.norm ? p[0] / 255.0f : (float)p[0];
    case kByte: { int v = (int8_t)p[0]; return a.norm ? std::max(v / 127.0f, -1.0f) : (float)v; }
    case kUShort: { uint16_t v; std::memcpy(&v, p, 2); return a.norm ? v / 65535.0f : (float)v; }
    case kShort: { int16_t v; std::memcpy(&v, p, 2); return a.norm ? std::max(v / 32767.0f, -1.0f) : (float)v; }
    case kUInt: { uint32_t v; std::memcpy(&v, p, 4); return (float)v; }
  }
  return 0;
}

uint32_t readIndex(const Accessor& a, size_t i) {
  const uint8_t* p = a.data + i * a.stride;
  if (a.comp == kUByte) return p[0];
  if (a.comp == kUShort) { uint16_t v; std::memcpy(&v, p, 2); return v; }
  uint32_t v;
  std::memcpy(&v, p, 4);
  return v;
}

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

}  // namespace

bool readPrimitive(const Context& c, const Json& prim, MeshData& out, bool& skip, std::string& err) {
  skip = false;
  out = MeshData();
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
  }
  return true;
}

}  // namespace gltf
}  // namespace ote
