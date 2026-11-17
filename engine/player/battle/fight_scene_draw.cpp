// Drawing of the battle scene (docs/BATTLE_SCENE.md "Layout"). Everything is placed from the renderer's size and the font's
// line height, so the same code serves the 480 x 270 landscape and the 270 x 480 portrait screens.
#include <algorithm>
#include <cmath>

#include "battle/fight_scene.h"
#include "ui/text.h"

namespace ote {

struct FightScene::Geo {
  int W = 0, H = 0;
  bool portrait = false;
  int lh = 8, rowH = 10;
  int boxW = 0, boxH = 0, boxY = 4, boxX[2] = {4, 4};
  int stageTop = 0, stageBottom = 0;
  int groundY = 0;
  int footX[2] = {0, 0};
  int scale[2] = {1, 1};  // each fighter's whole-number enlargement (a small map picture is enlarged more than a big animation frame)
  int frameW[2] = {24, 24}, frameH[2] = {24, 24};  // each fighter's picture in design pixels
};

namespace {

uint8_t clamp8(double v) { return (uint8_t)std::max(0.0, std::min(255.0, v)); }

// 0..1..0 over p in 0..1, peaking at `peak`.
double tri(double p, double peak) {
  if (p <= 0 || p >= 1) return 0;
  return p < peak ? p / peak : (1 - p) / (1 - peak);
}

}  // namespace

int FightScene::bgIndex() const { return background_; }

FightScene::Geo FightScene::geometry(const Renderer& r) const {
  Geo g;
  g.W = r.width();
  g.H = r.height();
  g.portrait = g.H > g.W;
  g.lh = r.lineHeight();
  g.rowH = g.lh + 2;
  const Database& db = *env_.battle->db;
  const int margin = 4;
  g.boxW = (g.W - 3 * margin) / 2;
  g.boxH = g.rowH * 3 + 8;
  g.boxX[0] = margin;
  g.boxX[1] = margin * 2 + g.boxW;
  g.stageTop = g.boxY + g.boxH + 4;
  g.stageBottom = g.H - (g.lh + 5);
  g.groundY = g.stageTop + (int)((g.stageBottom - g.stageTop) * (g.portrait ? 0.66 : 0.80));
  for (int s = 0; s < 2; ++s) {
    const Fighter& f = fighter_[s];
    int w = db.view.tileSize, h = db.view.tileSize;
    int sheet = -1;
    if (f.set >= 0) sheet = db.graphics.battle.sets[(size_t)f.set].anims[BaStand].sheet;
    else if (f.look) sheet = f.look->sheet;
    if (sheet >= 0) {
      w = db.graphics.sheets[(size_t)sheet].cellW;
      h = db.graphics.sheets[(size_t)sheet].cellH;
    }
    // Every animation of a set may have its own sheet: the biggest frame decides the size of the stage.
    if (f.set >= 0)
      for (const BattleAnim& a : db.graphics.battle.sets[(size_t)f.set].anims)
        if (a.any()) {
          w = std::max(w, db.graphics.sheets[(size_t)a.sheet].cellW);
          h = std::max(h, db.graphics.sheets[(size_t)a.sheet].cellH);
        }
    g.frameW[s] = w;
    g.frameH[s] = h;
  }
  // Each picture as big as the stage allows (whole numbers, up to 8): by height first, then both side by side must fit the width.
  const int room = std::max(8, g.groundY - g.stageTop - g.lh * 2 - 4);  // above the feet: the picture, then the floating text
  for (int i = 0; i < 2; ++i) {
    g.scale[i] = 1;
    for (int s = 1; s <= 8; ++s)
      if (s * g.frameH[i] <= room) g.scale[i] = s;
  }
  while (g.scale[0] * g.frameW[0] + g.scale[1] * g.frameW[1] > g.W - 8 && (g.scale[0] > 1 || g.scale[1] > 1)) {
    const int big = g.scale[0] * g.frameW[0] >= g.scale[1] * g.frameW[1] ? 0 : 1;
    if (g.scale[big] > 1) --g.scale[big];
    else --g.scale[1 - big];
  }
  const int pw0 = g.scale[0] * g.frameW[0], pw1 = g.scale[1] * g.frameW[1];
  if (script_.ranged()) {  // apart: the distance is the point
    g.footX[0] = std::max(pw0 / 2 + 2, g.W * 22 / 100);
    g.footX[1] = std::min(g.W - pw1 / 2 - 2, g.W - g.W * 22 / 100);
  } else {  // close: the pictures nearly touch
    const int sep = std::max(10, (pw0 + pw1) * 21 / 100);
    g.footX[0] = g.W / 2 - sep;
    g.footX[1] = g.W / 2 + sep;
  }
  return g;
}

void FightScene::drawBackground(Renderer& r, const Geo& g) const {
  const Database& db = *env_.battle->db;
  const BattleArt& art = db.graphics.battle;
  if (background_ >= 0 && env_.sprites) {
    const BattleBackground& bg = art.backgrounds[(size_t)background_];
    const SheetDef& sh = db.graphics.sheets[(size_t)bg.sheet];
    // Whole-number enlargement that covers the screen, centred (the overhang is off the screen).
    const int k = std::max(1, (int)std::ceil(std::max(g.W / (double)sh.cellW, g.H / (double)sh.cellH)));
    const int dw = sh.cellW * k, dh = sh.cellH * k;
    r.drawTexture(env_.sprites->texture(bg.sheet), bg.cell.col * sh.cellW, bg.cell.row * sh.cellH, sh.cellW, sh.cellH, (g.W - dw) / 2,
                  (g.H - dh) / 2, dw, dh);
    return;
  }
  // Generated: a sky, and ground in the colour of the terrain the struck fighter stands on.
  Color ground(96, 120, 80);
  if (terrain_ >= 0 && (size_t)terrain_ < db.terrains.size()) {
    uint32_t rgb = db.terrains[(size_t)terrain_].color;
    if (env_.styleIndex >= 0 && (size_t)env_.styleIndex < db.styles.size()) {
      const StyleDef& st = db.styles[(size_t)env_.styleIndex];
      if ((size_t)terrain_ < st.terrainColors.size() && st.terrainColors[(size_t)terrain_] >= 0) rgb = (uint32_t)st.terrainColors[(size_t)terrain_];
    }
    ground = Color::rgb(rgb);
  }
  const Color skyTop(22, 28, 54), skyLow = mix(Color(88, 104, 150), ground, 0.18f);
  const int horizon = g.groundY - std::max(6, std::min(g.scale[0], g.scale[1]) * 3);
  const int bands = 24;
  for (int i = 0; i < bands; ++i) {
    const int y0 = horizon * i / bands, y1 = horizon * (i + 1) / bands;
    r.fillRect(0, y0, g.W, y1 - y0 + 1, mix(skyTop, skyLow, (float)i / (bands - 1)));
  }
  r.fillRect(0, horizon, g.W, g.H - horizon, mix(ground, Color(0, 0, 0), 0.25f));
  r.fillRect(0, horizon, g.W, 2, mix(ground, Color(255, 255, 255), 0.25f));
  const int below = g.H - horizon - 2;
  for (int i = 0; i < 6; ++i)  // the ground gets darker towards the viewer
    r.fillRect(0, horizon + 2 + below * i / 6, g.W, below / 6 + 1, Color(0, 0, 0, (uint8_t)(10 + i * 8)));
}

void FightScene::drawFighter(Renderer& r, const Geo& g, int side) const {
  const Fighter& f = fighter_[side];
  const Database& db = *env_.battle->db;
  const BattleSet* set = setOf(side);
  const FightPose* pose = tl_.poseAt(side, t_);
  const int s = g.scale[side];
  const int dirTo = side == 0 ? 1 : -1;  // towards the foe
  double offX = 0, offY = 0, alpha = 1;
  Color tint(255, 255, 255);
  const BattleAnim* anim = nullptr;
  int frame = 0;
  if (set) {
    BattleAnimId id = BaStand;
    double local = t_;
    if (pose && set->anims[pose->anim].any()) {
      id = pose->anim;
      local = t_ - pose->start;
    }
    anim = &set->anims[id];
    frame = anim->frameAt((int)(local * 1000.0));
  }
  // The fallback motion: a pose the picture has no animation for (or no art at all) moves the stand picture instead.
  if (pose && (!set || !set->anims[pose->anim].any())) {
    const double p = pose->dur > 0 ? (t_ - pose->start) / pose->dur : 1;
    const int step = 3 * s;
    switch (pose->anim) {
      case BaAttack:
      case BaCast:
      case BaRanged:
        offX = dirTo * tri(p, kFallbackAttackHitMs / (double)kFallbackAttackMs) * (script_.ranged() ? 3 : 6) * s;
        break;
      case BaCrit:
        offX = dirTo * tri(p, kFallbackCritHitMs / (double)kFallbackCritMs) * 8 * s;
        if (p < 0.8 && (int)(p * 12) % 2 == 0) tint = Color(255, 240, 110);  // a yellow strobe
        break;
      case BaHurt:
        offX = (((int)(p * 18)) % 2 ? 1 : -1) * step * (1 - p) * 0.8;
        if (p < 0.6) tint = Color(255, 110, 110);
        break;
      case BaDodge: offX = -dirTo * tri(p, 0.35) * 10 * s; break;
      case BaDefeat:
        alpha = 1 - std::min(1.0, p);
        offY = std::min(1.0, p) * 4 * s;
        tint = Color(255, 150, 150);
        break;
      default: break;
    }
  }
  // Where it stands: its foot point on the ground line.
  int w, h, ax, ay;
  TextureId tex = 0;
  int sx = 0, sy = 0, sw = 0, sh = 0;
  bool glyphBlock = false;
  if (set && anim && anim->any()) {
    const SheetDef& sheet = db.graphics.sheets[(size_t)anim->sheet];
    const bool mirror = set->mirror && ((side == 1) != set->facesLeft);
    const SpriteCell cell = anim->frames[(size_t)std::min(frame, (int)anim->frames.size() - 1)];
    sw = sheet.cellW;
    sh = sheet.cellH;
    sy = cell.row * sh;
    if (mirror && env_.sprites->mirrored(anim->sheet)) {
      tex = env_.sprites->mirrored(anim->sheet);
      sx = (sheet.cols - 1 - cell.col) * sw;
    } else {
      tex = env_.sprites->texture(anim->sheet);
      sx = cell.col * sw;
    }
    ax = set->anchorX >= 0 ? set->anchorX : sw / 2;
    ay = set->anchorY >= 0 ? set->anchorY : sh;
    if (mirror && tex == env_.sprites->mirrored(anim->sheet)) ax = sw - ax;
    w = sw * s;
    h = sh * s;
  } else if (f.look) {
    const SheetDef& sheet = db.graphics.sheets[(size_t)f.look->sheet];
    const SpriteCell cell = pickCell(*f.look, 0, 0, 0, (uint32_t)(t_ * 1000.0));
    tex = env_.sprites->texture(f.look->sheet);
    sw = sheet.cellW;
    sh = sheet.cellH;
    sx = cell.col * sw;
    sy = cell.row * sh;
    ax = sw / 2;
    ay = sh;
    w = sw * s;
    h = sh * s;
  } else {  // no art at all: a block in the side's colour with the type's letter, like the map
    glyphBlock = true;
    sw = sh = db.view.tileSize;
    ax = sw / 2;
    ay = sh;
    w = sw * s;
    h = sh * s;
  }
  const int footX = g.footX[side] + (int)std::lround(offX), footY = g.groundY + (int)std::lround(offY);
  // A shadow on the ground.
  r.fillRect(g.footX[side] - w * 3 / 10, g.groundY - 1, w * 6 / 10, std::max(2, s), Color(0, 0, 0, (uint8_t)(90 * alpha)));
  const int x = footX - ax * s, y = footY - ay * s;
  if (glyphBlock) {
    const int bw = w * 3 / 4;
    const Color body = f.color;
    r.fillRect(footX - bw / 2, footY - h * 3 / 4, bw, h * 3 / 4, Color(body.r, body.g, body.b, (uint8_t)(255 * alpha)));
    r.drawRect(footX - bw / 2, footY - h * 3 / 4, bw, h * 3 / 4, Color(20, 20, 28, (uint8_t)(255 * alpha)));
    if (alpha > 0.5) drawGlyphCentered(r, footX, footY - h * 3 / 8, f.glyph, readableOn(body), std::max(1, bw * 3 / 4));
  } else if (alpha > 0) {
    Color mod(clamp8(tint.r), clamp8(tint.g), clamp8(tint.b), clamp8(255 * alpha));
    r.drawTexture(tex, sx, sy, sw, sh, x, y, w, h, mod);
  }
}

void FightScene::drawEffects(Renderer& r, const Geo& g) const {
  const Database& db = *env_.battle->db;
  const BattleArt& art = db.graphics.battle;
  for (const FightFx& fx : tl_.fx) {
    if (t_ < fx.t0 || t_ > fx.t0 + fx.dur + 0.001) continue;
    const int e = effectOfStrike_[(size_t)fx.strike];
    if (e < 0) continue;
    const BattleEffect& be = art.effects[(size_t)e];
    const BattleAnim& an = be.anim;
    const SheetDef& sheet = db.graphics.sheets[(size_t)an.sheet];
    const int s = g.scale[fx.fromSide];
    const double p = fx.dur > 0 ? std::min(1.0, (t_ - fx.t0) / fx.dur) : 1;
    auto bodyPoint = [&](int side, double& px, double& py) {
      px = g.footX[side];
      py = g.groundY - g.frameH[side] * g.scale[side] * 0.5;
    };
    double ax, ay, bx, by;
    bodyPoint(fx.fromSide, ax, ay);
    bodyPoint(fx.toSide, bx, by);
    double cx = bx, cy = by;
    if (fx.travel) {
      cx = ax + (bx - ax) * p;
      cy = ay + (by - ay) * p;
    }
    const int frame = an.frameAt((int)((t_ - fx.t0) * 1000.0));
    const SpriteCell cell = an.frames[(size_t)std::min(frame, (int)an.frames.size() - 1)];
    const bool mirror = fx.fromSide == 1;  // the art flies / faces right
    const TextureId tex = mirror ? env_.sprites->mirrored(an.sheet) : env_.sprites->texture(an.sheet);
    if (!tex) continue;
    const int sx = mirror ? (sheet.cols - 1 - cell.col) * sheet.cellW : cell.col * sheet.cellW;
    int anchorX = be.anchorX >= 0 ? be.anchorX : sheet.cellW / 2, anchorY = be.anchorY >= 0 ? be.anchorY : sheet.cellH / 2;
    if (mirror) anchorX = sheet.cellW - anchorX;
    r.drawTexture(tex, sx, cell.row * sheet.cellH, sheet.cellW, sheet.cellH, (int)std::lround(cx) - anchorX * s, (int)std::lround(cy) - anchorY * s,
                  sheet.cellW * s, sheet.cellH * s);
  }
}

void FightScene::drawBox(Renderer& r, const Geo& g, int side) const {
  const Fighter& f = fighter_[side];
  const int x = g.boxX[side], y = g.boxY, w = g.boxW, h = g.boxH;
  r.fillRect(x, y, w, h, Color(14, 18, 36, 225));
  r.drawRect(x, y, w, h, Color(120, 128, 170));
  r.fillRect(x + 1, y + 1, w - 2, 3, f.color);  // the side's colour along the top
  const int tx = x + 4;
  int ty = y + 5;
  const std::string lv = "Lv " + intStr(f.level);
  const int lvW = textW(r, lv);
  drawTextRight(r, x + w - 4, ty, lv, Color(190, 196, 225));
  drawTextShadow(r, tx, ty, fitText(r, f.name, w - 12 - lvW), Color(240, 242, 255));
  ty += g.rowH;
  drawTextShadow(r, tx, ty, f.weapon.empty() ? "-" : fitText(r, f.weapon, w - 8), Color(150, 156, 190));
  ty += g.rowH;
  const int hp = std::max(0, std::min(f.maxHp, tl_.hpAt(side, t_, f.startHp)));
  const std::string nums = intStr(hp) + "/" + intStr(f.maxHp);
  const int numW = textW(r, nums);
  const int barW = std::max(10, w - 12 - numW), barH = std::max(5, g.lh - 2);
  r.fillRect(tx, ty + 1, barW, barH, Color(40, 18, 22));
  const int fill = f.maxHp > 0 ? barW * hp / f.maxHp : 0;
  if (fill > 0) r.fillRect(tx, ty + 1, fill, barH, hpColor(hp, f.maxHp));
  r.drawRect(tx - 1, ty, barW + 2, barH + 2, Color(10, 10, 16));
  drawTextRight(r, x + w - 4, ty, nums, Color(235, 237, 245));
}

void FightScene::drawFloats(Renderer& r, const Geo& g) const {
  for (const Float& f : floats_) {
    const double age = t_ - f.born, k = age / f.life;
    int cx = g.footX[f.side];
    const int top = g.groundY - g.frameH[f.side] * g.scale[f.side];
    int y = top - g.lh - 2 - f.rise - (int)(age * (f.plate ? 4 : 14));
    y = std::max(g.stageTop, y);
    Color c = f.color;
    if (k > 0.7) c.a = clamp8(255 * (1 - (k - 0.7) / 0.3));
    const std::string text = fitText(r, f.text, g.W - 12);
    const int w = textW(r, text);
    cx = std::max(w / 2 + 4, std::min(cx, g.W - w / 2 - 4));
    if (f.plate) r.fillRect(cx - w / 2 - 3, y - 2, w + 6, g.lh + 4, Color(10, 12, 24, (uint8_t)(c.a * 220 / 255)));
    drawTextShadow(r, cx - w / 2, y, text, c);
  }
}

void FightScene::drawRange(Renderer& r, const Geo& g) const {
  // The distance cue of a ranged fight: a dotted line between the feet with the number of squares.
  const int y = g.groundY + std::max(3, std::min(g.scale[0], g.scale[1]) * 2);
  const int x0 = g.footX[0] + g.scale[0] * 4, x1 = g.footX[1] - g.scale[1] * 4;
  for (int x = x0; x < x1; x += 6) r.fillRect(x, y, 3, 1, Color(220, 225, 245, 160));
  r.fillRect(x0, y - 3, 1, 7, Color(220, 225, 245, 200));
  r.fillRect(x1, y - 3, 1, 7, Color(220, 225, 245, 200));
  const std::string label = "Range " + intStr(script_.distance);
  drawTextCentered(r, g.W / 2, y + 4, label, Color(220, 225, 245));
}

void FightScene::draw(Renderer& r) const {
  if (!active_) return;
  const Geo g = geometry(r);
  drawBackground(r, g);
  if (script_.ranged()) drawRange(r, g);
  // The one who is striking stands in front.
  int order[2] = {0, 1};
  const FightPose* p0 = tl_.poseAt(0, t_);
  if (p0 && (p0->anim == BaAttack || p0->anim == BaCrit || p0->anim == BaRanged || p0->anim == BaCast)) std::swap(order[0], order[1]);
  for (int i = 0; i < 2; ++i) drawFighter(r, g, order[i]);
  drawEffects(r, g);
  drawFloats(r, g);
  drawBox(r, g, 0);
  drawBox(r, g, 1);
  drawTextCentered(r, g.W / 2, g.H - g.lh - 3, "Confirm / tap: skip", Color(150, 158, 190));
  if (t_ - flash_ < 0.12) r.fillRect(0, 0, g.W, g.H, Color(255, 255, 255, (uint8_t)(110 * (1 - (t_ - flash_) / 0.12))));  // a critical hit
  double fade = 0;
  if (t_ < kFade) fade = 1 - t_ / kFade;
  else if (t_ > tl_.total - kFade) fade = (t_ - (tl_.total - kFade)) / kFade;
  if (fade > 0) r.fillRect(0, 0, g.W, g.H, Color(0, 0, 0, clamp8(255 * std::min(1.0, fade))));
}

}  // namespace ote
