// The strategy-RPG features in saves (docs/SRPG.md "Save format summary and versioning", docs/SAVES.md).
//
// save.cpp and campaign_save.cpp call the five pairs of the first group; each asks the areas in a fixed order
// (save_srpg.cpp). An area's functions live in its own file (save_skills.cpp, save_abilities.cpp,
// save_rescue.cpp, save_supports.cpp) and start as stubs that write nothing and refuse their keys, so a game that
// uses none of the features keeps its save bytes.
//
// Rules for every writer: a key is written only when the game uses the feature AND the value is not the default.
// Rules for every reader: a missing key is the default; a key in a game without the feature is refused with a
// path and a plain reason ("units[3].skills: this game has no skills"); every value is checked.
#pragma once
#include <string>

#include "core/base/json.h"
#include "core/battle/battle.h"
#include "core/battle/save_reader.h"

namespace ote {

struct Playthrough;  // core/campaign/playthrough.h

namespace savejson {

// ---- what the shared save code calls (save_srpg.cpp) ----
// One unit of a battle save. `where` is "units[3]". When reading, `b` holds the sides and every unit read so
// far; `u` is not in `b.units` yet.
void writeUnitSrpg(const Battle& b, const Unit& u, Json& j);
bool readUnitSrpg(Reader& rd, const Battle& b, const Json& j, const std::string& where, Unit& u);
// After every unit was read: the rules that tie units together (carry links, summoners).
bool checkUnitsSrpg(Reader& rd, Battle& b);
// The battle as a whole (`out` / `saved` is the "battle" object).
void writeBattleSrpg(const Battle& b, Json& out);
bool readBattleSrpg(Reader& rd, const Json& saved, Battle& b);
// One roster entry of a playthrough. `where` is "playthrough.roster[2]".
void writeRosterSrpg(const Database& db, const RosterUnit& r, Json& j);
bool readRosterSrpg(Reader& rd, const Database& db, const Json& j, const std::string& where, RosterUnit& r);
// The playthrough as a whole (`out` / `saved` is the "playthrough" object).
void writePlaythroughSrpg(const Database& db, const Playthrough& pt, Json& out);
bool readPlaythroughSrpg(Reader& rd, const Database& db, const Json& saved, Playthrough& pt);

// ---- weapon ranks (save_ranks.cpp, docs/SRPG.md section 7): "ranks" of a unit and of a roster entry ----
void writeRankXp(const Database& db, const std::vector<int>& xp, Json& j);
// `out` ends up sized to the game's weapon groups (empty without ranks); a missing key = all 0.
bool readRankXp(Reader& rd, const Database& db, const Json& j, const std::string& where, std::vector<int>& out);

// ---- skills (save_skills.cpp, step K1): units[i] "skills", "moveSpent", "acted", "refreshed"; roster "skills" ----
void writeUnitSkills(const Battle& b, const Unit& u, Json& j);
bool readUnitSkills(Reader& rd, const Battle& b, const Json& j, const std::string& where, Unit& u);
void writeRosterSkills(const Database& db, const RosterUnit& r, Json& j);
bool readRosterSkills(Reader& rd, const Database& db, const Json& j, const std::string& where, RosterUnit& r);

// ---- action points (save_ap.cpp, docs/SRPG.md "Action points"): units[i] "apAttacks" ----
void writeUnitAp(const Battle& b, const Unit& u, Json& j);
bool readUnitAp(Reader& rd, const Battle& b, const Json& j, const std::string& where, Unit& u);

// ---- conditions (save_conditions.cpp, docs/SRPG.md section 9): units[i] "conditions" ----
void writeUnitConditions(const Battle& b, const Unit& u, Json& j);
bool readUnitConditions(Reader& rd, const Battle& b, const Json& j, const std::string& where, Unit& u);

// ---- abilities and summons (save_abilities.cpp, step A1): units[i] "abilities", "summon" ----
void writeUnitAbilities(const Battle& b, const Unit& u, Json& j);
bool readUnitAbilities(Reader& rd, const Battle& b, const Json& j, const std::string& where, Unit& u);
bool checkUnitsAbilities(Reader& rd, const Battle& b);  // "summon.by" names an existing unit with a lower id

// ---- rescue and pair up (save_rescue.cpp, step R1): units[i] "carrying" ----
void writeUnitRescue(const Battle& b, const Unit& u, Json& j);
bool readUnitRescue(Reader& rd, const Battle& b, const Json& j, const std::string& where, Unit& u);
bool checkUnitsRescue(Reader& rd, Battle& b);     // the carry links; also sets Unit::carriedBy

// ---- supports (save_supports.cpp, step S1): "supports" of a battle and of a playthrough ----
void writeBattleSupports(const Battle& b, Json& out);
bool readBattleSupports(Reader& rd, const Json& saved, Battle& b);
void writePlaythroughSupports(const Database& db, const Playthrough& pt, Json& out);
bool readPlaythroughSupports(Reader& rd, const Database& db, const Json& saved, Playthrough& pt);

// ---- quotes (save_quotes.cpp): "quotes" of a battle and of a playthrough ----
void writeBattleQuotes(const Battle& b, Json& out);
bool readBattleQuotes(Reader& rd, const Json& saved, Battle& b);
void writePlaythroughQuotes(const Database& db, const Playthrough& pt, Json& out);
bool readPlaythroughQuotes(Reader& rd, const Database& db, const Json& saved, Playthrough& pt);

}  // namespace savejson
}  // namespace ote
