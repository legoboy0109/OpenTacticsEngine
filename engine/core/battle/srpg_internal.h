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
// Helpers of promotion.cpp. A unit's promotions that fit now by level and by the way it is asked for: through the
// item `item` (index into db->items), or (`item` = -1) offered at its level. `needStand`: only those whose unit type
// the unit could stand as on its square.
void promotionCandidates(const Unit& u, int item, bool needStand, std::vector<const PromotionDef*>& out) const;
bool canStandAs(const Unit& u, int type) const;
// The promotion that would happen (`unitType` -1 = the first that fits), or null with the reason in `why`.
const PromotionDef* chosenPromotion(const Unit& u, int item, int unitType, std::string* why) const;
// The item path of UseItem: promotes the unit that has just used `item` on itself.
void promoteByItem(int unit, int item, int unitType, std::vector<Event>& events);

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
// Helpers of the supports (supports.cpp, supports_action.cpp), kept in this section so other steps' appends at the
// end of the file do not meet them.
// The unit id of a character that still exists (on the board or paired behind a partner), -1 = none.
int supportUnit(int character) const;
// Are the two on one team and "near": a pair-up pair always is; otherwise both are on the board and within `range`
// squares. `uCell` is where `u` stands for this question (a forecast: where it would fight from; kNoCell = its own).
bool supportNear(const Unit& u, Cell uCell, const Unit& v, int range) const;
// Why `unit` cannot have the ready conversation with `target` while standing on `from` (null = it can).
const char* supportWhy(int unit, Cell from, int target) const;
// Adds points to a pair, makes the SupportPoints event, then gains the levels that need no conversation.
void supportGain(int pair, int by, std::vector<Event>& events);
// Gains the ready levels without a conversation, one SupportReached event (and story trigger) each.
void supportSettleLevels(std::vector<Event>& events);
// The event and the "support" story trigger of a level just gained.
void supportReached(int pair, int level, std::vector<Event>& events);
// One fighter's bonus from its partners (supportCombatMods asks it for each fighter).
void supportBonusFor(const Unit& u, Cell cell, CombatMods& out) const;

// ---- combat pipeline (combat_mods.cpp, combat.cpp, step F2) ----
// These replace two older declarations in battle.h, strikeForecast(atk, atkCell, def, defCell, weapon, out) and
// doStrike(atk, def, weapon, baseFlags, events), which are no longer defined or called. counterWeapon (battle.h)
// is now setting up step 1: the weapon the defender fights with, in range or not (see combat_mods.cpp).
// Setting up, steps 1 to 3: the answering weapon, both fighters' modifiers, and whether the defender counters.
// The caller has checked that the attack itself is allowed.
void setUpCombat(int attacker, int defender, int weapon, Cell from, CombatSetup& setup, CombatMods& modsA,
                 CombatMods& modsD) const;
// One side's strike numbers (`counter` = the defender's) with `own` = its modifiers and `foe` = the other's.
// False when that side's weapon cannot hurt the other at all.
bool strikeForecast(const CombatSetup& setup, bool counter, const CombatMods& own, const CombatMods& foe,
                    StrikeForecast& out) const;
// Adds one strike's hit, avoid, crit, critAvoid, damage and damageTaken to strike numbers (not its percents: in a
// real strike a critical multiplies between the two).
void foldStrikeMods(const StrikeMods& mods, StrikeForecast& numbers) const;
// Battle::forecast after its checks, also handing back the setup (applyAttack fights with it).
bool forecastFight(int attacker, int defender, int weapon, Cell from, Forecast& out, CombatSetup& setup) const;
// One strike, steps 1 to 8 of the contract. `extra` = the strike a "strikeAgain" gave (it cannot give another).
void doStrike(const CombatSetup& setup, bool counter, bool extra, std::vector<Event>& events);

