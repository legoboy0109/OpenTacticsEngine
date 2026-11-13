// Backend-independent text engine: UTF-8, measuring, TrueType rasterization
// (stb_truetype) and the built-in 8x8 pixel font, glyph atlas caching.
// No SDL here. A backend only supplies a TextTarget (below), which a console
// port can implement in about fifty lines: a handful of texture calls and
// the logical->output transform.
//
// Rendering model
//  * Glyphs live in RGBA atlas pages: RGB is always white, A is coverage.
//    The backend tints a sub-rectangle with a Color (colour mod + alpha mod).
//  * Built-in pixel font and smooth=false fonts are drawn in LOGICAL pixels
//    (the backend scales them with nearest filtering like everything else).
//  * smooth=true fonts are rasterized at the display's real resolution
//    (size * scale * output scale) and drawn 1:1 in OUTPUT pixels, so they
//    are sharp at any window size. Layout metrics (textWidth/lineHeight) are
//    always in logical pixels and do not depend on the window size.
#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "platform/platform.h"

struct stbtt_fontinfo;

namespace ote {

class TextTarget {
 public:
  virtual ~TextTarget() {}
  // Creates an RGBA texture (bytes in memory order R,G,B,A), blended,
  // nearest-filtered, initially transparent. Returns a handle >= 0 or -1.
  virtual int createTexture(int w, int h) = 0;
  // Overwrites a sub-rectangle with RGBA pixels (`pitch` bytes per row).
  virtual void updateTexture(int tex, int x, int y, int w, int h, const uint8_t* rgba, int pitch) = 0;
  virtual void destroyTexture(int tex) = 0;
  // Draws src (sx,sy,sw,sh) of the texture into dst (dx,dy,dw,dh), scaled to
  // fit, multiplied by `tint` (rgb and alpha). The dst rect is in logical
  // pixels, or - when outputSpace - in real output (window) pixels, ignoring
  // the logical->output transform and letterboxing.
  virtual void drawTexture(int tex, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh,
                           Color tint, bool outputSpace) = 0;
  // Current mapping logical->output: out = logical * scale + offset.
  virtual void outputTransform(float& scale, float& offsetX, float& offsetY) const = 0;
};

class TextEngine {
 public:
  explicit TextEngine(TextTarget& target);
  ~TextEngine();
  TextEngine(const TextEngine&) = delete;
  TextEngine& operator=(const TextEngine&) = delete;

  // See Renderer::setFont. Always leaves a usable font (built-in on failure).
  bool setFont(const FontRequest& font);
  void drawText(int x, int y, const char* text, Color color, int scale);
  int textWidth(const char* text, int scale) const;
  int lineHeight(int scale) const;

  // Drops every atlas page (call when the target's textures became invalid,
  // e.g. after a graphics-device reset).
  void clearCache();

 private:
  struct Glyph {
    int page = -1;           // -1: nothing to draw (space, empty outline)
    int ax = 0, ay = 0;      // position in the atlas page
    int w = 0, h = 0;        // size in pixels
    int xoff = 0, yoff = 0;  // top-left relative to pen / baseline (built-in font: to the line top)
  };
  struct Page {
    int tex = -1;
    int cx = 0, cy = 0, rowH = 0;  // shelf packer state
  };
  enum Mode { kBuiltin = 0, kHard = 1, kSmooth = 2 };

  Mode mode() const { return !ttf_ ? kBuiltin : font_.smooth ? kSmooth : kHard; }
  const Glyph& glyph(uint32_t cp, int pixelHeight, Mode m);
  Glyph rasterize(uint32_t cp, int pixelHeight, Mode m);
  bool place(int w, int h, Glyph& g);  // finds atlas room (may flush the cache)
  void upload(const Glyph& g, const std::vector<uint8_t>& alpha);
  int glyphIndex(uint32_t cp) const;
  int advanceUnits(uint32_t cp) const;  // TrueType advance in font units
  float unitScale(int pixelHeight) const;
  int pixelHeightFor(int scale) const;  // logical raster height for hard fonts

  TextTarget& target_;
  FontRequest font_;
  std::string data_;  // font file bytes; stb keeps pointers into it
  stbtt_fontinfo* ttf_ = nullptr;
  int ascent_ = 0, descent_ = 0;  // font units
  std::vector<Page> pages_;
  std::unordered_map<uint64_t, Glyph> cache_;
  Glyph none_;
  float lastOutScale_ = 0;
};

// Decodes one UTF-8 code point and advances `p`. Malformed input yields
// U+FFFD and consumes one byte, so it never loops or reads past the NUL.
uint32_t decodeUtf8(const char*& p);

}  // namespace ote
