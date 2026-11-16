// Loaders of the strategy-RPG data (contract: docs/SRPG.md "Files at a glance"). One file per area; the shared
// loaders (loader.cpp, loader_content.cpp, loader_items.cpp) call these and are not edited again.
//
// Load order (loader.cpp): game.json (with loadRescueRules / loadPromotionRules), loadSkills, loadAbilities (ids
// and shapes only), weapons, items, unit types (each calling the read...() pieces below), terrain,
// loadCharacters, research, structures, graphics, campaign, loadSupports, and last resolveSrpgRefs.
//
// Every stub of step F1 accepts a game without its file / key and refuses one that has it with
// "... is not supported by this build yet", until the area's step lands.
//
// APPEND ONLY for the steps after F1.
#pragma once
#include <string>

#include "core/data/database.h"

namespace ote {

class LoadCtx;
class Json;

// ---- whole files (each optional: a missing file is a game without the feature) ----
bool loadSkills(const std::string& path, Database& db, std::string& err);      // loader_skills.cpp (K1)
bool loadAbilities(const std::string& path, Database& db, std::string& err);   // loader_abilities.cpp (A1)
bool loadCharacters(const std::string& path, Database& db, std::string& err);  // loader_characters.cpp (C1)
bool loadSupports(const std::string& path, Database& db, std::string& err);    // loader_supports.cpp (S1)

// ---- game.json "rules" (loader_srpg_rules.cpp; `rules` is the "rules" object) ----
bool loadRescueRules(const LoadCtx& c, const Json& rules, Database& db);     // "rules.rescue" (R1)
bool loadPromotionRules(const LoadCtx& c, const Json& rules, Database& db);  // "rules.promotion" (P1)

// ---- keys on the entries of data/units.json, weapons.json, items.json ----
// `e` is the entry, `where` names it (`unit "lord"`). Each reads its own keys and leaves the others alone.
bool readUnitSkills(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, UnitTypeDef& u);      // "skills", "learns" (K1)
bool readUnitAbilities(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, UnitTypeDef& u);   // "abilities" (A1)
bool readUnitPromotions(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, UnitTypeDef& u);  // "promotions" (P1)
bool readUnitPairBonus(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, UnitTypeDef& u);   // "pairBonus" (R1)
bool readWeaponSkills(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, WeaponDef& w);      // "skills" (K1)
// Called after the item's kind is known and before the checks of what a kind needs: a usable item with
// `promotes`, or an accessory with `skills`, is valid without heal / raise / bonus.
bool readItemSkills(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, ItemDef& it);         // "skills" (K1)
bool readItemPromotes(const LoadCtx& c, const Json& e, const std::string& where, ItemDef& it);                           // "promotes" (P1)

// ---- cross references, once everything is loaded (loader_srpg_refs.cpp) ----
// Turns every reference between the new data into indices and checks it, area by area; also sets
// Database::turnFlagSkills. `root` is the game folder (the messages name the file an id was written in).
bool resolveSrpgRefs(const std::string& root, Database& db, std::string& err);
bool resolveSkillRefs(const std::string& root, Database& db, std::string& err);      // loader_skills.cpp (K1)
bool resolveAbilityRefs(const std::string& root, Database& db, std::string& err);    // loader_abilities.cpp (A1)
bool resolvePromotionRefs(const std::string& root, Database& db, std::string& err);  // loader_promotion.cpp (P1)
bool resolveCharacterRefs(const std::string& root, Database& db, std::string& err);  // loader_characters.cpp (C1)
bool resolveSupportRefs(const std::string& root, Database& db, std::string& err);    // loader_supports.cpp (S1)

// ---- skill blocks (loader_skill_blocks.cpp, step K1) ----
// Where a condition list is read decides which conditions it may hold: `foe` = "who": "foe" is allowed, `foePoint` =
// "initiating" and "distance" are (there is a combat), `formula` = "formula" is. `place` finishes the message
// "... cannot be used <place>". A menu ability's "if" (A1) uses the same reader with its own scope.
struct SkillCondScope {
  bool foe, foePoint, formula;
  const char* place;
};
// Reads a list of conditions into `out`. `where` names the owner (`skill "x": block 1`); `labelPrefix` starts the
// label of a formula ("data/skills.json: "), see ExprDef.
bool readSkillConds(const LoadCtx& c, Database& db, const Json& list, const std::string& where, const SkillCondScope& scope,
                    const std::string& labelPrefix, std::vector<SkillCond>& out);
// One block of a skill. `where` is `skill "x": block 2`.
bool readSkillBlock(const LoadCtx& c, Database& db, const Json& e, const std::string& where, const std::string& labelPrefix,
                    SkillBlock& out);
// After everything is loaded: do the weapon, terrain, structure and unit keys of the conditions exist?
bool resolveSkillConds(const LoadCtx& c, const Database& db, const std::string& where, const std::vector<SkillCond>& conds);

}  // namespace ote