// ---- rescue and pair up helpers (rescue.cpp, pair_up.cpp, step R1; append only) ----
bool carryAllowed(const Unit& stays, const Unit& leaves) const;  // rules.rescue.canCarry(a = stays, b = leaves)
// Why the order is not allowed (null = allowed), judged as if the unit stood on `from`. Rescue and PairUp share one.
const char* rescueWhy(const Unit& u, Cell from, int target) const;
const char* handWhy(const Unit& u, Cell from, int target, bool give) const;  // GiveUnit (give) / TakeUnit
const char* dropWhy(const Unit& u, Cell from, Cell cell) const;
const char* switchWhy(const Unit& u) const;
void applyRescueAction(const Action& action, std::vector<Event>& events);   // Rescue / PairUp
void applyDrop(const Action& action, std::vector<Event>& events);
void applyHand(const Action& action, std::vector<Event>& events);           // GiveUnit / TakeUnit
void applySwitchPair(const Action& action, std::vector<Event>& events);
// `leaves` goes off the board behind `stays` (links, square, capture); `carried` stands on `cell` again with its
// turn finished (links cleared). Callers push their events and call markSight() around them as event_visibility.cpp says.
void linkCarried(int stays, int leaves);
void unlinkCarried(int carried, Cell cell);
// Puts a unit someone carries down on the free square nearest to `around`; false (nothing changed) if there is none.
// Used when the carrier changes side (story) and for `moveUnit` on a carried unit.
bool putDownCarried(int carried, Cell around, std::vector<Event>& events);
// The unit a defeated side was carrying goes with it: killed where its carrier stood, no story event (checkDefeat
// does those after its sweep). Safe to call for every carried unit of the side.
void defeatCarried(int carried, std::vector<Event>& events);
// `moveUnit` / `removeUnit` on a carried unit (story_commands.cpp).
void storyMoveCarried(int carried, Cell to, std::vector<Event>& events);
void storyRemoveCarried(int carried, std::vector<Event>& events);
// changeSide on a carrier or a carried unit: puts the carried unit down first; false = no square, leave both as they are.
bool storySeparate(int unit, std::vector<Event>& events);
// A carried unit that was lost because there was no square for it (the carrier fell): its "defeated" story events
// run later, from settle(), because killUnit may be inside a sweep. {unit, killer}.
void flushLostCarried(std::vector<Event>& events);
std::vector<std::pair<int, int>> lostCarried_;

// ---- skills, step K1 (skills.cpp, skill_standing.cpp) ----
// The effects of the unit's "always" blocks whose conditions hold, in skill-list order. `unconditionedOnly` looks only
// at blocks without conditions: what the HP stat asks, so that judging an `hp` condition never asks for itself.
void standingEffects(const Unit& u, bool unconditionedOnly, std::vector<const SkillEffect*>& out) const;
int skillGrowthBonus(const Unit& u, int stat) const;  // the "growths" effects of its skills, in % points
int moveCostCap(const Unit& u) const;                 // the lowest "maxMoveCost" it has, 0 = none
bool passesEnemies(const Unit& u) const;              // has "pass": enemies
// Adds `skill` to Unit::learned unless it is there; true when it was added.
bool learnSkill(Unit& u, int skill) const;

// ---- skills, step K2 (skill_fx.cpp, skill_combat.cpp, skill_strike.cpp, skill_timed.cpp) ----
// What a skill block of a fighter is judged with in this combat: where it stands (the attacker: `setup.from`), the
// weapon it fights with, the foe, and whether it started the fight.
SkillCtx skillCombatCtx(const CombatSetup& setup, int unit) const;
// A fighter's skill list for this combat (weapon skills of the weapon it fights with; a defender with nothing that
// can answer has none).
void skillCombatList(const CombatSetup& setup, int unit, std::vector<int>& out) const;
bool skillBlockHolds(const SkillBlock& block, const SkillCtx& ctx) const;  // all its conditions hold
// The block's chance in percent, 0..100 (formula: a = owner, b = foe). Never rolls.
int skillChance(const SkillBlock& block, const SkillCtx& ctx) const;
// One strike's blocks (the striker's "strike", then the struck unit's "struck"). `rng` set = judge activations
// (roll them and hand back events); null with `notes` set = list the activations that could fire; both null = only
// the blocks without a chance (the forecast's passives). Never changes the battle.
void gatherStrike(const CombatSetup& setup, bool counter, Rng* rng, StrikeMods& out, std::vector<Event>* strikeEvents,
                  std::vector<Event>* struckEvents, std::vector<ForecastNote>* notes) const;
