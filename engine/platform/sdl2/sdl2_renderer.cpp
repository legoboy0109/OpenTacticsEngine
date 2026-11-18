#include "platform/sdl2/sdl2_renderer.h"

#include <cmath>

#include "core/base/log.h"

namespace ote {

// Atlas textures for the text engine. Handles index into tex_.
class SdlRenderer::Target : public TextTarget {
 public:
  Target(SDL_Renderer* r, int* lw, int* lh) : r_(r), lw_(lw), lh_(lh) {}
  ~Target() override {
    for (SDL_Texture* t : tex_)
      if (t) SDL_DestroyTexture(t);
  }

  int createTexture(int w, int h) override {
    // SDL_PIXELFORMAT_RGBA32 (bytes R,G,B,A in memory on any endianness) is 2.0.5+.
    SDL_Texture* t = SDL_CreateTexture(r_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
    if (!t) {
      OTE_ERROR("SDL_CreateTexture(%dx%d) failed: %s", w, h, SDL_GetError());
      return -1;
    }
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    std::vector<uint8_t> zero((size_t)w * h * 4, 0);  // new textures are undefined; start transparent
    SDL_UpdateTexture(t, nullptr, zero.data(), w * 4);
    for (size_t i = 0; i < tex_.size(); ++i)
      if (!tex_[i]) { tex_[i] = t; return (int)i; }
    tex_.push_back(t);
    return (int)tex_.size() - 1;
  }

  void updateTexture(int tex, int x, int y, int w, int h, const uint8_t* rgba, int pitch) override {
    SDL_Rect rc = {x, y, w, h};
    SDL_UpdateTexture(tex_[tex], &rc, rgba, pitch);
  }

  void destroyTexture(int tex) override {
    SDL_DestroyTexture(tex_[tex]);
    tex_[tex] = nullptr;
  }

  void drawTexture(int tex, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh, Color tint,
                   bool outputSpace) override {
    SDL_Texture* t = tex_[tex];
    SDL_SetTextureColorMod(t, tint.r, tint.g, tint.b);
    SDL_SetTextureAlphaMod(t, tint.a);
    SDL_Rect src = {sx, sy, sw, sh};
    SDL_Rect dst = {dx, dy, dw, dh};
    if (outputSpace) {  // real window pixels: leave the logical mode for this one draw
      SDL_RenderSetLogicalSize(r_, 0, 0);
      SDL_RenderCopy(r_, t, &src, &dst);
      SDL_RenderSetLogicalSize(r_, *lw_, *lh_);
    } else {
      SDL_RenderCopy(r_, t, &src, &dst);
    }
  }

  void outputTransform(float& scale, float& offsetX, float& offsetY) const override {
    int ow = 0, oh = 0;
    SDL_GetRendererOutputSize(r_, &ow, &oh);
    if (*lw_ <= 0 || *lh_ <= 0 || ow <= 0 || oh <= 0) {
      scale = 1;
      offsetX = offsetY = 0;
      return;
    }
    // SDL2's letterbox: uniform scale, content centred (integer-scale mode is off by default).
    float sx = (float)ow / (float)*lw_, sy = (float)oh / (float)*lh_;
    scale = sx < sy ? sx : sy;
    offsetX = std::floor(((float)ow - (float)*lw_ * scale) / 2.0f);
    offsetY = std::floor(((float)oh - (float)*lh_ * scale) / 2.0f);
  }

 private:
  SDL_Renderer* r_;
  int* lw_;
  int* lh_;
  std::vector<SDL_Texture*> tex_;
};

SdlRenderer::SdlRenderer(SDL_Window* window, SDL_Renderer* renderer, int w, int h, bool headless, int headlessScale)
    : window_(window), r_(renderer), w_(w), h_(h), headless_(headless), headlessScale_(headlessScale) {
  target_.reset(new Target(r_, &w_, &h_));
  text_.reset(new TextEngine(*target_));
}

SdlRenderer::~SdlRenderer() {
  for (SDL_Texture* t : images_)
    if (t) SDL_DestroyTexture(t);
  text_.reset();  // frees atlas textures through target_, before the SDL renderer goes away
  target_.reset();
}

void SdlRenderer::setLogicalSize(int width, int height) {
  if (width < 1 || height < 1) return;
  if (width == w_ && height == h_) return;
  w_ = width;
  h_ = height;
  if (headless_) {  // keep screenshots 1:1 with the logical frame
    SDL_SetWindowSize(window_, w_ * headlessScale_, h_ * headlessScale_);
    SDL_PumpEvents();  // let the window system apply the new size before the next draw
  }
  if (SDL_RenderSetLogicalSize(r_, w_, h_) != 0) OTE_WARN("logical size failed: %s", SDL_GetError());
  OTE_INFO("logical size %dx%d", w_, h_);
}

void SdlRenderer::displaySize(int& width, int& height) const {
  int ow = 0, oh = 0;
  SDL_GetRendererOutputSize(r_, &ow, &oh);
  if (ow < 1 || oh < 1 || w_ < 1 || h_ < 1) {
    width = w_;
    height = h_;
    return;
  }
  // The letterboxed rectangle: uniform scale (as in Target::outputTransform).
  float sx = (float)ow / (float)w_, sy = (float)oh / (float)h_, k = sx < sy ? sx : sy;
  width = (int)((float)w_ * k + 0.5f);
  height = (int)((float)h_ * k + 0.5f);
}

TextureId SdlRenderer::createTexture(const uint8_t* rgba, int width, int height, bool smooth) {
  if (!rgba || width < 1 || height < 1) return 0;
  // SDL2 fixes a texture's scaling when it is made, from this hint (the app keeps it at "0", nearest).
  if (smooth) SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
  SDL_Texture* t = SDL_CreateTexture(r_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
  if (smooth) SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  if (!t) {
    OTE_ERROR("createTexture(%dx%d) failed: %s", width, height, SDL_GetError());
    return 0;
  }
  SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
  if (SDL_UpdateTexture(t, nullptr, rgba, width * 4) != 0) {
    OTE_ERROR("createTexture: cannot upload pixels: %s", SDL_GetError());
    SDL_DestroyTexture(t);
    return 0;
  }
  for (size_t i = 0; i < images_.size(); ++i)
    if (!images_[i]) { images_[i] = t; return (TextureId)(i + 1); }
  images_.push_back(t);
  return (TextureId)images_.size();
}

void SdlRenderer::destroyTexture(TextureId id) {
  if (id < 1 || (size_t)id > images_.size() || !images_[(size_t)id - 1]) return;
  SDL_DestroyTexture(images_[(size_t)id - 1]);
  images_[(size_t)id - 1] = nullptr;
}

void SdlRenderer::updateTexture(TextureId id, const uint8_t* rgba) {
  if (!rgba || id < 1 || (size_t)id > images_.size() || !images_[(size_t)id - 1]) return;
  SDL_Texture* t = images_[(size_t)id - 1];
  int w = 0, h = 0;
  SDL_QueryTexture(t, nullptr, nullptr, &w, &h);
  if (SDL_UpdateTexture(t, nullptr, rgba, w * 4) != 0) OTE_ERROR("updateTexture: cannot upload pixels: %s", SDL_GetError());
}

void SdlRenderer::drawTexture(TextureId id, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh,
                              Color mod) {
  if (id < 1 || (size_t)id > images_.size() || !images_[(size_t)id - 1] || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
  SDL_Texture* t = images_[(size_t)id - 1];
  SDL_SetTextureBlendMode(t, additive_ ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);  // (a texture keeps its mode: set it on every draw)
  SDL_SetTextureColorMod(t, mod.r, mod.g, mod.b);
  SDL_SetTextureAlphaMod(t, mod.a);
  SDL_Rect src = {sx, sy, sw, sh};
  SDL_Rect dst = {dx, dy, dw, dh};
  SDL_RenderCopy(r_, t, &src, &dst);
}

void SdlRenderer::setColor(Color c) { SDL_SetRenderDrawColor(r_, c.r, c.g, c.b, c.a); }

void SdlRenderer::clear(Color c) {
  setColor(c);
  SDL_RenderClear(r_);
}

void SdlRenderer::fillRect(int x, int y, int w, int h, Color c) {
  if (w <= 0 || h <= 0) return;
  setColor(c);
  SDL_Rect rect = {x, y, w, h};
  SDL_RenderFillRect(r_, &rect);
}

void SdlRenderer::drawRect(int x, int y, int w, int h, Color c) {
  if (w <= 0 || h <= 0) return;
  // Four 1px strips, pixel-exact on every render driver (same as the SDL3 backend).
  fillRect(x, y, w, 1, c);
  if (h > 1) fillRect(x, y + h - 1, w, 1, c);
  if (h > 2) {
    fillRect(x, y + 1, 1, h - 2, c);
    if (w > 1) fillRect(x + w - 1, y + 1, 1, h - 2, c);
  }
}

bool SdlRenderer::saveScreenshot(const std::string& path) {
  // SDL2 reads only the current viewport (= the letterboxed content area with
  // a logical size set), so leave the logical mode to capture the whole
  // output like the SDL3 backend does (bars included).
  SDL_RenderSetLogicalSize(r_, 0, 0);
  int ow = 0, oh = 0;
  SDL_GetRendererOutputSize(r_, &ow, &oh);
  bool ok = false;
  std::vector<uint8_t> pixels((size_t)ow * oh * 4);
  if (SDL_RenderReadPixels(r_, nullptr, SDL_PIXELFORMAT_ARGB8888, pixels.data(), ow * 4) != 0) {
    OTE_ERROR("screenshot: SDL_RenderReadPixels failed: %s", SDL_GetError());
  } else {
    // SDL_CreateRGBSurfaceWithFormatFrom is 2.0.5+.
    SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormatFrom(pixels.data(), ow, oh, 32, ow * 4, SDL_PIXELFORMAT_ARGB8888);
    if (!surf) {
      OTE_ERROR("screenshot: cannot wrap pixels: %s", SDL_GetError());
    } else {
      ok = SDL_SaveBMP(surf, path.c_str()) == 0;
      if (!ok) OTE_ERROR("screenshot: cannot write '%s': %s", path.c_str(), SDL_GetError());
      SDL_FreeSurface(surf);
    }
  }
  SDL_RenderSetLogicalSize(r_, w_, h_);
  return ok;
}

}  // namespace ote
