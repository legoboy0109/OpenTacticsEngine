// Doors, chests and visitable places: the data side (docs/SRPG.md section 10). They are structure kinds with a "door", "chest"
// or "visit" block; every refusal names the kind, the map entry or the item in the existing style.
#include "core/data/loader_places.h"

#include <algorithm>

#include "core/data/loader_content.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

bool onlyKeys(const LoadCtx& c, const Json& o, std::initializer_list<const char*> keys, const std::string& where) {
  for (const Json::Member& m : o.members()) {
    bool known = false;
    for (const char* k : keys) known = known || m.first == k;
    if (known) continue;
    std::string list;
    for (const char* k : keys) list += (list.empty() ? "" : ", ") + q(k);
    return c.fail(where, "unknown key " + q(m.first) + " (expected " + list + ")");
  }
  return true;
}

// Resolves the "opens" lists of the items now that structure kinds exist (done once, before the kinds' own blocks are read,
// so a kind's "keys" can be checked against what an item opens).
bool resolveItemOpens(const LoadCtx& c, Database& db) {
  for (ItemDef& it : db.items) {
    it.opensDoors = it.opensChests = false;
    it.opensKinds.clear();
    for (const std::string& id : it.opensIds) {
      if (id == "door") { it.opensDoors = true; continue; }
      if (id == "chest") { it.opensChests = true; continue; }
      const int k = db.findStructure(id);
      if (k < 0)
        return c.fail("item \"" + it.id + "\"", "\"opens\" names " + q(id) + ", which is neither \"door\", \"chest\" nor a structure kind" +
                                                    (db.structures.empty() ? " (this game has no data/structures.json)" : ""));
      if (std::find(it.opensKinds.begin(), it.opensKinds.end(), k) == it.opensKinds.end()) it.opensKinds.push_back(k);
    }
  }
  return true;
}

bool readFrom(const LoadCtx& c, const Json& v, const std::string& where, PlaceDef& p) {
  if (!v.has("from")) return true;
  if (!v["from"].isString()) return c.fail(where, "\"from\" must be \"on\", \"next\" or \"either\"");
  const std::string f = v["from"].asString();
  if (f == "on") { p.fromOn = true; p.fromNext = false; }
  else if (f == "next") { p.fromOn = false; p.fromNext = true; }
  else if (f == "either") { p.fromOn = p.fromNext = true; }
  else return c.fail(where, "\"from\" must be \"on\", \"next\" or \"either\", not " + q(f));
  return true;
}

// "opens" / "becomes" / "ruin": the id of another kind that must have the same footprint (an upgrade keeps it).
bool readKindRef(const LoadCtx& c, const Database& db, const StructureDef& self, int selfIndex, const Json& v, const char* key,
                 const std::string& where, int& out) {
  if (!v.has(key)) return true;
  if (!v[key].isString()) return c.fail(where, q(key) + " must be the id of a structure kind");
  const std::string id = v[key].asString();
  const int k = db.findStructure(id);
  if (k < 0) return c.fail(where, q(key) + " names unknown structure " + q(id));
  if (k == selfIndex) return c.fail(where, q(key) + " names the kind itself; it must become another kind");
  const StructureDef& t = db.structures[(size_t)k];
  if (t.rings != self.rings || t.width != self.width || t.height != self.height)
    return c.fail(where, q(key) + " names " + q(id) + ", which has a different size; it must keep the footprint (width, height or rings)");
  out = k;
  return true;
}

bool readByList(const LoadCtx& c, const Json& v, const std::string& where, bool allowBreak, PlaceDef& p) {
  if (!v.has("by")) return true;
  const Json& by = v["by"];
  if (!by.isArray()) return c.fail(where, "\"by\" must be a list of \"key\", \"trait\"" + std::string(allowBreak ? ", \"break\"" : ""));
  p.byKey = p.byTrait = p.byBreak = false;
  for (size_t i = 0; i < by.size(); ++i) {
    const std::string w = by[i].isString() ? by[i].asString() : "";
    if (w == "key") p.byKey = true;
    else if (w == "trait") p.byTrait = true;
    else if (w == "break" && allowBreak) p.byBreak = true;
    else return c.fail(where, "\"by\" entry " + std::to_string(i + 1) + " must be \"key\", \"trait\"" + (allowBreak ? " or \"break\"" : "") + ", not " + q(w));
  }
  return true;
}

