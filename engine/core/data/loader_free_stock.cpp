// Loader: game.json "rules.items.freeBattleStorage" (docs/GAME_FORMAT.md "Free battles"): what the shared storage holds when a
// FREE battle starts. Read after the items and resources exist. Unknown keys are refused; every id is checked.
//   { "each": 3, "except": ["item"], "items": [ "id" | { "item": "id", "count": 5, "uses": 2 } ], "funds": { "gold": 200 } }
#include "core/data/loader_content.h"
#include "core/data/loader_util.h"

namespace ote {

bool loadFreeBattleStock(const std::string& gameFile, Database& db, std::string& err) {
  Json root;
  if (!loadObjectFile(gameFile, root, err)) return false;
  LoadCtx c(err, gameFile);
  const Json* rules;
  const Json* items;
  const Json* blk;
  if (!c.object(root, "rules", "", rules) || !c.object(*rules, "items", "rules", items)) return false;
  if (!items->has("freeBattleStorage")) return true;
  const std::string where = "rules.items.freeBattleStorage";
  if (!c.object(*items, "freeBattleStorage", "rules.items", blk)) return false;
  for (const Json::Member& m : blk->members())
    if (m.first != "each" && m.first != "except" && m.first != "items" && m.first != "funds")
      return c.fail(where, unknownKeyText(m.first, {"each", "except", "items", "funds"}));
  FreeBattleStock& s = db.rules.items.freeStock;
  s = FreeBattleStock();
  s.given = true;
  if (!c.integerIn(*blk, "each", where, 0, 99, s.each)) return false;
  if (blk->has("except")) {
    if (s.each == 0) return c.fail(where, "\"except\" only means something with \"each\"");
    std::vector<std::string> ids;
    if (!c.stringList(*blk, "except", where, ids)) return false;
    for (const std::string& id : ids) {
      const int i = db.findItem(id);
      if (i < 0) return c.fail(where, "\"except\" names unknown item \"" + id + "\"");
      s.except.push_back(i);
    }
  }
  if (blk->has("items")) {
    const Json* list;
    if (!c.array(*blk, "items", where, list)) return false;
    for (size_t i = 0; i < list->size(); ++i) {
      const Json& e = (*list)[i];
      const std::string at = "\"items\" entry " + std::to_string(i + 1);
      FreeStockCount n;
      n.count = 1;
      if (e.isString()) {
        n.grant.item = db.findItem(e.asString());
        if (n.grant.item < 0) return c.fail(where, at + " names unknown item \"" + e.asString() + "\"");
      } else if (e.isObject()) {
        for (const Json::Member& m : e.members())
          if (m.first != "item" && m.first != "count" && m.first != "uses")
            return c.fail(where, at + ": " + unknownKeyText(m.first, {"item", "count", "uses"}));
        std::string id;
        if (!c.str(e, "item", where, id, true)) return false;
        n.grant.item = db.findItem(id);
        if (n.grant.item < 0) return c.fail(where, at + " names unknown item \"" + id + "\"");
        if (!c.integerIn(e, "count", where, 0, 99, n.count)) return false;
        if (e.has("uses")) {
          const ItemDef& d = db.items[(size_t)n.grant.item];
          if (d.uses < 0) return c.fail(where, at + ": \"" + d.id + "\" has unlimited uses, so \"uses\" cannot be set");
          if (!c.integerIn(e, "uses", where, 1, d.uses, n.grant.uses)) return false;
        }
      } else return c.fail(where, at + " must be an item id or an object with \"item\"");
      if (n.grant.uses < 0) n.grant.uses = db.items[(size_t)n.grant.item].uses;
      for (const FreeStockCount& o : s.counts)
        if (o.grant.item == n.grant.item) return c.fail(where, at + ": \"" + db.items[(size_t)n.grant.item].id + "\" is listed twice");
      s.counts.push_back(n);
    }
  }
  if (blk->has("funds") && !c.amounts(db, *blk, "funds", where, s.funds)) return false;
  for (const Amount& a : s.funds)
    if (a.amount < 0) return c.fail(where, "\"funds\" amounts must not be negative");
  return true;
}

}  // namespace ote
