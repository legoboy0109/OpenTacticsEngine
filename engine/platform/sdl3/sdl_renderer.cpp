#include "platform/sdl3/sdl_renderer.h"

#include "core/base/log.h"

namespace ote {

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

void SdlRenderer::drawText(int x, int y, const char* text, Color c, int scale) {
  if (!text || !*text) return;
  if (scale < 1) scale = 1;
  setColor(c);
  // SDL scales the whole draw (including the position), so divide the
  // position by the scale to land on the requested logical pixel.
  SDL_SetRenderScale(r_, (float)scale, (float)scale);
  SDL_RenderDebugText(r_, (float)x / scale, (float)y / scale, text);
  SDL_SetRenderScale(r_, 1.0f, 1.0f);
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
