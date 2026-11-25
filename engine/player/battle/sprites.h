// Art in the battle scene: sheet textures of the loaded game, and drawing of
// terrain squares, structures and units with them (rules: docs/GRAPHICS.md "Drawing rules").
// A thing without art (or whose sheet failed to decode) gets no call here and
// keeps its placeholder look.
#pragma once
#include <string>
#include <vector>

#include "core/battle/battle.h"
#include "battle/art_budget.h"
#include "core/data/graphics.h"
#include "platform/common/image.h"
#include "platform/platform.h"

namespace ote {

struct DrawCtx;

// The textures of one game's sheets, made when first asked for and dropped again, least recently used first, when
// the art held goes over the platform's budget (artBudgetMB(); docs/GRAPHICS.md "Art memory"). Loaded when a game
// session starts (which only checks the sheet files' headers, nothing is decoded), released when it ends or the game
// changes. Does not free anything in its destructor (the renderer may already be gone); call release() first.
//
// The lookups are const and fill a cache (mutable): drawing code asks, and a sheet's first use decodes its PNG then.
// A TextureId handed out stays valid until the end of the frame (the caller of beginFrame() ends frames); after
// that the texture may be gone, so never keep one across frames: ask again.
class Sprites {
 public:
  // Checks every sheet file's header (exists, is a PNG, is at least one cell big; logs one error per bad sheet) and
  // prepares the cache. Decodes nothing.
  void load(Renderer& r, const std::string& gameDir, const Graphics& g);
  void release(Renderer& r);
  // A new frame begins (once per frame, before anything draws): what the last frame used may now be dropped.
  void beginFrame() { budget_.beginFrame(); }
  // Texture of a sheet, 0 when the sheet is not available (bad file, failed decode: logged once, never retried).
  TextureId texture(int sheet) const;
  // The sheet drawn from right to left (the battle scene's fighter on the right-hand side), only for the sheets the battle
  // scene's animation sets and effects use; 0 when there is none. Whole cells only: cell column c of the sheet is column
  // (cols - 1 - c) here, and the pixels right of the last whole cell are not part of it. Made when first asked for.
  TextureId mirrored(int sheet) const;
  // The decoded pixels of a sheet a tileset uses (the isometric view builds tile pictures from them); null for other sheets.
  // Kept (and counted in the budget) only for those sheets, as long as the sheet is held; valid until the end of the frame.
  const Image* pixels(int sheet) const;
  // Makes the texture of a sheet now (and its mirrored copy with `mirror`), with one decode: for a scene that is about to
  // use it, so the work happens during its transition and not in the middle of an animation. Same as asking for it.
  void request(int sheet, bool mirror = false) const;
  bool any() const { return !slots_.empty(); }
  bool has(const Look& look) const { return look.any() && texture(look.sheet) != 0; }
  // Drops everything not used in this frame (the OS asks for memory back). Textures are made again when drawn.
  void trimUnused() {
    budget_.beginFrame();  // (called between frames: what the last frame drew may go as well)
    trim(0);
  }
  // Art held right now (bytes) and the budget.
  uint64_t bytesHeld() const { return budget_.total(); }
  uint64_t budgetBytes() const { return budget_.budget(); }
  void setBudgetBytes(uint64_t bytes) { budget_.setBudget(bytes); }  // (tests; the player takes artBudgetMB() at load)

 private:
  struct Slot {
    bool bad = false;         // the file or its size is unusable, or the decode failed (the error is logged): no texture, ever
    bool mirrorOk = false;    // the sheet is drawn mirrored by the battle scene (mirrored() answers only for these)
    bool keepPixels = false;  // a tileset uses it: the decoded pixels stay while the sheet is held
    bool mirrorTried = false; // the mirrored copy was made (or failed): do not try again until the sheet is dropped
    int w = 0, h = 0;         // the image's size (from the header)
    int cols = 0, cellW = 0;  // for the mirrored copy
    TextureId tex = 0, mir = 0;
    Image pix;
  };
  void trim(uint64_t limit) const;  // drops least recently used sheets (not used this frame) until the total is within limit
  // Decodes the sheet and makes what is missing: the texture, and with `mirror` the mirrored copy. False when the sheet is bad.
  bool make(size_t i, bool mirror) const;
  uint64_t heldBytes(const Slot& s) const;
  void drop(Slot& s) const;

