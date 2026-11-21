// data/graphics.json "tilesets" (docs/GRAPHICS.md "Tile layer (hand-painted maps)"):
//   "tilesets": { "world": { "sheet": "world_tiles", "animations": { "40": { "frames": [40, 41, 42], "ms": 250 } } } }
// A tileset is a sheet; its cells are the tiles, numbered row by row from 0. An animation lets a tile cycle through other tiles of the
// same sheet. Presentation only. Every refusal names the tileset.
#include <algorithm>

#include "core/data/loader_content.h"
#include "core/data/loader_graphics_art.h"
#include "core/data/loader_util.h"

namespace ote {

bool readTilesets(const LoadCtx& c, Database& db, const Json& root) {
  const Json* all;
  if (!c.object(root, "tilesets", "", all)) return false;
  for (const Json::Member& e : all->members()) {
    const std::string where = "tileset \"" + e.first + "\"";
    if (e.first.empty() || !isStoryId(e.first)) return c.fail(where, "the id must be letters, digits, \"_\" and \"-\" (and not empty)");
    if (!e.second.isObject()) return c.fail(where, "must be an object such as { \"sheet\": \"world_tiles\" }");
    for (const Json::Member& k : e.second.members())
      if (k.first != "sheet" && k.first != "animations")
        return c.fail(where, "unknown key \"" + k.first + "\" (a tileset has \"sheet\" and optionally \"animations\")" + didYouMean(k.first, {"sheet", "animations"}));
    TilesetDef ts;
    ts.id = e.first;
    std::string sheetId;
    if (!c.str(e.second, "sheet", where, sheetId, true)) return false;
    ts.sheet = db.graphics.findSheet(sheetId);
    if (ts.sheet < 0) return c.fail(where, "unknown sheet \"" + sheetId + "\"");
    const SheetDef& sh = db.graphics.sheets[(size_t)ts.sheet];
    ts.cols = sh.cols;
    ts.rows = sh.rows;
    if (ts.count() > kMaxTilesetCells)
      return c.fail(where, "sheet \"" + sheetId + "\" has " + std::to_string(ts.count()) + " cells; a tileset can have at most " +
                               std::to_string(kMaxTilesetCells));
    if (e.second.has("animations")) {
      const Json* an;
      if (!c.object(e.second, "animations", where, an)) return false;
      for (const Json::Member& m : an->members()) {
        const std::string at = where + " animation \"" + m.first + "\"";
        bool digits = !m.first.empty() && m.first.size() <= 6;
        for (char ch : m.first) digits = digits && ch >= '0' && ch <= '9';
        if (!digits) return c.fail(at, "the key must be a tile number (0, 1, 2, ...)");
        TileAnim a;
        a.tile = std::stoi(m.first);
        if (a.tile >= ts.count())
          return c.fail(at, "tile " + std::to_string(a.tile) + " is outside the sheet (" + std::to_string(ts.count()) + " tiles)");
        if (!m.second.isObject()) return c.fail(at, "must be { \"frames\": [40, 41], \"ms\": 200 }");
        for (const Json::Member& k : m.second.members())
          if (k.first != "frames" && k.first != "ms") return c.fail(at, "unknown key \"" + k.first + "\" (use frames and ms)" + didYouMean(k.first, {"frames", "ms"}));
        const Json* frames;
        if (!c.array(m.second, "frames", at, frames, true)) return false;
        if (frames->size() < 2 || frames->size() > 64) return c.fail(at, "\"frames\" must list between 2 and 64 tiles");
        for (size_t i = 0; i < frames->size(); ++i) {
          const Json& f = (*frames)[i];
          if (!LoadCtx::isWhole(f) || f.asInt() < 0 || f.asInt() >= ts.count())
            return c.fail(at, "frame " + std::to_string(i + 1) + " must be a tile number from 0 to " + std::to_string(ts.count() - 1));
          a.frames.push_back(f.asInt());
        }
        if (!c.integer(m.second, "ms", at, a.ms)) return false;
        if (a.ms < 20 || a.ms > 60000) return c.fail(at, "\"ms\" must be between 20 and 60000");
        ts.anims.push_back(a);
      }
      std::sort(ts.anims.begin(), ts.anims.end(), [](const TileAnim& x, const TileAnim& y) { return x.tile < y.tile; });
    }
    db.graphics.tilesets.push_back(ts);
  }
  return true;
}

}  // namespace ote
