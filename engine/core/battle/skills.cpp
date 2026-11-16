// Skills in a battle (docs/SRPG.md "Skills" and "Combat pipeline").
// STUB (step F1): no game has skills yet (data/skills.json is refused by the loader), so every query answers
// "none" and every hook does nothing. Steps K1 (sources, standing effects, learning) and K2 (combat and timed
// effects) fill this file and its siblings (skill_cond.cpp, skill_standing.cpp, skill_combat.cpp, skill_timed.cpp).
#include "core/battle/battle.h"

namespace ote {

void Battle::skillsOf(const Unit&, int, std::vector<int>& out) const { out.clear(); }
bool Battle::hasSkill(const Unit&, int) const { return false; }
bool Battle::skillCondHolds(const SkillCond&, const SkillCtx&) const { return false; }

int Battle::skillStatBonus(const Unit&, int) const { return 0; }
int Battle::skillXp(const Unit&, int amount) const { return amount; }
bool Battle::hasMoveAgain(const Unit&) const { return false; }
void Battle::learnUpTo(Unit&) const {}
void Battle::skillsLevelUp(int, std::vector<Event>&) {}
void Battle::skillsTurnStart(std::vector<Event>&) {}
void Battle::skillsAfterCombat(int, int, int, int, std::vector<Event>&) {}

void Battle::skillCombatMods(const CombatSetup&, CombatMods&, CombatMods&) const {}
void Battle::auraCombatMods(const CombatSetup&, CombatMods&, CombatMods&) const {}
void Battle::strikeActivations(const CombatSetup&, bool, StrikeMods&, std::vector<Event>&, std::vector<Event>&) {}
void Battle::strikePassives(const CombatSetup&, bool, StrikeMods&) const {}
void Battle::forecastNotes(const CombatSetup&, std::vector<ForecastNote>& out) const { out.clear(); }

// The story loader refuses the commands and the condition until skills exist, so these are never reached.
void Battle::storyTeachSkill(const Command&, std::vector<Event>&) {}
void Battle::storyForgetSkill(const Command&, std::vector<Event>&) {}
bool Battle::storyHasSkill(const Condition&) const { return false; }

}  // namespace ote
