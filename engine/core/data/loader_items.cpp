// data/items.json and the item lists other files carry (docs/ITEMS.md).
#include "core/data/loader_content.h"
#include "core/data/loader_conditions.h"
#include "core/data/loader_places.h"
#include "core/data/loader_srpg.h"
#include "core/data/loader_staves.h"
#include "core/data/loader_util.h"

namespace ote {

bool readItemGrants(const LoadCtx& c, const Database& db, const Json& o, const char* key, const std::string& where,
                    bool allowDrop, int maxCount, std::vector<ItemGrant>& out) {
  const Json* list;
  if (!c.array(o, key, where, list)) return false;
  std::string k = std::string("\"") + key + "\"";
  if (list->size() > 0 && db.items.empty())
    return c.fail(where, k + " is given but the game has no items (data/items.json)");
  if (maxCount >= 0 && (int)list->size() > maxCount)
    return c.fail(where, k + " has " + std::to_string(list->size()) + " entries; a unit can carry " +
                             std::to_string(maxCount) + " (rules.items.inventorySize)");
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& e = (*list)[i];
    std::string at = k + " entry " + std::to_string(i + 1);
    std::string id;
    ItemGrant g;
    bool hasUses = false;
    if (e.isString()) id = e.asString();
    else if (e.isObject()) {
      if (!c.str(e, "item", where, id, true)) return c.fail(where, at + " needs an \"item\"");
      hasUses = e.has("uses");
      if (hasUses && (!LoadCtx::isWhole(e["uses"]) || e["uses"].asInt() < 1 || e["uses"].asInt() > 999))
        return c.fail(where, at + ": \"uses\" must be a whole number from 1 to 999");
      if (hasUses) g.uses = e["uses"].asInt();
      if (e.has("drop")) {
        if (!allowDrop) return c.fail(where, at + ": \"drop\" only means something on a unit");
        if (!e["drop"].isBool()) return c.fail(where, at + ": \"drop\" must be true or false");
        g.drop = e["drop"].asBool();
      }
    } else return c.fail(where, at + " must be an item id or an object with \"item\"");
    g.item = db.findItem(id);
    if (g.item < 0) return c.fail(where, at + " names unknown item \"" + id + "\"");
    const ItemDef& def = db.items[(size_t)g.item];
    if (!hasUses) g.uses = def.uses;
    else if (def.uses < 0) return c.fail(where, at + ": \"" + id + "\" has unlimited uses, so \"uses\" cannot be set");
    else if (g.uses > def.uses)
      return c.fail(where, at + ": \"uses\" is more than the " + std::to_string(def.uses) + " a new \"" + id + "\" has");
    out.push_back(g);
  }
  return true;
}

