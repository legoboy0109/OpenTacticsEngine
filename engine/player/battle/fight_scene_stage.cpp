// The optional fixed stage of the battle scene (docs/BATTLE_SCENE.md "Fixed stage"): everything is placed in the stage's own pixels
// and drawn at one whole-number scale; the numbers come from the pure stageGeometry (core/data/battle_stage.h).
#include <algorithm>
#include <cmath>

#include "battle/fight_scene.h"
#include "battle/fight_scene_geo.h"

namespace ote {

void FightScene::stageLayout(Geo& g) const {
  const Database& db = *env_.battle->db;
  const BattleStage& st = db.graphics.battle.stage;
  // The platform pictures' size (both sides share one sheet, so one size).
  int pw[2] = {0, 0}, ph[2] = {0, 0};
  if (platform_ >= 0 && env_.sprites) {
    const SheetDef& sh = db.graphics.sheets[(size_t)db.graphics.battle.platforms[(size_t)platform_].sheet];
    pw[0] = pw[1] = sh.cellW;
    ph[0] = ph[1] = sh.cellH;
  }
  // Range bands (docs/BATTLE_SCENE.md): the layout of this fight's distance; a band that shows one fighter at a time moves the picture with the camera.
  const StageLayout lay = stageLayoutFor(st, script_.distance);
  g.panning = lay.each;
  g.viewX = lay.each ? lay.viewX(tl_.cameraAt(t_)) : 0.0;
  g.scrollBackdrop = lay.each && lay.scrollBackdrop;
  g.sg = stageGeometryAt(g.W, g.H, g.stageTop, g.stageBottom, st, script_.distance, pw, ph, g.viewX);
  g.stage = g.sg.valid;
  const int k = g.sg.scale;
  g.groundY = (g.sg.feet[0][1] + g.sg.feet[1][1]) / 2;
  for (int s = 0; s < 2; ++s) {
    g.footX[s] = g.sg.feet[s][0];
    g.footY[s] = g.sg.feet[s][1];
    g.scale[s] = k;
  }
  // A fighter with no set (the map picture) is enlarged, on top of the stage's scale, to about the height of an animated fighter:
  // the other side's set from its foot point up, else 30% of the stage's height (all in stage pixels).
  for (int s = 0; s < 2; ++s) {
    if (fighter_[s].set >= 0) continue;
    int target = st.height * 30 / 100;
    const int o = 1 - s;
    if (fighter_[o].set >= 0) {
      const BattleSet& bs = db.graphics.battle.sets[(size_t)fighter_[o].set];
      target = bs.anchorY >= 0 ? bs.anchorY : g.frameH[o];
    }
    int m = 1;
    for (int t = 2; t <= 8; ++t)
      if (t * g.frameH[s] <= target) m = t;
    g.scale[s] = k * m;
  }
}

// The backdrop at the stage's scale: its top-left on the stage's top-left when it is the stage's size, else centred across and placed
// up / down by `align`; whatever falls outside the stage is not drawn.
void FightScene::drawStageBackground(Renderer& r, const Geo& g) const {
  const Database& db = *env_.battle->db;
  const BattleStage& st = db.graphics.battle.stage;
  const BattleBackground& bg = db.graphics.battle.backgrounds[(size_t)background_];
  const SheetDef& sh = db.graphics.sheets[(size_t)bg.sheet];
  const int k = g.sg.scale;
  r.fillRect(0, 0, g.W, g.H, Color(10, 13, 28));  // the bars around (or under) the stage
  const int ox =(st.width - sh.cellW) / 2;
  const int oy = st.align == BattleStage::AlignTop ? 0 : st.align == BattleStage::AlignBottom ? st.height - sh.cellH : (st.height - sh.cellH) / 2;
  // The part of the picture inside the stage, in picture pixels.
  const int j0 = std::max(0, -oy), j1 = std::min(sh.cellH, st.height - oy);
  if (g.scrollBackdrop && sh.cellW > 0 && j1 > j0) {
    // "backdrop": "scroll": the picture moves with the camera by the same pixels as the world and repeats sideways, so no strip is empty.
    // Stage column x shows picture column (x + viewX - ox) modulo the picture's width; drawn in strips, one per repeat.
    const long view = std::lround(g.viewX);
    const TextureId tex = env_.sprites->texture(bg.sheet);
    for (int x = 0; x < st.width;) {
      const long col = (((x + view - ox) % sh.cellW) + sh.cellW) % sh.cellW;
      const int w = (int)std::min<long>(sh.cellW - col, st.width - x);
      r.drawTexture(tex, bg.cell.col * sh.cellW + (int)col, bg.cell.row * sh.cellH + j0, w, j1 - j0, g.sg.x + x * k, g.sg.y + (oy + j0) * k, w * k, (j1 - j0) * k);
      x += w;
    }
    return;
  }
  const int i0 = std::max(0, -ox), i1 = std::min(sh.cellW, st.width - ox);
  if (i1 <= i0 || j1 <= j0) return;
  r.drawTexture(env_.sprites->texture(bg.sheet), bg.cell.col * sh.cellW + i0, bg.cell.row * sh.cellH + j0, i1 - i0, j1 - j0,
                g.sg.x + (ox + i0) * k, g.sg.y + (oy + j0) * k, (i1 - i0) * k, (j1 - j0) * k);
}

// A platform at its declared place; the right-hand one is the left one turned round unless the platform names a picture of its own for it.
void FightScene::drawStagePlatform(Renderer& r, const Geo& g, int side) const {
  const Database& db = *env_.battle->db;
  const BattlePlatform& p = db.graphics.battle.platforms[(size_t)platform_];
  const SheetDef& sh = db.graphics.sheets[(size_t)p.sheet];
  if (g.sg.platformW[side] <= 0) return;
  const bool far = platformFar_ && p.hasFar;
  SpriteCell cell = far ? p.farCell : p.cell;
  bool turn = false;
  if (side == 1) {
    if (far && p.hasFarRight) cell = p.farRightCell;
    else if (!far && p.hasRight) cell = p.rightCell;
    else turn = true;
  }
  TextureId tex = env_.sprites->texture(p.sheet);
  int sx = cell.col * sh.cellW;
  if (turn && env_.sprites->mirrored(p.sheet)) {
    tex = env_.sprites->mirrored(p.sheet);
    sx = (sh.cols - 1 - cell.col) * sh.cellW;
  }
  r.drawTexture(tex, sx, cell.row * sh.cellH, sh.cellW, sh.cellH, g.sg.platform[side][0], g.sg.platform[side][1], g.sg.platformW[side],
                g.sg.platformH[side]);
}

}  // namespace ote
