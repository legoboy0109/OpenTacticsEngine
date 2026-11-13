// SDL3 implementation of ote::Renderer (the only place drawing touches SDL).
#pragma once
#include <SDL3/SDL.h>

#include <string>

#include "platform/platform.h"

namespace ote {

class SdlRenderer : public Renderer {
 public:
  SdlRenderer(SDL_Renderer* renderer, int w, int h) : r_(renderer), w_(w), h_(h) {}
  int width() const override { return w_; }
  int height() const override { return h_; }
  void clear(Color c) override;
  void fillRect(int x, int y, int w, int h, Color c) override;
  void drawRect(int x, int y, int w, int h, Color c) override;
  void drawText(int x, int y, const char* text, Color c, int scale = 1) override;

  SDL_Renderer* handle() const { return r_; }
  // Saves what the player sees (the whole output, including letterbox bars)
  // as BMP. Call after drawing, before present.
  bool saveScreenshot(const std::string& path);

 private:
  void setColor(Color c);
  SDL_Renderer* r_;
  int w_, h_;
};

}  // namespace ote
