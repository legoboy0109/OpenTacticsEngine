// Playthrough <-> JSON. Written as defensively as the battle save: a save file is user data that
// may be damaged or hand-edited, so every value is checked and `out` changes only if all is well.
#include <cmath>
#include <set>

#include "core/battle/save_items.h"
#include "core/battle/save_reader.h"
#include "core/battle/save_research.h"
#include "core/campaign/campaign_save.h"

namespace ote {
namespace {

using savejson::Reader;
using savejson::kMaxLevel;
using savejson::kMaxNumber;
using savejson::kMaxUnits;

Json num(double v) { return Json::makeNumber(v); }

const char* const kPhaseNames[] = {"before", "battle", "after", "finished", "over"};

std::vector<ItemStack> stacks(const std::vector<ItemGrant>& items) {
  std::vector<ItemStack> out;
  for (const ItemGrant& g : items) {
    ItemStack s;
    s.item = g.item;
    s.uses = g.uses;
    s.drop = g.drop;
    out.push_back(s);
  }
  return out;
}

std::vector<ItemGrant> grantsOf(const std::vector<ItemStack>& items) {
  std::vector<ItemGrant> out;
  for (const ItemStack& s : items) {
    ItemGrant g;
    g.item = s.item;
    g.uses = s.uses;
    g.drop = s.drop;
    out.push_back(g);
  }
  return out;
}

Json intList(const std::vector<int>& v) {
  Json a = Json::makeArray();
  for (int i : v) a.push(num(i));
  return a;
}

// A list of exactly `count` whole numbers within +-limit.
bool readInts(Reader& rd, const Json& o, const char* key, const std::string& where, int count, int limit, std::vector<int>& out) {
  const Json* list;
  if (!rd.array(o, key, where, count, 0, list)) return false;
  out.assign((size_t)count, 0);
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& v = (*list)[i];
    if (!v.isNumber() || v.asNumber() != std::floor(v.asNumber()) || std::fabs(v.asNumber()) > limit)
      return rd.fail(where + "." + key + "[" + std::to_string(i) + "]", "not a whole number within " + std::to_string(limit));
    out[i] = (int)v.asNumber();
  }
  return true;
}

bool readId(Reader& rd, const Json& o, const char* key, const std::string& where, std::string& out) {
  if (!o[key].isString() || !isStoryId(o[key].asString())) return rd.fail(where + "." + key, "missing or not an id (letters, digits, _ and -)");
  out = o[key].asString();
  return true;
}

bool readUnit(Reader& rd, const Database& db, const Json& j, size_t index, RosterUnit& r) {
  std::string where = "playthrough.roster[" + std::to_string(index) + "]";
  if (!j.isObject()) return rd.fail(where, "not an object");
  if (!readId(rd, j, "id", where, r.id) || !readId(rd, j, "side", where, r.side)) return false;
  if (!rd.integer(j, "type", where, 0, (int)db.unitTypes.size() - 1, r.type)) return false;
  if (!j["name"].isString() || j["name"].asString().size() > 200) return rd.fail(where + ".name", "missing, not text or unreasonably long");
  r.name = j["name"].asString();
  if (!rd.integer(j, "level", where, 1, kMaxLevel, r.level) || !rd.integer(j, "xp", where, 0, kMaxNumber, r.xp)) return false;
  if (!readInts(rd, j, "stats", where, (int)db.stats.size(), kMaxNumber, r.stats)) return false;
  std::vector<ItemStack> items;
  if (!savejson::readItems(rd, db, j, "items", where, db.rules.items.inventorySize, items)) return false;
  r.items = grantsOf(items);
  return rd.boolean(j, "gone", where, r.gone);
}

bool readSide(Reader& rd, const Database& db, const Json& j, size_t index, CarriedSide& s) {
  std::string where = "playthrough.sides[" + std::to_string(index) + "]";
  if (!j.isObject()) return rd.fail(where, "not an object");
  if (!readId(rd, j, "id", where, s.id)) return false;
  if (!readInts(rd, j, "resources", where, (int)db.resources.size(), kMaxNumber, s.resources)) return false;
  std::vector<ItemStack> storage;
  if (!savejson::readItems(rd, db, j, "storage", where, 100000, storage)) return false;
  if (!storage.empty() && db.rules.items.storage == ItemStorage::Off)
    return rd.fail(where + ".storage", "the game has no shared storage");
  s.storage = grantsOf(storage);
  return savejson::readTechList(rd, db, j, "research", where, s.research);
}

}  // namespace

