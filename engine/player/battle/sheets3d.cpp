#include "battle/sheets3d.h"

#include <cstring>

#include "core/base/log.h"
#include "platform/common/image.h"

namespace ote {

Texture3DId Sheets3D::texture(Renderer3D& r3d, int sheet) {
  if (!g_ || sheet < 0 || (size_t)sheet >= g_->sheets.size()) return 0;
  if (sheets_.size() < g_->sheets.size()) sheets_.resize(g_->sheets.size());
  Entry& e = sheets_[(size_t)sheet];
  if (e.tried) return e.tex;
  e.tried = true;
  const SheetDef& s = g_->sheets[(size_t)sheet];
  Image img;
  std::string err;
  if (!loadPngFile(dir_ + "/" + s.file, img, err)) return 0;  // Sprites has reported it already
  int limit = r3d.caps().maxTextureSize, w = 1, h = 1;
  while (w < img.width) w *= 2;
  while (h < img.height) h *= 2;
  if (w > limit || h > limit) {
    OTE_ERROR("art sheet \"%s\" (%s): %dx%d is too large for the 3D views (at most %d a side); "
              "what uses it keeps the placeholder look there",
              s.id.c_str(), s.file.c_str(), img.width, img.height, limit);
    return 0;
  }
  std::vector<uint8_t> px((size_t)w * h * 4, 0);
  for (int y = 0; y < img.height; ++y)
    std::memcpy(&px[(size_t)y * w * 4], &img.rgba[(size_t)y * img.width * 4], (size_t)img.width * 4);
  e.tex = r3d.createTexture(px.data(), w, h, false, s.smooth);  // smooth sheets also get mipmaps
  e.w = w;
  e.h = h;
  return e.tex;
}

void Sheets3D::uv(int sheet, int x, int y, int w, int h, float& u0, float& v0, float& u1, float& v1) const {
  const Entry& e = sheets_[(size_t)sheet];
  // Of a texel. Hard pixels only need a hair; blended ones read half a texel past
  // the edge, so they are pulled in that far or the next cell's colours bleed in.
  const float in = smooth(sheet) ? 0.5f : 0.02f;
  u0 = (x + in) / e.w;
  v0 = (y + in) / e.h;
  u1 = (x + w - in) / e.w;
  v1 = (y + h - in) / e.h;
}

void Sheets3D::release(Renderer3D& r3d) {
  for (Entry& e : sheets_)
    if (e.tex) r3d.destroyTexture(e.tex);
  sheets_.clear();
}

}  // namespace ote
