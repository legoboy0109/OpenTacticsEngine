// SDL3 implementation of ote::Renderer (the only place drawing touches SDL).
// Text is drawn by the shared TextEngine through the TextTarget below.
#pragma once
#include <SDL3/SDL.h>

#include <memory>
#include <string>
#include <vector>

#include "platform/common/pixel_snap.h"
#include "platform/common/text_engine.h"
#include "platform/platform.h"

namespace ote {

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
  void setIntegerScale(bool on) override;
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

  SDL_Renderer* handle() const { return r_; }
  // Saves what the player sees (the whole output, including letterbox bars)
  // as BMP. Call after drawing, before present.
  bool saveScreenshot(const std::string& path);

 private:
  class Target;  // TextTarget on top of SDL textures
  void setColor(Color c);
  void applyPresentation();  // letterbox or whole-number scaling, by the setting and the output size
  void refreshMap();  // re-reads the logical -> output mapping (pixel_snap.h); once per frame and when the logical size changes
  SDL_Window* window_;
  SDL_Renderer* r_;
  int w_, h_;
  bool headless_;
  int headlessScale_;
  bool integerScale_ = true;  // setIntegerScale (whole_scale.h)
  SDL_RendererLogicalPresentation mode_ = SDL_LOGICAL_PRESENTATION_LETTERBOX;  // what is set on the SDL renderer now (sdl_app.cpp starts with the letterbox)
  int presentedW_ = 0, presentedH_ = 0;  // the logical size that mode_ was set for (0 = not yet: the first call sets it)
  OutputMap map_;  // where the logical frame sits in the output, for placing rectangles on whole device pixels
  bool additive_ = false;  // setAdditiveBlend: the sheets are drawn with SDL_BLENDMODE_ADD
  std::vector<SDL_Texture*> images_;  // TextureId n lives at images_[n - 1]; null = free slot
  std::unique_ptr<Target> target_;
  std::unique_ptr<TextEngine> text_;
};

}  // namespace ote
