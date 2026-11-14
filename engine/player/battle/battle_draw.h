// Drawing functions of the battle scene (all take the pieces they need, no
// state of their own).
#pragma once
#include <cmath>
#include <string>
#include <vector>

#include "battle/battle_view.h"
#include "battle/iso_tiles.h"
#include "battle/layout.h"
#include "battle/map3d.h"
#include "battle/map_proj.h"
#include "battle/sprites.h"
#include "core/battle/battle.h"
#include "platform/platform.h"

namespace ote {

struct DrawCtx {
  const Battle& battle;
  const BattleView& view;
  const Layout& layout;
  const Camera& cam;
  float time = 0;
  const StyleDef* style = nullptr;  // the visual style in use (null = defaults)
  const Sprites* sprites = nullptr;  // the game's sheet textures (null = placeholders only)
  int styleIndex = 0;                // index of `style` in Database::styles (picks the art set)
  const MapProj* proj = nullptr;     // where squares are on screen (null = plain top-down)
  IsoTiles* tiles = nullptr;         // pictures the isometric view makes for itself
  Map3D* map3d = nullptr;            // the drawer of the 3D kinds of view (null in the others)

  // A spot in flat map pixels (what BattleView animates) -> screen pixels.
  void toScreen(float fx, float fy, int& sx, int& sy) const {
    float wx = fx, wy = fy;
    if (proj) proj->toWorld(fx, fy, wx, wy);
    sx = (int)std::lround(wx) - cam.ix();
    sy = (int)std::lround(wy) - cam.iy();
  }
  bool iso() const { return proj && proj->iso() && tiles; }
  bool view3d() const { return proj && proj->view3d() && map3d; }  // 3D isometric or full 3D
  // Squares are shown at their heights and units stand upright on them.
  bool raised() const { return iso() || view3d(); }
};

// The side whose colour and art a unit is drawn with: its own, except while the view has not yet
// played the SideChanged entry that moved it (BattleView::push keeps the old side until then).
inline int shownSide(const DrawCtx& c, const Unit& u) {
  if ((size_t)u.id < c.view.units.size() && c.view.units[(size_t)u.id].side >= 0) return c.view.units[(size_t)u.id].side;
  return u.side;
}

// Fill colour of a terrain under the visual style in use (the terrain's own colour unless the style recolors it).
inline uint32_t terrainRgb(const DrawCtx& c, int terrain) {
  const StyleDef* st = c.style;
  size_t t = (size_t)terrain;
  if (st && t < st->terrainColors.size() && st->terrainColors[t] >= 0) return (uint32_t)st->terrainColors[t];
  return c.battle.db->terrains[t].color;
}

// Fill colour of a structure's placeholder block under the visual style in use.
inline uint32_t structureRgb(const DrawCtx& c, int structureType) {
  const StyleDef* st = c.style;
  size_t t = (size_t)structureType;
  if (st && t < st->structureColors.size() && st->structureColors[t] >= 0) return (uint32_t)st->structureColors[t];
  return c.battle.db->structures[t].color;
}

// Per-cell highlight masks for a selected unit (empty = nothing highlighted).
struct Overlay {
  std::vector<uint8_t> reach, attack;
  bool any() const { return !reach.empty(); }
};

// Terrain, then structures and units together (sorted by the row of their
// bottom edge), overlays, capture numbers, cursor. Draws beyond the viewport edge
// on purpose; the panel and hint strip are painted afterwards to cover it.
void drawMap(Renderer& r, const DrawCtx& c, const Overlay& overlay, Cell cursor, bool showCursor);

// The same for the isometric kind of view (iso_draw.cpp; rules: docs/VIEWS.md).
void drawMapIso(Renderer& r, const DrawCtx& c, const Overlay& overlay, Cell cursor, bool showCursor);

// One structure (picture or placeholder block, owner frame, highlight re-tint on
// its footprint). Lives in structure_draw.cpp; drawMap calls it in sorted order.
void drawStructure(Renderer& r, const DrawCtx& c, int structure, int camX, int camY, const Overlay& overlay);

// Floating numbers; call once with screenSpace=false (before the panel) and
// once with true (after it).
void drawFloats(Renderer& r, const DrawCtx& c, bool screenSpace);
void drawBanner(Renderer& r, const Layout& L, const std::string& text, Color color, int scale, const std::string& sub);
void drawLevelUp(Renderer& r, const Layout& L, const LevelUpPanel& p);

// Info panel (right side) and the hint strip (bottom).
void drawPanel(Renderer& r, const DrawCtx& c, Cell cursor, bool autoplay);
void drawHint(Renderer& r, const Layout& L, const std::string& hint);

// Screen position where resource gain floats should appear.
void panelResourceAnchor(const Layout& L, int resourceCount, int& x, int& y);

// Combat forecast box for attacker -> defender.
void drawForecast(Renderer& r, const DrawCtx& c, int attacker, int defender, int weapon, Cell targetCell);

}  // namespace ote