bool readKeys(const LoadCtx& c, const Database& db, const Json& v, const std::string& where, const StructureDef& s, int selfIndex, PlaceDef& p) {
  std::vector<std::string> ids;
  if (!c.stringList(v, "keys", where, ids)) return false;
  for (const std::string& id : ids) {
    const int it = db.findItem(id);
    if (it < 0) return c.fail(where, "\"keys\" names unknown item " + q(id) + (db.items.empty() ? " (this game has no items, data/items.json)" : ""));
    const ItemDef& d = db.items[(size_t)it];
    if (!d.isKey()) return c.fail(where, "\"keys\" lists item " + q(id) + ", which opens nothing (give it an \"opens\" list in data/items.json)");
    const bool fits = (p.isDoor() && d.opensDoors) || (p.isChest() && d.opensChests) ||
                      std::find(d.opensKinds.begin(), d.opensKinds.end(), selfIndex) != d.opensKinds.end();
    if (!fits) return c.fail(where, "\"keys\" lists item " + q(id) + ", but its \"opens\" does not name \"" + (p.isDoor() ? "door" : "chest") + "\" or " + q(s.id));
    if (std::find(p.keys.begin(), p.keys.end(), it) != p.keys.end()) return c.fail(where, "\"keys\" lists " + q(id) + " twice");
    p.keys.push_back(it);
  }
  return true;
}

bool readDoorOrChest(const LoadCtx& c, Database& db, const Json& v, int index, const char* key, StructureDef& s) {
  const bool door = std::string(key) == "door";
  const std::string where = "structure " + q(s.id) + ": \"" + key + "\"";
  if (!v.isObject()) return c.fail(where, "must be an object such as { \"opens\": \"gate_open\" }");
  if (door ? !onlyKeys(c, v, {"opens", "locked", "keys", "by", "hp", "stats", "from"}, where)
           : !onlyKeys(c, v, {"opens", "locked", "keys", "by", "from"}, where))
    return false;
  PlaceDef p;
  p.kind = door ? PlaceKind::Door : PlaceKind::Chest;
  if (!v.has("opens")) return c.fail(where, "missing required \"opens\" (the structure kind it becomes when opened)");
  if (!readKindRef(c, db, s, index, v, "opens", where, p.to)) return false;
  if (!c.boolean(v, "locked", where, p.locked) || !readFrom(c, v, where, p)) return false;
  if (door) {
    if (v.has("hp")) {
      if (!LoadCtx::isWhole(v["hp"]) || v["hp"].asInt() < 1 || v["hp"].asInt() > kMaxPlaceHp)
        return c.fail(where, "\"hp\" must be a whole number from 1 to " + std::to_string(kMaxPlaceHp) + " (leave it out for a door that cannot be attacked)");
      p.hp = v["hp"].asInt();
      p.byBreak = true;
    }
    if (v.has("stats")) {
      if (p.hp == 0) return c.fail(where, "\"stats\" are the numbers its damage formula reads, so they need \"hp\" (a door that can be attacked)");
      p.stats.assign(db.stats.size(), 0);
      if (!c.statMap(db, v, "stats", where, p.stats)) return false;
    }
  }
  if (!readByList(c, v, where, door, p)) return false;
  if (p.byBreak && p.hp == 0) return c.fail(where, "\"by\" has \"break\" but the door has no \"hp\"");
  // A closed door must block somebody, otherwise "opening" it changes nothing.
  if (door) {
    bool blocks = false;
    for (int cost : s.moveCost) blocks = blocks || cost == 0;
    if (!blocks) return c.fail(where, "a door must stop at least one movement type: give the kind \"moveCost\" 0 for the ones it blocks");
  }
  if (v.has("keys") && !p.byKey) return c.fail(where, "\"keys\" are given but \"by\" does not list \"key\"");
  if (!readKeys(c, db, v, where, s, index, p)) return false;
  s.place = p;
  return true;
}

