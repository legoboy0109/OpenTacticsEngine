// The "prep" block of a map file (docs/EVENTS.md "Preparation"): deployment slots, the limit, required / forbidden /
// fixed characters and the preparation shop. Every refusal says what is wrong and where, in plain words.
#include <algorithm>
#include <memory>
#include <set>

#include "core/data/loader_content.h"
#include "core/data/loader_prep.h"
#include "core/data/loader_util.h"
#include "core/data/prep.h"

namespace ote {

namespace {

bool onlyKeys(const LoadCtx& c, const Json& o, std::initializer_list<const char*> keys, const std::string& where) {
  for (const Json::Member& m : o.members()) {
    bool known = false;
    for (const char* k : keys) known = known || m.first == k;
    if (known) continue;
    std::string list;
    for (const char* k : keys) list += (list.empty() ? "" : ", ") + std::string("\"") + k + "\"";
    return c.fail(where, "unknown key \"" + m.first + "\" (expected " + list + ")");
  }
  return true;
}

// A list of story ids ("forbidden", "fixed"): well-formed, no repeats.
bool readIds(const LoadCtx& c, const Json& prep, const char* key, std::vector<std::string>& out) {
  if (!prep.has(key)) return true;
  const std::string where = "prep";
  if (!prep[key].isArray()) return c.fail(where, std::string("\"") + key + "\" must be a list of character ids");
  const Json& list = prep[key];
  for (size_t i = 0; i < list.size(); ++i) {
    if (!list[i].isString() || !isStoryId(list[i].asString()))
      return c.fail(where, std::string("\"") + key + "\" entry " + std::to_string(i + 1) +
                               " must be a character id (letters, digits, \"_\" and \"-\")");
    const std::string id = list[i].asString();
    if (std::find(out.begin(), out.end(), id) != out.end())
      return c.fail(where, std::string("\"") + key + "\" lists \"" + id + "\" twice");
    out.push_back(id);
  }
  return true;
}

bool has(const std::vector<std::string>& v, const std::string& id) { return std::find(v.begin(), v.end(), id) != v.end(); }

bool readSlots(const LoadCtx& c, const Database& db, const Json& prep, const MapDef& m, const std::vector<int>& structureAt,
               PrepDef& out) {
  if (!prep.has("slots") || !prep["slots"].isArray() || prep["slots"].size() == 0)
    return c.fail("prep", "\"slots\" must be a list of squares such as { \"x\": 1, \"y\": 4 } (at least one)");
  const Json& list = prep["slots"];
  if (list.size() > 64) return c.fail("prep", "\"slots\" has more than 64 squares");
  std::set<int> seen;
  for (size_t i = 0; i < list.size(); ++i) {
    const std::string where = "prep: slot " + std::to_string(i + 1);
    const Json& e = list[i];
    if (!e.isObject()) return c.fail(where, "must be an object such as { \"x\": 1, \"y\": 4 }");
    if (!onlyKeys(c, e, {"x", "y"}, where)) return false;
    PrepSlot s;
    if (!c.integer(e, "x", where, s.x, true) || !c.integer(e, "y", where, s.y, true)) return false;
    if (s.x < 0 || s.y < 0 || s.x >= m.width || s.y >= m.height)
      return c.fail(where, "square " + std::to_string(s.x) + "," + std::to_string(s.y) + " is outside the map (" +
                               std::to_string(m.width) + " x " + std::to_string(m.height) + ")");
    const int cell = s.y * m.width + s.x;
    if (!seen.insert(cell).second)
      return c.fail(where, "square " + std::to_string(s.x) + "," + std::to_string(s.y) + " is already another slot");
    // At least one kind of mover must be able to stand there, or nobody could ever be deployed on it.
    const int terrain = m.terrain[(size_t)cell], structure = structureAt[(size_t)cell];
    const int structureType = structure < 0 ? -1 : m.structures[(size_t)structure].type;
    bool anyone = false;
    for (size_t mt = 0; mt < db.moveTypes.size() && !anyone; ++mt) anyone = enterCostOf(db, terrain, structureType, (int)mt) > 0;
    if (!anyone)
      return c.fail(where, "square " + std::to_string(s.x) + "," + std::to_string(s.y) + " is terrain \"" +
                               db.terrains[(size_t)terrain].id + "\", which no kind of unit can stand on");
    out.slots.push_back(s);
  }
  return true;
}

bool readRequired(const LoadCtx& c, const Json& prep, PrepDef& out) {
  if (!prep.has("required")) return true;
  if (!prep["required"].isArray())
    return c.fail("prep", "\"required\" must be a list of character ids, or { \"character\": \"id\", \"slot\": 2 }");
  const Json& list = prep["required"];
  std::set<int> usedSlots;
  for (size_t i = 0; i < list.size(); ++i) {
    const std::string where = "prep: required entry " + std::to_string(i + 1);
    PrepRequired r;
    if (list[i].isString()) {
      r.id = list[i].asString();
    } else if (list[i].isObject()) {
      if (!onlyKeys(c, list[i], {"character", "slot"}, where)) return false;
      if (!c.str(list[i], "character", where, r.id, true)) return false;
      if (list[i].has("slot")) {
        int n = 0;
        if (!c.integer(list[i], "slot", where, n)) return false;
        if (n < 1 || n > (int)out.slots.size())
          return c.fail(where, "\"slot\" " + std::to_string(n) + " does not exist (this map has " +
                                   std::to_string(out.slots.size()) + " slots, numbered from 1)");
        r.slot = n - 1;
        if (!usedSlots.insert(r.slot).second)
          return c.fail(where, "slot " + std::to_string(n) + " is already fixed for another required character");
      }
    } else {
      return c.fail(where, "must be a character id, or { \"character\": \"id\", \"slot\": 2 }");
    }
    if (!isStoryId(r.id)) return c.fail(where, "\"" + r.id + "\" is not a character id (letters, digits, \"_\" and \"-\")");
    for (const PrepRequired& o : out.required)
      if (o.id == r.id) return c.fail(where, "\"" + r.id + "\" is listed twice");
    if (has(out.forbidden, r.id)) return c.fail(where, "\"" + r.id + "\" is also forbidden on this map");
    if (has(out.fixed, r.id)) return c.fail(where, "\"" + r.id + "\" is also a fixed unit (a fixed unit is already always in)");
    out.required.push_back(r);
  }
  return true;
}

bool readShop(const LoadCtx& c, const Database& db, const Json& prep, PrepDef& out) {
  if (!prep.has("shop")) return true;
  const Json& s = prep["shop"];
  if (!s.isObject()) return c.fail("prep", "\"shop\" must be an object such as { \"sells\": [\"potion\"] }");
  if (!onlyKeys(c, s, {"sells"}, "prep: \"shop\"")) return false;
  std::vector<std::string> ids;
  if (!s.has("sells") || !c.stringList(s, "sells", "prep: \"shop\"", ids) || ids.empty())
    return c.fail("prep: \"shop\"", "\"sells\" must list at least one item id");
  for (const std::string& iid : ids) {
    const int it = db.findItem(iid);
    if (it < 0) return c.fail("prep: \"shop\"", "\"sells\" names unknown item \"" + iid + "\" (data/items.json)");
    if (db.items[(size_t)it].price.empty())
      return c.fail("prep: \"shop\"", "\"sells\" lists \"" + iid + "\", which has no \"price\" (data/items.json)");
    if (std::find(out.shop.begin(), out.shop.end(), it) != out.shop.end())
      return c.fail("prep: \"shop\"", "\"sells\" lists \"" + iid + "\" twice");
    out.shop.push_back(it);
  }
  out.hasShop = true;
  return true;
}

}  // namespace

bool loadMapPrep(const LoadCtx& c, const Database& db, const Json& root, MapDef& m, const std::vector<int>& structureAt) {
  if (!root.has("prep")) return true;
  const Json& prep = root["prep"];
  if (!prep.isObject()) return c.fail("", "\"prep\" must be an object (the preparation screen, docs/EVENTS.md \"Preparation\")");
  if (!onlyKeys(c, prep, {"slots", "limit", "required", "forbidden", "fixed", "shop", "side"}, "prep")) return false;
  PrepDef out;

  // The side the deployed characters join: a human side (default: the first one).
  std::string side;
  if (!c.str(prep, "side", "prep", side)) return false;
  if (prep.has("side")) {
    out.side = findMapSide(m, side);
    if (out.side < 0) return c.fail("prep", "\"side\" names unknown side \"" + side + "\"");
    if (m.sides[(size_t)out.side].ai) return c.fail("prep", "\"side\" \"" + side + "\" is a computer side; the preparation is for a human side");
  } else {
    for (size_t i = 0; i < m.sides.size() && out.side < 0; ++i)
      if (!m.sides[i].ai) out.side = (int)i;
    if (out.side < 0) return c.fail("prep", "the map has no human side, so nobody can be prepared");
  }

  if (!readSlots(c, db, prep, m, structureAt, out)) return false;
  out.limit = (int)out.slots.size();
  if (prep.has("limit")) {
    if (!c.integer(prep, "limit", "prep", out.limit)) return false;
    if (out.limit < 1) return c.fail("prep", "\"limit\" must be at least 1");
    if (out.limit > (int)out.slots.size())
      return c.fail("prep", "\"limit\" " + std::to_string(out.limit) + " is more than the map's " +
                                std::to_string(out.slots.size()) + " slots");
  }
  if (!readIds(c, prep, "forbidden", out.forbidden) || !readIds(c, prep, "fixed", out.fixed)) return false;

  // Fixed units are placed units of the preparation side; forbidden ones are not placed there.
  for (const std::string& id : out.fixed) {
    bool placed = false;
    for (const MapUnitDef& u : m.units) placed = placed || u.id == id;
    if (!placed) return c.fail("prep", "\"fixed\" names \"" + id + "\", but the map does not place a unit with that id");
    if (has(out.forbidden, id)) return c.fail("prep", "\"" + id + "\" is both fixed and forbidden");
  }
  for (const std::string& id : out.forbidden)
    for (const MapUnitDef& u : m.units)
      if (u.id == id && u.side == out.side)
        return c.fail("prep", "\"" + id + "\" is forbidden, but the map places that unit on the preparation side");

  if (!readRequired(c, prep, out)) return false;
  if ((int)out.required.size() > out.limit)
    return c.fail("prep", std::to_string(out.required.size()) + " required characters do not fit the limit of " +
                              std::to_string(out.limit));

  // A slot must be free of the units that stay where the map puts them: computer units, fixed units, unnamed units and
  // units of other sides. The choosable units' own squares do not matter (the slots decide where they start).
  for (size_t i = 0; i < out.slots.size(); ++i)
    for (const MapUnitDef& u : m.units) {
      const bool choosable = u.side == out.side && !u.id.empty() && !has(out.fixed, u.id);
      if (!choosable && u.x == out.slots[i].x && u.y == out.slots[i].y)
        return c.fail("prep: slot " + std::to_string(i + 1), "is the square of a unit that stays where it is placed (" +
                                                                 (u.id.empty() ? std::string("an unnamed unit") : "\"" + u.id + "\"") +
                                                                 " at " + std::to_string(u.x) + "," + std::to_string(u.y) + ")");
    }

  if (!readShop(c, db, prep, out)) return false;
  m.prep = std::make_shared<const PrepDef>(out);
  return true;
}

}  // namespace ote
