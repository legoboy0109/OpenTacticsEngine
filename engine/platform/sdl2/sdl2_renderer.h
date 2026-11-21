// SDL2 implementation of ote::Renderer. Plain SDL 2.0.x calls only (see the
// notes at the few places that need something newer than ~2.0.10).
// Text is drawn by the shared TextEngine through the TextTarget in the .cpp.
#pragma once
#include <SDL.h>

#include <memory>
#include <string>
#include <vector>

#include "platform/common/pixel_snap.h"
#include "platform/common/screen_fit.h"
#include "platform/common/text_engine.h"
#include "platform/platform.h"

namespace ote {

// A display with a fixed size and a TV's overscan (the original Xbox): instead of SDL's letterbox the logical screen goes
// into a centred rectangle inside the title-safe area (platform/common/screen_fit.h).
struct FixedScreen {
  int w = 640, h = 480;
  bool anamorphic = false;      // 480 mode shown as 16:9: pixels 4/3 wider than high
  bool highDefinition = false;  // picks the default safe-area margin
};

class SdlRenderer : public Renderer {
 public:
  // `headless`: the window always follows the logical size times `headlessScale`, so screenshots stay 1:1
  // with the (scaled) logical frame.
  SdlRenderer(SDL_Window* window, SDL_Renderer* renderer, int w, int h, bool headless, int headlessScale);
  ~SdlRenderer() override;
  int width() const override { return w_; }
  int height() const override { return h_; }
  void displaySize(int& width, int& height) const override;
  void clear(Color c) override;
  void fillRect(int x, int y, int w, int h, Color c) override;
  void drawRect(int x, int y, int w, int h, Color c) override;
  void setLogicalSize(int width, int height) override;
  void setSafeArea(int percent) override;
  bool setFont(const FontRequest& font) override { return text_->setFont(font); }
  void drawText(int x, int y, const char* text, Color c, int scale = 1) override {
    text_->drawText(x, y, text, c, scale);
  }
  int textWidth(const char* text, int scale = 1) const override { return text_->textWidth(text, scale); }
  int lineHeight(int scale = 1) const override { return text_->lineHeight(scale); }

  TextureId createTexture(const uint8_t* rgba, int width, int height, bool smooth = false) override;
  void destroyTexture(TextureId id) override;
  void updateTexture(TextureId id, const uint8_t* rgba) override;
  void drawTexture(TextureId id, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh,
                   Color mod = Color(255, 255, 255)) override;
  bool setAdditiveBlend(bool on) override {
    additive_ = on;
    return true;
  }

  // Switches to the fixed-screen placement (no-op for desktop windows, which never call it).
  void setFixedScreen(const FixedScreen& screen);

  SDL_Renderer* handle() const { return r_; }
  // Saves what the player sees (the whole output, including letterbox bars)
  // as BMP. Call after drawing, before present.
  bool saveScreenshot(const std::string& path);

 private:
  class Target;  // TextTarget on top of SDL textures
  void setColor(Color c);
  void refreshMap();         // desktop windows: where SDL puts the logical frame in the output (pixel_snap.h); once per frame
  // One textured / filled rectangle of the logical frame, on whole device pixels where SDL can take fractional coordinates.
  void copyLogical(SDL_Texture* t, const SDL_Rect& src, const SDL_Rect& dst);
  void fillLogical(const SDL_Rect& rect);
  void applyLogical();       // SDL_RenderSetLogicalSize, or the fixed-screen rectangle
  void enterOutputSpace();   // fixed screen only: draw in screen pixels until leaveOutputSpace()
  void leaveOutputSpace();
#ifdef OTE_NXDK
  double pixelsOf(int w, int h, bool outputSpace) const;  // frame profile: destination pixels of a draw (nxdk_prof.h)
  bool chipScaleOn() const { return chipScale_; }
  static int chipFactor();
  bool chipSmooth() const;
#endif
  SDL_Window* window_;
  SDL_Renderer* r_;
  int w_, h_;
  bool headless_;
  int headlessScale_;
  bool fixed_ = false;  // fixed-screen placement (setFixedScreen)
  FixedScreen screen_;
  ScreenFit fit_;
  int safePercent_ = 0;  // 0 = the default for the kind of display
  bool outputMode_ = false;
  OutputMap map_;  // unknown (rectangles unchanged) with a fixed screen and before the first frame
#ifdef OTE_NXDK
  // Chip scaling (docs/XBOX_OG.md "Picture"): the 2D layer is drawn at the game's own size times `canvasFactor_` (a whole number,
  // 1 by default) in the corner of the surface, and the graphics chip stretches it over the fitted rectangle `fit_`.
  bool chipScale_ = false;
  int canvasFactor_ = 1;
  SDL_Rect canvasViewport_ = {0, 0, 0, 0};
#endif
  bool additive_ = false;  // setAdditiveBlend: the sheets are drawn with SDL_BLENDMODE_ADD
  std::vector<uint32_t> uploadScratch_;  // converted pixels while uploading (sdl2_pixels.h); unused with the default format
  std::vector<SDL_Texture*> images_;  // TextureId n lives at images_[n - 1]; null = free slot
  std::unique_ptr<Target> target_;
  std::unique_ptr<TextEngine> text_;
};

}  // namespace ote