// A timed block (turnStart / defeat / afterCombat) of `owner` whose conditions hold: rolls its chance, applies its
// effects and makes the events. `foe` = the other fighter or -1.
void runTimedBlock(int owner, int foe, int skill, const SkillBlock& block, const SkillCtx& ctx, std::vector<Event>& events);
// "actAgain" was given to the attacker during a combat's after-hooks: endOrder must not end its turn again.
int actAgainPending_ = -1;

// ---- menu abilities and summons, step A1 (abilities.cpp and friends; append only) ----
// Why the unit cannot use the ability now, whatever the target: as if it stood on `from` (`from` other than its own
// square counts as having moved). False = cannot; `why` gets the reason. Uses, cooldown, once per turn, the summon
// limit and the conditions that are about the user.
bool abilityStateOk(const Unit& u, int ability, Cell from, std::string* why) const;
// Is this target legal for the ability if the unit stood on `from`: in range, seen, of the right side, the squares
// the effect needs free (judged on what the unit's team sees), the conditions about the target. A unit target is
// `target` (a "self" ability: -1 or the unit itself), a square target is `cell`.
bool abilityTargetOk(const Unit& u, int ability, Cell from, int target, Cell cell, std::string* why) const;
// A square `moveType` could stand on: on the map, enterCost > 0, and no unit on it that `viewTeam` sees (-1 = the
// truth: no unit at all). `ignore` = a unit that is about to leave (counts as not there).
bool squareFreeFor(Cell c, int moveType, int viewTeam, int ignore) const;
// Ability moves (ability_moves.cpp). plan: on the truth, from the user's own square; do: puts the units on their
// squares (a capture they leave is dropped), UnitRelocated for each; story "reach" events are left to the caller.
void planAbilityMove(const Unit& user, const AbilityDef& def, int target, AbilityPlan& out) const;
void doAbilityMoves(const AbilityPlan& plan, std::vector<Event>& events);
// Summons (summons.cpp). summonUnit: the new unit appears on `cell` (UnitAdded), false if it could not. removeSummon:
// a summon leaves the board for good (UnitRemoved: no defeat, no drops, no XP) and, in turn, its own summons.
bool summonUnit(int summoner, int ability, Cell cell, std::vector<Event>& events);
void removeSummon(int unit, std::vector<Event>& events);
// "givesXp": false: the foe of this experience award is a summon that gives none (the last Attack event of the
// fight names the fighters).
bool summonGivesNoXp(int unit, const std::vector<Event>& events) const;
// Objectives and story conditions "defeated" / "dead" / "alive": a filter by side, team or "any" does not see a summon.
bool summonIgnored(const UnitFilter& filter, const Unit& u) const {
  return u.summoner >= 0 && (filter.kind == UnitFilter::Kind::Any || filter.kind == UnitFilter::Kind::Side ||
                             filter.kind == UnitFilter::Kind::Team);
}
// Notes that the unit used the ability (count, cooldown, once per turn).
void noteAbilityUse(int unit, int ability);

// ---- weapon ranks and stat caps (ranks.cpp, caps.cpp; docs/SRPG.md section 7) ----
// Raises the unit's weapon experience, per group, to what its class and character give (never lowers). `events`
// (optional) gets a RankUp for each group whose rank rose. Called from dressUnit (silent) and promote.
void rankRaiseToClass(Unit& u, int unit, std::vector<Event>* events) const;
// A map unit's own "ranks": values >= 0 replace the group's experience.
void rankOverride(Unit& u, const std::vector<int>& values) const;
// After a combat's strikes (growth "use"): experience for each fighter that struck with a weapon that has a group;
// `from` = index into `events` where the combat's events start.
void ranksAfterCombat(std::vector<Event>& events, size_t from, int attacker, int defender);
// The one place a permanent stat grows: a positive `amount` is cut by the cap; returns the change that happened.
int raiseStat(Unit& u, int stat, int amount) const;

