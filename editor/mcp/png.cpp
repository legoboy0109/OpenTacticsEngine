#include "png.h"

#include <algorithm>
#include <cstring>

namespace ote_mcp {

namespace {

uint32_t le32(const uint8_t* p) { return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24; }
uint16_t le16(const uint8_t* p) { return (uint16_t)(p[0] | p[1] << 8); }

// ---- checksums --------------------------------------------------------------------------------------------------
uint32_t crc32(const uint8_t* data, size_t n, uint32_t crc = 0) {
  static uint32_t table[256];
  static bool made = false;
  if (!made) {
    for (uint32_t i = 0; i < 256; ++i) {
      uint32_t c = i;
      for (int k = 0; k < 8; ++k) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
      table[i] = c;
    }
    made = true;
  }
  crc = ~crc;
  for (size_t i = 0; i < n; ++i) crc = table[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
  return ~crc;
}

uint32_t adler32(const std::string& s) {
  uint32_t a = 1, b = 0;
  for (unsigned char c : s) {
    a = (a + c) % 65521;
    b = (b + a) % 65521;
  }
  return b << 16 | a;
}

void be32(std::string& o, uint32_t v) {
  for (int s = 24; s >= 0; s -= 8) o += (char)(v >> s & 0xFF);
}

// ---- deflate, fixed Huffman -------------------------------------------------------------------------------------
struct BitWriter {
  std::string out;
  uint32_t acc = 0;
  int bits = 0;
  void put(uint32_t value, int n) {  // least significant bit first (extra bits, block header)
    acc |= value << bits;
    bits += n;
    while (bits >= 8) { out += (char)(acc & 0xFF); acc >>= 8; bits -= 8; }
  }
  void putCode(uint32_t code, int n) {  // Huffman codes go most significant bit first
    uint32_t rev = 0;
    for (int i = 0; i < n; ++i) rev |= (code >> i & 1) << (n - 1 - i);
    put(rev, n);
  }
  void flush() { if (bits > 0) { out += (char)(acc & 0xFF); acc = 0; bits = 0; } }
};

void putLiteralOrLength(BitWriter& w, int sym) {
  if (sym < 144) w.putCode(0x30 + (uint32_t)sym, 8);
  else if (sym < 256) w.putCode(0x190 + (uint32_t)(sym - 144), 9);
  else if (sym < 280) w.putCode((uint32_t)(sym - 256), 7);
  else w.putCode(0xC0 + (uint32_t)(sym - 280), 8);
}

const int kLenBase[29] = {3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
const int kLenExtra[29] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
const int kDistBase[30] = {1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
const int kDistExtra[30] = {0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

void putMatch(BitWriter& w, int len, int dist) {
  int li = 28;
  while (kLenBase[li] > len) --li;
  putLiteralOrLength(w, 257 + li);
  w.put((uint32_t)(len - kLenBase[li]), kLenExtra[li]);
  int di = 29;
  while (kDistBase[di] > dist) --di;
  w.putCode((uint32_t)di, 5);
  w.put((uint32_t)(dist - kDistBase[di]), kDistExtra[di]);
}

}  // namespace

std::string zlibCompress(const std::string& data) {
  BitWriter w;
  w.out += "\x78\x01";  // zlib header: deflate, 32K window, no preset dictionary
  w.put(1, 1);           // one final block ...
  w.put(1, 2);           // ... with the fixed Huffman code
  const size_t n = data.size();
  const auto* d = reinterpret_cast<const uint8_t*>(data.data());
  const size_t kWindow = 32768, kHash = 1 << 15;
  std::vector<int> head(kHash, -1), prev(n ? n : 1, -1);
  auto hashAt = [&](size_t i) { return (size_t)(((uint32_t)d[i] << 10 ^ (uint32_t)d[i + 1] << 5 ^ d[i + 2]) * 2654435761u >> 17) & (kHash - 1); };
  size_t i = 0;
  while (i < n) {
    int bestLen = 0, bestDist = 0;
    if (i + 3 <= n) {
      const size_t h = hashAt(i);
      int cand = head[h];
      int tries = 24;
      while (cand >= 0 && i - (size_t)cand <= kWindow && tries-- > 0) {
        size_t l = 0;
        const size_t maxLen = std::min<size_t>(258, n - i);
        while (l < maxLen && d[(size_t)cand + l] == d[i + l]) ++l;
        if ((int)l > bestLen) { bestLen = (int)l; bestDist = (int)(i - (size_t)cand); if (l == maxLen) break; }
        cand = prev[(size_t)cand];
      }
    }
    const size_t step = bestLen >= 3 ? (size_t)bestLen : 1;
    if (bestLen >= 3) putMatch(w, bestLen, bestDist);
    else putLiteralOrLength(w, d[i]);
    for (size_t k = 0; k < step; ++k, ++i)  // every position of what was just emitted joins the hash chains
      if (i + 3 <= n) {
        const size_t h = hashAt(i);
        prev[i] = head[h];
        head[h] = (int)i;
      }
  }
  putLiteralOrLength(w, 256);  // end of block
  w.flush();
  be32(w.out, adler32(data));
  return w.out;
}

namespace {
void chunk(std::string& png, const char* type, const std::string& body) {
  be32(png, (uint32_t)body.size());
  const size_t start = png.size();
  png += type;
  png += body;
  be32(png, crc32(reinterpret_cast<const uint8_t*>(png.data() + start), png.size() - start));
}
}  // namespace

std::string encodePng(const Image& img) {
  std::string png = "\x89PNG\r\n\x1a\n";
  std::string ihdr;
  be32(ihdr, (uint32_t)img.width);
  be32(ihdr, (uint32_t)img.height);
  ihdr += std::string("\x08\x02\x00\x00\x00", 5);  // 8 bit, truecolour, deflate, filter method 0, no interlace
  chunk(png, "IHDR", ihdr);
  // Every row but the first is stored with filter 2 ("Up"): flat pictures turn into runs of zeros the compressor loves.
  const size_t stride = (size_t)img.width * 3;
  std::string raw;
  raw.reserve((stride + 1) * (size_t)img.height);
  for (int y = 0; y < img.height; ++y) {
    const uint8_t* row = img.rgb.data() + (size_t)y * stride;
    raw += (char)(y == 0 ? 0 : 2);
    for (size_t x = 0; x < stride; ++x) raw += (char)(y == 0 ? row[x] : (uint8_t)(row[x] - row[x - stride]));
  }
  chunk(png, "IDAT", zlibCompress(raw));
  chunk(png, "IEND", "");
  return png;
}

bool decodeBmp(const std::string& bytes, Image& out, std::string& err) {
  const auto* b = reinterpret_cast<const uint8_t*>(bytes.data());
  if (bytes.size() < 54 || b[0] != 'B' || b[1] != 'M') { err = "not a BMP file"; return false; }
  const uint32_t dataOffset = le32(b + 10), headerSize = le32(b + 14);
  if (headerSize < 40) { err = "unsupported BMP header"; return false; }
  const int32_t w = (int32_t)le32(b + 18);
  int32_t h = (int32_t)le32(b + 22);
  const uint16_t bpp = le16(b + 28);
  const uint32_t compression = le32(b + 30);
  const bool topDown = h < 0;
  if (topDown) h = -h;
  if ((bpp != 24 && bpp != 32) || (compression != 0 && compression != 3)) { err = "unsupported BMP format (need 24 or 32 bit, uncompressed)"; return false; }
  if (w <= 0 || h <= 0 || w > 16384 || h > 16384) { err = "BMP size out of range"; return false; }
  const size_t rowBytes = ((size_t)w * bpp / 8 + 3) / 4 * 4;
  if ((size_t)dataOffset + rowBytes * (size_t)h > bytes.size()) { err = "BMP file is cut short"; return false; }
  // 32 bit with a bit-field header: the masks say where blue / green / red sit; SDL writes the usual B, G, R, A order.
  out.width = w;
  out.height = h;
  out.rgb.assign((size_t)w * (size_t)h * 3, 0);
  for (int32_t y = 0; y < h; ++y) {
    const uint8_t* src = b + dataOffset + rowBytes * (size_t)(topDown ? y : h - 1 - y);
    uint8_t* dst = out.rgb.data() + (size_t)y * (size_t)w * 3;
    for (int32_t x = 0; x < w; ++x) {
      const uint8_t* px = src + (size_t)x * bpp / 8;
      dst[x * 3 + 0] = px[2];
      dst[x * 3 + 1] = px[1];
      dst[x * 3 + 2] = px[0];
    }
  }
  return true;
}

Image shrinkToFit(const Image& in, int maxSide) {
  const int longer = std::max(in.width, in.height);
  if (longer <= maxSide || maxSide < 1) return in;
  const int f = (longer + maxSide - 1) / maxSide;  // whole-number factor
  Image out;
  out.width = std::max(1, in.width / f);
  out.height = std::max(1, in.height / f);
  out.rgb.assign((size_t)out.width * (size_t)out.height * 3, 0);
  for (int y = 0; y < out.height; ++y)
    for (int x = 0; x < out.width; ++x) {
      unsigned sum[3] = {0, 0, 0};
      for (int dy = 0; dy < f; ++dy)
        for (int dx = 0; dx < f; ++dx) {
          const uint8_t* p = in.rgb.data() + ((size_t)(y * f + dy) * (size_t)in.width + (size_t)(x * f + dx)) * 3;
          for (int c = 0; c < 3; ++c) sum[c] += p[c];
        }
      for (int c = 0; c < 3; ++c) out.rgb[((size_t)y * (size_t)out.width + (size_t)x) * 3 + (size_t)c] = (uint8_t)(sum[c] / (unsigned)(f * f));
    }
  return out;
}

}  // namespace ote_mcp
