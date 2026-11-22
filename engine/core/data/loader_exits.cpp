// A map's exit squares (docs/AI.md "Looting and escaping"): "exits": [ { "x": 0, "y": 4 }, ... ]. A computer unit in mode "escape" (or a
// looter with nothing left to take) walks to the nearest one and leaves the map there. Absent = none; the map plays as before.
#include "core/data/loader_graphics_art.h"
#include "core/data/loader_util.h"

namespace ote {

bool readMapExits(const LoadCtx& c, const Json& root, MapDef& m) {
  m.exits.clear();
  if (!root.has("exits")) return true;
  const Json& v = root["exits"];
  if (!v.isArray() || v.size() == 0 || v.size() > (size_t)kMaxMapExits)
    return c.fail("", "\"exits\" must be a list of 1 to " + std::to_string(kMaxMapExits) + " squares such as [ { \"x\": 0, \"y\": 4 } ]");
  for (size_t i = 0; i < v.size(); ++i) {
    const std::string where = "exits[" + std::to_string(i) + "]";
    const Json& e = v[i];
    if (!e.isObject()) return c.fail(where, "an exit must be a square { \"x\": 0, \"y\": 4 }");
    if (!c.onlyKeys(e, {"x", "y"}, where)) return false;
    int x = 0, y = 0;
    if (!c.integerFrom(e, "x", where, 0, 9999, x) || !c.integerFrom(e, "y", where, 0, 9999, y)) return false;
    if (!e.has("x") || !e.has("y")) return c.fail(where, "an exit needs both \"x\" and \"y\"");
    if (x >= m.width || y >= m.height)
      return c.fail(where, "the square " + std::to_string(x) + "," + std::to_string(y) + " is outside the map (" + std::to_string(m.width) + " x " +
                               std::to_string(m.height) + ")");
    for (const auto& o : m.exits)
      if (o.first == x && o.second == y) return c.fail(where, "the square " + std::to_string(x) + "," + std::to_string(y) + " is listed twice");
    m.exits.emplace_back(x, y);
  }
  return true;
}

}  // namespace ote
