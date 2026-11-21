// A small PNG encoder (no library: stb_image_write is not vendored) and a BMP reader for the player's screenshots.
// The deflate stream uses LZ77 with the fixed Huffman code, which is plenty for flat game pictures; the point is a result
// small enough to hand to a model as an image.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ote_mcp {

struct Image {
  int width = 0, height = 0;
  std::vector<uint8_t> rgb;  // width * height * 3, top row first
};

// Reads an uncompressed 24 or 32 bit BMP (what SDL_SaveBMP writes). False with `err` on anything else.
bool decodeBmp(const std::string& bytes, Image& out, std::string& err);

// Shrinks by whole-number box averaging so the longer side is at most `maxSide` (never enlarges).
Image shrinkToFit(const Image& in, int maxSide);

// Encodes as an 8-bit RGB PNG.
std::string encodePng(const Image& img);

// Raw deflate of `data` in a zlib wrapper (exposed for the unit test, which inflates it with an independent decoder).
std::string zlibCompress(const std::string& data);

}  // namespace ote_mcp
