// Art for a game: sheets (grid images) and which cells draw each terrain,
// structure and unit type (docs/GRAPHICS.md, file data/graphics.json).
//
// The core only describes art; it never decodes or draws pixels. The player
// loads each SheetDef::file as a texture and asks pickCell() which cell to
// draw. A game without graphics.json has no sheets and every Look is None,
// which means "draw the placeholder (colour + letter)".
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/data/battle_art.h"
#include "core/data/sprite_cell.h"
#include "core/data/tileset.h"
#include "core/grid/grid.h"

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
  bool smooth = false;        // detailed art: the 3D views blend texels (bilinear, mipmaps) instead of hard pixels
};

// Walking frames of a unit picture (docs/GRAPHICS.md "Walking"): used while the unit moves along its path in the views that
// draw pictures. The frames come from their own sheet (its cell size may differ from the standing picture's).
enum WalkDir { WalkDown, WalkUp, WalkRight, WalkLeft, kWalkDirs };  // (the order breaks ties, see WalkLook::pick)
struct WalkLook {
  int sheet = -1;                       // index into Graphics::sheets; -1 = no walking frames
  std::vector<SpriteCell> frames[kWalkDirs];  // empty = this direction is not given
  bool mirrored[kWalkDirs] = {};        // the frames are those of the opposite side, drawn turned round
  int frameMs = 150;                    // time per frame

  bool any() const { return sheet >= 0; }
  // The direction to show for a step whose way on the screen is (dx, dy) (y grows downwards): the given direction nearest to
  // it (the largest dot product; a tie goes to the first of down, up, right, left). -1 when none is given.
  int pick(float dx, float dy) const;
  // The cell of direction `dir` at running time `timeMs` (the frames loop).
  SpriteCell cell(int dir, uint32_t timeMs) const {
    const std::vector<SpriteCell>& f = frames[dir];
    return f[(timeMs / (uint32_t)(frameMs < 1 ? 1 : frameMs)) % f.size()];
  }
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
  // Still: 1. Variants/Animated: 2+. Join: the top-left cell of each 4 x 4
  // block of pieces; more than one block = the join is animated (frameMs).
  std::vector<SpriteCell> cells;
  int frameMs = 400;              // Animated: time per frame
  // Join only: per terrain index, 1 = counts as "the same" when choosing a
  // piece (always 1 for the terrain itself).
  std::vector<uint8_t> joinWith;
  bool joinEdge = true;           // Join only: the map edge counts as a neighbour
  WalkLook walk;                  // units only (2D, 2D isometric and the 3D views' pictures): frames while moving
  // Portraits only (docs/GRAPHICS.md "Portraits"): which way the picture naturally faces, so the stage can turn it towards
  // the middle of the screen. 0 = not said (drawn as it is), 1 = faces left, 2 = faces right.
  int faces = 0;
  // Portraits only: other cells of the same sheet, "talk" (cycled while the speaker's line is being typed) and "blink" (shown for a
  // moment now and then). Empty = the portrait never changes.
  std::vector<SpriteCell> talk, blink;

  bool any() const { return kind != None; }
};

// A 3D model in place of a picture (docs/GRAPHICS.md "Models"; only in styles
// whose kind of view is 3D). The core only carries the description; the
// player loads the file.
enum ModelAnim { AnimIdle, AnimMove, AnimAttack, AnimDie, kModelAnims };
struct ModelLook {
  std::string file;        // .glb / .gltf inside the game folder, e.g. "models/lord.glb"; empty = no model
  float height = 0;        // how tall it stands, in squares; 0 = the standard size
  float turn = 0;          // degrees added to the way it faces (a model that does not face +Z)
  std::string sideMaterial = "side";  // the material that takes the side's colour; empty = none
  std::string clips[kModelAnims] = {"Idle", "Move", "Attack", "Die"};  // animation names in the file

  bool any() const { return !file.empty(); }
};

