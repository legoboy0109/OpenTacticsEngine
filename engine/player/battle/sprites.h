// Art in the battle scene: sheet textures of the loaded game, and drawing of
// terrain squares and units with them (rules: docs/GRAPHICS.md "Drawing rules").
// A thing without art (or whose sheet failed to decode) gets no call here and
// keeps its placeholder look.
#pragma once
#include <string>
#include <vector>

#include "core/battle/battle.h"
#include "core/data/graphics.h"
#include "platform/platform.h"

namespace ote {

struct DrawCtx;

// The textures of one game's sheets. Loaded when a game session starts,
// released when it ends or the game changes. Does not free anything in its
// destructor (the renderer may already be gone); call release() first.
class Sprites {
 public:
  // Decodes every sheet file of the game (logs one error per bad sheet).
  void load(Renderer& r, const std::string& gameDir, const Graphics& g);
  void release(Renderer& r);
  // Texture of a sheet, 0 when the sheet is not available.
  TextureId texture(int sheet) const {
    return sheet >= 0 && (size_t)sheet < tex_.size() ? tex_[(size_t)sheet] : 0;
  }
  bool any() const { return !tex_.empty(); }
  bool has(const Look& look) const { return look.any() && texture(look.sheet) != 0; }

 private:
  std::vector<TextureId> tex_;
};

// The look of a terrain / of a unit in the style being drawn, or null when it
// has none usable (no art, or its sheet is missing).
const Look* terrainArt(const DrawCtx& c, int terrain);
const Look* unitArt(const DrawCtx& c, const Unit& u);

// Draws the art of the square `cell` into the rectangle (stretched, nearest).
// The caller has filled the placeholder colour under it already. `look` comes
// from terrainArt().
void drawTerrainArt(Renderer& r, const DrawCtx& c, const Look& look, Cell cell, int dx, int dy, int dw, int dh);

// Draws a unit with art on the square whose top-left is (sx, sy): side marker,
// then the sprite. `scale` < 1 is the pop-in; `alpha` 0..1; `done` = has acted.
void drawUnitArt(Renderer& r, const DrawCtx& c, const Look& look, const Unit& u, int sx, int sy, float scale,
                 float alpha, bool done);

}  // namespace ote
