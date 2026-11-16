// PRIVATE members of class Battle for the strategy-RPG features (contract: docs/SRPG.md). This file is included
// INSIDE the class body in battle.h (private section) and nowhere else; it is not a header of its own.
//
// It exists so that the steps after F1 never edit battle.h: a step that needs one more private function appends
// it here (APPEND ONLY). The shared files call the hooks of the first group; each hook asks the areas in a fixed
// order (srpg_hooks.cpp) and every area's part starts as a stub in the area's own file that does nothing.

// ---- the hooks the shared code calls (srpg_hooks.cpp) ----
// Battle::stat = srpgStat(u, stat, unitStat(...)): own value, accessories and research come in as `base`; this
// adds the standing skill bonuses, then the carry penalty / pair bonus. Returns `base` untouched when nothing applies.
int srpgStat(const Unit& u, int stat, int base) const;
// (Re)derives what follows from a unit's type, story id and level: its character, and the skills it has learned
// by its level. Called once the unit's type, id and level are final and BEFORE its hit points are set (a standing
// skill may raise the maximum): makeUnit, applyBuild, applyRoster, storyAddUnit. Safe to call twice.
void dressUnit(Unit& u) const;
// Ends a unit's order. Every action that ends a unit's turn goes through this (never `done = true` by hand,
// except Wait, an ambush and a story's endTurn): a unit with "move again" that has movement left keeps a move
// (`acted`) instead of being done.
void endOrder(int unit);
// Start of the active side's turn, after repair and supply and before the story's turn events: ability waits and
// summon lifetimes, automatic promotions that were postponed, then "turnStart" skill blocks.
void srpgTurnStart(std::vector<Event>& events);
// The EndTurn action, before the next side's turn begins: supports' "ended the turn side by side".
void srpgTurnEnd(std::vector<Event>& events);
// A unit left the board for good (killUnit, the removeUnit command), called once it is off its square. `by` = the
// unit that defeated it, -1 = nobody. Its summons vanish; a unit it carried is put down or lost.
// May add events and move units: the caller keeps no reference into `units` across it.
void srpgUnitGone(int unit, int by, std::vector<Event>& events);
// Right after a LevelUp event: skills learned at the new level, then an automatic promotion.
// May run story events (a promotion): the caller keeps no reference into `units` across it.
void srpgLevelUp(int unit, std::vector<Event>& events);
// After a combat's experience awards, before the attacker's order ends: "defeat" / "afterCombat" skill blocks
// (attacker first), then supports' "fought together". `hpA` / `hpD` = the fighters' HP before the fight.
void srpgAfterCombat(int attacker, int defender, int hpA, int hpD, std::vector<Event>& events);

// ---- characters (characters.cpp, step C1) ----
void characterDress(Unit& u) const;  // sets Unit::character from the story id

// ---- skills (skills.cpp and friends, steps K1 / K2) ----
int skillStatBonus(const Unit& u, int stat) const;  // sum of the "stats" effects of its "always" blocks that hold
int skillXp(const Unit& u, int amount) const;       // an XP amount passed through its xpPercent effects
bool hasMoveAgain(const Unit& u) const;             // has a "moveAgain" effect that holds
// Learns every "learns" entry of its type and character at or below its level that it does not know yet (no
// event): a new unit, a promotion.
void learnUpTo(Unit& u) const;
void skillsLevelUp(int unit, std::vector<Event>& events);  // the entries for exactly the new level (SkillLearned)
void skillsTurnStart(std::vector<Event>& events);
void skillsAfterCombat(int attacker, int defender, int hpA, int hpD, std::vector<Event>& events);
// Combat pipeline sources (called by combatMods): the two fighters' own "combat" blocks, then other units' auras.
void skillCombatMods(const CombatSetup& setup, CombatMods& attacker, CombatMods& defender) const;
void auraCombatMods(const CombatSetup& setup, CombatMods& attacker, CombatMods& defender) const;
// One strike (doStrike step 2): the striker's "strike" blocks, then the struck unit's "struck" blocks; activations
// are rolled with `rng`. `counter` = the defender strikes. The SkillActivated events are handed back for the
// caller to place: `strikeEvents` always come before the Attack event, `struckEvents` only when the strike hits.
void strikeActivations(const CombatSetup& setup, bool counter, StrikeMods& out, std::vector<Event>& strikeEvents,
                       std::vector<Event>& struckEvents);
// The same without randomness, for the forecast: only the "strike" / "struck" blocks that have no chance.
void strikePassives(const CombatSetup& setup, bool counter, StrikeMods& out) const;
// The activations that could fire in this fight, in roll order (Forecast::notes).
void forecastNotes(const CombatSetup& setup, std::vector<ForecastNote>& out) const;
void storyTeachSkill(const Command& c, std::vector<Event>& events);
void storyForgetSkill(const Command& c, std::vector<Event>& events);
bool storyHasSkill(const Condition& c) const;

// ---- promotion (promotion.cpp, step P1) ----
// The one function every trigger ends in. `def` = the promotion entry of the unit's type (null = a story's free
// class change: base difference, rules.promotion.levelAfter). False (nothing happened) when the unit is not on
// the board or could not stand on its square as the new type.
bool promote(int unit, int toType, const PromotionDef* def, std::vector<Event>& events);
bool validatePromote(const Action& action, std::string* why) const;
void applyPromote(const Action& action, std::vector<Event>& events);
void promotionLevelUp(int unit, std::vector<Event>& events);   // "atLevel": "auto"
void promotionTurnStart(std::vector<Event>& events);            // automatic promotions that had to wait
void storyPromote(const Command& c, std::vector<Event>& events);

// ---- menu abilities and summons (abilities.cpp and friends, step A1) ----
bool validateAbility(const Action& action, std::string* why) const;
void applyAbility(const Action& action, std::vector<Event>& events);
void abilitiesTurnStart(std::vector<Event>& events);                 // waits drop, "used this turn" clears, lifetimes
void summonsUnitGone(int unit, std::vector<Event>& events);          // its summons vanish (in turn theirs)

// ---- rescue and pair up (rescue.cpp, pair_up.cpp, step R1) ----
bool validateRescue(const Action& action, std::string* why) const;   // Rescue, Drop, GiveUnit, TakeUnit, PairUp, SwitchPair
void applyRescue(const Action& action, std::vector<Event>& events);
int rescueStat(const Unit& u, int stat, int value) const;             // carrierStats and the pair bonus
void rescueUnitGone(int unit, int by, std::vector<Event>& events);   // the carrier fell: put down or lost

// ---- supports (supports.cpp, step S1) ----
void supportsInit();  // Battle::init: every pair at its "start" points and level 0
bool validateSupport(const Action& action, std::string* why) const;
void applySupport(const Action& action, std::vector<Event>& events);
void supportsTurnEnd(std::vector<Event>& events);
void supportsAfterCombat(int attacker, int defender, std::vector<Event>& events);
// `healer` restored HP of `target` with an item used on an ally or a heal ability.
void supportsHealed(int healer, int target, std::vector<Event>& events);
void supportCombatMods(const CombatSetup& setup, CombatMods& attacker, CombatMods& defender) const;
void storyAddSupport(const Command& c, std::vector<Event>& events);
bool storySupportHolds(const Condition& c) const;
