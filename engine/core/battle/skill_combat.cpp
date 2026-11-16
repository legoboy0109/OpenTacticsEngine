// Skills in a combat (docs/SRPG.md "Skills" and "Combat pipeline"): the sources of combat modifiers, the strike
// activations and the forecast notes.
// STUB (step K1 left these for step K2, which owns this file): every query answers "none", so a skill with a
// combat block loads and shows up in `skillsOf` but changes no fight yet.
#include "core/battle/battle.h"

namespace ote {

void Battle::skillCombatMods(const CombatSetup&, CombatMods&, CombatMods&) const {}
void Battle::auraCombatMods(const CombatSetup&, CombatMods&, CombatMods&) const {}
void Battle::strikeActivations(const CombatSetup&, bool, StrikeMods&, std::vector<Event>&, std::vector<Event>&) {}
void Battle::strikePassives(const CombatSetup&, bool, StrikeMods&) const {}
void Battle::forecastNotes(const CombatSetup&, std::vector<ForecastNote>& out) const { out.clear(); }

}  // namespace ote
