#include "platform/sdl3/sdl_renderer.h"

#include <cmath>

#include "core/base/log.h"
#include "platform/common/whole_scale.h"

namespace ote {

// Atlas textures for the text engine. Handles index into tex_.
class SdlRenderer::Target : public TextTarget {
 public:
  Target(SDL_Renderer* r, int* lw, const OutputMap* map) : r_(r), lw_(lw), map_(map) {}
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
    if (!outputSpace) snapRect(*map_, dx, dy, dw, dh, dst.x, dst.y, dst.w, dst.h);  // whole device pixels (pixel_snap.h)
    if (outputSpace) {  // real window pixels: step out of the logical presentation for this one draw
      int plw = 0, plh = 0;
      SDL_RendererLogicalPresentation mode = SDL_LOGICAL_PRESENTATION_LETTERBOX;  // (letterbox or integer scale: put back what was)
      SDL_GetRenderLogicalPresentation(r_, &plw, &plh, &mode);
      SDL_SetRenderLogicalPresentation(r_, 0, 0, SDL_LOGICAL_PRESENTATION_DISABLED);
      SDL_RenderTexture(r_, t, &src, &dst);
      SDL_SetRenderLogicalPresentation(r_, plw, plh, mode);
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
  const OutputMap* map_;
  std::vector<SDL_Texture*> tex_;
};

SdlRenderer::SdlRenderer(SDL_Window* window, SDL_Renderer* renderer, int w, int h, bool headless, int headlessScale)
    : window_(window), r_(renderer), w_(w), h_(h), headless_(headless), headlessScale_(headlessScale) {
  target_.reset(new Target(r_, &w_, &map_));
  text_.reset(new TextEngine(*target_));
}

SdlRenderer::~SdlRenderer() {
  for (SDL_Texture* t : images_)
    if (t) SDL_DestroyTexture(t);
  text_.reset();  // frees atlas textures through target_, before the SDL renderer goes away
  target_.reset();
}

// Whole-number scaling (platform/common/whole_scale.h): SDL's INTEGER_SCALE mode when the output holds at least 1x the logical
// size, else the letterbox (SDL's own integer mode would clamp to 1x and cut the picture off). The mode follows the output size,
// so this runs every frame (clear()) and whenever the setting or the logical size changes. Headless output is a whole multiple
// of the logical size by construction: it stays on the letterbox, screenshots unchanged.
void SdlRenderer::applyPresentation() {
  SDL_RendererLogicalPresentation want = SDL_LOGICAL_PRESENTATION_LETTERBOX;
  int ow = 0, oh = 0;
  if (integerScale_ && !headless_ && SDL_GetRenderOutputSize(r_, &ow, &oh) && wholeScaleRect(w_, h_, ow, oh).k >= 1)
    want = SDL_LOGICAL_PRESENTATION_INTEGER_SCALE;
  if (want == mode_ && presentedW_ == w_ && presentedH_ == h_) return;
  if (!SDL_SetRenderLogicalPresentation(r_, w_, h_, want)) OTE_WARN("logical presentation failed: %s", SDL_GetError());
  mode_ = want;
  presentedW_ = w_;
  presentedH_ = h_;
}

void SdlRenderer::setIntegerScale(bool on) {
  if (on == integerScale_) return;
  integerScale_ = on;
  applyPresentation();
  refreshMap();
}

void SdlRenderer::refreshMap() {
  applyPresentation();  // the output may have changed size since the last frame
  SDL_FRect rc;
  if (SDL_GetRenderLogicalPresentationRect(r_, &rc) && w_ > 0 && h_ > 0 && rc.w > 0 && rc.h > 0) {
    // SDL3 (SDL_render.c, UpdateLogicalPresentation): the scale is the rect's size over the logical size on each axis, and the
    // viewport, in which every vertex is placed, starts at the FLOORED offset (a letterbox offset of .5 is not applied).
    map_.scaleX = rc.w / (float)w_;
    map_.scaleY = rc.h / (float)h_;
    map_.offX = std::floor(rc.x);
    map_.offY = std::floor(rc.y);
    map_.known = true;
    // SDL's software renderer (a browser that gave no WebGL) truncates the scaled rectangle numbers to ints, so its snapped
    // rectangles need the nudge (pixel_snap.h); the GPU renderers place real floats and must not get one.
    const char* name = SDL_GetRendererName(r_);
    map_.nudge = name && SDL_strcmp(name, "software") == 0 ? 1.0f / 32 : 0.0f;
  } else {
    map_ = OutputMap();
  }
}

void SdlRenderer::setLogicalSize(int width, int height) {
  if (width < 1 || height < 1) return;
  if (width == w_ && height == h_) return;
  w_ = width;
  h_ = height;
  if (headless_) {  // keep screenshots 1:1 with the logical frame
    SDL_SetWindowSize(window_, w_ * headlessScale_, h_ * headlessScale_);
    SDL_SyncWindow(window_);
  }
  refreshMap();  // sets the presentation for the new logical size
  OTE_INFO("logical size %dx%d", w_, h_);
}

void SdlRenderer::displaySize(int& width, int& height) const {
  SDL_FRect rc;  // the letterboxed rectangle, in output pixels
  if (SDL_GetRenderLogicalPresentationRect(r_, &rc) && rc.w >= 1 && rc.h >= 1) {
    width = (int)(rc.w + 0.5f);
    height = (int)(rc.h + 0.5f);
  } else {
    width = w_;
    height = h_;
  }
}

TextureId SdlRenderer::createTexture(const uint8_t* rgba, int width, int height, bool smooth) {
  if (!rgba || width < 1 || height < 1) return 0;
  SDL_Texture* t = SDL_CreateTexture(r_, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STATIC, width, height);
  if (!t) {
    OTE_ERROR("createTexture(%dx%d) failed: %s", width, height, SDL_GetError());
    return 0;
  }
  SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
  SDL_SetTextureScaleMode(t, smooth ? SDL_SCALEMODE_LINEAR : SDL_SCALEMODE_NEAREST);
  if (!SDL_UpdateTexture(t, nullptr, rgba, width * 4)) {
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
  float fw = 0, fh = 0;
  SDL_GetTextureSize(t, &fw, &fh);
  int w = (int)fw;
  if (!SDL_UpdateTexture(t, nullptr, rgba, w * 4)) OTE_ERROR("updateTexture: cannot upload pixels: %s", SDL_GetError());
}

void SdlRenderer::drawTexture(TextureId id, int sx, int sy, int sw, int sh, int dx, int dy, int dw, int dh,
                              Color mod) {
  if (id < 1 || (size_t)id > images_.size() || !images_[(size_t)id - 1] || sw <= 0 || sh <= 0 || dw <= 0 || dh <= 0) return;
  SDL_Texture* t = images_[(size_t)id - 1];
  SDL_SetTextureBlendMode(t, additive_ ? SDL_BLENDMODE_ADD : SDL_BLENDMODE_BLEND);  // (a texture keeps its mode: set it on every draw)
  SDL_SetTextureColorMod(t, mod.r, mod.g, mod.b);
  SDL_SetTextureAlphaMod(t, mod.a);
  SDL_FRect src = {(float)sx, (float)sy, (float)sw, (float)sh};
  SDL_FRect dst;
  snapRect(map_, dx, dy, dw, dh, dst.x, dst.y, dst.w, dst.h);  // whole device pixels (pixel_snap.h)
  SDL_RenderTexture(r_, t, &src, &dst);
}

void SdlRenderer::setColor(Color c) { SDL_SetRenderDrawColor(r_, c.r, c.g, c.b, c.a); }

void SdlRenderer::clear(Color c) {
  refreshMap();  // the window may have been resized since the last frame
  setColor(c);
  SDL_RenderClear(r_);
}

void SdlRenderer::fillRect(int x, int y, int w, int h, Color c) {
  if (w <= 0 || h <= 0) return;
  setColor(c);
  SDL_FRect rect;
  snapRect(map_, x, y, w, h, rect.x, rect.y, rect.w, rect.h);
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
