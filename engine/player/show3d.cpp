#include "show3d.h"

namespace ote {

void Show3D::show(Renderer& r, const Renderer3D& r3d, int x, int y, int w, int h) {
  int pw = 0, ph = 0;
  const uint8_t* px = r3d.framePixels(pw, ph);
  if (!px || pw < 1 || ph < 1) return;
  if (tex_ && (pw != w_ || ph != h_)) release(r);
  if (!tex_) {
    tex_ = r.createTexture(px, pw, ph);
    w_ = pw;
    h_ = ph;
  } else {
    r.updateTexture(tex_, px);
  }
  r.drawTexture(tex_, 0, 0, pw, ph, x, y, w, h);
}

void Show3D::release(Renderer& r) {
  if (tex_) r.destroyTexture(tex_);
  tex_ = 0;
  w_ = h_ = 0;
}

}  // namespace ote
