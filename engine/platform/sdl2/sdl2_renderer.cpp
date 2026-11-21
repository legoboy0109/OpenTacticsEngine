#include "platform/sdl2/sdl2_renderer.h"

#include <cmath>
#include <cstdlib>
#include <cstring>

#include "core/base/fileio.h"
#include "core/base/log.h"
#include "platform/sdl2/sdl2_pixels.h"
#ifdef OTE_NXDK
#include "platform/nxdk/nxdk_present.h"
#include "platform/nxdk/nxdk_prof.h"
#endif

#ifdef OTE_NXDK
// A timed span of SDL's CPU renderer, with the pixels it writes (docs/XBOX_OG.md "Speed").
#define OTE_PROF_SDL(kind, pixels)                                \
  prof::Span profSpan(prof::kSdl2d);                              \
  prof::Span profKind(kind);                                      \
  prof::count(prof::kSdlPixels, (pixels))
#else
#define OTE_PROF_SDL(kind, pixels) ((void)0)
#endif

namespace ote {

// Atlas textures for the text engine. Handles index into tex_.
class SdlRenderer::Target : public TextTarget {
 public:
  Target(SdlRenderer* owner, SDL_Renderer* r, int* lw, int* lh) : owner_(owner), r_(r), lw_(lw), lh_(lh) {}
  ~Target() override {
    for (SDL_Texture* t : tex_)
      if (t) SDL_DestroyTexture(t);
  }

  int createTexture(int w, int h) override {
    SDL_Texture* t = SDL_CreateTexture(r_, kUploadFormat, SDL_TEXTUREACCESS_STATIC, w, h);  // see sdl2_pixels.h
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
    int outPitch = pitch;
    const void* px = uploadPixels(rgba, w, h, pitch, scratch_, outPitch);
    SDL_UpdateTexture(tex_[tex], &rc, px, outPitch);
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
    OTE_PROF_SDL(prof::kSdlGlyph, owner_->pixelsOf(dw, dh, outputSpace));
    if (owner_->fixed_) {
      // Fixed screen: switching SDL's viewport per glyph would flush its command queue every time, so the screen-pixel mode
      // stays on across a run of glyphs and is left by the next ordinary draw (SdlRenderer::leaveOutputSpace callers).
      if (outputSpace) {
        owner_->enterOutputSpace();
#ifdef OTE_NXDK
        const bool chipStretches = owner_->chipScale_;  // the chip stretches anamorphically itself
#else
        const bool chipStretches = false;
#endif
        if (!chipStretches && owner_->fit_.kx != owner_->fit_.ky) {  // anamorphic: squeeze the glyph (laid out at the y scale) to the x scale
          const float r = owner_->fit_.kx / owner_->fit_.ky;
          dst.x = owner_->fit_.x + (int)std::floor((float)(dx - owner_->fit_.x) * r + 0.5f);
          dst.w = dw * r < 1.0f ? 1 : (int)std::floor((float)dw * r + 0.5f);
        }
      } else {
        owner_->leaveOutputSpace();
      }
      SDL_RenderCopy(r_, t, &src, &dst);
    } else if (outputSpace) {  // real window pixels: leave the logical mode for this one draw
      SDL_RenderSetLogicalSize(r_, 0, 0);
      SDL_RenderCopy(r_, t, &src, &dst);
      SDL_RenderSetLogicalSize(r_, *lw_, *lh_);
    } else {
      owner_->copyLogical(t, src, dst);
    }
  }