bool loadItems(const std::string& path, Database& db, std::string& err) {
  db.items.clear();
  db.staves.clear();
  if (!fileExists(path)) return true;
  Json root;
  if (!loadObjectFile(path, root, err)) return false;
  LoadCtx c(err, path);
  const Json* list;
  if (!c.array(root, "items", "", list, true)) return false;
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& e = (*list)[i];
    std::string where = "items[" + std::to_string(i) + "]";
    if (!e.isObject()) return c.fail(where, "must be an object");
    ItemDef it;
    if (!c.str(e, "id", where, it.id, true)) return false;
    if (it.id.empty()) return c.fail(where, "\"id\" must not be empty");
    where = "item \"" + it.id + "\"";
    if (db.findItem(it.id) >= 0) return c.fail(where, "duplicate id");
    it.name = it.id;
    if (!c.str(e, "name", where, it.name) || !c.str(e, "description", where, it.description)) return false;

    std::string kind;
    if (!c.str(e, "kind", where, kind, true)) return false;
    if (kind == "usable") it.kind = ItemKind::Usable;
    else if (kind == "weapon") it.kind = ItemKind::Weapon;
    else if (kind == "accessory") it.kind = ItemKind::Accessory;
    else if (kind == "staff") it.kind = ItemKind::Staff;  // docs/SRPG.md section 8
    else return c.fail(where, "\"kind\" must be \"usable\", \"weapon\", \"accessory\" or \"staff\", not \"" + kind + "\"");

    if (e.has("uses")) {
      if (!c.integer(e, "uses", where, it.uses)) return false;
      if (it.uses < 1 || it.uses > 999) return c.fail(where, "\"uses\" must be from 1 to 999 (leave it out for unlimited)");
    }
    if (e.has("repairable")) {  // docs/SRPG.md section 8: a staff's "repair" skips items marked false
      if (!e["repairable"].isBool()) return c.fail(where, "\"repairable\" must be true or false");
      if (it.uses < 0) return c.fail(where, "\"repairable\" only matters for an item with \"uses\" (an item without them never wears)");
      it.repairable = e["repairable"].asBool();
    }
    if (e.has("stealable")) {  // docs/SRPG.md "Steal": false = a steal ability can never take this item
      if (!e["stealable"].isBool()) return c.fail(where, "\"stealable\" must be true or false");
      it.stealable = e["stealable"].asBool();
    }
    if (e.has("sellable")) {  // docs/EVENTS.md "Preparation": false = a shop that buys items never takes this one
      if (!e["sellable"].isBool()) return c.fail(where, "\"sellable\" must be true or false");
      it.sellable = e["sellable"].asBool();
    }
    if (!c.amounts(db, e, "price", where, it.price)) return false;
    for (const Amount& a : it.price)
      if (a.amount < 0) return c.fail(where, "\"price\" must not be negative");

    // Each kind has its own fields; one of another kind is a mistake worth pointing out.
    auto only = [&](const char* key, ItemKind k, const char* kindName) {
      if (!e.has(key) || it.kind == k) return true;
      return c.fail(where, std::string("\"") + key + "\" belongs to items of kind \"" + kindName + "\"");
    };
    if (!(it.kind == ItemKind::Staff || only("target", ItemKind::Usable, "usable")) || !only("heal", ItemKind::Usable, "usable") ||
        !only("raise", ItemKind::Usable, "usable") || !only("weapon", ItemKind::Weapon, "weapon") ||
        !only("bonus", ItemKind::Accessory, "accessory") || !only("promotes", ItemKind::Usable, "usable") ||
        !only("skills", ItemKind::Accessory, "accessory") || !only("cure", ItemKind::Usable, "usable") ||
        !only("inflict", ItemKind::Usable, "usable") || !only("opens", ItemKind::Usable, "usable"))
      return false;
    // Strategy-RPG features (docs/SRPG.md): a promotion item, an accessory's skills. Read before the checks below:
    // either one makes an item valid without heal / raise / bonus.
    if (!readItemPromotes(c, e, where, it) || !readItemSkills(c, db, e, where, it) || !readItemConditions(c, db, e, where, it) ||
        !readItemOpens(c, e, where, it))  // a key (docs/SRPG.md section 10) is valid without heal / raise
      return false;

    if (it.kind == ItemKind::Usable) {
      std::string target = "self";
      if (!c.str(e, "target", where, target)) return false;
      if (target != "self" && target != "ally") return c.fail(where, "\"target\" must be \"self\" or \"ally\"");
      it.targetAlly = target == "ally";
      if (e.has("heal")) {
        const Json& h = e["heal"];
        if (h.isString() && h.asString() == "full") it.heal = kHealFull;
        else if (LoadCtx::isWhole(h) && h.asInt() >= 1 && h.asInt() <= 1000000) it.heal = h.asInt();
        else return c.fail(where, "\"heal\" must be a whole number of 1 or more, or \"full\"");
      }
      it.raise.assign(db.stats.size(), 0);
      if (!c.statMap(db, e, "raise", where, it.raise)) return false;
      bool raises = false;
      for (int v : it.raise) raises = raises || v != 0;
      if (!raises) it.raise.clear();
      if (it.heal == 0 && !raises && !it.promotes && it.cures.empty() && !it.cureAll && it.inflict < 0 && !it.isKey())
        return c.fail(where, std::string("a usable item needs \"heal\" and/or \"raise\"") +
                                 (db.conditions.empty() ? "" : " (or \"cure\" / \"inflict\" / \"opens\")"));
    } else if (it.kind == ItemKind::Weapon) {
      if (!db.rules.items.carriedWeapons)
        return c.fail(where, "an item of kind \"weapon\" needs carried weapons: set \"rules\": { \"items\": { \"weapons\": "
                             "\"carried\" } } in game.json");
      std::string wid;
      if (!c.str(e, "weapon", where, wid, true)) return false;
      it.weapon = db.findWeapon(wid);
      if (it.weapon < 0) return c.fail(where, "unknown weapon \"" + wid + "\" (data/weapons.json)");
    } else if (it.kind == ItemKind::Staff) {
      if (!readStaffItem(c, db, e, where, it)) return false;
    } else {
      it.bonus.assign(db.stats.size(), 0);
      if (!c.statMap(db, e, "bonus", where, it.bonus)) return false;
      bool any = false;
      for (int v : it.bonus) any = any || v != 0;
      if (!any && it.skills.empty()) return c.fail(where, "an accessory needs a \"bonus\" that changes at least one stat, or \"skills\"");
    }
    db.items.push_back(it);
  }
  return true;
}

}  // namespace ote
