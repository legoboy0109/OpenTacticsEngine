// "upgrades" of structure kinds (docs/ECONOMY.md "Upgradable structures"). Read after the whole
// structure list, because a kind may name one that comes later in the file.
#include "core/data/loader_content.h"
#include "core/data/loader_util.h"

namespace ote {

bool loadStructureUpgrades(const LoadCtx& c, const Json& list, Database& db) {
  for (size_t i = 0; i < list.size(); ++i) {
    const Json& e = list[i];
    if (!e.has("upgrades")) continue;
    StructureDef& from = db.structures[i];
    std::string where = "structure \"" + from.id + "\"";
    const Json* ups;
    if (!c.array(e, "upgrades", where, ups)) return false;
    for (size_t k = 0; k < ups->size(); ++k) {
      const Json& u = (*ups)[k];
      std::string at = "\"upgrades\" entry " + std::to_string(k + 1);
      if (!u.isObject()) return c.fail(where, at + " must be an object { \"to\": ..., \"cost\": ... }");
      for (const Json::Member& m : u.members())
        if (m.first != "to" && m.first != "cost" && m.first != "requires")
          return c.fail(where, at + ": unknown key \"" + m.first + "\" (expected \"to\", \"cost\", \"requires\")");
      UpgradeDef up;
      std::string to;
      if (!c.str(u, "to", where, to, true)) return false;
      up.to = db.findStructure(to);
      if (up.to < 0) return c.fail(where, at + " names unknown structure \"" + to + "\" in \"to\"");
      if (up.to == (int)i) return c.fail(where, at + ": a structure cannot be upgraded to itself");
      const StructureDef& target = db.structures[(size_t)up.to];
      if (target.rings != from.rings)
        return c.fail(where, at + ": \"" + to + "\" has rings " + std::to_string(target.rings) + " but \"" + from.id +
                                 "\" has " + std::to_string(from.rings) + "; an upgrade keeps the footprint");
      if (target.width != from.width || target.height != from.height)
        return c.fail(where, at + ": \"" + to + "\" is " + std::to_string(target.width) + " x " + std::to_string(target.height) +
                                 " squares but \"" + from.id + "\" is " + std::to_string(from.width) + " x " +
                                 std::to_string(from.height) + "; an upgrade keeps the footprint");
      for (const UpgradeDef& other : from.upgrades)
        if (other.to == up.to) return c.fail(where, at + ": \"" + to + "\" is listed twice");
      if (!c.amounts(db, u, "cost", where + ": " + at, up.cost)) return false;
      for (const Amount& a : up.cost)
        if (a.amount < 0) return c.fail(where, at + ": \"cost\" must not be negative");
      if (u.has("requires")) {
        std::string need;
        if (!c.str(u, "requires", where, need, true)) return false;
        up.needs = db.findTechnology(need);
        if (up.needs < 0)
          return c.fail(where, at + ": \"requires\" names unknown technology \"" + need + "\"" +
                                   (db.research.empty() ? " (this game has no data/research.json)" : ""));
      }
      from.upgrades.push_back(up);
    }
  }
  return true;
}

}  // namespace ote