void savePlaythrough(const Database& db, const Playthrough& pt, Json& out) {
  out = Json::makeObject();
  out.set("chapter", num(pt.chapter));
  out.set("phase", Json::makeString(kPhaseNames[(int)pt.phase]));
  Json roster = Json::makeArray();
  for (const RosterUnit& r : pt.roster) {
    Json j = Json::makeObject();
    j.set("id", Json::makeString(r.id));
    j.set("type", num(r.type));
    j.set("name", Json::makeString(r.name));
    j.set("level", num(r.level));
    j.set("xp", num(r.xp));
    j.set("stats", intList(r.stats));
    if (!r.items.empty()) j.set("items", savejson::itemsJson(stacks(r.items)));
    j.set("side", Json::makeString(r.side));
    j.set("gone", Json::makeBool(r.gone));
    roster.push(j);
  }
  out.set("roster", roster);
  Json sides = Json::makeArray();
  for (const CarriedSide& s : pt.sides) {
    Json j = Json::makeObject();
    j.set("id", Json::makeString(s.id));
    j.set("resources", intList(s.resources));
    if (!s.storage.empty()) j.set("storage", savejson::itemsJson(stacks(s.storage)));
    if (!s.research.empty()) j.set("research", savejson::techListJson(s.research));
    sides.push(j);
  }
  out.set("sides", sides);
  Json sw = Json::makeArray();
  for (uint8_t b : pt.switches) sw.push(Json::makeBool(b != 0));
  out.set("switches", sw);
  out.set("variables", intList(pt.variables));
}

bool restorePlaythrough(const Database& db, const Json& saved, Playthrough& out, std::string& err) {
  Reader rd(err);
  if (!db.campaign) {
    err = "playthrough: the game has no campaign";
    return false;
  }
  if (!saved.isObject()) {
    err = "playthrough: not an object";
    return false;
  }
  const std::string top = "playthrough";
  Playthrough pt;
  if (!rd.integer(saved, "chapter", top, 0, (int)db.campaign->chapters.size() - 1, pt.chapter)) return false;
  if (!saved["phase"].isString()) return rd.fail("playthrough.phase", "missing or not text");
  int phase = -1;
  for (int i = 0; i < 5; ++i)
    if (saved["phase"].asString() == kPhaseNames[i]) phase = i;
  if (phase < 0) return rd.fail("playthrough.phase", "\"" + saved["phase"].asString() + "\" is not before, battle, after, finished or over");
  pt.phase = (Phase)phase;

  const Json* roster;
  if (!rd.array(saved, "roster", top, -1, kMaxUnits, roster)) return false;
  std::set<std::string> ids;
  for (size_t i = 0; i < roster->size(); ++i) {
    RosterUnit r;
    if (!readUnit(rd, db, (*roster)[i], i, r)) return false;
    if (!ids.insert(r.id).second)
      return rd.fail("playthrough.roster[" + std::to_string(i) + "].id", "\"" + r.id + "\" belongs to another unit too");
    pt.roster.push_back(r);
  }
  const Json* sides;
  if (!rd.array(saved, "sides", top, -1, 1000, sides)) return false;
  ids.clear();
  for (size_t i = 0; i < sides->size(); ++i) {
    CarriedSide s;
    if (!readSide(rd, db, (*sides)[i], i, s)) return false;
    if (!ids.insert(s.id).second)
      return rd.fail("playthrough.sides[" + std::to_string(i) + "].id", "\"" + s.id + "\" is listed twice");
    pt.sides.push_back(s);
  }
  const Json* sw;
  if (!rd.array(saved, "switches", top, (int)db.switches.size(), 0, sw)) return false;
  for (size_t i = 0; i < sw->size(); ++i) {
    if (!(*sw)[i].isBool()) return rd.fail("playthrough.switches[" + std::to_string(i) + "]", "not true/false");
    pt.switches.push_back((*sw)[i].asBool() ? 1 : 0);
  }
  if (!readInts(rd, saved, "variables", top, (int)db.variables.size(), kMaxStoryNumber, pt.variables)) return false;
  out = pt;
  return true;
}

}  // namespace ote
