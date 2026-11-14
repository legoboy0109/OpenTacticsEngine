// glTF loader: numbers, files, data: URIs and bufferViews.
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "gfx3d/model_internal.h"

namespace ote {
namespace gltf {

bool asIndex(const Json& v, size_t& out) {
  if (!v.isNumber()) return false;
  double d = v.asNumber();
  if (!(d >= 0 && d <= 1e9) || d != std::floor(d)) return false;
  out = (size_t)d;
  return true;
}

bool readFileBytes(const std::string& path, std::vector<uint8_t>& out) {
  out.clear();
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return false;
  uint8_t buf[65536];
  size_t n;
  const size_t kMax = (size_t)512 * 1024 * 1024;  // a runaway file must not eat all memory
  bool ok = true;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) {
    out.insert(out.end(), buf, buf + n);
    if (out.size() > kMax) {
      ok = false;
      break;
    }
  }
  std::fclose(f);
  return ok;
}

namespace {

bool decodeBase64(const std::string& s, size_t from, std::vector<uint8_t>& out) {
  out.clear();
  uint32_t acc = 0;
  int bits = 0;
  for (size_t i = from; i < s.size(); ++i) {
    char ch = s[i];
    int v;
    if (ch >= 'A' && ch <= 'Z') v = ch - 'A';
    else if (ch >= 'a' && ch <= 'z') v = ch - 'a' + 26;
    else if (ch >= '0' && ch <= '9') v = ch - '0' + 52;
    else if (ch == '+') v = 62;
    else if (ch == '/') v = 63;
    else if (ch == '=') break;
    else if (ch == '\n' || ch == '\r' || ch == ' ') continue;
    else return false;
    acc = (acc << 6) | (uint32_t)v;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out.push_back((uint8_t)((acc >> bits) & 0xFF));
    }
  }
  return true;
}

// URIs are percent-encoded ("my%20model.bin").
std::string percentDecode(const std::string& s) {
  std::string r;
  for (size_t i = 0; i < s.size(); ++i) {
    if (s[i] == '%' && i + 2 < s.size() && std::isxdigit((unsigned char)s[i + 1]) &&
        std::isxdigit((unsigned char)s[i + 2])) {
      r += (char)std::strtol(s.substr(i + 1, 2).c_str(), nullptr, 16);
      i += 2;
    } else {
      r += s[i];
    }
  }
  return r;
}

// Files must stay inside the model's folder: a downloaded model must not be able to read other files.
bool pathEscapes(const std::string& p) {
  if (p.empty() || p[0] == '/' || p[0] == '\\' || (p.size() > 1 && p[1] == ':')) return true;
  size_t start = 0;
  while (start <= p.size()) {
    size_t end = p.find_first_of("/\\", start);
    if (end == std::string::npos) end = p.size();
    if (p.compare(start, end - start, "..") == 0) return true;
    start = end + 1;
  }
  return false;
}

}  // namespace

bool loadUri(const std::string& baseDir, const std::string& uri, std::vector<uint8_t>& out, std::string& err) {
  if (uri.compare(0, 5, "data:") == 0) {
    size_t comma = uri.find(',');
    size_t b64 = uri.find(";base64");
    if (comma == std::string::npos || b64 == std::string::npos || b64 > comma) {
      err = "it has an embedded data URI that is not base64";
      return false;
    }
    if (!decodeBase64(uri, comma + 1, out)) {
      err = "it has an embedded data URI with damaged base64 text";
      return false;
    }
    return true;
  }
  std::string rel = percentDecode(uri);
  if (rel.find("://") != std::string::npos) {
    err = "it refers to a file on the internet (\"" + uri + "\"), which is not supported";
    return false;
  }
  if (pathEscapes(rel)) {
    err = "it refers to a file outside its own folder (\"" + uri + "\")";
    return false;
  }
  std::string path = baseDir.empty() ? rel : baseDir + "/" + rel;
  if (!readFileBytes(path, out)) {
    err = "its file \"" + uri + "\" is missing or cannot be read";
    return false;
  }
  return true;
}

bool viewData(const Context& c, size_t view, const uint8_t*& data, size_t& size, size_t& stride, std::string& err) {
  const Json& v = c.json["bufferViews"][view];
  char msg[112];
  if (!v.isObject()) {
    std::snprintf(msg, sizeof msg, "it uses buffer view %zu, which does not exist", view);
    err = msg;
    return false;
  }
  size_t buf = 0, off = 0, len = 0, str = 0;
  if (!asIndex(v["buffer"], buf) || buf >= c.buffers.size() || !asIndex(v["byteLength"], len) ||
      (v.has("byteOffset") && !asIndex(v["byteOffset"], off)) || (v.has("byteStride") && !asIndex(v["byteStride"], str))) {
    std::snprintf(msg, sizeof msg, "its buffer view %zu is damaged (bad buffer, offset or length)", view);
    err = msg;
    return false;
  }
  const Buffer& b = c.buffers[buf];
  if (off > b.size || len > b.size - off) {
    std::snprintf(msg, sizeof msg, "its buffer view %zu reaches past the end of buffer %zu", view, buf);
    err = msg;
    return false;
  }
  data = b.data + off;
  size = len;
  stride = str;
  return true;
}

}  // namespace gltf
}  // namespace ote
