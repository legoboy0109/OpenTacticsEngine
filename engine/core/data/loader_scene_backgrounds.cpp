// data/graphics.json "backgrounds": the full-screen pictures a story scene can show (docs/GRAPHICS.md "Backgrounds").
// Presentation only. { "forest": { "sheet": "bg_forest" } } = the sheet's whole image; adding "cell": [col, row] = just
// that cell of the sheet. The id is what the `background` story command names.
#include "core/data/events.h"
#include "core/data/loader_content.h"
#include "core/data/loader_graphics_art.h"
#include "core/data/loader_util.h"
#include "core/data/scene_fx.h"

namespace ote {

bool readSceneBackgrounds(const LoadCtx& c, Database& db, const Json& root) {
  const Json* m;
  if (!c.object(root, "backgrounds", "", m)) return false;
  for (const Json::Member& e : m->members()) {
    const std::string where = "background \"" + e.first + "\"";
    if (e.first.empty() || !isStoryId(e.first))
      return c.fail(where, "the id must be letters, digits, \"_\" and \"-\" (and not empty)");
    if (!e.second.isObject()) return c.fail(where, "must be an object: { \"sheet\": \"id\" } or { \"sheet\": \"id\", \"cell\": [column, row] }");
    for (const Json::Member& k : e.second.members())
      if (k.first != "sheet" && k.first != "cell")
        return c.fail(where, "unknown key \"" + k.first + "\" (a background has \"sheet\" and optionally \"cell\")" + didYouMean(k.first, {"sheet", "cell"}));
    SceneBackground bg;
    bg.id = e.first;
    std::string sheetId;
    if (!c.str(e.second, "sheet", where, sheetId, true)) return false;
    bg.sheet = db.graphics.findSheet(sheetId);
    if (bg.sheet < 0) return c.fail(where, "unknown sheet \"" + sheetId + "\"");
    const SheetDef& sh = db.graphics.sheets[(size_t)bg.sheet];
    if (e.second.has("cell")) {
      const Json& j = e.second["cell"];
      if (!j.isArray() || j.size() != 2 || !LoadCtx::isWhole(j[0]) || !LoadCtx::isWhole(j[1]) || j[0].asInt() < 0 || j[1].asInt() < 0 ||
          j[0].asInt() > 30000 || j[1].asInt() > 30000)
        return c.fail(where, "\"cell\" must be [column, row] (whole numbers, counted from 0)");
      bg.whole = false;
      bg.cell.col = (int16_t)j[0].asInt();
      bg.cell.row = (int16_t)j[1].asInt();
      if (bg.cell.col >= sh.cols || bg.cell.row >= sh.rows)
        return c.fail(where, "cell [" + std::to_string(bg.cell.col) + ", " + std::to_string(bg.cell.row) + "] is outside sheet \"" + sh.id +
                                 "\" (" + std::to_string(sh.cols) + " x " + std::to_string(sh.rows) + " cells)");
    }
    db.graphics.backgrounds.push_back(bg);
  }
  return true;
}

}  // namespace ote