bool readVisit(const LoadCtx& c, Database& db, const Json& v, int index, StructureDef& s) {
  const std::string where = "structure " + q(s.id) + ": \"visit\"";
  if (!v.isObject()) return c.fail(where, "must be an object such as { \"becomes\": \"village_shut\" }");
  if (!onlyKeys(c, v, {"becomes", "ruin", "repeat", "from"}, where)) return false;
  PlaceDef p;
  p.kind = PlaceKind::Visit;
  p.fromNext = false;  // a village is visited from its own square unless the maker says "either" / "next"
  if (!readKindRef(c, db, s, index, v, "becomes", where, p.to) || !readKindRef(c, db, s, index, v, "ruin", where, p.destroyedTo) ||
      !c.boolean(v, "repeat", where, p.repeat) || !readFrom(c, v, where, p))
    return false;
  if (p.repeat && p.to >= 0) return c.fail(where, "\"repeat\" and \"becomes\" contradict each other: a place that changes kind after a visit cannot be visited again");
  s.place = p;
  return true;
}

}  // namespace

bool loadPlaceRules(const LoadCtx& c, const Json& rules, Database& db) {
  db.rules.places = PlaceRules();
  if (!rules.has("places")) return true;
  const Json& j = rules["places"];
  if (!j.isObject()) return c.fail("rules.places", "must be an object such as { \"actions\": \"menu\" }");
  if (!onlyKeys(c, j, {"actions"}, "rules.places")) return false;
  if (j.has("actions")) {
    const std::string a = j["actions"].isString() ? j["actions"].asString() : "";
    if (a != "menu" && a != "auto")
      return c.fail("rules.places", "\"actions\" must be \"menu\" (explicit Open / Visit entries in the unit's menu) or \"auto\" (it happens when a unit ends its move there)");
    db.rules.places.automatic = a == "auto";
  }
  return true;
}

bool readItemOpens(const LoadCtx& c, const Json& e, const std::string& where, ItemDef& it) {
  if (!e.has("opens")) return true;
  if (!e["opens"].isArray() || e["opens"].size() == 0)
    return c.fail(where, "\"opens\" must be a list such as [\"door\"], [\"chest\"] or [\"door\", \"chest\", \"<structure id>\"]");
  for (size_t i = 0; i < e["opens"].size(); ++i) {
    if (!e["opens"][i].isString() || e["opens"][i].asString().empty()) return c.fail(where, "\"opens\" entry " + std::to_string(i + 1) + " must be text");
    it.opensIds.push_back(e["opens"][i].asString());
  }
  return true;
}

bool loadStructurePlaces(const LoadCtx& c, const Json& list, Database& db) {
  if (!resolveItemOpens(c, db)) return false;
  for (size_t i = 0; i < list.size(); ++i) {
    const Json& e = list[i];
    StructureDef& s = db.structures[i];
    if (e.has("blocksSight") && !c.boolean(e, "blocksSight", "structure " + q(s.id), s.blocksSight)) return false;
    int blocks = 0;
    for (const char* k : {"door", "chest", "visit"}) blocks += e.has(k);
    if (blocks > 1) return c.fail("structure " + q(s.id), "has more than one of \"door\", \"chest\" and \"visit\"; a kind is one of them");
    if (e.has("door") && !readDoorOrChest(c, db, e["door"], (int)i, "door", s)) return false;
    if (e.has("chest") && !readDoorOrChest(c, db, e["chest"], (int)i, "chest", s)) return false;
    if (e.has("visit") && !readVisit(c, db, e["visit"], (int)i, s)) return false;
  }
  return true;
}

bool kindAcceptsKey(const Database& db, const StructureDef& kind, int kindIndex, int item) {
  if (item < 0 || (size_t)item >= db.items.size()) return false;
  const ItemDef& d = db.items[(size_t)item];
  if (!d.isKey()) return false;
  const PlaceDef& p = kind.place;
  const bool opens = (p.isDoor() && d.opensDoors) || (p.isChest() && d.opensChests) ||
                     std::find(d.opensKinds.begin(), d.opensKinds.end(), kindIndex) != d.opensKinds.end();
  if (!opens) return false;
  return p.keys.empty() || std::find(p.keys.begin(), p.keys.end(), item) != p.keys.end();
}

bool itemOpensKind(const Database& db, int item, int structureKind) {
  if (structureKind < 0 || (size_t)structureKind >= db.structures.size()) return false;
  return kindAcceptsKey(db, db.structures[(size_t)structureKind], structureKind, item);
}

// ---- map placements ----

