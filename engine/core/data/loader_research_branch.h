// The branching-research keys of a technology (docs/ECONOMY.md "Branching research"): tree, tier,
// exclusiveGroup, requiresStructures, bonus.income and grants.skills.
#pragma once
#include "core/data/loader_util.h"

namespace ote {

// One entry of a skill list that a whole side gets (a technology's "grants".skills, a faction's "modifiers"): a skill id or
// { "skill": id, "filter": { "type": id } | { "tag": text } | {} }. `at` names the place for messages.
bool readSkillGrant(const LoadCtx& c, const Database& db, const Json& g, const std::string& at, SkillGrant& out);
// Just the filter part: { "type": id } | { "tag": text } | {}. Sets `filterType` / `filterTag` (left alone for {}).
bool readGrantFilter(const LoadCtx& c, const Database& db, const Json& filter, const std::string& at, int& filterType, std::string& filterTag);
// Reads the keys of entry `e` into `t` (all optional). `where` names the entry for messages.
bool readBranchKeys(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, TechnologyDef& t);
// "bonus": { "income": { resource: n } }; `bonus` is the "bonus" object, `where` names the entry.
bool readBonusIncome(const LoadCtx& c, const Database& db, const Json& bonus, const std::string& where, TechnologyDef& t);

// Structures load after research: turns each requiresStructureIds into kinds, naming the file and entry on a typo.
bool resolveResearchStructures(const std::string& path, Database& db, std::string& err);

}  // namespace ote
