#include "platform/sdl3/sdl_renderer.h"

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
    SDL_Texture* t = SDL_CreateTexture(r_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, w, h);
    if (!t) {
      OTE_ERROR("SDL_CreateTexture(%dx%d) failed: %s", w, h, SDL_GetError());
      return -1;
    }
    SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(t, SDL_SCALEMODE_NEAREST);
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
    SDL_FRect src = {(float)sx, (float)sy, (float)sw, (float)sh};
    SDL_FRect dst = {(float)dx, (float)dy, (float)dw, (float)dh};
    if (outputSpace) {  // real window pixels: step out of the logical presentation for this one draw
      SDL_SetRenderLogicalPresentation(r_, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
      SDL_RenderTexture(r_, t, &src, &dst);
      SDL_SetRenderLogicalPresentation(r_, *lw_, *lh_, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    } else {
      SDL_RenderTexture(r_, t, &src, &dst);
    }
  }

  void outputTransform(float& scale, float& offsetX, float& offsetY) const override {
    SDL_FRect rc;
    if (SDL_GetRenderLogicalPresentationRect(r_, &rc) && *lw_ > 0) {
      scale = rc.w / (float)*lw_;
      offsetX = rc.x;
      offsetY = rc.y;
    } else {
      scale = 1;
      offsetX = offsetY = 0;
    }
  }

 private:
  SDL_Renderer* r_;
  int* lw_;
  int* lh_;
  std::vector<SDL_Texture*> tex_;
};

SdlRenderer::SdlRenderer(SDL_Window* window, SDL_Renderer* renderer, int w, int h, bool headless)
    : window_(window), r_(renderer), w_(w), h_(h), headless_(headless) {
  target_.reset(new Target(r_, &w_, &h_));
  text_.reset(new TextEngine(*target_));
}

SdlRenderer::~SdlRenderer() {
  text_.reset();  // frees atlas textures through target_, before the SDL renderer goes away
  target_.reset();
}

void SdlRenderer::setLogicalSize(int width, int height) {
  if (width < 1 || height < 1) return;
  if (width == w_ && height == h_) return;
  w_ = width;
  h_ = height;
  if (headless_) {  // keep screenshots 1:1 with the logical frame
    SDL_SetWindowSize(window_, w_, h_);
    SDL_SyncWindow(window_);
  }
  if (!SDL_SetRenderLogicalPresentation(r_, w_, h_, SDL_LOGICAL_PRESENTATION_LETTERBOX))
    OTE_WARN("logical presentation failed: %s", SDL_GetError());
  OTE_INFO("logical size %dx%d", w_, h_);
}

void SdlRenderer::setColor(Color c) { SDL_SetRenderDrawColor(r_, c.r, c.g, c.b, c.a); }

void SdlRenderer::clear(Color c) {
  setColor(c);
  SDL_RenderClear(r_);
}

void SdlRenderer::fillRect(int x, int y, int w, int h, Color c) {
  if (w <= 0 || h <= 0) return;
  setColor(c);
  SDL_FRect rect = {(float)x, (float)y, (float)w, (float)h};
  SDL_RenderFillRect(r_, &rect);
}

void SdlRenderer::drawRect(int x, int y, int w, int h, Color c) {
  if (w <= 0 || h <= 0) return;
  // Four 1px strips: SDL_RenderRect's edge placement differs between render
  // backends, strips are pixel-exact everywhere.
  fillRect(x, y, w, 1, c);
  if (h > 1) fillRect(x, y + h - 1, w, 1, c);
  if (h > 2) {
    fillRect(x, y + 1, 1, h - 2, c);
    if (w > 1) fillRect(x + w - 1, y + 1, 1, h - 2, c);
  }
}

bool SdlRenderer::saveScreenshot(const std::string& path) {
  // A null rect reads the whole output as the player sees it. In headless
  // mode the window equals the logical size, so this is exactly the logical
  // frame; in a scaled window it is the scaled picture with letterbox bars.
  SDL_Surface* surf = SDL_RenderReadPixels(r_, nullptr);
  if (!surf) {
    OTE_ERROR("screenshot: SDL_RenderReadPixels failed: %s", SDL_GetError());
    return false;
  }
  bool ok = SDL_SaveBMP(surf, path.c_str());
  if (!ok) OTE_ERROR("screenshot: cannot write '%s': %s", path.c_str(), SDL_GetError());
  SDL_DestroySurface(surf);
  return ok;
}

}  // namespace ote