namespace {

bool readLoot(const LoadCtx& c, const Database& db, const Json& list, const std::string& where, std::vector<PlaceLoot>& out) {
  if (!list.isArray()) return c.fail(where, "\"contents\" must be a list of items and amounts");
  if (list.size() == 0) return c.fail(where, "\"contents\" is empty: a chest with nothing in it");
  if ((int)list.size() > kMaxChestLoot)
    return c.fail(where, "\"contents\" has " + std::to_string(list.size()) + " entries; a chest holds at most " + std::to_string(kMaxChestLoot));
  for (size_t i = 0; i < list.size(); ++i) {
    const Json& e = list[i];
    const std::string at = "\"contents\" entry " + std::to_string(i + 1);
    PlaceLoot l;
    if (e.isString()) {
      l.item = db.findItem(e.asString());
      if (l.item < 0) return c.fail(where, at + " names unknown item " + q(e.asString()) + (db.items.empty() ? " (this game has no items, data/items.json)" : ""));
      l.uses = db.items[(size_t)l.item].uses;
    } else if (e.isObject() && e.has("item")) {
      if (!onlyKeys(c, e, {"item", "uses"}, where + ": " + at)) return false;
      std::string id;
      if (!c.str(e, "item", where, id, true)) return false;
      l.item = db.findItem(id);
      if (l.item < 0) return c.fail(where, at + " names unknown item " + q(id));
      const ItemDef& def = db.items[(size_t)l.item];
      l.uses = def.uses;
      if (e.has("uses")) {
        if (def.uses < 0) return c.fail(where, at + ": " + q(id) + " has unlimited uses, so \"uses\" cannot be set");
        if (!LoadCtx::isWhole(e["uses"]) || e["uses"].asInt() < 1 || e["uses"].asInt() > def.uses)
          return c.fail(where, at + ": \"uses\" must be a whole number from 1 to " + std::to_string(def.uses));
        l.uses = e["uses"].asInt();
      }
    } else if (e.isObject() && e.has("resource")) {
      if (!onlyKeys(c, e, {"resource", "amount"}, where + ": " + at)) return false;
      std::string id;
      if (!c.str(e, "resource", where, id, true)) return false;
      l.resource = db.findResource(id);
      if (l.resource < 0) return c.fail(where, at + " names unknown resource " + q(id));
      if (!c.integer(e, "amount", where, l.amount, true) || l.amount < 1 || l.amount > kMaxStoryNumber)
        return c.fail(where, at + ": \"amount\" must be a whole number of 1 or more");
    } else {
      return c.fail(where, at + " must be an item id, { \"item\": id, \"uses\": n } or { \"resource\": id, \"amount\": n }");
    }
    out.push_back(l);
  }
  return true;
}

}  // namespace

bool readPlacedPlace(const LoadCtx& c, const Database& db, const MapDef& m, const Json& e, const std::string& where,
                     const std::vector<Cell>& cells, MapStructureDef& s) {
  const StructureDef& def = db.structures[(size_t)s.type];
  const PlaceDef& p = def.place;
  if (e.has("id")) {
    if (!e["id"].isString() || !isStoryId(e["id"].asString())) return c.fail(where, "\"id\" must be letters, digits, \"_\" and \"-\"");
    s.id = e["id"].asString();
    for (const MapStructureDef& o : m.structures)
      if (o.id == s.id) return c.fail(where, "\"id\" " + q(s.id) + " is already used by another structure of the map");
  }
  if (e.has("contents")) {
    if (!p.isChest()) return c.fail(where, "\"contents\" only belongs to a chest (a kind with a \"chest\" block)");
    if (!readLoot(c, db, e["contents"], where, s.loot)) return false;
  } else if (p.isChest()) {
    return c.fail(where, "a chest needs \"contents\": what is inside (an item id, { \"item\": id } or { \"resource\": id, \"amount\": n })");
  }
  if (e.has("locked")) {
    if (!p.isDoor() && !p.isChest()) return c.fail(where, "\"locked\" only belongs to a door or a chest");
    if (!e["locked"].isBool()) return c.fail(where, "\"locked\" must be true or false");
    s.locked = e["locked"].asBool() ? 1 : 0;
  }
  if (e.has("visitSides") || e.has("visitUnits")) {
    if (!p.isVisit()) return c.fail(where, "\"visitSides\" / \"visitUnits\" only belong to a place that can be visited (a kind with a \"visit\" block)");
    std::vector<std::string> ids;
    if (!c.stringList(e, "visitSides", where, ids)) return false;
    for (const std::string& id : ids) {
      const int side = findMapSide(m, id);
      if (side < 0) return c.fail(where, "\"visitSides\" names unknown side " + q(id));
      s.visitSides.push_back(side);
    }
    if (!c.stringList(e, "visitUnits", where, s.visitUnits)) return false;  // the ids are checked once the units are loaded (checkPlaceVisitors)
  }
  // A door that opens onto squares nothing can enter is a mistake: judge the opened kind on every square it covers.
  if (p.isDoor() && p.to >= 0) {
    for (Cell cell : cells) {
      const int terrain = m.terrain[(size_t)cell];
      bool any = false;
      for (size_t mt = 0; mt < db.moveTypes.size(); ++mt) any = any || enterCostOf(db, terrain, p.to, (int)mt) > 0;
      if (!any)
        return c.fail(where, "the door opens onto terrain " + q(db.terrains[(size_t)terrain].id) + " that no movement type can enter, so opening it would lead nowhere");
    }
  }
  return true;
}

