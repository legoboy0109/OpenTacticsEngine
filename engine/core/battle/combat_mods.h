// What skills, auras and supports change in a combat (docs/SRPG.md "Combat pipeline"): the bag one fighter gets
// for a whole combat (CombatMods), the bag of one strike (StrikeMods), and the small rules every source shares.
//
// The sources fill the bags (skill_combat.cpp, supports.cpp); combat_mods.cpp and combat.cpp read them. A bag that
// nobody touched changes nothing: a game without skills or supports fights exactly as it did before the bags.
#pragma once
#include <functional>
#include <vector>

#include "core/battle/battle.h"

namespace ote {

// One fighter's modifiers for one combat. Numbers add; percents are applied one after another in the order they
// were added; yes/no effects are "any"; multiStrike keeps the highest.
struct CombatMods {
  std::vector<int> stats;  // per stat index, added to its stats inside the formulas; empty = nothing (see addStat)
  int hit = 0;             // added to its own hit %
  int avoid = 0;           // taken from the foe's hit %
  int crit = 0;            // added to its own crit %
  int critAvoid = 0;       // taken from the foe's crit %
  int damage = 0;          // added to the damage of each of its strikes
  int damageTaken = 0;     // added to the damage of each strike it takes
  std::vector<int> damagePercent;       // its strike damage is multiplied by each in turn (d = d * p / 100)
  std::vector<int> damageTakenPercent;  // the same for the strikes it takes
  int strikes = 0;                      // added to its number of strikes
  int multiStrike = 1;                  // each of its strikes becomes this many in a row; 1 = none
  bool strikeFirst = false;             // when defending, it strikes before the attacker in each exchange
  bool counterAnyRange = false;         // its counterattack ignores its weapon's range
  bool noCounter = false;               // its foe does not counterattack

  // Adds `by` to one stat (the list grows as needed; indices past the game's stats are simply never read).
  void addStat(int stat, int by) {
    if (stat < 0) return;
    if ((size_t)stat >= stats.size()) stats.resize((size_t)stat + 1, 0);
    stats[(size_t)stat] += by;
  }
  void raiseMultiStrike(int n) {
    if (n > multiStrike) multiStrike = n;
  }
};

// What applies to ONE strike: the striker's "strike" blocks and the struck unit's "struck" blocks, already put
// together (both sides' numbers are about this one strike, so there is one bag and not two).
struct StrikeMods {
  int hit = 0;          // striker: added to this strike's hit %
  int avoid = 0;        // struck: taken from it
  int crit = 0;         // striker
  int critAvoid = 0;    // struck
  int damage = 0;       // striker: added to this strike's damage
  int damageTaken = 0;  // struck: added to it too
  std::vector<int> damagePercent;       // striker's, applied first, in the order added
  std::vector<int> damageTakenPercent;  // struck unit's, applied after them
  int drain = 0;             // the striker heals this percent of the damage dealt (several add up)
  bool strikeAgain = false;  // one more strike straight after this one (ignored on that extra strike)
  bool survive = false;      // a strike that would defeat the struck unit leaves it at 1 HP if it had more
};

// The most damage a single strike can do; also keeps the percent arithmetic far from overflowing.
constexpr int kMaxStrikeDamage = 1000000;

// `value` multiplied by each percent in turn with whole-number division, kept within 0..kMaxStrikeDamage.
int applyPercents(int value, const std::vector<int>& percents);

// The activation rule (docs/SRPG.md "Chance"): 0 or less never applies and 100 or more always applies, and
// NEITHER consumes a random number; only 1..99 is rolled. Every chance of a skill block goes through this, so
// adding a skill that cannot fire never shifts the random numbers of a fight.
bool rollActivation(Rng& rng, int chance);

// Test-only source of modifiers (Battle::testMods; docs/SRPG.md step F2). Each function that is set is called
// right after the real sources, with the bags they filled, and may change them freely. Nothing outside the unit
// tests sets Battle::testMods, so it costs one null check per forecast and strike.
struct CombatTestMods {
  // End of Battle::combatMods (so: for every forecast and again before every strike).
  std::function<void(const Battle&, const CombatSetup&, CombatMods& attacker, CombatMods& defender)> combat;
  // One strike of a real fight, after strikeActivations: may roll with the battle's rng and add SkillActivated
  // events (`strikeEvents` come before the Attack event, `struckEvents` too but only when the strike hits).
  std::function<void(Battle&, const CombatSetup&, bool counter, StrikeMods&, std::vector<Event>& strikeEvents,
                     std::vector<Event>& struckEvents)> strike;
  // The forecast's side of the same: after strikePassives (blocks without a chance).
  std::function<void(const Battle&, const CombatSetup&, bool counter, StrikeMods&)> passive;
  // After forecastNotes.
  std::function<void(const Battle&, const CombatSetup&, std::vector<ForecastNote>&)> notes;
};

}  // namespace ote
