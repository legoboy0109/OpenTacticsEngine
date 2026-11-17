// Small state and query types of the strategy-RPG features (contract: docs/SRPG.md) that Battle and its users
// share. Included by battle.h before Unit is defined, so nothing here may need Battle.
//
// APPEND ONLY for the steps after F1: add fields at the end of a struct and new types at the end of the file.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "core/base/types.h"

namespace ote {

// One ability a unit has used this battle (Unit::abilityUses; docs/SRPG.md "Cost, uses, cooldown").
struct AbilityUse {
  int ability = -1;   // index into db->abilities
  int used = 0;       // times this battle
  int wait = 0;       // turns of its side still to wait
  bool turn = false;  // used this turn (a free ability: once per turn)
};

// Closeness of the support pairs: one entry per pair of db->supports.pairs, in file order. Empty in a game
// without pairs.
struct SupportState {
  std::vector<int> points;     // 0..kMaxSupportPoints
  std::vector<uint8_t> level;  // 0 = none .. the number of the pair's levels
};

// One entry of a unit's ability menu: usable now, or the plain reason it is not.
struct AbilityOption {
  int ability = -1;
  bool ok = false;
  std::string why;  // when !ok: "ready in 2 turns", "no uses left this battle", ...
};

// An activation that could fire in a forecast fight (Forecast::notes), in roll order.
struct ForecastNote {
  int unit = -1;
  int skill = -1;
  int chance = 0;  // percent, judged on the state before the fight
};

// Who fights whom with what (docs/SRPG.md "Combat pipeline"): what the sources of combat modifiers are asked with.
struct CombatSetup {
  int attacker = -1, defender = -1;  // unit ids
  int weaponA = -1;                  // the attacker's weapon (index into db->weapons)
  int weaponD = -1;                  // the defender's answering weapon, -1 = none
  Cell from = kNoCell;               // the square the attacker fights from (a forecast: may differ from its own)
  bool counters = false;             // the defender counterattacks (set once the modifiers are known)
};

// What one fighter gets for one combat, and for one strike: defined by the combat pipeline
// (core/battle/combat_mods.h, step F2).
struct CombatMods;
struct StrikeMods;

// One step of a fight's order (Battle::strikeOrder): who strikes and how many times in a row.
struct StrikeStep {
  bool counter = false;  // false = the attacker strikes, true = the defender
  int count = 1;
};

// What a skill condition is judged for (Battle::skillCondHolds): the skill's owner, and the foe where there is one.
struct SkillCtx {
  int unit = -1;             // the owner (unit id)
  Cell cell = kNoCell;       // the square it stands on (a forecast: would stand on); kNoCell = its own
  int weapon = -1;           // its weapon in use (index into db->weapons), -1 = none
  int foe = -1;              // the other fighter / an ability's target unit, -1 = none
  Cell foeCell = kNoCell;    // kNoCell = the foe's own square
  int foeWeapon = -1;
  bool combat = false;       // a foe point: "initiating" and "distance" have a meaning
  bool initiating = false;   // the owner started this combat
};

// A test-only source of combat modifiers (Battle::testMods; defined in core/battle/combat_mods.h, step F2).
struct CombatTestMods;

// What an ability that moves units will do, worked out on the truth before anything changes (step A1,
// ability_moves.cpp): the units to put on squares, the target first. `blocker` = the unit that stopped or prevented
// the move (-1 = none).
struct AbilityMove {
  int unit = -1;
  Cell to = kNoCell;
};
struct AbilityPlan {
  std::vector<AbilityMove> moves;
  int blocker = -1;
};

}  // namespace ote

namespace ote {

// ---- staves (docs/SRPG.md section 8) ----
// One entry of a unit's staff list: usable now, or the plain reason it is not.
struct StaffOption {
  int slot = -1;  // position in the unit's inventory
  int item = -1;  // index into db->items
  bool ok = false;
  std::string why;  // when !ok: "Needs rank C", "Only Mara can wield this", "nobody in reach needs that", ...
};
// What a staff would do to one unit.
struct StaffHit {
  int unit = -1;
  int amount = 0;   // HP it would restore (all effects together, capped by the unit's maximum)
  int nominal = 0;  // what the staff gives before that cap (the AI judges a small heal against it)
  int hpBefore = 0, hpAfter = 0;
  int hit = 100;   // percent chance; 100 when the staff cannot miss
  // Stage 2: where the staff would put the unit (pull, swap, and a warp once its square is chosen), and for a swap where
  // the user goes; kNoCell = it does not move. `repairItem` = the picked item (an index into db->items) with its uses
  // before and after; -1 when no item is chosen yet (or nothing to repair).
  Cell moveTo = kNoCell, userTo = kNoCell;
  int repairItem = -1, usesBefore = -1, usesAfter = -1;
};
// A forecast of one use (Battle::staffForecast): no randomness is consumed.
struct StaffForecast {
  int user = -1;               // the unit using the staff
  int slot = -1, item = -1;
  int center = -1;             // the picked unit (-1 = nothing is picked)
  std::vector<StaffHit> hits;  // the units really affected, ascending id
  bool canMiss = false;
  int xp = 0;                  // experience the user gets (before skills' xpPercent)
  int usesLeft = -1;           // -1 = unlimited
  bool breaks = false;         // this is its last use
  int minRange = 1, maxRange = 1;
  bool needsPick = true;
  // Stage 2. A warp that still needs its square / a repair that still needs its item (the picks come after the unit).
  bool needsDest = false, needsItem = false;
  // A light staff: `target` of the forecast is the picked square; `area` = the squares it would light, `newSquares` = how
  // many of them the team does not see now. The turns and radius are the staff's.
  bool isLight = false;
  Cell lightCell = kNoCell;
  std::vector<Cell> area;
  int newSquares = 0, lightTurns = 1, lightRadius = 0;
};

// A light a staff made (docs/SRPG.md section 8, stage 2): the side's team sees every square within `radius` of `cell` until
// the side's turn has started `turns` more times. Saved while it lasts ("lights"), only in a battle that has some.
struct LightArea {
  int side = -1;
  Cell cell = kNoCell;
  int radius = 0;
  int turns = 1;
};

}  // namespace ote
