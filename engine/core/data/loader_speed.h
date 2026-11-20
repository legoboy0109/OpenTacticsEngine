// Loader of the speed turn order (docs/TURN_ORDER.md): game.json "rules"."turnOrder" and the charge keys of abilities and weapons.
// Every refusal names the file, the entry and the key.
#pragma once
#include "core/data/database.h"
#include "core/data/loader_util.h"

namespace ote {

// game.json "rules"."turnOrder": "sides" (or missing), "speed", or { "mode": "speed", "stat", "threshold", "preview", "roundTicks", "ctCost" }.
// Called after the rules it cannot be combined with (action points, production) are known.
bool loadSpeedRules(const LoadCtx& c, const Json& rules, Database& db);
// "chargeSpeed", "chargeTargets", "interruptedBy" of an ability or a weapon entry (`where` names it). Refused in a game that is not a speed game.
// `what` is "ability" or "weapon" for the messages.
bool readCharge(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, const char* what, ChargeDef& out);

}  // namespace ote
