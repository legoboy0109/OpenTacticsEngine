// game.json "grid" (docs/HEX.md "Data"): the one grid kind of the whole game.
#include "core/data/loader_content.h"
#include "core/data/loader_util.h"

namespace ote {

bool readGameGrid(const LoadCtx& c, const Json& root, Database& db) {
  db.gridType = GridType::Square;
  db.hexLayout = HexLayout::Pointy;
  if (!root.has("grid")) return true;
  const Json* g;
  if (!c.object(root, "grid", "", g)) return false;
  for (const Json::Member& m : g->members())
    if (m.first != "type" && m.first != "layout")
      return c.fail("grid", unknownKeyText(m.first, {"type", "layout"}));
  std::string type = "square", layout;
  if (!c.str(*g, "type", "grid", type) || !c.str(*g, "layout", "grid", layout)) return false;
  if (type == "square") {
    if (g->has("layout")) return c.fail("grid", "\"layout\" is only for hexagon games (\"type\": \"hex\")");
    return true;
  }
  if (type != "hex") return c.fail("grid", "\"type\" must be \"square\" or \"hex\", not \"" + type + "\"");
  if (!g->has("layout"))
    return c.fail("grid", "a hexagon game needs a \"layout\": \"pointy\" or \"flat\"");
  if (layout != "pointy" && layout != "flat")
    return c.fail("grid", "\"layout\" must be \"pointy\" or \"flat\", not \"" + layout + "\"");
  db.gridType = GridType::Hex;
  db.hexLayout = layout == "flat" ? HexLayout::Flat : HexLayout::Pointy;
  return true;
}

}  // namespace ote
