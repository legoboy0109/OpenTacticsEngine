// Playthrough <-> JSON. Written as defensively as the battle save: a save file is user data that
// may be damaged or hand-edited, so every value is checked and `out` changes only if all is well.
#include <cmath>
#include <set>

#include "core/battle/save_items.h"
#include "core/battle/save_reader.h"
#include "core/battle/save_research.h"
#include "core/battle/save_srpg.h"
#include "core/campaign/campaign_save.h"
#include "core/campaign/prep.h"

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

bool readUnit(Reader& rd, const Database& db, const Json& j, const std::string& list, size_t index, RosterUnit& r) {
  std::string where = list + "[" + std::to_string(index) + "]";
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
  if (!savejson::readRosterSrpg(rd, db, j, where, r)) return false;
  return rd.boolean(j, "gone", where, r.gone);
}

bool readSide(Reader& rd, const Database& db, const Json& j, const std::string& list, size_t index, CarriedSide& s) {
  std::string where = list + "[" + std::to_string(index) + "]";
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

// The preparation screen's state and the last deployment (docs/SAVES.md): written only when there is any, so a game
// without preparations saves exactly as before.
void writePrep(const PrepState& prep, const std::vector<std::string>& lastDeployed, Json& out) {
  if (prep.any()) {
    Json p = Json::makeObject();
    p.set("open", Json::makeBool(prep.open));
    p.set("folded", Json::makeBool(prep.folded));
    Json list = Json::makeArray();
    for (const PrepDeploy& d : prep.deployed) {
      Json e = Json::makeObject();
      e.set("id", Json::makeString(d.id));
      e.set("slot", num(d.slot));
      list.push(e);
    }
    p.set("deployed", list);
    out.set("prep", p);
  }
  if (!lastDeployed.empty()) {
    Json list = Json::makeArray();
    for (const std::string& id : lastDeployed) list.push(Json::makeString(id));
    out.set("lastDeployed", list);
  }
}

bool readPrep(Reader& rd, const Json& saved, const std::string& top, PrepState& prep, std::vector<std::string>& lastDeployed) {
  if (saved.has("prep")) {
    const Json& p = saved["prep"];
    const std::string where = top + ".prep";
    if (!p.isObject()) return rd.fail(where, "not an object");
    if (!rd.boolean(p, "open", where, prep.open) || !rd.boolean(p, "folded", where, prep.folded)) return false;
    const Json* list;
    if (!rd.array(p, "deployed", where, -1, 64, list)) return false;
    for (size_t i = 0; i < list->size(); ++i) {
      const std::string w = where + ".deployed[" + std::to_string(i) + "]";
      const Json& e = (*list)[i];
      PrepDeploy d;
      if (!e.isObject()) return rd.fail(w, "not an object");
      if (!readId(rd, e, "id", w, d.id) || !rd.integer(e, "slot", w, 0, 63, d.slot)) return false;
      prep.deployed.push_back(d);
    }
  }
  if (saved.has("lastDeployed")) {
    const Json* list;
    if (!rd.array(saved, "lastDeployed", top, -1, 1000, list)) return false;
    for (size_t i = 0; i < list->size(); ++i) {
      const Json& v = (*list)[i];
      if (!v.isString() || !isStoryId(v.asString()))
        return rd.fail(top + ".lastDeployed[" + std::to_string(i) + "]", "not an id (letters, digits, _ and -)");
      lastDeployed.push_back(v.asString());
    }
  }
  return true;
}

Json rosterJson(const Database& db, const Roster& roster) {
  Json out = Json::makeArray();
  for (const RosterUnit& r : roster) {
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
    savejson::writeRosterSrpg(db, r, j);  // docs/SRPG.md: learned skills, only when there are any
    out.push(j);
  }
  return out;
}

Json sidesJson(const std::vector<CarriedSide>& list) {
  Json sides = Json::makeArray();
  for (const CarriedSide& s : list) {
    Json j = Json::makeObject();
    j.set("id", Json::makeString(s.id));
    j.set("resources", intList(s.resources));
    if (!s.storage.empty()) j.set("storage", savejson::itemsJson(stacks(s.storage)));
    if (!s.research.empty()) j.set("research", savejson::techListJson(s.research));
    sides.push(j);
  }
  return sides;
}

// "roster" and "sides" of `saved` (the playthrough itself, or one parked party), read into `roster` / `sides`; `top` names it in messages.
bool readRosterAndSides(Reader& rd, const Database& db, const Json& saved, const std::string& top, Roster& roster, std::vector<CarriedSide>& sides) {
  const Json* list;
  if (!rd.array(saved, "roster", top, -1, kMaxUnits, list)) return false;
  std::set<std::string> ids;
  for (size_t i = 0; i < list->size(); ++i) {
    RosterUnit r;
    if (!readUnit(rd, db, (*list)[i], top + ".roster", i, r)) return false;
    if (!ids.insert(r.id).second)
      return rd.fail(top + ".roster[" + std::to_string(i) + "].id", "\"" + r.id + "\" belongs to another unit too");
    roster.push_back(r);
  }
  if (!rd.array(saved, "sides", top, -1, 1000, list)) return false;
  ids.clear();
  for (size_t i = 0; i < list->size(); ++i) {
    CarriedSide s;
    if (!readSide(rd, db, (*list)[i], top + ".sides", i, s)) return false;
    if (!ids.insert(s.id).second)
      return rd.fail(top + ".sides[" + std::to_string(i) + "].id", "\"" + s.id + "\" is listed twice");
    sides.push_back(s);
  }
  return true;
}

// Parties (docs/EVENTS.md "Parties", docs/SAVES.md): written only by a campaign that has them, so every other campaign saves as before.
// The active party's data is the playthrough's own roster / sides / prep; `parties` holds the others that were used, `mergedParties`
// the ones merged away.
void writeParties(const Database& db, const Playthrough& pt, Json& out) {
  if (!db.campaign || db.campaign->parties.empty()) return;
  out.set("party", Json::makeString(pt.party));
  Json parked = Json::makeArray();
  for (const ParkedParty& p : pt.parked) {
    Json j = Json::makeObject();
    j.set("id", Json::makeString(p.id));
    j.set("roster", rosterJson(db, p.roster));
    j.set("sides", sidesJson(p.sides));
    writePrep(p.prep, p.lastDeployed, j);
    parked.push(j);
  }
  if (!pt.parked.empty()) out.set("parties", parked);
  if (!pt.merged.empty()) {
    Json m = Json::makeArray();
    for (const std::string& id : pt.merged) m.push(Json::makeString(id));
    out.set("mergedParties", m);
  }
}

bool readPartyId(Reader& rd, const Database& db, const Json& o, const char* key, const std::string& where, std::string& out) {
  if (!readId(rd, o, key, where, out)) return false;
  if (db.campaign->findParty(out) < 0) return rd.fail(where + "." + key, "\"" + out + "\" is not one of the campaign's parties");
  return true;
}

bool readParties(Reader& rd, const Database& db, const Json& saved, Playthrough& pt) {
  const bool has = !db.campaign->parties.empty();
  if (!has) {
    for (const char* k : {"party", "parties", "mergedParties"})
      if (saved.has(k)) return rd.fail(std::string("playthrough.") + k, "the campaign has no parties");
    return true;
  }
  pt.party = db.campaign->parties[0].id;  // a save without the key plays the first party
  if (saved.has("party") && !readPartyId(rd, db, saved, "party", "playthrough", pt.party)) return false;
  std::set<std::string> seen{pt.party};
  if (saved.has("mergedParties")) {
    const Json* list;
    if (!rd.array(saved, "mergedParties", "playthrough", -1, 1000, list)) return false;
    for (size_t i = 0; i < list->size(); ++i) {
      const std::string w = "playthrough.mergedParties[" + std::to_string(i) + "]";
      const Json& v = (*list)[i];
      if (!v.isString() || db.campaign->findParty(v.asString()) < 0) return rd.fail(w, "not one of the campaign's parties");
      if (!seen.insert(v.asString()).second) return rd.fail(w, "\"" + v.asString() + "\" is the active party or listed twice");
      pt.merged.push_back(v.asString());
    }
  }
  if (saved.has("parties")) {
    const Json* list;
    if (!rd.array(saved, "parties", "playthrough", -1, 1000, list)) return false;
    for (size_t i = 0; i < list->size(); ++i) {
      const std::string w = "playthrough.parties[" + std::to_string(i) + "]";
      const Json& j = (*list)[i];
      if (!j.isObject()) return rd.fail(w, "not an object");
      ParkedParty p;
      if (!readPartyId(rd, db, j, "id", w, p.id)) return false;
      if (!seen.insert(p.id).second) return rd.fail(w + ".id", "\"" + p.id + "\" is the active party, merged away or listed twice");
      if (!readRosterAndSides(rd, db, j, w, p.roster, p.sides) || !readPrep(rd, j, w, p.prep, p.lastDeployed)) return false;
      pt.parked.push_back(p);
    }
  }
  return true;
}

}  // namespace

