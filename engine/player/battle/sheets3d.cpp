#include "battle/sheets3d.h"

#include <algorithm>
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
  // (Memory is short on a console: the decoded picture is not kept next to its padded copy.)
  std::vector<uint8_t> px;
  if (w == img.width && h == img.height) {
    px.swap(img.rgba);
  } else {
    px.assign((size_t)w * h * 4, 0);
    for (int y = 0; y < img.height; ++y)
      std::memcpy(&px[(size_t)y * w * 4], &img.rgba[(size_t)y * img.width * 4], (size_t)img.width * 4);
    std::vector<uint8_t>().swap(img.rgba);
  }
  // Over what the backend takes: shrunk by 2 as often as it needs (colours weighted by alpha, so cut-out edges stay clean).
  // uv() keeps speaking in the sheet's own pixels.
  int shrink = 1;
  std::vector<uint8_t> tmp;
  int tw = w, th = h;
  while (tw > limit || th > limit) {
    const int nw = std::max(1, tw / 2), nh = std::max(1, th / 2);
    tmp.assign((size_t)nw * nh * 4, 0);
    for (int y = 0; y < nh; ++y)
      for (int x = 0; x < nw; ++x) {
        int sum[3] = {0, 0, 0}, a = 0;
        for (int k = 0; k < 4; ++k) {
          const uint8_t* p = &px[((size_t)std::min(th - 1, y * 2 + (k >> 1)) * tw + std::min(tw - 1, x * 2 + (k & 1))) * 4];
          for (int c = 0; c < 3; ++c) sum[c] += p[c] * p[3];
          a += p[3];
        }
        uint8_t* o = &tmp[((size_t)y * nw + x) * 4];
        for (int c = 0; c < 3; ++c) o[c] = a ? (uint8_t)((sum[c] + a / 2) / a) : 0;
        o[3] = (uint8_t)((a + 2) / 4);
      }
    px.swap(tmp);
    std::vector<uint8_t>().swap(tmp);  // the bigger picture goes now, not at the end
    tw = nw;
    th = nh;
    shrink *= 2;
  }
  if (shrink > 1)
    OTE_INFO("art sheet \"%s\" (%s): %dx%d is more than the 3D views take (%d a side); shown at 1/%d of its size", s.id.c_str(),
             s.file.c_str(), img.width, img.height, limit, shrink);
  e.tex = r3d.createTexture(px.data(), tw, th, false, s.smooth);  // smooth sheets also get mipmaps
  e.w = w;
  e.h = h;
  e.shrink = shrink;
  return e.tex;
}

void Sheets3D::uv(int sheet, int x, int y, int w, int h, float& u0, float& v0, float& u1, float& v1) const {
  const Entry& e = sheets_[(size_t)sheet];
  // Of a texel. Hard pixels only need a hair; blended ones read half a texel past
  // the edge, so they are pulled in that far or the next cell's colours bleed in.
  const float in = (smooth(sheet) ? 0.5f : 0.02f) * (float)e.shrink;  // (a texel of the shrunk texture is `shrink` sheet pixels)
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
