// Tilesets for hand-painted maps (docs/GRAPHICS.md "Tile layer (hand-painted maps)"). A tileset is a sheet whose cells are the tiles,
// numbered row by row from 0; a map's tile layer says per square which tile to draw instead of the terrain's generic look. Presentation
// only: the rules never see tiles.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/data/sprite_cell.h"

namespace ote {

// A tile that cycles through other tiles of its sheet (water, lava).
struct TileAnim {
  int tile = 0;             // the tile (index) that is animated
  std::vector<int> frames;  // tiles shown in turn (indices of the same sheet)
  int ms = 200;             // time per frame
};

struct TilesetDef {
  std::string id;
  int sheet = -1;                // index into Graphics::sheets
  int cols = 0, rows = 0;        // the sheet's cell grid (copied by the loader so a tile index needs no sheet lookup)
  std::vector<TileAnim> anims;   // sorted by tile

  int count() const { return cols * rows; }
  // The tile to draw for `tile` at running time `timeMs` (animated tiles cycle; others are themselves).
  int frame(int tile, uint32_t timeMs) const {
    for (const TileAnim& a : anims)
      if (a.tile == tile && !a.frames.empty()) return a.frames[(timeMs / (uint32_t)(a.ms < 1 ? 1 : a.ms)) % a.frames.size()];
    return tile;
  }
  // The sheet cell of tile index `tile`.
  SpriteCell cell(int tile) const {
    SpriteCell c;
    c.col = (int16_t)(tile % cols);
    c.row = (int16_t)(tile / cols);
    return c;
  }
};

const int kMaxTilesetCells = 65536;  // a tile index is four hex digits in a map file
const int kNoTile = -1;

}  // namespace ote