  void outputTransform(float& scale, float& offsetX, float& offsetY) const override {
    if (owner_->fixed_) {  // the canvas rectangle; text is laid out at the vertical scale (see drawTexture)
#ifdef OTE_NXDK
      if (owner_->chipScale_) {  // the canvas is drawn at the game's size times a whole number; the chip does the rest
        scale = (float)owner_->canvasFactor_;
        offsetX = offsetY = 0;
        return;
      }
#endif
      scale = owner_->fit_.ky;
      offsetX = (float)owner_->fit_.x;
      offsetY = (float)owner_->fit_.y;
      return;
    }
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
  SdlRenderer* owner_;
  SDL_Renderer* r_;
  int* lw_;
  int* lh_;
  std::vector<SDL_Texture*> tex_;
  std::vector<uint32_t> scratch_;  // converted pixels while uploading (sdl2_pixels.h)
};

SdlRenderer::SdlRenderer(SDL_Window* window, SDL_Renderer* renderer, int w, int h, bool headless, int headlessScale)
    : window_(window), r_(renderer), w_(w), h_(h), headless_(headless), headlessScale_(headlessScale) {
  target_.reset(new Target(this, r_, &w_, &h_));
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
  applyLogical();
  OTE_INFO("logical size %dx%d", w_, h_);
}

// SDL2's logical-size letterbox (SDL_RenderSetLogicalSize): the scale per axis is the viewport's size over the logical size (read
// back with SDL_RenderGetScale), and the viewport sits at ((output - size) / 2, whole-number division). Checked against
// SDL_RenderLogicalToWindow on SDL 2.32 for wide, tall and exact-fit windows.
void SdlRenderer::refreshMap() {
  map_ = OutputMap();
#if SDL_VERSION_ATLEAST(2, 0, 10)
  if (fixed_ || w_ < 1 || h_ < 1) return;
  float kx = 1, ky = 1;
  int ow = 0, oh = 0;
  SDL_RenderGetScale(r_, &kx, &ky);
  SDL_GetRendererOutputSize(r_, &ow, &oh);
  if (kx <= 0 || ky <= 0 || ow < 1 || oh < 1) return;
  const int vw = (int)std::floor((float)w_ * kx + 0.5f), vh = (int)std::floor((float)h_ * ky + 0.5f);
  map_.scaleX = kx;
  map_.scaleY = ky;
  map_.offX = (float)((ow - vw) / 2);
  map_.offY = (float)((oh - vh) / 2);
  map_.known = true;
#endif
}

void SdlRenderer::copyLogical(SDL_Texture* t, const SDL_Rect& src, const SDL_Rect& dst) {
#if SDL_VERSION_ATLEAST(2, 0, 10)
  if (map_.known) {
    SDL_FRect f;
    snapRect(map_, dst.x, dst.y, dst.w, dst.h, f.x, f.y, f.w, f.h);
    SDL_RenderCopyF(r_, t, &src, &f);
    return;
  }
#endif
  SDL_RenderCopy(r_, t, &src, &dst);
}

void SdlRenderer::fillLogical(const SDL_Rect& rect) {
#if SDL_VERSION_ATLEAST(2, 0, 10)
  if (map_.known) {
    SDL_FRect f;
    snapRect(map_, rect.x, rect.y, rect.w, rect.h, f.x, f.y, f.w, f.h);
    SDL_RenderFillRectF(r_, &f);
    return;
  }
#endif
  SDL_RenderFillRect(r_, &rect);
}

void SdlRenderer::applyLogical() {
  if (!fixed_) {
    if (SDL_RenderSetLogicalSize(r_, w_, h_) != 0) OTE_WARN("logical size failed: %s", SDL_GetError());
    refreshMap();
    return;
  }
  map_ = OutputMap();
  ScreenFitIn in;
  in.logicalW = w_;
  in.logicalH = h_;
  in.screenW = screen_.w;
  in.screenH = screen_.h;
  in.anamorphic = screen_.anamorphic;
  in.safePercent = safePercent_ > 0 ? safePercent_ : defaultSafePercent(screen_.highDefinition);
  fit_ = fitCanvas(in);
#ifdef OTE_NXDK
  {  // the hardware 3D picture is placed in the same rectangle (platform/nxdk/nxdk_present.h)
    NxdkFit f;
    f.x = fit_.x;
    f.y = fit_.y;
    f.w = fit_.w;
    f.h = fit_.h;
    f.kx = fit_.kx;
    f.ky = fit_.ky;
    // The chip stretches the 2D layer when it owns the screen and the canvas (at its whole-number factor) fits the surface.
    chipScale_ = nxdkChipPresents();
    canvasFactor_ = chipFactor();
    if (chipScale_ && (w_ * canvasFactor_ > screen_.w || h_ * canvasFactor_ > screen_.h)) canvasFactor_ = 1;
    if (chipScale_ && (w_ > screen_.w || h_ > screen_.h)) chipScale_ = false;  // a canvas bigger than the screen: the old way
    if (chipScale_) {
      f.canvasW = w_ * canvasFactor_;
      f.canvasH = h_ * canvasFactor_;
      f.smooth = chipSmooth();
    }
    nxdkSetFit(f);
  }
  if (chipScale_) {
    canvasViewport_ = {0, 0, w_ * canvasFactor_, h_ * canvasFactor_};
    SDL_RenderSetLogicalSize(r_, 0, 0);
    SDL_RenderSetScale(r_, 1.0f, 1.0f);
    SDL_RenderSetViewport(r_, &canvasViewport_);
    SDL_RenderSetScale(r_, (float)canvasFactor_, (float)canvasFactor_);
    outputMode_ = false;
    OTE_INFO("screen fit: %dx%d canvas drawn at x%d (%dx%d pixels), stretched by the chip to %dx%d on %dx%d at %d,%d (scale %d/1000 x %d/1000, %s, safe area %d%%)",
             w_, h_, canvasFactor_, w_ * canvasFactor_, h_ * canvasFactor_, fit_.w, fit_.h, screen_.w, screen_.h, fit_.x, fit_.y,
             (int)(fit_.kx * 1000.0f + 0.5f), (int)(fit_.ky * 1000.0f + 0.5f), chipSmooth() ? "linear" : "nearest", in.safePercent);
    return;
  }
#endif
  // SDL's own logical-size code does the same three steps: viewport in screen pixels (scale 1), then the scale.
  SDL_RenderSetLogicalSize(r_, 0, 0);
  SDL_RenderSetScale(r_, 1.0f, 1.0f);
  SDL_Rect vp = {fit_.x, fit_.y, fit_.w, fit_.h};
  SDL_RenderSetViewport(r_, &vp);
  SDL_RenderSetScale(r_, fit_.kx, fit_.ky);
  outputMode_ = false;
  OTE_INFO("screen fit: %dx%d canvas on %dx%d at %d,%d (scale %d/1000 x %d/1000, safe area %d%%)", fit_.w, fit_.h, screen_.w, screen_.h,
           fit_.x, fit_.y, (int)(fit_.kx * 1000.0f + 0.5f), (int)(fit_.ky * 1000.0f + 0.5f), in.safePercent);
}

#ifdef OTE_NXDK
// OTE_XBOX_CANVAS=N (args.txt, debug): draw the 2D layer at N times the game's size (default 1; the chip scales the rest).
int SdlRenderer::chipFactor() {
  const char* e = getenv("OTE_XBOX_CANVAS");
  const int n = e ? atoi(e) : 1;
  return n < 1 ? 1 : n > 4 ? 4 : n;
}

// OTE_XBOX_FILTER=point|linear (args.txt, debug) decides how the chip stretches the canvas; by default whole-number stretches
// stay sharp and the others are filtered linearly (docs/XBOX_OG.md "Picture").
bool SdlRenderer::chipSmooth() const {
  const char* e = getenv("OTE_XBOX_FILTER");
  if (e && strcmp(e, "point") == 0) return false;
  if (e && strcmp(e, "linear") == 0) return true;
  const float kx = fit_.kx / (float)canvasFactor_, ky = fit_.ky / (float)canvasFactor_;
  const bool whole = std::fabs(kx - std::floor(kx + 0.5f)) < 0.02f && std::fabs(ky - std::floor(ky + 0.5f)) < 0.02f;
  return !whole;
}

double SdlRenderer::pixelsOf(int w, int h, bool outputSpace) const {
  const double k = fixed_ && !outputSpace && !chipScale_ ? (double)fit_.kx * fit_.ky : (fixed_ && !outputSpace ? (double)canvasFactor_ * canvasFactor_ : 1.0);
  return (double)w * h * k;
}
#endif

void SdlRenderer::enterOutputSpace() {
  if (!fixed_ || outputMode_) return;
  SDL_RenderSetScale(r_, 1.0f, 1.0f);
#ifdef OTE_NXDK
  if (chipScale_) {  // the canvas is the output: same rectangle, scale 1
    SDL_RenderSetViewport(r_, &canvasViewport_);
    outputMode_ = true;
    return;
  }
#endif
  SDL_RenderSetViewport(r_, nullptr);
  outputMode_ = true;
}

void SdlRenderer::leaveOutputSpace() {
  if (!fixed_ || !outputMode_) return;
#ifdef OTE_NXDK
  if (chipScale_) {
    SDL_RenderSetScale(r_, (float)canvasFactor_, (float)canvasFactor_);
    outputMode_ = false;
    return;
  }
#endif
  SDL_Rect vp = {fit_.x, fit_.y, fit_.w, fit_.h};
  SDL_RenderSetViewport(r_, &vp);
  SDL_RenderSetScale(r_, fit_.kx, fit_.ky);
  outputMode_ = false;
}

void SdlRenderer::setFixedScreen(const FixedScreen& screen) {
  fixed_ = true;
  screen_ = screen;
  applyLogical();
}

void SdlRenderer::setSafeArea(int percent) {
  if (percent < 0) percent = 0;
  if (percent == safePercent_) return;
  safePercent_ = percent;
  if (fixed_) applyLogical();
}

void SdlRenderer::displaySize(int& width, int& height) const {
  if (fixed_) {
    width = fit_.w;
    height = fit_.h;
    return;
  }
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
  SDL_Texture* t = SDL_CreateTexture(r_, kUploadFormat, SDL_TEXTUREACCESS_STATIC, width, height);
  if (smooth) SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "0");
  if (!t) {
    OTE_ERROR("createTexture(%dx%d) failed: %s", width, height, SDL_GetError());
    return 0;
  }
  SDL_SetTextureBlendMode(t, SDL_BLENDMODE_BLEND);
  int outPitch = width * 4;
  const void* px = uploadPixels(rgba, width, height, width * 4, uploadScratch_, outPitch);
  if (SDL_UpdateTexture(t, nullptr, px, outPitch) != 0) {
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
  int outPitch = w * 4;
  const void* px = uploadPixels(rgba, w, h, w * 4, uploadScratch_, outPitch);
  if (SDL_UpdateTexture(t, nullptr, px, outPitch) != 0) OTE_ERROR("updateTexture: cannot upload pixels: %s", SDL_GetError());
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
  OTE_PROF_SDL(prof::kSdlCopy, pixelsOf(dw, dh, false));
  leaveOutputSpace();
  copyLogical(t, src, dst);
}

void SdlRenderer::setColor(Color c) {
  SDL_SetRenderDrawColor(r_, c.r, c.g, c.b, c.a);
#ifdef OTE_NXDK
  // SDL's CPU renderer blends every fill pixel by pixel while the blend mode is on, also an opaque one; a plain fill is several
  // times cheaper and is what most of a frame is (docs/XBOX_OG.md "Memory and speed").
  SDL_SetRenderDrawBlendMode(r_, c.a == 255 ? SDL_BLENDMODE_NONE : SDL_BLENDMODE_BLEND);
#endif
}

void SdlRenderer::clear(Color c) {
  OTE_PROF_SDL(prof::kSdlFill, pixelsOf(w_, h_, false) + (fixed_ && !chipScaleOn() ? (double)screen_.w * screen_.h : 0.0));
#ifdef OTE_NXDK
  if (chipScale_) {  // the margin is the chip's business (it blacks it out before the first layer); only the canvas is drawn here
    leaveOutputSpace();
    setColor(c);
    SDL_RenderFillRect(r_, nullptr);
    return;
  }
#endif
  if (fixed_) {
    // The margin around the canvas stays black (SDL's clear may or may not respect the viewport); the canvas gets the colour.
    enterOutputSpace();
    SDL_SetRenderDrawColor(r_, 0, 0, 0, 255);
    SDL_RenderClear(r_);
    leaveOutputSpace();
    setColor(c);
    SDL_RenderFillRect(r_, nullptr);  // the whole viewport = the canvas
    return;
  }
  refreshMap();  // the window may have been resized since the last frame
  setColor(c);
  SDL_RenderClear(r_);
}

void SdlRenderer::fillRect(int x, int y, int w, int h, Color c) {
  if (w <= 0 || h <= 0) return;
  OTE_PROF_SDL(prof::kSdlFill, pixelsOf(w, h, false));
  leaveOutputSpace();
  setColor(c);
  SDL_Rect rect = {x, y, w, h};
  fillLogical(rect);
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
  if (fixed_) enterOutputSpace();
  else SDL_RenderSetLogicalSize(r_, 0, 0);
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
#ifdef OTE_NXDK
      // SDL's own file open passes the path to fopen as it is; the Xbox kernel needs backslashes (openFile converts).
      FILE* fp = openFile(path, "wb");
      ok = fp && SDL_SaveBMP_RW(surf, SDL_RWFromFP(fp, SDL_TRUE), 1) == 0;
#else
      ok = SDL_SaveBMP(surf, path.c_str()) == 0;
#endif
      if (!ok) OTE_ERROR("screenshot: cannot write '%s': %s", path.c_str(), SDL_GetError());
      SDL_FreeSurface(surf);
    }
  }
  if (fixed_) leaveOutputSpace();
  else SDL_RenderSetLogicalSize(r_, w_, h_);
  return ok;
}

}  // namespace ote
