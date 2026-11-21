// A map's tile grid as plain data (Qt-free, so the native editor's painter and the MCP server can share it): the terrain rows
// with their legend, the optional `heights` and the optional `tileLayer`, read from and written back to a map file's Json.
// Nothing here touches files or knows about undo: callers read a grid, change it, and write back only the pieces that changed.
#pragma once
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "core/base/json.h"

namespace ote_editor {

struct MapGrid {
  int width = 0, height = 0;
  bool hex = false;
  bool flatTop = false;  // hexagons only: game.json grid.layout == "flat" (columns run straight down)
  std::vector<std::pair<char, std::string>> legend;  // symbol -> terrain id, in file order
  std::vector<std::string> rows;                      // `tiles`: `height` strings of `width` symbols
  bool hasHeights = false;
  std::vector<int> heights;  // width * height, row by row (empty without `heights`)
  bool hasLayer = false;
  std::string layerTileset;
  std::vector<int> layer;  // width * height tile numbers, -1 = no tile (empty without `tileLayer`)

  bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < width && y < height; }
  char symbolAt(int x, int y) const { return rows[(size_t)y][(size_t)x]; }
  // The terrain id a symbol stands for, "" when the legend does not know it.
  std::string terrainOf(char symbol) const;
  std::string terrainAt(int x, int y) const { return terrainOf(symbolAt(x, y)); }
  // The first symbol that names `terrain`, or 0.
  char symbolFor(const std::string& terrain) const;
  int heightAt(int x, int y) const { return hasHeights ? heights[(size_t)y * (size_t)width + (size_t)x] : 0; }
  int layerAt(int x, int y) const { return hasLayer ? layer[(size_t)y * (size_t)width + (size_t)x] : -1; }
};

// Reads grid, legend, tiles, heights and tileLayer. False with a plain-words reason when a part is not in the shape the loader
// wants (the painter then stays out of the way and the tree view still works).
bool readMapGrid(const ote::Json& map, bool flatTop, MapGrid& out, std::string& err);

// The pieces as they are written to the file. A tileLayer row is the 4-hex-digit string form ("...." = no tile).
ote::Json tilesRowJson(const MapGrid& g, int y);
ote::Json tilesJson(const MapGrid& g);
ote::Json heightsRowJson(const MapGrid& g, int y);
ote::Json heightsJson(const MapGrid& g);
ote::Json layerRowJson(const MapGrid& g, int y);
ote::Json layerCellsJson(const MapGrid& g);

// The legend symbol for painting `terrain`: the one that already names it, else a free one chosen in the map's own style (the
// kind of symbols its legend mostly uses: lower-case letters, upper-case letters, digits or punctuation). Preference: the
// terrain's own glyph (data/terrain.json, may be 0), then letters of its id, then the rest of the style. `isNew` says the
// caller must add the symbol to the legend. Returns 0 when no printable symbol is left.
char legendSymbolFor(const MapGrid& g, const std::string& terrain, char glyphHint, bool& isNew);

// The cells (row-major indexes) joined to (x, y) through neighbours that show the same symbol; hexagon-aware.
std::vector<int> floodRegion(const MapGrid& g, int x, int y);

// ---- resizing ----
struct ResizeSpec {
  int width = 1, height = 1;
  int anchorX = 0, anchorY = 0;  // which part of the old map stays put: 0 start (left/top), 1 middle, 2 end (right/bottom)
  char fill = '.';               // symbol of the new squares (must be in the legend)
};
// Size a structure covers (data/structures.json width / height, hexagons: rings 0 or 1).
struct Footprint {
  int w = 1, h = 1, rings = 0;
};
// Something that does not fit the new map and is removed with it.
struct LostThing {
  std::string kind;   // "unit", "structure", "preparation slot", "map change square"
  std::string label;  // "knight (gilda)", "city"; empty when the entry names nothing (a deployment slot)
  int x = 0, y = 0;   // where it stood in the old map
};
struct ResizeResult {
  int dx = 0, dy = 0;                                    // how far the old map moved inside the new one
  std::vector<std::pair<std::string, ote::Json>> set;    // top-level keys of the map file whose value changes, in file order
  std::vector<LostThing> lost;
};
// Where the old map's top-left square lands in the new map. Hexagon maps only move by whole pairs of rows (pointy) or columns (flat),
// because odd rows / columns are drawn half a step over and an odd move would change the picture.
void resizeOffset(const MapGrid& g, const ResizeSpec& spec, int& dx, int& dy);
// Works out everything a resize changes: grid, tiles, heights, tileLayer (kept consistent), and the x / y of units, structures,
// preparation slots and map-change squares (moved with the anchor, removed when they no longer fit: `lost`). Events' own
// coordinates are not touched. Does not modify `map`.
bool resizeMap(const ote::Json& map, const MapGrid& g, const ResizeSpec& spec, const std::map<std::string, Footprint>& footprints,
               ResizeResult& out, std::string& err);

}  // namespace ote_editor
