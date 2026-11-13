// Drawing functions of the battle scene (all take the pieces they need, no
// state of their own).
#pragma once
#include <string>
#include <vector>

#include "battle/battle_view.h"
#include "battle/layout.h"
#include "core/battle/battle.h"
#include "platform/platform.h"

namespace ote {

struct DrawCtx {
  const Battle& battle;
  const BattleView& view;
  const Layout& layout;
  const Camera& cam;
  float time = 0;
};

// Per-cell highlight masks for a selected unit (empty = nothing highlighted).
struct Overlay {
  std::vector<uint8_t> reach, attack;
  bool any() const { return !reach.empty(); }
};

// Terrain, owners, overlays, units, cursor. Draws beyond the viewport edge
// on purpose; the panel and hint strip are painted afterwards to cover it.
void drawMap(Renderer& r, const DrawCtx& c, const Overlay& overlay, Cell cursor, bool showCursor);

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
