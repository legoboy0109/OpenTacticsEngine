// PNG decoding for sprite sheets. Lives in the platform layer (the core never
// decodes images). No SDL, no exceptions; only PNG is compiled in.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace ote {

struct Image {
  int width = 0, height = 0;
  std::vector<uint8_t> rgba;  // width * height * 4 bytes, R,G,B,A, not premultiplied
};

// Decodes PNG bytes. False with a plain-English reason in err on failure.
bool decodePng(const uint8_t* data, size_t size, Image& out, std::string& err);
// Reads and decodes a PNG file. err does not repeat the file name.
bool loadPngFile(const std::string& path, Image& out, std::string& err);

}  // namespace ote
