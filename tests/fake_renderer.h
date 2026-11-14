// A renderer that draws nothing and measures text as 6 x 8 pixels per character, for tests of UI
// logic that needs text widths (word-wrap, dialogue box layout) without a window.
#pragma once
#include "platform/platform.h"

namespace ote_test {

// Every character is 6 pixels wide and 8 high; nothing is drawn.
class FakeRenderer : public ote::Renderer {
 public:
  int width() const override { return 480; }
  int height() const override { return 270; }
  void displaySize(int& w, int& h) const override { w = 480; h = 270; }
  void clear(ote::Color) override {}
  void fillRect(int, int, int, int, ote::Color) override {}
  void drawRect(int, int, int, int, ote::Color) override {}
  void setLogicalSize(int, int) override {}
  bool setFont(const ote::FontRequest&) override { return true; }
  void drawText(int, int, const char*, ote::Color, int) override {}
  int textWidth(const char* text, int scale) const override {
    int n = 0;
    for (const char* p = text; *p; ++p) n += ((unsigned char)*p & 0xC0) != 0x80;
    return n * 6 * scale;
  }
  int lineHeight(int scale) const override { return 8 * scale; }
  ote::TextureId createTexture(const uint8_t*, int, int, bool) override { return 0; }
  void destroyTexture(ote::TextureId) override {}
  void updateTexture(ote::TextureId, const uint8_t*) override {}
  void drawTexture(ote::TextureId, int, int, int, int, int, int, int, int, ote::Color) override {}
};

}  // namespace ote_test
