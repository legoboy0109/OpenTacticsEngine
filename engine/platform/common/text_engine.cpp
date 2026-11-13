#include "platform/common/text_engine.h"

#include <cmath>

#include "core/base/json.h"
#include "core/base/log.h"
#include "platform/common/bitmap_font.h"

// Declarations only (the implementation lives in stb_truetype_impl.cpp).
#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wall"
#pragma GCC diagnostic ignored "-Wextra"
#endif
#include "../../../third_party/stb/stb_truetype.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

namespace ote {
namespace {

constexpr int kPageSize = 512;      // atlas page edge in pixels (1 MB as RGBA)
constexpr size_t kMaxPages = 4;     // cache budget; beyond this everything is dropped and re-rasterized
constexpr int kMaxRaster = 256;     // largest glyph raster height we accept
constexpr int kBuiltinSize = 8;

int roundi(float v) { return (int)std::floor(v + 0.5f); }

}  // namespace

uint32_t decodeUtf8(const char*& p) {
  const unsigned char* s = (const unsigned char*)p;
  unsigned c = s[0];
  if (c == 0) return 0;  // caller stops at NUL; do not advance
  if (c < 0x80) { p += 1; return c; }
  int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC2 ? 1 : -1;
  if (c > 0xF4 || extra < 0) { p += 1; return 0xFFFD; }
  uint32_t cp = c & (0x3F >> extra);
  for (int i = 1; i <= extra; ++i) {
    if ((s[i] & 0xC0) != 0x80) { p += 1; return 0xFFFD; }  // truncated sequence (also stops at NUL)
    cp = (cp << 6) | (s[i] & 0x3F);
  }
  p += extra + 1;
  if (cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return 0xFFFD;
  return cp;
}

TextEngine::TextEngine(TextTarget& target) : target_(target) {}

TextEngine::~TextEngine() {
  clearCache();
  delete ttf_;
}

void TextEngine::clearCache() {
  for (Page& p : pages_)
    if (p.tex >= 0) target_.destroyTexture(p.tex);
  pages_.clear();
  cache_.clear();
}

bool TextEngine::setFont(const FontRequest& req) {
  clearCache();
  delete ttf_;
  ttf_ = nullptr;
  data_.clear();
  font_ = req;
  if (font_.size < 4) font_.size = 4;
  if (font_.size > 128) font_.size = 128;
  lastOutScale_ = 0;
  if (req.file.empty()) {
    // The 8x8 bitmap font only grows in whole multiples (size 12 -> 16 px, ...).
    font_.size = kBuiltinSize * ((font_.size + kBuiltinSize / 2) / kBuiltinSize);  // size was clamped to >= 4, so >= 1x
    font_.smooth = false;
    return true;
  }
  bool ok = false;
  if (!readTextFile(req.file, data_)) {
    OTE_ERROR("font: cannot read '%s'", req.file.c_str());
  } else {
    const unsigned char* bytes = (const unsigned char*)data_.data();
    int offset = stbtt_GetFontOffsetForIndex(bytes, 0);
    ttf_ = new stbtt_fontinfo();
    if (offset < 0 || !stbtt_InitFont(ttf_, bytes, offset)) {
      OTE_ERROR("font: '%s' is not a usable TrueType/OpenType font", req.file.c_str());
      delete ttf_;
      ttf_ = nullptr;
    } else {
      int gap = 0;
      stbtt_GetFontVMetrics(ttf_, &ascent_, &descent_, &gap);
      ok = ascent_ > descent_;
      if (!ok) { delete ttf_; ttf_ = nullptr; OTE_ERROR("font: '%s' has no vertical metrics", req.file.c_str()); }
    }
  }
  if (!ok) {
    data_.clear();
    font_ = FontRequest();
    font_.file.clear();
    font_.size = kBuiltinSize;
    font_.smooth = false;
    OTE_WARN("font: falling back to the built-in pixel font");
    return false;
  }
  return true;
}

// --- metrics -------------------------------------------------------------

float TextEngine::unitScale(int pixelHeight) const {
  return stbtt_ScaleForPixelHeight(ttf_, (float)pixelHeight);
}

int TextEngine::pixelHeightFor(int scale) const {
  int ph = font_.size * (scale < 1 ? 1 : scale);
  return ph > kMaxRaster ? kMaxRaster : ph;
}

int TextEngine::lineHeight(int scale) const {
  if (scale < 1) scale = 1;
  return font_.size * scale;  // built-in font: 8 x its whole-number multiple
}

int TextEngine::glyphIndex(uint32_t cp) const {
  if (cp < 32) cp = ' ';
  int gi = stbtt_FindGlyphIndex(ttf_, (int)cp);
  if (gi == 0 && cp != '?') {
    int q = stbtt_FindGlyphIndex(ttf_, '?');
    if (q != 0) return q;
  }
  return gi;  // may be 0 = the font's .notdef
}

int TextEngine::advanceUnits(uint32_t cp) const {
  int adv = 0, lsb = 0;
  stbtt_GetGlyphHMetrics(ttf_, glyphIndex(cp), &adv, &lsb);
  return adv;
}

int TextEngine::textWidth(const char* text, int scale) const {
  if (!text) return 0;
  if (scale < 1) scale = 1;
  if (!ttf_) {
    int n = 0;
    for (const char* p = text; *p;) { decodeUtf8(p); ++n; }
    return n * font_.size * scale;
  }
  float sc = unitScale(pixelHeightFor(scale));
  if (font_.smooth) {  // fractional pen: the real advances, rounded once at the end
    long units = 0;
    for (const char* p = text; *p;) units += advanceUnits(decodeUtf8(p));
    return roundi((float)units * sc);
  }
  int w = 0;  // hard pixels: every advance is a whole logical pixel count
  for (const char* p = text; *p;) w += roundi((float)advanceUnits(decodeUtf8(p)) * sc);
  return w;
}

// --- atlas ---------------------------------------------------------------

bool TextEngine::place(int w, int h, Glyph& g) {
  if (w + 2 > kPageSize || h + 2 > kPageSize) return false;
  for (int attempt = 0; attempt < 2; ++attempt) {
    if (!pages_.empty()) {
      Page& pg = pages_.back();
      if (pg.cx + w + 1 > kPageSize) { pg.cy += pg.rowH + 1; pg.cx = 0; pg.rowH = 0; }
      if (pg.cy + h + 1 <= kPageSize) {
        g.page = (int)pages_.size() - 1;
        g.ax = pg.cx;
        g.ay = pg.cy;
        pg.cx += w + 1;
        if (h > pg.rowH) pg.rowH = h;
        return true;
      }
    }
    if (pages_.size() >= kMaxPages) {
      // Over budget: drop everything (glyphs re-rasterize on demand). Safe
      // because callers never hold Glyph references across glyph() calls.
      OTE_DEBUG("text: atlas full, flushing glyph cache");
      clearCache();
    }
    Page np;
    np.tex = target_.createTexture(kPageSize, kPageSize);
    if (np.tex < 0) { OTE_ERROR("text: cannot create atlas texture"); return false; }
    pages_.push_back(np);
  }
  return false;
}

void TextEngine::upload(const Glyph& g, const std::vector<uint8_t>& alpha) {
  std::vector<uint8_t> rgba((size_t)g.w * g.h * 4);
  for (size_t i = 0; i < alpha.size(); ++i) {
    rgba[i * 4 + 0] = rgba[i * 4 + 1] = rgba[i * 4 + 2] = 255;
    rgba[i * 4 + 3] = alpha[i];
  }
  target_.updateTexture(pages_[g.page].tex, g.ax, g.ay, g.w, g.h, rgba.data(), g.w * 4);
}

TextEngine::Glyph TextEngine::rasterize(uint32_t cp, int ph, Mode m) {
  Glyph g;
  std::vector<uint8_t> alpha;
  if (m == kBuiltin) {
    g.w = g.h = 8;
    const unsigned char* rows = bitmapGlyph((char)cp);
    alpha.assign(64, 0);
    for (int yy = 0; yy < 8; ++yy)
      for (int xx = 0; xx < 8; ++xx)
        if ((rows[yy] >> xx) & 1) alpha[yy * 8 + xx] = 255;  // bit 0 = leftmost pixel
  } else {
    float sc = unitScale(ph);
    int gi = glyphIndex(cp);
    int x0, y0, x1, y1;
    stbtt_GetGlyphBitmapBox(ttf_, gi, sc, sc, &x0, &y0, &x1, &y1);
    g.w = x1 - x0;
    g.h = y1 - y0;
    g.xoff = x0;
    g.yoff = y0;
    if (g.w <= 0 || g.h <= 0) return g;  // blank (space)
    alpha.assign((size_t)g.w * g.h, 0);
    stbtt_MakeGlyphBitmap(ttf_, alpha.data(), g.w, g.h, g.w, sc, sc, gi);
    if (m == kHard)  // crisp pixels: no partial coverage
      for (uint8_t& a : alpha) a = a >= 128 ? 255 : 0;
  }
  if (!place(g.w, g.h, g)) { g.page = -1; g.w = g.h = 0; return g; }
  upload(g, alpha);
  return g;
}

const TextEngine::Glyph& TextEngine::glyph(uint32_t cp, int ph, Mode m) {
  uint64_t key = (uint64_t)cp | ((uint64_t)ph << 24) | ((uint64_t)m << 48);
  auto it = cache_.find(key);
  if (it != cache_.end()) return it->second;
  Glyph g = rasterize(cp, ph, m);  // may flush the cache; insert only afterwards
  return cache_.emplace(key, g).first->second;
}

// --- drawing -------------------------------------------------------------

void TextEngine::drawText(int x, int y, const char* text, Color c, int scale) {
  if (!text || !*text) return;
  if (scale < 1) scale = 1;
  Mode m = mode();

  if (m == kBuiltin) {
    int pen = 0, cell = font_.size * scale;
    for (const char* p = text; *p;) {
      uint32_t cp = decodeUtf8(p);
      if (cp < 32) cp = ' ';
      else if (cp > 126) cp = '?';
      if (cp != ' ') {
        const Glyph& g = glyph(cp, kBuiltinSize, kBuiltin);
        if (g.page >= 0) target_.drawTexture(pages_[g.page].tex, g.ax, g.ay, 8, 8, x + pen, y, cell, cell, c, false);
      }
      pen += cell;
    }
    return;
  }

  if (m == kHard) {
    int ph = pixelHeightFor(scale);
    float sc = unitScale(ph);
    int base = y + roundi((float)ascent_ * sc), pen = 0;
    for (const char* p = text; *p;) {
      uint32_t cp = decodeUtf8(p);
      const Glyph& g = glyph(cp < 32 ? ' ' : cp, ph, kHard);
      if (g.page >= 0)
        target_.drawTexture(pages_[g.page].tex, g.ax, g.ay, g.w, g.h, x + pen + g.xoff, base + g.yoff, g.w, g.h, c, false);
      pen += roundi((float)advanceUnits(cp) * sc);
    }
    return;
  }

  // Smooth: rasterize at real output resolution, place on whole output pixels.
  float os, offX, offY;
  target_.outputTransform(os, offX, offY);
  if (os < 0.05f) os = 0.05f;
  if (std::fabs(os - lastOutScale_) > 1e-4f) {  // window resized: old rasters are the wrong size
    clearCache();
    lastOutScale_ = os;
  }
  int ph = roundi((float)font_.size * scale * os);
  if (ph < 1) ph = 1;
  if (ph > kMaxRaster) ph = kMaxRaster;
  float sc = unitScale(ph);
  int topY = roundi(offY + (float)y * os);
  int baseY = topY + roundi((float)ascent_ * sc);
  float penX = offX + (float)x * os;
  long units = 0;
  for (const char* p = text; *p;) {
    uint32_t cp = decodeUtf8(p);
    const Glyph& g = glyph(cp < 32 ? ' ' : cp, ph, kSmooth);
    if (g.page >= 0) {
      int gx = roundi(penX + (float)units * sc) + g.xoff;
      target_.drawTexture(pages_[g.page].tex, g.ax, g.ay, g.w, g.h, gx, baseY + g.yoff, g.w, g.h, c, true);
    }
    units += advanceUnits(cp);
  }
}

}  // namespace ote
