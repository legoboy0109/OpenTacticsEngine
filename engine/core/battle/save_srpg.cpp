// The strategy-RPG features in saves: the fixed order in which the shared save code asks the areas (save_srpg.h).
// The order of the writers is the order of the keys in the file; never change it once a step has landed.
#include "core/battle/save_srpg.h"

namespace ote {
namespace savejson {

void writeUnitSrpg(const Battle& b, const Unit& u, Json& j) {
  writeUnitSkills(b, u, j);
  writeUnitAbilities(b, u, j);
  writeUnitRescue(b, u, j);
  writeRankXp(*b.db, u.weaponXp, j);
  writeUnitConditions(b, u, j);
}

bool readUnitSrpg(Reader& rd, const Battle& b, const Json& j, const std::string& where, Unit& u) {
  return readUnitSkills(rd, b, j, where, u) && readUnitAbilities(rd, b, j, where, u) && readUnitRescue(rd, b, j, where, u) &&
         readRankXp(rd, *b.db, j, where, u.weaponXp) && readUnitConditions(rd, b, j, where, u);
}

bool checkUnitsSrpg(Reader& rd, Battle& b) { return checkUnitsAbilities(rd, b) && checkUnitsRescue(rd, b); }

void writeBattleSrpg(const Battle& b, Json& out) {
  writeBattleSupports(b, out);
  writeBattleQuotes(b, out);
}
bool readBattleSrpg(Reader& rd, const Json& saved, Battle& b) {
  return readBattleSupports(rd, saved, b) && readBattleQuotes(rd, saved, b);
}

void writeRosterSrpg(const Database& db, const RosterUnit& r, Json& j) {
  writeRosterSkills(db, r, j);
  writeRankXp(db, r.weaponXp, j);
}
bool readRosterSrpg(Reader& rd, const Database& db, const Json& j, const std::string& where, RosterUnit& r) {
  return readRosterSkills(rd, db, j, where, r) && readRankXp(rd, db, j, where, r.weaponXp);
}

void writePlaythroughSrpg(const Database& db, const Playthrough& pt, Json& out) {
  writePlaythroughSupports(db, pt, out);
  writePlaythroughQuotes(db, pt, out);
}
bool readPlaythroughSrpg(Reader& rd, const Database& db, const Json& saved, Playthrough& pt) {
  return readPlaythroughSupports(rd, db, saved, pt) && readPlaythroughQuotes(rd, db, saved, pt);
}

}  // namespace savejson
}  // namespace ote
