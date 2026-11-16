// Standing terrain on the 3D maps (docs/GRAPHICS.md "Standing terrain"): the upright picture or the models a
// terrain has in a 3D style, standing on the ground of every cell of that terrain. The ground block and its
// flat ground picture are drawn by Map3D::drawTops as before; this adds what stands on them.
//
// Placement rule (so a unit on a forest or a mountain stays readable):
//  - a standing PICTURE faces the camera like a unit does and stands towards the BACK of its cell (kStandBack
//    squares behind the middle, as seen from the camera), so a unit in the middle of the cell is in front of it;
//  - a standing MODEL stands in the middle of the cell and a unit on that cell is moved kModelAhead squares
//    towards the camera (like a unit on a model structure), so it stands in front of the model;
//  - the depth buffer sorts everything else (units behind a forest are covered by it, as behind a building;
//    they are drawn again faintly over it, like units behind higher blocks);
//  - fog dims it like the ground, and cells in move / attack range tint it blue / red so the highlight that
//    lies on the ground (and may be hidden by the trees) still reads.
#include <algorithm>
#include <cmath>

#include "battle/battle_draw.h"
#include "battle/map3d.h"
#include "battle/map3d_sizes.h"

namespace ote {

namespace {

const float kPi = 3.14159265f;

Material3D standFlat(Blend3D blend) {
  Material3D m;
  m.lit = false;
  m.twoSided = true;
  m.blend = blend;
  return m;
}

}  // namespace

const StandLook* Map3D::standOf(const DrawCtx& c, int terrain) const {
  const GraphicsSet* set = c.battle.db->graphics.set(c.styleIndex);
  if (!set || terrain < 0 || (size_t)terrain >= set->stands.size()) return nullptr;
  const StandLook& s = set->stands[(size_t)terrain];
  return s.any() ? &s : nullptr;
}

// Which cells have a standing part (terrain never changes during a battle).
void Map3D::markStands(const DrawCtx& c) {
  const Battle& b = c.battle;
  standAt_.assign((size_t)(c.proj->gridW() * c.proj->gridH()), 0);
  anyStand_ = false;
  for (int y = 0; y < c.proj->gridH(); ++y)
    for (int x = 0; x < c.proj->gridW(); ++x) {
      Cell cell = b.grid.at(x, y);
      if (standOf(c, b.cells[cell].terrain)) {
        standAt_[(size_t)cell] = 1;
        anyStand_ = true;
      }
    }
}

bool Map3D::standAt(Cell cell) const { return cell >= 0 && (size_t)cell < standAt_.size() && standAt_[(size_t)cell]; }

// Whether a standing part on a cell nearer the camera than (x, y) can cover it (the cells straight ahead, or the
// three ahead when the camera looks along a diagonal).
bool Map3D::standInFront(const DrawCtx& c, int x, int y) const {
  if (!anyStand_) return false;
  const Grid& g = c.battle.grid;
  int dx = cx_ > 0.38f ? 1 : cx_ < -0.38f ? -1 : 0, dy = cz_ > 0.38f ? 1 : cz_ < -0.38f ? -1 : 0;
  auto at = [&](int ax, int ay) { return ax >= 0 && ay >= 0 && ax < g.width() && ay < g.height() && standAt(g.at(ax, ay)); };
  return (dx && at(x + dx, y)) || (dy && at(x, y + dy)) || (dx && dy && at(x + dx, y + dy));
}

// How far towards the camera a unit standing on this cell is moved: in front of a standing model.
float Map3D::standAhead(const DrawCtx& c, Cell cell) const {
  if (!standAt(cell)) return 0;
  const StandLook* s = standOf(c, c.battle.cells[cell].terrain);
  return s && s->isModel() ? kModelAhead : 0;
}

void Map3D::drawStands(const DrawCtx& c, const Overlay& ov) {
  if (!anyStand_) return;
  const Battle& b = c.battle;
  const MapProj& g = *c.proj;
  const bool fogged = c.view.fogged();
  const uint32_t ms = (uint32_t)(c.time * 1000.0f);
  for (int y = 0; y < g.gridH(); ++y)
    for (int x = 0; x < g.gridW(); ++x) {
      const Cell cell = b.grid.at(x, y);
      if (!standAt(cell)) continue;
      // Out of the picture (with room for tall things): nothing to draw. Matters on big maps and for models.
      {
        float wx, wy;
        const float X = x + 0.5f - g.gridW() * 0.5f, Z = y + 0.5f - g.gridH() * 0.5f, Y = g.elevation(x, y) * g.stepY();
        if (!g.project3(X, Y, Z, wx, wy)) continue;
        const float room = g.pixelsAt(X, Y, Z) * 2.5f;
        const float px = wx - c.cam.ix(), py = wy - c.cam.iy();
        if (px < -room || py < -room || px > c.layout.vw + room || py > c.layout.vh + room) continue;
      }
      const StandLook& s = *standOf(c, b.cells[cell].terrain);
      // Brightness: the fog's dimming; a wash of the highlight's colour in move / attack range.
      float k = fogged && c.view.dimmed(cell) ? kFogDim : 1.0f;
      Rgba3D wash(1, 1, 1);
      if (!ov.reach.empty() && ov.reach[cell]) wash = Rgba3D(0.62f, 0.76f, 1.0f);
      else if (!ov.attack.empty() && ov.attack[cell]) wash = Rgba3D(1.0f, 0.6f, 0.6f);
      float sx = 0, sz = 0, turn = 0;
      if (s.scatter) standScatter(x, y, sx, sz, turn);
      Spot mid = spot(c, x + 0.5f, y + 0.5f, 0);
      mid.Y = g.elevation(x, y) * g.stepY();

      if (s.isModel()) {
        const ModelLook& ml = s.models[(size_t)pickStandModel(s, x, y)];
        const ModelAsset* asset = models_.get(r3d_, ml.file);
        if (!asset) continue;  // logged once by Models3D; the ground picture stays
        ModelActor* actor = standActor(asset);
        float size = ml.height / asset->height;
        Mat4 place = Mat4::translate(Vec3(mid.X + sx, mid.Y, mid.Z + sz)) *
                     Mat4::rotate(Quat::axisAngle(Vec3(0, 1, 0), (ml.turn + turn) * kPi / 180.0f)) * Mat4::scale(Vec3(size, size, size));
        ModelTint tint;
        tint.dim = k;
        tint.wash = wash;
        drawModel(r3d_, *asset, *actor, place, tint);
        continue;
      }

      const Look& pic = s.picture;
      if (!sheets_.texture(r3d_, pic.sheet)) continue;  // an unusable sheet: only the ground shows
      const SheetDef& sh = b.db->graphics.sheets[(size_t)pic.sheet];
      SpriteCell sc = pickStandCell(s, x, y, ms);
      Material3D m = standFlat(Blend3D::Cutout);
      float uv[4];
      if (!picture(c, pic, sc.col * sh.cellW, sc.row * sh.cellH, sh.cellW, sh.cellH, m, uv)) continue;
      // Width: the tile size is one square, so a tile-wide picture fills the cell's width.
      float wSquares = s.width > 0 ? s.width / (float)std::max(1, g.tile()) : 1.0f;
      float wPx = wSquares * pixels_;
      Spot feet = mid;
      // Towards the back of the cell as the camera sees it; scatter slides it sideways only.
      feet.X += -cx_ * kStandBack + rx_ * sx;
      feet.Z += -cz_ * kStandBack + rz_ * sx;
      uint8_t r = (uint8_t)(255 * k * wash.r), gg = (uint8_t)(255 * k * wash.g), bb = (uint8_t)(255 * k * wash.b);
      upright(batch_.mesh(m), feet, wPx, wPx * sh.cellH / std::max(1, sh.cellW), 0, Tint3D(r, gg, bb, 255), uv);
    }
}

// The one actor every cell of a model shares (the models stand still: rest pose, nothing per cell).
ModelActor* Map3D::standActor(const ModelAsset* asset) {
  for (auto& e : standActors_)
    if (e.first == asset) return &e.second;
  standActors_.emplace_back(asset, ModelActor());
  ModelActor* a = &standActors_.back().second;
  poseActor(r3d_, *asset, *a, -1, 0, false);
  return a;
}

}  // namespace ote