// ---- staves (staves.cpp; docs/SRPG.md section 8) ----
bool validateStaff(const Action& action, std::string* why) const;
void applyStaff(const Action& action, std::vector<Event>& events);
// The units (ascending id) the staff would really affect for the picked unit `center` (-1 = nobody picked) with the user
// on `from`: the target rule, the area, the conditions, and "would change" (a heal needs a hurt unit).
void staffAffected(const Unit& user, int item, int slot, int center, Cell from, std::vector<int>& out) const;
// Would this one effect change `t` (the user on `from`, the staff in `slot`)? null = yes, else the plain reason it would not.
const char* staffEffectWhy(const Unit& user, const StaffDef& d, const StaffEffect& fx, int slot, const Unit& t, Cell from) const;
// The shared tail of a use: the use is spent, then experience and rank experience, then the order ends (staves_apply.cpp).
void finishStaffUse(int uid, int slot, const StaffDef& d, int center, std::vector<Event>& events);
void applyStaffLight(const Action& action, std::vector<Event>& events);
// Lights whose time is up leave at the start of their side's turn (turn.cpp).
void lightsTurnStart();
// A staff formula (`expr` of Database::exprs): a = user, b = `target` (null = none), w = the staff, the user on `from`.
double staffEval(int expr, const Unit& user, const Unit* target, const StaffDef& d, Cell from, double def) const;
// What one effect would restore to `t` before the unit's maximum caps it (0 = nothing).
int staffHealAmount(const Unit& user, const StaffDef& d, const StaffEffect& fx, const Unit& t, Cell from) const;
// Adds weapon experience in `group` (never above the top rank's); the rank before and after.
void addRankXp(Unit& u, int group, int amount, int& before, int& after) const;

// ---- conditions (conditions.cpp; docs/SRPG.md section 9) ----
// Puts the condition on the unit (replaces first, refresh when it has it), the events; false with `resisted` when the unit is immune
// (ConditionResisted). `source` = the unit that did it (-1 = none).
bool inflictCondition(int unit, int condition, int turns, int source, std::vector<Event>& events, bool* resisted = nullptr);
// Removes the listed conditions ("all" = every one); returns how many went. `reason` is the ConditionEnded `c`.
int cureConditions(int unit, const std::vector<int>& ids, bool all, int reason, std::vector<Event>& events);
// The unit's own conditions as hidden skills, in the order applied (skillsOf appends them after the skills it has).
void conditionSkills(const Unit& u, std::vector<int>& out) const;
// The start of a turn (turn.cpp / srpg_hooks.cpp), in this order: wear-off rolls and the count-down (conditionsTurnStart), then the
// "turnStart" blocks run with the skills' (skill_timed.cpp), then conditionsPrevent marks units that cannot act as done, then
// conditionsAct lets berserk units take their own action. conditionsTurnEnd: the end of the side's turn, a condition whose last turn it was goes.
void conditionsTurnStart(std::vector<Event>& events);
void conditionsPrevent(std::vector<Event>& events);
void conditionsAct(std::vector<Event>& events);
void conditionsTurnEnd(std::vector<Event>& events);
// The story (story_conditions.cpp): addCondition / removeCondition run through inflictCondition / cureConditions; hasCondition tests.
void storyAddCondition(const Command& c, std::vector<Event>& events);
void storyRemoveCondition(const Command& c, std::vector<Event>& events);
bool storyHasCondition(const Condition& c) const;
// Units a tick (a "hurt" that may kill) defeated while the blocks were still running: their story events and the win checks follow.
void conditionsSettleDeaths(std::vector<Event>& events);
// A strike that did damage ends the conditions marked "endsWhenStruck".
void conditionsStruck(int unit, std::vector<Event>& events);
// A berserk unit's choice (conditions_berserk.cpp): the square to stand on and the target, or only a square to walk to. False = nothing to do.
struct BerserkPlan {
  Cell moveTo = kNoCell;  // kNoCell = stays
  int target = -1;        // -1 = nobody to hit this turn
  int weapon = -1;
};
bool berserkPlan(const Unit& u, BerserkPlan& out) const;
// May `a` fight `d`: normally an enemy, but a berserk attacker fights anyone.
bool mayFight(const Unit& a, const Unit& d) const;
std::vector<int> condDeaths_;  // see conditionsSettleDeaths
bool stoppedMay(const Unit& u, bool counter) const;
// A defender that cannot dodge is always hit: the strike's hit chance becomes 100 (combat_mods.cpp).
void noDodge(const CombatSetup& setup, bool counter, StrikeForecast& f) const;
