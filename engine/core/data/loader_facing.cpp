// Loader of the facing rules and of a map unit's "facing". Every refusal names the file, the entry and the key.
#include "core/data/loader_facing.h"

#include "core/grid/facing.h"

namespace ote {

namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

bool onlyThese(const LoadCtx& c, const Json& block, const std::string& at, std::initializer_list<const char*> keys) {
  for (const Json::Member& m : block.members()) {
    bool known = false;
    for (const char* k : keys) known = known || m.first == k;
    if (known) continue;
    std::string list;
    for (const char* k : keys) list += (list.empty() ? "" : ", ") + q(k);
    return c.fail(at, "unknown key " + q(m.first) + " (expected " + list + ")");
  }
  return true;
}

}  // namespace

bool loadFacingRules(const LoadCtx& c, const Json& rules, Database& db) {
  FacingRules& f = db.rules.facing;
  f = FacingRules();
  if (!rules.has("facing")) return true;
  const Json* blk;
  if (!c.object(rules, "facing", "rules", blk, true)) return false;
  const std::string at = "rules.facing";
  if (!onlyThese(c, *blk, at, {"arcs"})) return false;
  if (blk->has("arcs")) {
    const Json* arcs;
    if (!c.object(*blk, "arcs", at, arcs, true)) return false;
    if (!onlyThese(c, *arcs, at + ", \"arcs\"", {"front", "side", "back"})) return false;
    static const char* const keys[3] = {"front", "side", "back"};
    for (int i = 0; i < 3; ++i) {
      if (!arcs->has(keys[i])) continue;
      const std::string w = at + ", \"arcs\", " + q(keys[i]);
      const Json* arc;
      if (!c.object(*arcs, keys[i], at + ", \"arcs\"", arc, true)) return false;
      if (!onlyThese(c, *arc, w, {"name"})) return false;
      std::string name;
      if (!c.str(*arc, "name", w, name)) return false;
      if (arc->has("name")) {
        if (name.empty() || name.size() > 16) return c.fail(w, "\"name\" must be 1 to 16 characters (the word the forecast shows for the arc)");
        f.arcName[i] = name;
      }
    }
  }
  f.on = true;
  return true;
}

bool readUnitFacing(const LoadCtx& c, const Database& db, const MapDef& m, const Json& e, const std::string& where, MapUnitDef& u) {
  u.facing = -1;
  if (!e.has("facing")) return true;
  if (!db.rules.facing.on) return c.fail(where, "\"facing\" needs \"rules\": { \"facing\": ... } in game.json");
  std::string name;
  if (!c.str(e, "facing", where, name, true)) return false;
  const Grid grid(m.gridType, m.width, m.height, m.hexLayout);
  u.facing = directionFromName(grid, name);
  if (u.facing >= 0) return true;
  std::string list;
  for (int d = 0; d < grid.directionCount(); ++d) list += (list.empty() ? "" : ", ") + q(directionName(grid, d));
  return c.fail(where, "\"facing\" must be one of " + list + " on this map's grid (not " + q(name) + ")");
}

}  // namespace ote
