#include "ui/scene_stage_draw.h"

#include <algorithm>
#include <cmath>

#include "core/data/scene_fx.h"
#include "ui/dialogue_portrait.h"

namespace ote {

CoverFit fitCover(int sw, int sh, int W, int H) {
  CoverFit f{};
  if (sw < 1 || sh < 1 || W < 1 || H < 1) return f;
  float s = std::max((float)W / (float)sw, (float)H / (float)sh);
  if (s > 1.0f) s = std::ceil(s - 1e-4f);  // bigger: whole pixels only
  f.scale = s;
  f.srcW = std::min(sw, (int)std::ceil((float)W / s - 1e-4f));
  f.srcH = std::min(sh, (int)std::ceil((float)H / s - 1e-4f));
  f.srcX = (sw - f.srcW) / 2;
  f.srcY = (sh - f.srcH) / 2;
  f.dstW = (int)std::lround((double)f.srcW * s);
  f.dstH = (int)std::lround((double)f.srcH * s);
  f.dstX = (W - f.dstW) / 2;
  f.dstY = (H - f.dstH) / 2;
  return f;
}

namespace {

// Where the scene background `id` is in its sheet's texture (a whole image, or one cell). tex 0 = none.
PortraitArt backgroundArt(const StageDrawCtx& ctx, const std::string& id) {
  PortraitArt art;
  const int i = ctx.db.graphics.findBackground(id);
  if (i < 0 || !ctx.sprites) return art;
  const SceneBackground& bg = ctx.db.graphics.backgrounds[(size_t)i];
  const SheetDef& sh = ctx.db.graphics.sheets[(size_t)bg.sheet];
  art.tex = ctx.sprites->texture(bg.sheet);
  if (bg.whole) {
    art.sx = art.sy = 0;
    art.sw = sh.width;
    art.sh = sh.height;
  } else {
    art.sx = bg.cell.col * sh.cellW;
    art.sy = bg.cell.row * sh.cellH;
    art.sw = sh.cellW;
    art.sh = sh.cellH;
  }
  return art;
}

void drawPicture(Renderer& r, const PortraitArt& art, int dx, int dy, uint8_t alpha) {
  if (!art.any() || alpha == 0) return;
  const CoverFit f = fitCover(art.sw, art.sh, r.width(), r.height());
  r.drawTexture(art.tex, art.sx + f.srcX, art.sy + f.srcY, f.srcW, f.srcH, f.dstX + dx, f.dstY + dy, f.dstW, f.dstH,
                Color(255, 255, 255, alpha));
}

uint32_t hash32(uint32_t v) {
  v ^= v >> 16;
  v *= 0x7feb352dU;
  v ^= v >> 15;
  v *= 0x846ca68bU;
  v ^= v >> 16;
  return v;
}

}  // namespace

void drawStageBackground(Renderer& r, const StageDrawCtx& ctx, const SceneStage& stage) {
  if (!stage.backgroundShown()) return;
  const int dx = stage.shakeX(), dy = stage.shakeY();
  const float t = stage.backgroundProgress();
  if (stage.previousBackground().empty() && t >= 1) {
    r.fillRect(0, 0, r.width(), r.height(), Color(0, 0, 0));  // behind the picture (also what a shake uncovers)
    drawPicture(r, backgroundArt(ctx, stage.background()), dx, dy, 255);
    return;
  }
  // A cross-fade: the old picture (or the map, when there was none) gives way to the new one.
  if (!stage.previousBackground().empty()) {
    r.fillRect(0, 0, r.width(), r.height(), Color(0, 0, 0));
    drawPicture(r, backgroundArt(ctx, stage.previousBackground()), dx, dy, stage.background().empty() ? (uint8_t)(255 * (1 - t)) : 255);
  }
  if (!stage.background().empty()) {
    if (stage.previousBackground().empty()) r.fillRect(0, 0, r.width(), r.height(), Color(0, 0, 0, (uint8_t)(255 * t)));
    drawPicture(r, backgroundArt(ctx, stage.background()), dx, dy, (uint8_t)(255 * t));
  }
}

void drawStagePortraits(Renderer& r, const StageDrawCtx& ctx, const SceneStage& stage, int baseY) {
  const int W = r.width();
  const int margin = std::max(2, W / 60);
  for (const SceneStage::Portrait& p : stage.portraits()) {
    const PortraitArt art = portraitArt(ctx.db, ctx.sprites, ctx.styleIdx, p.art);
    if (!art.any()) continue;
    // Tall enough to read at a glance, never more than half the free height or a third of the width; whole-number
    // scales when it grows (pixel art), a fraction only when it has to shrink.
    const int maxH = std::max(16, std::min(baseY - 2, r.height() * 55 / 100)), maxW = std::max(16, W / 3);
    float s = std::min((float)maxH / (float)art.sh, (float)maxW / (float)art.sw);
    if (s >= 1.0f) s = std::floor(s);
    const int dw = std::max(1, (int)std::lround((double)art.sw * s)), dh = std::max(1, (int)std::lround((double)art.sh * s));
    const int x = p.position == kPosLeft ? margin + stage.shakeX() : p.position == kPosRight ? W - dw - margin + stage.shakeX() : (W - dw) / 2 + stage.shakeX();
    const bool lit = stage.speaker() == p.key;
    r.drawTexture(art.tex, art.sx, art.sy, art.sw, art.sh, x, baseY - dh + stage.shakeY(), dw, dh,
                  lit ? Color(255, 255, 255) : Color(105, 105, 120));
  }
}

void drawStageWeather(Renderer& r, const SceneStage& stage) {
  const std::string& w = stage.weather();
  if (w.empty()) return;
  const int W = r.width(), H = r.height();
  const float t = stage.clock();
  const bool rain = w == "rain";
  if (rain) r.fillRect(0, 0, W, H, Color(16, 24, 48, 46));  // a grey-blue cast
  const int count = std::max(20, W * H / (rain ? 1300 : 1700));
  for (int i = 0; i < count; ++i) {
    const uint32_t h = hash32((uint32_t)i * 2654435761u + 77u), h2 = hash32(h + 13u);
    const float speed = rain ? 220.0f + (float)(h % 120) : 26.0f + (float)(h % 34);
    const float fall = std::fmod(t * speed + (float)(h2 % 4096) / 4096.0f * (float)(H + 12), (float)(H + 12));
    const float baseX = (float)(h % (uint32_t)(W + 40)) - 20.0f;
    if (rain) {
      const int y = (int)fall - 8, x = (int)(baseX + fall * 0.12f);
      r.fillRect(x, y, 1, 5, Color(175, 195, 235, 150));
    } else {
      const int y = (int)fall - 4, x = (int)(baseX + std::sin(t * 0.9f + (float)i) * 6.0f);
      const int size = 1 + (int)(h2 % 2);
      r.fillRect(x, y, size, size, Color(240, 244, 255, 215));
    }
  }
}

void drawStageFade(Renderer& r, const SceneStage& stage) {
  if (stage.fadeAmount() <= 0) return;
  const uint32_t c = stage.fadeColor();
  r.fillRect(0, 0, r.width(), r.height(), Color::rgb(c, (uint8_t)std::lround(255.0 * std::min(1.0f, stage.fadeAmount()))));
}

}  // namespace ote
