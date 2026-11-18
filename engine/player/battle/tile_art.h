// The tile layer of a map in the player (docs/GRAPHICS.md "Tile layer (hand-painted maps)"): a square that has a tile is drawn from the
// map's tileset instead of its terrain's generic look. Presentation only. A square whose tile cannot be drawn (the sheet failed to
// load) falls back to the terrain's look.
#pragma once
#include "battle/battle_draw.h"
#include "battle/sprites.h"

namespace ote {

// The picture of the tile on `cell` at the battle's running time (animated tiles cycle): its texture and the rectangle of its sheet.
// False when the map has no tile layer, the square has no tile, or the sheet has no texture. Cheap when the map has no layer.
bool tileSrc(const DrawCtx& c, Cell cell, SpriteSrc& out);
// Does `cell` show a tile (the 3D ground and the standing parts ask this)?
bool hasTileArt(const DrawCtx& c, Cell cell);
// The tileset sheet's index of the map (for the 3D drawer's textures).
int tileSheet(const DrawCtx& c);
// 2D view: the tile stretched into the rectangle (nearest), like terrain art.
void drawTileArt(Renderer& r, const DrawCtx& c, const SpriteSrc& src, int dx, int dy, int dw, int dh);
// Isometric view: a block whose top face is the tile laid flat (projected onto the diamond, drawn over `sideRgb`), side faces
// `sideH` pixels high; same size as IsoTiles::block(1, 1). Cached in `c.tiles` per tile, size and side colour.
IsoPicture isoTileBlock(Renderer& r, const DrawCtx& c, const IsoGeometry& g, Cell cell, int sideH, uint32_t sideRgb);

}  // namespace ote
