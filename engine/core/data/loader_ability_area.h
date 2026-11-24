// An ability's "area" and its list of "inflict" entries (docs/SRPG.md "Areas and several effects for abilities"). Definitions:
// loader_ability_area.cpp; called by loader_abilities.cpp (area) and loader_fidelity.cpp (inflict).
#pragma once
#include <string>

#include "core/data/database.h"

namespace ote {

class LoadCtx;
class Json;

// "inflict": one { "condition", "turns", "chance" } object or a list of them (1..8, each condition once). Fills AbilityDef::inflicts and mirrors
// the first entry into inflictCond / inflictTurns / inflictChance / inflictChanceExpr.
bool readAbilityInflicts(const LoadCtx& c, Database& db, const Json& v, const std::string& where, AbilityDef& a);
// "area": read after the effect, the target and the charge are known (it refuses the effects and targets it cannot go with).
bool readAbilityArea(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, AbilityDef& a);

}  // namespace ote
