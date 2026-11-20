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

// The unit type a unit is drawn as: its own, except while the view has not yet played the UnitPromoted entry that
// changed it (the sprite changes in the middle of that animation; battle_view_promotion.cpp).
inline int shownType(const DrawCtx& c, const Unit& u) {
  if ((size_t)u.id < c.view.units.size() && c.view.units[(size_t)u.id].type >= 0) return c.view.units[(size_t)u.id].type;
  return u.type;
}

// ---- Fog of war (docs/ECONOMY.md "Fog of war > Player"). The view knows who the screen is drawn for; with no fog
// (or the maker's show-all) none of these changes anything.
constexpr float kFogDim = 0.55f;  // brightness of a square the viewer does not see (the 2D veil is black with alpha 115)
constexpr int kFogDimAlpha = 115;
inline Color fogDim(Color c) {
  return Color((uint8_t)(c.r * kFogDim), (uint8_t)(c.g * kFogDim), (uint8_t)(c.b * kFogDim), c.a);
}
// A structure none of whose squares the viewer sees (so its picture is dimmed too).
inline bool structureDimmed(const DrawCtx& c, int structure) {
  if (!c.view.fogged()) return false;
  std::vector<Cell> cells;
  c.battle.structureCells(structure, cells);  // every square / hexagon it covers
  for (Cell cell : cells)
    if (cell != kNoCell && !c.view.dimmed(cell)) return false;
  return true;
}
// Which kind a structure is drawn as: for a viewer the kind it last saw (an upgrade done out of its sight stays
// hidden), else the true one.
inline int shownKind(const DrawCtx& c, int structure) {
  return c.view.fogged() ? c.battle.seenKind(c.view.viewerTeam(), structure) : c.battle.structures[(size_t)structure].type;
}
// Whose colours a structure is drawn in: for a viewer the owner it last saw (not the truth), else its owner.
inline int shownOwner(const DrawCtx& c, int structure) {
  return c.view.fogged() ? c.battle.seenOwner(c.view.viewerTeam(), structure) : c.battle.structures[(size_t)structure].owner;
}