  mutable std::vector<Slot> slots_;
  mutable ArtBudget budget_;
  Renderer* r_ = nullptr;
  std::string dir_;
  const Graphics* g_ = nullptr;
};

// The look of a terrain / of a unit in the style being drawn, or null when it
// has none usable (no art, or its sheet is missing).
const Look* terrainArt(const DrawCtx& c, int terrain);
const Look* unitArt(const DrawCtx& c, const Unit& u);
// The look of a structure (its owner's own picture if the game gives one).
const Look* structureArt(const DrawCtx& c, const StructureState& s);

// Where a picture sits in its sheet's texture, for drawers that place it
// themselves (the isometric view). tex 0 = nothing to draw.
struct SpriteSrc {
  TextureId tex = 0;
  int x = 0, y = 0, w = 0, h = 0;
};
// The piece of a terrain look for the square `cell` (neighbours and time pick it).
SpriteSrc terrainSrc(const DrawCtx& c, const Look& look, Cell cell);
// The piece of a unit or structure look at the battle's running time.
SpriteSrc lookSrc(const DrawCtx& c, const Look& look);

// Draws the art of the square `cell` into the rectangle (stretched, nearest).
// The caller has filled the placeholder colour under it already. `look` comes
// from terrainArt().
void drawTerrainArt(Renderer& r, const DrawCtx& c, const Look& look, Cell cell, int dx, int dy, int dw, int dh);

// Draws a structure picture across its footprint whose top-left is (sx, sy) and
// which is fw x fh pixels: width = fw, height keeps the cell's proportions, the
// bottom edge sits on the footprint's bottom edge (so tall pictures stick out
// above it). `look` comes from structureArt(). Frames of an animated look follow
// the battle's running time.
void drawStructureArt(Renderer& r, const DrawCtx& c, const Look& look, int sx, int sy, int fw, int fh);

// The picture of a unit right now: its walking frame while it moves along its path (and its look has walk frames), else the
// standing one. `src` is cut from sheet `sheet` (always the normal texture; `flip` = draw it turned round). `k` is the picture's
// width relative to the standing picture's cell (1 for standing; a walk sheet with bigger cells gives more), so that both are
// drawn at the same pixels per art pixel and the walking figure is not shrunk. Docs: docs/GRAPHICS.md "Walking".
struct UnitPic {
  SpriteSrc src;
  int sheet = -1;
  bool flip = false;
  float k = 1;
};
// While the unit strikes, is hit or falls and its look has that pose, the pose's frame; standing with a known facing and a look with
// "facing" frames, the frame of that facing (docs/GRAPHICS.md "Poses and facing").
UnitPic unitPic(const DrawCtx& c, const Look& look, const Unit& u);
// Where `pic` drawn `drawnW` pixels wide moves by its sheet's "unitOffset" (docs/GRAPHICS.md "Unit offset"), in screen pixels.
void unitPicOffset(const DrawCtx& c, const UnitPic& pic, int drawnW, int& ox, int& oy);
// The texture and source rectangle for drawing `pic` with the 2D renderer (a turned-round picture comes from the sheet's mirrored copy).
SpriteSrc drawSrc(const DrawCtx& c, const UnitPic& pic);

// Draws a unit with art on the square whose top-left is (sx, sy): side marker,
// then the sprite. `scale` < 1 is the pop-in; `alpha` 0..1; `done` = has acted.
void drawUnitArt(Renderer& r, const DrawCtx& c, const Look& look, const Unit& u, int sx, int sy, float scale,
                 float alpha, bool done);

}  // namespace ote