// The standing part of a terrain in a 3D style (docs/GRAPHICS.md "Standing terrain"): trees, a peak, a wall
// standing on the cell's ground. Either an upright picture that faces the camera or 3D models; the ground
// picture (the terrain's own Look) stays underneath. The core only describes it; the 3D map draws it.
struct StandLook {
  Look picture;                  // an upright picture (cell, variants or frames); kind None = not a picture
  float width = 0;               // picture: how wide, in the game's tile pixels (tileSize = one square); 0 = one square
  std::vector<ModelLook> models; // models: one per variant (all share height and turn); empty = not a model
  bool scatter = false;          // a small deterministic shift of every cell's standing part (and, for models, a turn)

  bool isModel() const { return !models.empty(); }
  bool any() const { return picture.any() || isModel(); }
};

struct UnitLook {
  Look base;
  ModelLook model;  // when set, `base` and `sides` are empty
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
  std::vector<Look> terrain;        // per terrain index
  std::vector<StandLook> stands;    // per terrain index: what stands on the ground (3D styles only; else none)
  std::vector<UnitLook> units;      // per unit type index
  std::vector<UnitLook> structures; // per structure type index (same shape as a unit's look)
  // Portraits of the dialogue box (docs/EVENTS.md): id -> one cell (Look::Still). The id is the
  // line's own `portrait` or the speaking unit's story id. A portrait is a face, not a view-kind
  // dependent picture, so every style starts from the general ones (unlike terrain and units).
  std::vector<std::pair<std::string, Look>> portraits;

  // The portrait with this id, or null.
  const Look* portrait(const std::string& id) const {
    for (const auto& p : portraits)
      if (p.first == id) return &p.second;
    return nullptr;
  }
};

// A full-screen picture of a story scene (docs/GRAPHICS.md "Backgrounds"): the whole image of a sheet, or one cell of it.
struct SceneBackground {
  std::string id;
  int sheet = -1;
  bool whole = true;  // true: the sheet's whole image; false: just `cell`
  SpriteCell cell;
};

// A small picture drawn on the map sprite of the units that carry a tag (docs/GRAPHICS.md "Unit badges"): a boss marker. The same in every style.
struct BadgeDef {
  enum class Sides { All, Computer, Player, List };
  enum class Anchor { TopLeft, TopRight, BottomLeft, BottomRight, Center };
  std::string id;
  int sheet = -1;
  bool whole = true;  // true: the sheet's whole image; false: just `cell`
  SpriteCell cell;
  std::string tag;    // a unit type's tag or a character's tag
  Sides sides = Sides::All;
  std::vector<std::string> sideIds;  // Sides::List: map side ids
  Anchor anchor = Anchor::BottomRight;  // which corner (or the middle) of the unit's square the same corner of the badge is put on
  int dx = 0, dy = 0;  // then moved by this, in pixels of one square of the game's tile size (it grows and shrinks with the map zoom)
};

