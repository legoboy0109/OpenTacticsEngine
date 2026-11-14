// glTF loader: reading accessors (typed arrays in the buffers), range-checked.
#include <algorithm>
#include <cstdio>
#include <cstring>

#include "gfx3d/model_internal.h"

namespace ote {
namespace gltf {

namespace {

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
  if (t == "MAT4") return 16;
  return 0;  // other matrices: not used
}

bool fail(std::string& err, const char* fmt, size_t a) {
  char msg[160];
  std::snprintf(msg, sizeof msg, fmt, a);
  err = msg;
  return false;
}

// Checks everything about accessor `idx` against the real buffer sizes, so reads below cannot go out of range.
}  // namespace

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

namespace {
// glTF numbers are little-endian whatever the machine is; read them byte by
// byte so big-endian consoles (PS3, Wii U, Xbox 360) get the same values.
uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | (p[1] << 8)); }
uint32_t le32(const uint8_t* p) { return p[0] | (p[1] << 8) | (p[2] << 16) | ((uint32_t)p[3] << 24); }

}  // namespace

// Component k of element i as a float (normalised integers mapped to 0..1 / -1..1).
float readComp(const Accessor& a, size_t i, int k) {
  const uint8_t* p = a.data + i * a.stride + (size_t)k * (size_t)compSize(a.comp);
  switch (a.comp) {
    case kFloat: { uint32_t bits = le32(p); float f; std::memcpy(&f, &bits, 4); return f; }
    case kUByte: return a.norm ? p[0] / 255.0f : (float)p[0];
    case kByte: { int v = (int8_t)p[0]; return a.norm ? std::max(v / 127.0f, -1.0f) : (float)v; }
    case kUShort: { uint16_t v = le16(p); return a.norm ? v / 65535.0f : (float)v; }
    case kShort: { int16_t v = (int16_t)le16(p); return a.norm ? std::max(v / 32767.0f, -1.0f) : (float)v; }
    case kUInt: return (float)le32(p);
  }
  return 0;
}

uint32_t readIndex(const Accessor& a, size_t i) {
  const uint8_t* p = a.data + i * a.stride;
  if (a.comp == kUByte) return p[0];
  if (a.comp == kUShort) return le16(p);
  return le32(p);
}

}  // namespace gltf
}  // namespace ote
