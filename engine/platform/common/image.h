// PNG and JPEG decoding (sprite sheets, model textures). Lives in the platform
// layer (the core never decodes images). No SDL, no exceptions; only PNG and
// JPEG are compiled in.
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

// Decodes PNG or JPEG bytes (told apart by their first bytes). False with a
// plain-English reason in err on failure.
bool decodeImage(const uint8_t* data, size_t size, Image& out, std::string& err);
// Decodes PNG bytes only. False with a plain-English reason in err on failure.
bool decodePng(const uint8_t* data, size_t size, Image& out, std::string& err);
// The size of a PNG from its header alone (the first 24 bytes; no pixels are decoded). False with a plain-English
// reason in err when the bytes are not a PNG or the header is damaged. Same wording as decodePng for the same faults.
bool readPngSize(const uint8_t* data, size_t size, int& width, int& height, std::string& err);
// The same for a file: reads only its first bytes (and checks the file is not larger than loadPngFile accepts).
bool readPngFileSize(const std::string& path, int& width, int& height, std::string& err);
// Reads and decodes a PNG file. err does not repeat the file name.
bool loadPngFile(const std::string& path, Image& out, std::string& err);

}  // namespace ote