struct Graphics {
  std::vector<SheetDef> sheets;
  std::vector<BadgeDef> badges;  // root key "badges"
  // Backgrounds of story scenes (root key "backgrounds"; the same for every style). Not the battle scene's (`battle`).
  std::vector<SceneBackground> backgrounds;
  // Tilesets of hand-painted maps (root key "tilesets", the same for every style; docs/GRAPHICS.md "Tile layer").
  std::vector<TilesetDef> tilesets;
  int findTileset(const std::string& id) const {
    for (size_t i = 0; i < tilesets.size(); ++i)
      if (tilesets[i].id == id) return (int)i;
    return -1;
  }
  int findBackground(const std::string& id) const {
    for (size_t i = 0; i < backgrounds.size(); ++i)
      if (backgrounds[i].id == id) return (int)i;
    return -1;
  }
  // Draw a small base in the side's colour under units that have art, so
  // sides can be told apart whatever the art looks like.
  bool unitMarker = true;
  // The side-view battle scene's art (docs/BATTLE_SCENE.md); empty when the game gives none.
  BattleArt battle;
  // Icons (docs/GRAPHICS.md "Icons"): the small picture the player draws before a weapon's or an item's name in a list. One
  // Look::Still per weapon index / item index (kind None = no icon); the same in every style.
  std::vector<Look> weaponIcons, itemIcons;
  const Look* weaponIcon(int w) const { return w >= 0 && (size_t)w < weaponIcons.size() && weaponIcons[(size_t)w].any() ? &weaponIcons[(size_t)w] : nullptr; }
  const Look* itemIcon(int i) const { return i >= 0 && (size_t)i < itemIcons.size() && itemIcons[(size_t)i].any() ? &itemIcons[(size_t)i] : nullptr; }
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

// The JoinBit set of neighbours of square (x, y) that count as "the same" for
// a Join look: the neighbour's terrain index (from terrainAt) is flagged in
// look.joinWith, or the neighbour is off the map (gridW x gridH) and
// look.joinEdge is set. Squares only touch N, E, S, W here.
// terrainAt is any callable (x, y) -> terrain index. A template rather than
// std::function: this runs for every visible square every frame.
template <class TerrainAt>
int joinMaskAt(const Look& look, int x, int y, int gridW, int gridH, const TerrainAt& terrainAt) {
  static const int dx[4] = {0, 1, 0, -1}, dy[4] = {-1, 0, 1, 0};
  static const int bit[4] = {JoinN, JoinE, JoinS, JoinW};
  int mask = 0;
  for (int i = 0; i < 4; ++i) {
    int nx = x + dx[i], ny = y + dy[i];
    if (nx < 0 || ny < 0 || nx >= gridW || ny >= gridH) {
      if (look.joinEdge) mask |= bit[i];
      continue;
    }
    int t = terrainAt(nx, ny);
    if (t >= 0 && (size_t)t < look.joinWith.size() && look.joinWith[(size_t)t]) mask |= bit[i];
  }
  return mask;
}

// The same for a hexagon game (docs/HEX.md "Join pieces on hexagons"): bit i is set when the neighbour in
// direction i (Grid::neighbor order) counts as the same, or is off the map and look.joinEdge is set.
// terrainAt is any callable (Cell) -> terrain index. Pass the result to pickCell with blockSide 8.
template <class TerrainAt>
int joinMaskHex(const Look& look, const Grid& grid, Cell c, const TerrainAt& terrainAt) {
  int mask = 0;
  for (int i = 0; i < 6; ++i) {
    Cell nb = grid.neighbor(c, i);
    if (nb == kNoCell) {
      if (look.joinEdge) mask |= 1 << i;
      continue;
    }
    int t = terrainAt(nb);
    if (t >= 0 && (size_t)t < look.joinWith.size() && look.joinWith[(size_t)t]) mask |= 1 << i;
  }
  return mask;
}

// Side of a join block in sheet cells: 4 (16 pieces) on squares, 8 (64 pieces) on hexagons.
inline int joinBlockSide(const Grid& grid) { return grid.isHex() ? 8 : 4; }

// The sheet cell to draw for `look` on map square (x, y).
//   joinMask: JoinBit set of neighbours that count as the same (Join only).
//   timeMs:   running time, for Animated.
// Join pieces sit in a 4x4 block: piece n (the mask value 0..15) is at
// column n % 4, row n / 4, counted from the look's top-left cell. Hexagon games use
// blockSide 8: piece n (0..63) at column n % 8, row n / 8.
// Must not be called for Look::None.
SpriteCell pickCell(const Look& look, int x, int y, int joinMask, uint32_t timeMs, int blockSide = 4);

// The standing part of terrain on map square (x, y), chosen exactly like terrain variants (a stable pick from the
// position, so a forest is not a row of identical trees and nothing flickers).
//   pictures: the sheet cell (variants by position, frames by time);
//   models:   the index into look.models.
SpriteCell pickStandCell(const StandLook& look, int x, int y, uint32_t timeMs);
int pickStandModel(const StandLook& look, int x, int y);
// The deterministic scatter of square (x, y): a shift on the ground in squares (-0.18 .. 0.18 each way) and a
// turn in degrees (0 .. 360). Only meaningful for a look with `scatter`.
void standScatter(int x, int y, float& dx, float& dz, float& turnDeg);

// Reads the pixel size from a PNG file's header. False (with a reason in
// err, no file name) if the file is missing or is not a PNG.
bool readPngSize(const std::string& path, int& width, int& height, std::string& err);

}  // namespace ote