// The structure as the viewer pictures it: position as it is, kind and owner as last seen. Drawing code reads
// everything that looks like the structure (art, model, colour, name) from this copy, never from the truth.
inline StructureState shownStructure(const DrawCtx& c, int structure) {
  StructureState s = c.battle.structures[(size_t)structure];
  s.type = shownKind(c, structure);
  s.owner = shownOwner(c, structure);
  return s;
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

// Per-cell highlight masks for a selected unit (empty = nothing highlighted), and, apart, the reach of the enemies the player marked
// (enemy_ranges.h): `foeMove` / `foeAttack` point at the masks of the scene's EnemyRanges (null = none; both set, one entry per cell).
// Every kind of view reads a square's tint through toneOf(), so the five views cannot differ: the player's own blue / red win over the
// enemies' violet / orange on one square.
struct Overlay {
  std::vector<uint8_t> reach, attack;
  const std::vector<uint8_t>* foeMove = nullptr;
  const std::vector<uint8_t>* foeAttack = nullptr;
  // Speed turn order (docs/TURN_ORDER.md "Player"): the squares of the units that are charging an action and the squares those actions are aimed
  // at (null = none; one entry per cell). Read through toneOf like the rest, so every kind of view paints them.
  const std::vector<uint8_t>* chargeFrom = nullptr;
  const std::vector<uint8_t>* chargeAim = nullptr;
  bool any() const { return !reach.empty() || foeMove || chargeFrom; }
};

enum class Tone : uint8_t { None, Move, Attack, FoeMove, FoeAttack, ChargeFrom, ChargeAim };

inline Tone toneOf(const Overlay& ov, Cell cell) {
  if (!ov.reach.empty()) {
    if (ov.reach[(size_t)cell]) return Tone::Move;
    if (ov.attack[(size_t)cell]) return Tone::Attack;
  }
  if (ov.chargeAim && (*ov.chargeAim)[(size_t)cell]) return Tone::ChargeAim;
  if (ov.chargeFrom && (*ov.chargeFrom)[(size_t)cell]) return Tone::ChargeFrom;
  if (ov.foeMove) {
    if ((*ov.foeMove)[(size_t)cell]) return Tone::FoeMove;
    if ((*ov.foeAttack)[(size_t)cell]) return Tone::FoeAttack;
  }
  return Tone::None;
}

// The colour of a tone (not None): blue / red for the player's own unit, violet / orange for what an enemy can reach / hit.
struct ToneRgb { uint8_t r, g, b; };
inline ToneRgb toneRgb(Tone t) {
  switch (t) {
    case Tone::Move: return {50, 110, 255};
    case Tone::Attack: return {255, 50, 50};
    case Tone::FoeMove: return {150, 80, 230};
    case Tone::ChargeFrom: return {30, 200, 190};  // a unit that is charging an action (teal)
    case Tone::ChargeAim: return {235, 60, 190};   // the square (or unit) it will hit (magenta)
    default: return {255, 170, 20};
  }
}
// The enemies' tones are drawn a little more opaque: violet and orange over green ground would otherwise read as grey and olive.
inline uint8_t toneAlpha(Tone t, int alpha) {
  return (uint8_t)(t == Tone::FoeMove || t == Tone::FoeAttack || t == Tone::ChargeFrom || t == Tone::ChargeAim ? alpha + 50 : alpha);
}
inline Color toneColor(Tone t, int alpha) {
  const ToneRgb c = toneRgb(t);
  return Color(c.r, c.g, c.b, toneAlpha(t, alpha));
}

// Terrain, then structures and units together (sorted by the row of their
// bottom edge), overlays, capture numbers, cursor. Draws beyond the viewport edge
// on purpose; the panel and hint strip are painted afterwards to cover it.
void drawMap(Renderer& r, const DrawCtx& c, const Overlay& overlay, Cell cursor, bool showCursor);

// The same for the isometric kind of view (iso_draw.cpp; rules: docs/VIEWS.md).
void drawMapIso(Renderer& r, const DrawCtx& c, const Overlay& overlay, Cell cursor, bool showCursor);

// The same for a hexagon map (hex_iso_draw.cpp; rules: docs/HEX.md "2D isometric").
void drawMapHexIso(Renderer& r, const DrawCtx& c, const Overlay& overlay, Cell cursor, bool showCursor);

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
// The commander block of the panel for `side` (commander_panel.cpp): a rule, name and meter numbers, the bar with a tick per power, the active power.
// Advances `y` by what it drew and returns that height (0 for a side without a commander).
int drawCommanderMeter(Renderer& r, const DrawCtx& c, int side, int x, int& y, int w);

// Combat forecast box for attacker -> defender.
// `extra`: one more line along the foot of the box (an area attack's other victims; empty = none).
void drawForecast(Renderer& r, const DrawCtx& c, int attacker, int defender, int weapon, Cell targetCell, const std::string& extra = std::string());
// The box of an area attack aimed at `cell` (forecast_area.cpp; docs/SRPG.md "Area attacks and line fire"): the usual forecast of the unit on
// the square plus a line for everybody else the attack hits, or, for an empty square, a box of its own.
void drawAreaForecast(Renderer& r, const DrawCtx& c, int attacker, int weapon, Cell cell);
// The compact line for the victims other than the aimed one, "+2 more: 5, 7 dmg" (allies "ally 4"); empty when there are none that can be seen.
std::string areaOthersText(const Battle& b, const AreaForecast& f);

// The forecast box of a staff (staff_forecast.cpp; docs/SRPG.md section 8): what the use would do to each unit hit.
void drawStaffForecast(Renderer& r, const DrawCtx& c, const StaffForecast& f, Cell targetCell);
// The forecast box of an attack on a door (place_forecast.cpp; docs/SRPG.md section 10).
void drawPlaceForecast(Renderer& r, const DrawCtx& c, const PlaceForecast& f, Cell targetCell);

}  // namespace ote
