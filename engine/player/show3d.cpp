#include "show3d.h"

#include <algorithm>

namespace ote {

void Show3D::fit(const Renderer& r, const Renderer3D& r3d, Frame3D& f) const {
  f.renderWidth = f.renderHeight = f.samples = 0;
  bool display = mode_ == "display" || (mode_ != "game" && r3d.caps().fullResolution);
  if (!display || r.width() < 1 || r.height() < 1 || f.width < 1 || f.height < 1) return;
  int dw = 0, dh = 0;
  r.displaySize(dw, dh);
  // Pixels the display has per logical pixel. Never below 1: a small window
  // keeps the logical picture and lets the 2D renderer shrink it.
  float k = std::min((float)dw / (float)r.width(), (float)dh / (float)r.height());
  if (k <= 1.0001f) return;
  int w = (int)((float)f.width * k + 0.5f), h = (int)((float)f.height * k + 0.5f);
  int longer = std::max(w, h);
  if (longer > kMaxSide) {
    w = std::max(1, (int)((long long)w * kMaxSide / longer));
    h = std::max(1, (int)((long long)h * kMaxSide / longer));
  }
  if (w <= f.width && h <= f.height) return;
  f.renderWidth = w;
  f.renderHeight = h;
  f.samples = 4;  // edges: smoothed where the backend can (the OpenGL one)
}

void Show3D::show(Renderer& r, const Renderer3D& r3d, int x, int y, int w, int h) {
  int pw = 0, ph = 0;
  const uint8_t* px = r3d.framePixels(pw, ph);
  if (!px || pw < 1 || ph < 1) return;
  // Bigger than its rectangle: scale smoothly (near 1:1 on the display, so this
  // is a hair of resampling). Smaller: hard pixels, as the logical picture always had.
  bool smooth = pw > w || ph > h;
  if (tex_ && (pw != w_ || ph != h_ || smooth != smooth_)) release(r);
  if (!tex_) {
    tex_ = r.createTexture(px, pw, ph, smooth);
    w_ = pw;
    h_ = ph;
    smooth_ = smooth;
  } else {
    r.updateTexture(tex_, px);
  }
  r.drawTexture(tex_, 0, 0, pw, ph, x, y, w, h);
}

void Show3D::release(Renderer& r) {
  if (tex_) r.destroyTexture(tex_);
  tex_ = 0;
  w_ = h_ = 0;
  smooth_ = false;
}

}  // namespace ote
