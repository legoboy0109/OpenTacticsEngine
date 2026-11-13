// Art for a game: sheets (grid images) and which cells draw each terrain
// and unit type (docs/GRAPHICS.md, file data/graphics.json).
//
// The core only describes art; it never decodes or draws pixels. The player
// loads each SheetDef::file as a texture and asks pickCell() which cell to
// draw. A game without graphics.json has no sheets and every Look is None,
// which means "draw the placeholder (colour + letter)".
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ote {

// Largest sheet image, in pixels per side. Chosen so the same art loads on
// old consoles (many cap textures at 2048).
const int kMaxSheetSize = 2048;

struct SheetDef {
  std::string id;
  std::string file;        // PNG inside the game folder, e.g. "art/terrain.png"
  int cellW = 16, cellH = 16;
  int width = 0, height = 0;  // image size in pixels (read from the PNG header)
  int cols = 0, rows = 0;     // whole cells that fit
};

struct SpriteCell {
  int16_t col = 0, row = 0;
  bool operator==(const SpriteCell& o) const { return col == o.col && row == o.row; }
};

// How one thing is drawn.
struct Look {
  enum Kind {
    None,      // no art: placeholder colour + letter
    Still,     // one cell
    Variants,  // several cells; each map square always shows the same one
    Animated,  // several cells played in a loop
    Join       // 16 pieces in a 4x4 block; the piece depends on the neighbours
  };
  Kind kind = None;
  int sheet = -1;                 // index into Graphics::sheets
  std::vector<SpriteCell> cells;  // Still: 1, Variants/Animated: 2+, Join: 1 (top-left of the block)
  int frameMs = 400;              // Animated: time per frame
  // Join only: per terrain index, 1 = counts as "the same" when choosing a
  // piece (always 1 for the terrain itself).
  std::vector<uint8_t> joinWith;
  bool joinEdge = true;           // Join only: the map edge counts as a neighbour

  bool any() const { return kind != None; }
};

struct UnitLook {
  Look base;
  // Different art for particular sides, by map side id. Checked first.
  std::vector<std::pair<std::string, Look>> sides;

  const Look& forSide(const std::string& sideId) const {
    for (const auto& s : sides)
      if (s.first == sideId) return s.second;
    return base;
  }
};

// One complete set of looks.
struct GraphicsSet {
  std::vector<Look> terrain;    // per terrain index
  std::vector<UnitLook> units;  // per unit type index
};

struct Graphics {
  std::vector<SheetDef> sheets;
  // Draw a small base in the side's colour under units that have art, so
  // sides can be told apart whatever the art looks like.
  bool unitMarker = true;
  // Per visual style index (same order as Database::styles), already merged:
  // the style's own entries on top of the entries shared by all styles.
  // Empty when the game has no graphics.json.
  std::vector<GraphicsSet> styles;

  bool any() const { return !sheets.empty(); }
  int findSheet(const std::string& id) const;
  // The set for a style index, or null when the game has no art.
  const GraphicsSet* set(int styleIndex) const {
    if (styles.empty()) return nullptr;
    return &styles[(size_t)(styleIndex >= 0 && styleIndex < (int)styles.size() ? styleIndex : 0)];
  }
};

// Neighbour bits for Join looks.
enum JoinBit { JoinN = 1, JoinE = 2, JoinS = 4, JoinW = 8 };

// The sheet cell to draw for `look` on map square (x, y).
//   joinMask: JoinBit set of neighbours that count as the same (Join only).
//   timeMs:   running time, for Animated.
// Join pieces sit in a 4x4 block: piece n (the mask value 0..15) is at
// column n % 4, row n / 4, counted from the look's top-left cell.
// Must not be called for Look::None.
SpriteCell pickCell(const Look& look, int x, int y, int joinMask, uint32_t timeMs);

// Reads the pixel size from a PNG file's header. False (with a reason in
// err, no file name) if the file is missing or is not a PNG.
bool readPngSize(const std::string& path, int& width, int& height, std::string& err);

}  // namespace ote
