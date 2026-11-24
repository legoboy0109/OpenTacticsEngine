// Frames per direction of a unit picture in data/graphics.json: "walk" (docs/GRAPHICS.md "Walking") and the poses "facing",
// "attack", "hurt" and "ko" (docs/GRAPHICS.md "Poses and facing"), which share walk's shape. Also the cell helpers the other
// graphics readers use.
#include "core/data/loader_graphics_art.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {
std::string q(const std::string& s) { return "\"" + s + "\""; }
}  // namespace

bool readArtCell(const LoadCtx& c, const Json& j, const std::string& where, const char* key, SpriteCell& out) {
  if (!j.isArray() || j.size() != 2 || !LoadCtx::isWhole(j[0]) || !LoadCtx::isWhole(j[1]) || j[0].asInt() < 0 ||
      j[1].asInt() < 0 || j[0].asInt() > 30000 || j[1].asInt() > 30000)
    return c.fail(where, q(key) + " must be [column, row] (whole numbers, counted from 0)");
  out.col = (int16_t)j[0].asInt();
  out.row = (int16_t)j[1].asInt();
  return true;
}

bool checkCellsInside(const LoadCtx& c, const SheetDef& sh, SpriteCell at, int w, int h, const std::string& where) {
  if (at.col + w <= sh.cols && at.row + h <= sh.rows) return true;
  std::string what = (w == 1 && h == 1) ? "cell [" + std::to_string(at.col) + ", " + std::to_string(at.row) + "]"
                                        : "the " + std::to_string(w) + " x " + std::to_string(h) +
                                              " block at [" + std::to_string(at.col) + ", " +
                                              std::to_string(at.row) + "]";
  return c.fail(where, what + " is outside sheet " + q(sh.id) + " (" + std::to_string(sh.cols) + " x " +
                           std::to_string(sh.rows) + " cells)");
}

// Keys: "sheet" (default: the picture's own), "down", "up", "side" (faces right; the left is it turned round), "left" / "right"
// (own frames instead), the diagonals "downLeft", "downRight", "upLeft", "upRight" (a missing one of a pair is the other turned
// round), "frameMs". Each direction is a list of [column, row] cells of the sheet. At least one direction is needed; one that is
// not given is not drawn (the nearest given direction is used instead).
bool readWalk(const LoadCtx& c, const Database& db, const Json& j, const std::string& where, int standSheet, WalkLook& out,
              const char* name) {
  out = WalkLook();
  const std::string w = where + " " + name;
  static const char* const kKnown[] = {"sheet", "down", "up", "side", "left", "right", "downLeft", "downRight", "upLeft", "upRight", "frameMs"};
  if (!j.isObject()) return c.fail(w, "must be an object with \"down\", \"up\", \"side\" (or \"left\" / \"right\", or the diagonals) and \"frameMs\"");
  for (const Json::Member& m : j.members()) {
    bool known = false;
    for (const char* k : kKnown) known |= m.first == k;
    if (!known)
      return c.fail(w, "has an unknown key " + q(m.first) + " (use sheet, down, up, side, left, right, downLeft, downRight, upLeft, upRight and frameMs)" +
                           didYouMean(m.first, {"sheet", "down", "up", "side", "left", "right", "downLeft", "downRight", "upLeft", "upRight", "frameMs"}));
  }
  std::string sheetId;
  if (!c.str(j, "sheet", w, sheetId)) return false;
  out.sheet = sheetId.empty() ? standSheet : db.graphics.findSheet(sheetId);
  if (out.sheet < 0) return c.fail(w, "unknown sheet " + q(sheetId));
  const SheetDef& sh = db.graphics.sheets[(size_t)out.sheet];
  std::vector<SpriteCell> given[kWalkDirs];
  bool side = false;
  static const struct { const char* key; int dir; } kDirs[] = {
      {"down", WalkDown}, {"up", WalkUp}, {"right", WalkRight}, {"left", WalkLeft}, {"side", -1},
      {"downLeft", WalkDownLeft}, {"downRight", WalkDownRight}, {"upLeft", WalkUpLeft}, {"upRight", WalkUpRight}};
  std::vector<SpriteCell> sideFrames;
  bool any = false;
  for (const auto& d : kDirs) {
    if (!j.has(d.key)) continue;
    const Json* list;
    if (!c.array(j, d.key, w, list, true)) return false;
    if (list->size() < 1 || list->size() > 64) return c.fail(w, q(d.key) + " must list between 1 and 64 cells");
    std::vector<SpriteCell>& cells = d.dir >= 0 ? given[d.dir] : sideFrames;
    for (size_t i = 0; i < list->size(); ++i) {
      SpriteCell cell;
      if (!readArtCell(c, (*list)[i], w, d.key, cell) || !checkCellsInside(c, sh, cell, 1, 1, w)) return false;
      cells.push_back(cell);
    }
    side |= d.dir < 0;
    any = true;
  }
  if (!any) return c.fail(w, "needs at least one of \"down\", \"up\", \"side\", \"left\", \"right\", \"downLeft\", \"downRight\", \"upLeft\" or \"upRight\"");
  out.frames[WalkDown] = given[WalkDown];
  out.frames[WalkUp] = given[WalkUp];
  // "side" faces right; the left is the same turned round unless "left" is given. "right" / "left" given alone mirror each other.
  out.frames[WalkRight] = !given[WalkRight].empty() ? given[WalkRight] : side ? sideFrames : given[WalkLeft];
  out.mirrored[WalkRight] = given[WalkRight].empty() && !side && !given[WalkLeft].empty();
  out.frames[WalkLeft] = !given[WalkLeft].empty() ? given[WalkLeft] : out.frames[WalkRight];
  out.mirrored[WalkLeft] = given[WalkLeft].empty() && !out.frames[WalkRight].empty();
  // The diagonals pair up the same way, each without a "side": down-left with down-right, up-left with up-right.
  static const int kPairs[2][2] = {{WalkDownLeft, WalkDownRight}, {WalkUpLeft, WalkUpRight}};
  for (const auto& p : kPairs)
    for (int k = 0; k < 2; ++k) {
      const int me = p[k], other = p[1 - k];
      out.frames[me] = !given[me].empty() ? given[me] : given[other];
      out.mirrored[me] = given[me].empty() && !given[other].empty();
    }
  if (!c.integer(j, "frameMs", w, out.frameMs)) return false;
  if (out.frameMs < 20 || out.frameMs > 60000) return c.fail(w, "\"frameMs\" must be between 20 and 60000");
  return true;
}

bool readUnitPoses(const LoadCtx& c, const Database& db, const Json& j, const std::string& where, int standSheet, Look& out) {
  if (!j.has("facing") && !j.has("attack") && !j.has("hurt") && !j.has("ko")) return true;
  auto poses = std::make_shared<UnitPoses>();
  const struct { const char* key; WalkLook* to; } kPoses[] = {
      {"facing", &poses->facing}, {"attack", &poses->attack}, {"hurt", &poses->hurt}, {"ko", &poses->ko}};
  for (const auto& p : kPoses)
    if (j.has(p.key) && !readWalk(c, db, j[p.key], where, standSheet, *p.to, p.key)) return false;
  out.poses = poses;
  return true;
}

}  // namespace ote
