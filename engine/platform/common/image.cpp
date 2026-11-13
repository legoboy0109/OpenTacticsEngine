#include "platform/common/image.h"

#include <cstdio>
#include <cstring>

#include "../../../third_party/stb/stb_image.h"  // declarations only; the code is in stb_image_impl.cpp

namespace ote {

namespace {
const size_t kMaxFileBytes = (size_t)64 * 1024 * 1024;  // far beyond any sheet (2048 x 2048 PNG)
}

bool decodePng(const uint8_t* data, size_t size, Image& out, std::string& err) {
  out = Image();
  if (!data || size < 8 || std::memcmp(data, "\x89PNG\r\n\x1a\n", 8) != 0) {
    err = "the file is not a PNG image";
    return false;
  }
  int w = 0, h = 0, n = 0;
  unsigned char* px = stbi_load_from_memory(data, (int)size, &w, &h, &n, 4);
  if (!px) {
    const char* why = stbi_failure_reason();
    err = std::string("the PNG image cannot be decoded (") + (why ? why : "unknown error") + ")";
    return false;
  }
  out.width = w;
  out.height = h;
  out.rgba.assign(px, px + (size_t)w * h * 4);
  stbi_image_free(px);
  return true;
}

bool loadPngFile(const std::string& path, Image& out, std::string& err) {
  out = Image();
  FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) {
    err = "the file is missing or cannot be read";
    return false;
  }
  std::vector<uint8_t> bytes;
  uint8_t buf[16384];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) {
    bytes.insert(bytes.end(), buf, buf + n);
    if (bytes.size() > kMaxFileBytes) break;
  }
  std::fclose(f);
  if (bytes.size() > kMaxFileBytes) {
    err = "the file is far too large for a sprite sheet";
    return false;
  }
  return decodePng(bytes.data(), bytes.size(), out, err);
}

}  // namespace ote
