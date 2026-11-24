// Battle <-> JSON (see save.h and docs/SAVES.md).
//
// Restoring is written defensively: a save file is user data that may be
// damaged or hand-edited, so every value is checked before it is used and a
// problem produces a readable message, never a crash. The new state is built
// in a copy of the battle and only assigned when everything checked out.
#include "core/battle/save.h"
#include "core/battle/save_difficulty.h"
#include "core/battle/save_downed.h"
#include "core/battle/save_behaviour.h"

#include "core/battle/save_fog.h"
#include "core/battle/save_mapstate.h"
#include "core/battle/save_items.h"
#include "core/battle/save_places.h"
#include "core/battle/save_mapweapon.h"
#include "core/battle/save_reader.h"
#include "core/battle/save_commander.h"
#include "core/battle/save_faction.h"
#include "core/battle/save_research.h"
#include "core/battle/save_srpg.h"
#include "core/battle/save_story.h"
#include "core/battle/save_supply.h"

#include <cmath>
#include <cstdio>
#include <set>

namespace ote {
namespace {

Json num(double v) { return Json::makeNumber(v); }
Json boolean(bool v) { return Json::makeBool(v); }

// 64-bit values do not fit the JSON number type (a double keeps 53 bits), so
// the random generator's state is stored as 16 hex digits in a string.
std::string hex64(uint64_t v) {
  char buf[17];
  std::snprintf(buf, sizeof buf, "%016llx", (unsigned long long)v);
  return buf;
}

bool parseHex64(const Json& j, uint64_t& out) {
  if (!j.isString()) return false;
  std::string s = j.asString();
  if (s.size() != 16) return false;
  uint64_t v = 0;
  for (char c : s) {
    int d;
    if (c >= '0' && c <= '9') d = c - '0';
    else if (c >= 'a' && c <= 'f') d = c - 'a' + 10;
    else return false;
    v = (v << 4) | (uint64_t)d;
  }
  out = v;
  return true;
}

using savejson::Reader;
using savejson::kMaxLevel;
using savejson::kMaxNumber;
using savejson::kMaxUnits;

using savejson::itemsJson;
using savejson::readItems;

bool readSide(Reader& rd, const Database& db, const Json& j, const std::string& where, SideState& s) {
  if (!j.isObject()) return rd.fail(where, "not an object");
  if (!readItems(rd, db, j, "storage", where, 100000, s.storage)) return false;
  if (!s.storage.empty() && db.rules.items.storage == ItemStorage::Off)
    return rd.fail(where + ".storage", "the game has no shared storage");
  const Json* res;
  if (!rd.array(j, "resources", where, (int)s.resources.size(), 0, res)) return false;
  for (size_t i = 0; i < res->size(); ++i) {
    const Json& v = (*res)[i];
    if (!v.isNumber() || v.asNumber() != std::floor(v.asNumber()) || std::fabs(v.asNumber()) > kMaxNumber)
      return rd.fail(where + ".resources[" + std::to_string(i) + "]", "not a whole number");
    s.resources[i] = (int)v.asNumber();
  }
  if (!savejson::readSideResearch(rd, db, j, where, s)) return false;
  if (!savejson::readSideModifiers(rd, db, j, where, s)) return false;
  if (!savejson::readSideFaction(rd, db, j, where, s)) return false;  // after the modifiers: its skills go in front of them
  if (!savejson::readSideCommander(rd, db, j, where, s)) return false;  // after the faction: its passive skills follow the faction's (docs/COMMANDERS.md)
  return rd.boolean(j, "defeated", where, s.defeated) && rd.boolean(j, "everHadUnits", where, s.everHadUnits) &&
         rd.boolean(j, "hadHq", where, s.hadHq) && rd.boolean(j, "leaderLost", where, s.leaderLost);
}

bool readUnit(Reader& rd, const Battle& b, const Json& j, int index, Unit& u) {
  std::string where = "units[" + std::to_string(index) + "]";
  if (!j.isObject()) return rd.fail(where, "not an object");
  const Database& db = *b.db;
  u.id = index;
  if (!rd.integer(j, "type", where, 0, (int)db.unitTypes.size() - 1, u.type)) return false;
  if (!rd.integer(j, "side", where, 0, (int)b.sides.size() - 1, u.side)) return false;
  if (!rd.boolean(j, "alive", where, u.alive)) return false;
  if (!rd.integer(j, "cell", where, -1, b.grid.cellCount() - 1, u.cell)) return false;
  if (u.alive && u.cell == kNoCell) return rd.fail(where + ".cell", "a living unit has no cell");
  if (!savejson::readUnitDowned(rd, b, j, where, u)) return false;  // docs/SRPG.md "Downed units and revive": a downed unit keeps its cell
  if (!u.alive && !u.downed && u.cell != kNoCell) return rd.fail(where + ".cell", "a dead unit still has a cell");
  if (j.has("hidden")) {  // docs/EVENTS.md: a hidden unit is off the map, alive in the story
    if (!rd.boolean(j, "hidden", where, u.hidden)) return false;
    if (u.hidden && (u.alive || u.downed)) return rd.fail(where + ".hidden", "a unit on the map is not hidden");
  }
  if (!j["name"].isString()) return rd.fail(where + ".name", "missing or not text");
  u.name = j["name"].asString();
  if (u.name.size() > 200) return rd.fail(where + ".name", "unreasonably long");
  if (!readUnitStoryId(rd, j, where, u.storyId)) return false;
  if (!rd.integer(j, "level", where, 1, kMaxLevel, u.level)) return false;
  if (!rd.integer(j, "xp", where, 0, kMaxNumber, u.xp) || !savejson::readMerit(rd, db, j, where, u.merit)) return false;
  const Json* stats;
  if (!rd.array(j, "stats", where, (int)db.stats.size(), 0, stats)) return false;
  u.stats.assign(db.stats.size(), 0);
  for (size_t i = 0; i < stats->size(); ++i) {
    const Json& v = (*stats)[i];
    if (!v.isNumber() || v.asNumber() != std::floor(v.asNumber()) || std::fabs(v.asNumber()) > kMaxNumber)
      return rd.fail(where + ".stats[" + std::to_string(i) + "]", "not a whole number");
    u.stats[i] = (int)v.asNumber();
  }
  if (!readItems(rd, db, j, "items", where, db.rules.items.inventorySize, u.items)) return false;
  if (!savejson::readUnitSupply(rd, db, j, where, u)) return false;
  // Strategy-RPG features (docs/SRPG.md): who the unit is (derived from its id, never saved), then its own keys.
  // Before the hit points: learned skills may raise the maximum.
  u.character = b.characterOf(u);
  if (!savejson::readUnitSrpg(rd, b, j, where, u)) return false;
  if (!savejson::readUnitGoals(rd, j, where, u)) return false;  // docs/SRPG.md section 10
  if (!savejson::readUnitBehaviour(rd, db, j, where, u)) return false;  // docs/AI.md
  int maxHp = b.stat(u, db.rules.statHp);  // as the rules see it: accessories, research, standing skills
  if (!rd.integer(j, "hp", where, 0, maxHp > 0 ? maxHp : 0, u.hp)) return false;
  if (u.alive && u.hp < 1) return rd.fail(where + ".hp", "a living unit has no hit points");
  if (u.hidden && u.hp < 1) return rd.fail(where + ".hp", "a hidden unit keeps its hit points");
  if (u.downed && u.hp != 0) return rd.fail(where + ".hp", "a downed unit has no hit points");
  return rd.boolean(j, "moved", where, u.moved) && rd.boolean(j, "done", where, u.done) &&
         rd.boolean(j, "leader", where, u.leader);
}

bool readStructure(Reader& rd, const Battle& b, const Json& j, int index, StructureState& st) {
  std::string where = "structures[" + std::to_string(index) + "]";
  if (!j.isObject()) return rd.fail(where, "not an object");
  const std::vector<int> extra = b.changeKindsOf(index);
  if (!savejson::readStructureKind(rd, *b.db, j, where, st, &extra)) return false;  // before anything that depends on the kind
  int points = b.defOf(st).capturePoints;
  if (!rd.integer(j, "owner", where, -1, (int)b.sides.size() - 1, st.owner)) return false;
  if (!rd.integer(j, "capture", where, 0, points > 0 ? points : 0, st.capture)) return false;
  if (!rd.integer(j, "capturer", where, -1, (int)b.units.size() - 1, st.capturer)) return false;
  if (!savejson::readStructurePlace(rd, b, j, where, st)) return false;  // doors, chests, visits (docs/SRPG.md section 10)
  if (!savejson::readStructureWeapon(rd, b, j, where, st)) return false;  // map weapon uses (docs/SRPG.md "Map weapons")
  if (st.capturer >= 0) {
    const Unit& u = b.units[(size_t)st.capturer];
    if (!u.alive) return rd.fail(where + ".capturer", "unit " + std::to_string(st.capturer) + " is dead");
    if (b.structureAt[(size_t)u.cell] != index)
      return rd.fail(where + ".capturer", "unit " + std::to_string(st.capturer) + " is not standing on this structure");
  } else if (st.capture != points) {
    return rd.fail(where + ".capture", "progress is saved but nobody is capturing");
  }
  return true;
}

}  // namespace

void saveBattleState(const Battle& b, Json& out) {
  out = Json::makeObject();
  out.set("round", num(b.round));
  out.set("activeSide", num(b.activeSide));
  out.set("over", boolean(b.over));
  out.set("winnerTeam", num(b.winnerTeam));
  Json rng = Json::makeObject();
  rng.set("state", Json::makeString(hex64(b.rng.state)));
  rng.set("inc", Json::makeString(hex64(b.rng.inc)));
  out.set("rng", rng);

  Json sides = Json::makeArray();
  for (const SideState& s : b.sides) {
    Json j = Json::makeObject();
    Json res = Json::makeArray();
    for (int r : s.resources) res.push(num(r));
    j.set("resources", res);
    j.set("defeated", boolean(s.defeated));
    j.set("everHadUnits", boolean(s.everHadUnits));
    j.set("hadHq", boolean(s.hadHq));
    j.set("leaderLost", boolean(s.leaderLost));
    if (!s.storage.empty()) j.set("storage", itemsJson(s.storage));
    Json research;
    if (savejson::researchJson(s, research)) j.set("research", research);
    Json modifiers;
    if (savejson::modifiersJson(s, modifiers)) j.set("modifiers", modifiers);
    savejson::writeSideFaction(*b.db, s, j);
    savejson::writeSideCommander(*b.db, s, j);
    sides.push(j);
  }
  out.set("sides", sides);

  Json units = Json::makeArray();
  for (const Unit& u : b.units) {
    Json j = Json::makeObject();
    j.set("type", num(u.type));
    j.set("side", num(u.side));
    j.set("cell", num(u.cell));
    j.set("name", Json::makeString(u.name));
    if (!u.storyId.empty()) j.set("id", Json::makeString(u.storyId));
    j.set("level", num(u.level));
    j.set("xp", num(u.xp));
    savejson::writeMerit(*b.db, u.merit, j);
    Json stats = Json::makeArray();
    for (int s : u.stats) stats.push(num(s));
    j.set("stats", stats);
    j.set("hp", num(u.hp));
    j.set("moved", boolean(u.moved));
    j.set("done", boolean(u.done));
    j.set("alive", boolean(u.alive));
    if (u.hidden) j.set("hidden", boolean(true));  // docs/SAVES.md: only a hidden unit has the key
    savejson::writeUnitDowned(u, j);  // docs/SRPG.md "Downed units and revive": only a downed unit has the keys
    j.set("leader", boolean(u.leader));
    if (!u.items.empty()) j.set("items", itemsJson(u.items));
    savejson::writeUnitSupply(*b.db, u, j);
    savejson::writeUnitSrpg(b, u, j);  // docs/SRPG.md: only what the game uses and is not the default
    savejson::writeUnitGoals(u, j);
    savejson::writeUnitBehaviour(u, j);
    units.push(j);
  }
  out.set("units", units);

  Json structures = Json::makeArray();
  int index = 0;
  for (const StructureState& s : b.structures) {
    Json j = Json::makeObject();
    j.set("owner", num(s.owner));
    j.set("capture", num(s.capture));
    j.set("capturer", num(s.capturer));
    savejson::writeStructureKind(s, j);
    savejson::writeStructurePlace(b, s, j);
    savejson::writeStructureWeapon(b, index, j);  // only a structure with a map weapon that has been used (save_mapweapon.cpp)
    structures.push(j);
    ++index;
  }
  out.set("structures", structures);
  savejson::writeMapState(b, out);  // only after a map change or a fog switch (save_mapstate.cpp)
  savejson::writeSeenOwners(b, out);
  savejson::writeSeenKinds(b, out);
  savejson::writeLights(b, out);
  saveStoryState(b, out);
  savejson::writeBattleSrpg(b, out);
  savejson::writeBattleDifficulty(b, out);  // docs/DIFFICULTY.md: only in a game with difficulties
}

bool restoreBattleState(Battle& battle, const Json& saved, std::string& err) {
  Reader rd(err);
  if (!battle.db || battle.sides.empty()) {
    err = "the battle was not initialised before restoring";
    return false;
  }
  if (!saved.isObject()) {
    err = "battle: not an object";
    return false;
  }
  Battle b = battle;  // work on a copy; `battle` only changes when all is well
  const std::string top = "battle";

  if (!rd.integer(saved, "round", top, 1, kMaxNumber, b.round)) return false;
  if (!rd.integer(saved, "activeSide", top, 0, (int)b.sides.size() - 1, b.activeSide)) return false;
  if (!rd.boolean(saved, "over", top, b.over)) return false;
  if (!rd.integer(saved, "winnerTeam", top, -1, b.teamCount - 1, b.winnerTeam)) return false;
  const Json* rng;
  if (!rd.object(saved, "rng", top, rng)) return false;
  if (!parseHex64((*rng)["state"], b.rng.state)) return rd.fail("battle.rng.state", "not a 16-digit hex value");
  if (!parseHex64((*rng)["inc"], b.rng.inc) || (b.rng.inc & 1) == 0)
    return rd.fail("battle.rng.inc", "not an odd 16-digit hex value");

  const Json* sides;
  if (!rd.array(saved, "sides", top, (int)b.sides.size(), 0, sides)) return false;
  for (size_t i = 0; i < b.sides.size(); ++i)
    if (!readSide(rd, *b.db, (*sides)[i], "sides[" + std::to_string(i) + "]", b.sides[i])) return false;

  // Units: the whole list is replaced (units are created and die during play).
  const Json* units;
  if (!rd.array(saved, "units", top, -1, kMaxUnits, units)) return false;
  b.units.clear();
  b.units.reserve(units->size());
  b.occupant.assign(b.occupant.size(), -1);
  std::set<std::string> ids;  // story ids are unique among units
  for (size_t i = 0; i < units->size(); ++i) {
    Unit u;
    if (!readUnit(rd, b, (*units)[i], (int)i, u)) return false;
    if (!u.storyId.empty()) {
      if (!ids.insert(u.storyId).second)
        return rd.fail("units[" + std::to_string(i) + "].id", "\"" + u.storyId + "\" belongs to another unit too");
    }
    if (u.alive || u.downed) {  // a downed unit still holds its square
      if (b.occupant[(size_t)u.cell] != -1)
        return rd.fail("units[" + std::to_string(i) + "].cell",
                       "unit " + std::to_string(b.occupant[(size_t)u.cell]) + " is already on cell " + std::to_string(u.cell));
      b.occupant[(size_t)u.cell] = u.id;
    }
    b.units.push_back(u);
  }
  if (!savejson::checkUnitsSrpg(rd, b)) return false;  // carry links, summoners: rules that tie units together

  const Json* structures;
  if (!rd.array(saved, "structures", top, (int)b.structures.size(), 0, structures)) return false;
  for (size_t i = 0; i < b.structures.size(); ++i)
    if (!readStructure(rd, b, (*structures)[i], (int)i, b.structures[i])) return false;

  if (!savejson::readMapState(rd, saved, b)) return false;  // before anything that needs to know whether fog is on
  if (!savejson::readSeenOwners(rd, saved, b)) return false;
  if (!savejson::readSeenKinds(rd, saved, b)) return false;
  if (!savejson::readLights(rd, saved, b)) return false;
  if (!restoreStoryState(rd, saved, b)) return false;
  if (!savejson::readBattleSrpg(rd, saved, b)) return false;
  if (!savejson::readBattleDifficulty(rd, saved, b)) return false;

  battle = b;
  return true;
}

}  // namespace ote