void savePlaythrough(const Database& db, const Playthrough& pt, Json& out) {
  out = Json::makeObject();
  out.set("chapter", num(pt.chapter));
  out.set("phase", Json::makeString(kPhaseNames[(int)pt.phase]));
  out.set("roster", rosterJson(db, pt.roster));
  out.set("sides", sidesJson(pt.sides));
  Json sw = Json::makeArray();
  for (uint8_t b : pt.switches) sw.push(Json::makeBool(b != 0));
  out.set("switches", sw);
  out.set("variables", intList(pt.variables));
  savejson::writePlaythroughSrpg(db, pt, out);  // docs/SRPG.md: supports, only in a game with support pairs
  writePrep(pt.prep, pt.lastDeployed, out);
  writeParties(db, pt, out);
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

  if (!readRosterAndSides(rd, db, saved, top, pt.roster, pt.sides)) return false;
  const Json* sw;
  if (!rd.array(saved, "switches", top, (int)db.switches.size(), 0, sw)) return false;
  for (size_t i = 0; i < sw->size(); ++i) {
    if (!(*sw)[i].isBool()) return rd.fail("playthrough.switches[" + std::to_string(i) + "]", "not true/false");
    pt.switches.push_back((*sw)[i].asBool() ? 1 : 0);
  }
  if (!readInts(rd, saved, "variables", top, (int)db.variables.size(), kMaxStoryNumber, pt.variables)) return false;
  if (!savejson::readPlaythroughSrpg(rd, db, saved, pt)) return false;
  if (!readPrep(rd, saved, top, pt.prep, pt.lastDeployed)) return false;
  if (pt.prep.any() && pt.phase != Phase::Battle) return rd.fail("playthrough.prep", "a preparation only exists in the battle phase");
  if (!readParties(rd, db, saved, pt)) return false;
  out = pt;
  return true;
}

}  // namespace ote