bool checkPlaceVisitors(const LoadCtx& c, const Database& db, const MapDef& m) {
  for (size_t i = 0; i < m.structures.size(); ++i) {
    const MapStructureDef& s = m.structures[i];
    for (const std::string& u : s.visitUnits) {
      bool known = db.findCharacter(u) >= 0;
      for (const MapUnitDef& mu : m.units) known = known || mu.id == u;
      if (!known)
        return c.fail("structures[" + std::to_string(i) + "] (" + db.structures[(size_t)s.type].id + " at " + std::to_string(s.x) + "," + std::to_string(s.y) + ")",
                      "\"visitUnits\" names " + q(u) + ", which is neither a unit of this map (give it an \"id\") nor a character");
    }
  }
  return true;
}

bool readUnitGoals(const LoadCtx& c, const MapDef& m, const Json& e, const std::string& where, MapUnitDef& u) {
  if (!e.has("goesFor")) return true;
  if (!e["goesFor"].isArray() || e["goesFor"].size() == 0)
    return c.fail(where, "\"goesFor\" must be a list such as [\"chests\", \"villages\", \"doors\"]");
  if (u.side >= 0 && u.side < (int)m.sides.size() && !m.sides[(size_t)u.side].ai)
    return c.fail(where, "\"goesFor\" tells the computer opponent what to do, but this unit is on a side a person plays (controller \"human\")");
  for (size_t i = 0; i < e["goesFor"].size(); ++i) {
    const std::string w = e["goesFor"][i].isString() ? e["goesFor"][i].asString() : "";
    if (w == "chests") u.aiGoals |= kAiChests;
    else if (w == "villages") u.aiGoals |= kAiPlaces;
    else if (w == "doors") u.aiGoals |= kAiDoors;
    else return c.fail(where, "\"goesFor\" entry " + std::to_string(i + 1) + " must be \"chests\", \"villages\" or \"doors\", not " + q(w));
  }
  return true;
}

bool readUnlockKinds(const LoadCtx& c, const Json& v, const std::string& where, bool allowNeeds, bool& doors, bool& chests,
                     std::string& needs) {
  doors = chests = false;
  needs.clear();
  const std::string bad = "\"unlock\" must be true, a list such as [\"door\", \"chest\"]" +
                          std::string(allowNeeds ? " or { \"kinds\": [...], \"needs\": \"<item id>\" }" : "");
  const Json* kinds = &v;
  if (v.isBool()) {
    if (!v.asBool()) return c.fail(where, bad);
    doors = chests = true;
    return true;
  }
  if (v.isObject() && allowNeeds) {
    if (!onlyKeys(c, v, {"kinds", "needs"}, where + ": \"unlock\"")) return false;
    if (v.has("needs")) {
      if (!v["needs"].isString() || v["needs"].asString().empty()) return c.fail(where, "\"unlock\": \"needs\" must be an item id");
      needs = v["needs"].asString();
    }
    if (!v.has("kinds")) { doors = chests = true; return true; }
    kinds = &v["kinds"];
  }
  if (!kinds->isArray() || kinds->size() == 0) return c.fail(where, bad);
  for (size_t i = 0; i < kinds->size(); ++i) {
    const std::string w = (*kinds)[i].isString() ? (*kinds)[i].asString() : "";
    if (w == "door") doors = true;
    else if (w == "chest") chests = true;
    else return c.fail(where, "\"unlock\" kinds must be \"door\" or \"chest\", not " + q(w));
  }
  return true;
}

}  // namespace ote
