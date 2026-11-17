// Loader of data/conditions.json and of every key that names a condition: a weapon's "inflict", a usable item's "cure" /
// "inflict", a staff's "inflict" / "cure" effects and the skill effect "immune" (contract: docs/SRPG.md section 9).
//
// Conditions load right after skills and before abilities, weapons, items and unit types, so those can name them and get
// indices at once. A condition's blocks are skill blocks (loader_skill_blocks.cpp), kept as a hidden SkillDef whose index is
// ConditionDef::skill, so every machine of the skill system serves them; what the blocks name (weapon groups, terrain,
// abilities, other conditions) is checked last, in resolveConditionRefs.
#pragma once
#include <string>

#include "core/data/database.h"

namespace ote {

class LoadCtx;
class Json;

bool loadConditions(const std::string& path, Database& db, std::string& err);

// A weapon's "inflict": { "condition": "poison", "chance": 30, "turns": 2 }: becomes a hidden skill with one "strike" block.
bool readWeaponInflict(const LoadCtx& c, Database& db, const Json& e, const std::string& where, WeaponDef& w);
// A usable item's "cure": [ids] | "all" and "inflict": { "condition", "turns" }.
bool readItemConditions(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, ItemDef& it);

// The parts a staff's effects share with the above. `at` names the place (`"effects" entry 2`).
// "inflict": { "condition": id, "turns": n }; `chance` is read only when `allowChance`.
struct InflictSpec {
  int condition = -1;
  int turns = 0;               // 0 = the condition's own
  const Json* chance = nullptr;
};
bool readInflictSpec(const LoadCtx& c, const Database& db, const Json& v, const std::string& where, const std::string& at,
                     bool allowChance, InflictSpec& out);
// "cure": ["poison", "sleep"] or "all".
bool readCureList(const LoadCtx& c, const Database& db, const Json& v, const std::string& where, const std::string& at,
                  std::vector<int>& ids, bool& all);

// What a block names, as indices: the conditions of "immune" and "inflict" effects.
bool resolveBlockConditions(const LoadCtx& c, const Database& db, const std::string& where, SkillBlock& block);
// In resolveSrpgRefs: the conditions' replaces lists, blocks and the groups their "forbid" effects name.
bool resolveConditionRefs(const std::string& root, Database& db, std::string& err);

}  // namespace ote
