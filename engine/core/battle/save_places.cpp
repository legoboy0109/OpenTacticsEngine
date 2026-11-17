#include "core/battle/save_places.h"

namespace ote {
namespace savejson {

namespace {
Json num(double v) { return Json::makeNumber(v); }
}  // namespace

void writeStructurePlace(const Battle& b, const StructureState& s, Json& out) {
  const PlaceDef& p = b.defOf(s).place;
  if (p.isDoor() || p.isChest()) out.set("locked", Json::makeBool(s.locked));
  if (s.hp >= 0) out.set("hp", num(s.hp));
  if (s.used) out.set("used", Json::makeBool(true));
  if (!s.loot.empty()) {
    Json list = Json::makeArray();
    for (const PlaceLoot& l : s.loot) {
      Json e = Json::makeObject();
      if (l.item >= 0) {
        e.set("item", num(l.item));
        e.set("uses", num(l.uses));
      } else {
        e.set("resource", num(l.resource));
        e.set("amount", num(l.amount));
      }
      list.push(e);
    }
    out.set("loot", list);
  }
}

bool readStructurePlace(Reader& rd, const Battle& b, const Json& j, const std::string& where, StructureState& st) {
  const Database& db = *b.db;
  const PlaceDef& p = b.defOf(st).place;
  st.locked = p.isDoor() || p.isChest() ? p.locked : false;
  st.hp = p.isDoor() && p.hp > 0 ? p.hp : -1;
  st.used = false;
  st.loot.clear();
  if (j.has("locked")) {
    if (!p.isDoor() && !p.isChest()) return rd.fail(where + ".locked", "this structure is not a door or chest");
    if (!rd.boolean(j, "locked", where, st.locked)) return false;
  }
  if (j.has("hp")) {
    if (!p.isDoor() || p.hp <= 0) return rd.fail(where + ".hp", "this structure is not a door with hit points");
    if (!rd.integer(j, "hp", where, 1, p.hp, st.hp)) return false;
  }
  if (j.has("used") && !rd.boolean(j, "used", where, st.used)) return false;  // (a visited place has often become another kind)
  if (j.has("loot")) {
    if (!p.isChest()) return rd.fail(where + ".loot", "this structure is not a chest");
    const Json* list;
    if (!rd.array(j, "loot", where, -1, kMaxChestLoot, list)) return false;
    for (size_t i = 0; i < list->size(); ++i) {
      const std::string at = where + ".loot[" + std::to_string(i) + "]";
      const Json& e = (*list)[i];
      if (!e.isObject()) return rd.fail(at, "not an object");
      PlaceLoot l;
      if (e.has("item")) {
        if (!rd.integer(e, "item", at, 0, (int)db.items.size() - 1, l.item)) return false;
        if (!rd.integer(e, "uses", at, -1, 999, l.uses)) return false;
      } else {
        if (!rd.integer(e, "resource", at, 0, (int)db.resources.size() - 1, l.resource)) return false;
        if (!rd.integer(e, "amount", at, 1, kMaxNumber, l.amount)) return false;
      }
      st.loot.push_back(l);
    }
  }
  return true;
}

void writeUnitGoals(const Unit& u, Json& out) {
  if (u.aiGoals != 0) out.set("goes", num(u.aiGoals));
}

bool readUnitGoals(Reader& rd, const Json& j, const std::string& where, Unit& u) {
  u.aiGoals = 0;
  if (!j.has("goes")) return true;
  return rd.integer(j, "goes", where, 1, kAiChests | kAiPlaces | kAiDoors, u.aiGoals);
}

}  // namespace savejson
}  // namespace ote
