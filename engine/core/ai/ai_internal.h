// Internals shared by the AI's source files. Not a public interface:
// everything the rest of the engine may call is declared in ai.h.
#pragma once
#include <algorithm>
#include <array>
#include <map>
#include <memory>
#include <vector>

#include "core/ai/ai_field_cache.h"
#include "core/battle/battle.h"

namespace ote {

// ai_ap.cpp: the computer opponent's turn in a game with action points (docs/AI.md "Action points"); aiTakeStep hands over to it.
bool aiTakeApStep(Battle& battle, std::vector<Event>& events);

// ai_speed.cpp: the computer opponent's turn in a game with the speed turn order (docs/AI.md "Speed turn order"); aiTakeStep hands over to it.
bool aiTakeSpeedStep(Battle& battle, std::vector<Event>& events);

namespace ai {

constexpr int kFar = 1 << 28;  // "unreachable" in a distance field

// One unit's chosen turn: optionally move, then do `kind`.
struct Order {
  double score = 0;
  Cell moveTo = kNoCell;               // kNoCell = act from the current cell
  ActionType kind = ActionType::Wait;  // Attack, Capture, UseItem, UseStaff, Supply or Wait
  int target = -1, weapon = -1;        // Attack only
  int slot = -1;                       // UseItem / UseStaff: the inventory slot (UseItem: target = the unit itself)
  int ability = -1;                    // Ability only: index into db->abilities (target = unit or -1, cell = square or kNoCell)
  Cell cell = kNoCell;                 // Ability only: the target square of a "cell" ability
  // "staffFirst" (ai_staff_first.cpp): the staff use the unit makes instead of this order when playOrder's roll (0..99) is at most `firstChance`.
  std::shared_ptr<Order> first;
  int firstChance = 0;
  int structure = -1;                  // OpenPlace / VisitPlace: the structure (OpenPlace: slot = the key's inventory slot or -1)
  // Kinds added by AI1 besides the ones above: Promote (takes the first offered promotion, in place) and Ability.
};

// Cached data shared by all unit evaluations of ONE aiTakeStep call.
// Nothing about the game survives between calls (the AI is stateless on purpose: the same Battle always yields
// the same decision, and copies of a Battle just work). The one exception is the content-keyed memo of
// single-goal distance fields in ai_field_cache.h, which can only ever return what a recompute would.
struct Context {
  explicit Context(const Battle& battle);
  const Battle& b;
  int side;
  std::vector<std::vector<int>> enemyField;  // per move type; empty = not built yet
  std::vector<int> captureTargets;           // capturable structures not claimed by a friendly capturer
  std::vector<int> claimant;                 // per structure: friendly unit allowed to capture it now, or -1
  std::vector<Cell> assigned;                // per unit id: square of a capture target to walk to, or kNoCell
  std::vector<std::shared_ptr<GoalFieldSet>> goalSets;  // per move type: single-goal fields, shared across Contexts (ai_field_cache.h)

  // One claim per structure: a second capturer standing on another square of the
  // same structure would only reset the first one's progress.
  bool mayCapture(const Unit& u, Cell at) const {
    int s = b.structureIndexAt(at);
    return s >= 0 && s < (int)claimant.size() && (claimant[(size_t)s] < 0 || claimant[(size_t)s] == u.id);
  }
  const std::vector<int>& enemyFieldFor(int moveType);
  const std::vector<int>& goalFieldFor(int moveType, Cell goal);
  // The same for a particular unit: with height rules and a limited jump the fields go around the cliffs it cannot take (ai_goals.cpp).
  const std::vector<int>& enemyFieldFor(const Unit& u);
  const std::vector<int>& goalFieldFor(const Unit& u, Cell goal);
  GoalFieldSet& goalSetFor(int moveType);  // this move type's cached fields, looked up once per Context
  // Height rules: fields of units with a limited jump, per (move type, jump up, jump down).
  std::map<std::array<int, 3>, std::vector<int>> jumpEnemyField;
  std::map<std::array<int, 3>, std::shared_ptr<GoalFieldSet>> jumpSets;
  bool jumpKey(const Unit& u, std::array<int, 3>& key) const;
  GoalFieldSet& jumpSetFor(const std::array<int, 3>& key);
};

// ---- ai_knowledge.cpp ----
// What the side knows (docs/ECONOMY.md "Fog of war"). Everything in the AI that reads an enemy unit, a structure's
// owner or capture state goes through these; without fog they answer from the truth.
// An enemy of `side`'s team that its team sees now.
bool knownEnemy(const Battle& b, int side, const Unit& other);
// The owner the side's team believes the structure has.
int knownOwner(const Battle& b, int side, int structure);
// The kind the side's team believes the structure is (an upgrade it has not seen is not known to it): what it
// plans on, e.g. the value of a capture target or the income it expects.
const StructureDef& knownDef(const Battle& b, int side, int structure);
// Battle::canCaptureAt, but for a structure the team cannot see judged on the owner it last saw.
bool knownCapturable(const Battle& b, const Unit& u, Cell at);
// Where the side should head to find the enemy: the enemy units it sees; with none in sight (and fog on) the
// structures it last saw in enemy hands, else the unseen square farthest from all of its own.
void enemyGoals(const Battle& b, int side, std::vector<Cell>& out);

// ---- ai_goals.cpp ----
// Travel cost from every cell to the nearest goal, summing terrain entry
// costs for `moveType` and ignoring units (so a blocked chokepoint still
// attracts). kFar where no path exists.
// `jumpUp` / `jumpDown` (height rules): the walker cannot take a step that climbs / drops more than that (kNoHeightLimit = any).
void distanceField(const Battle& b, int moveType, const std::vector<Cell>& goals,
                   std::vector<int>& out, int jumpUp = kNoHeightLimit, int jumpDown = kNoHeightLimit);
// Fills captureTargets and assigns each ready capturer a distinct structure:
// the unit walks to the nearest of that structure's squares it can enter and
// that is free.
void assignCaptureGoals(Context& cx);

// ---- ai_facing.cpp (docs/AI.md "Facing") ----
// The direction a unit turns to when it ends its turn by waiting: toward the nearest foe its team sees; -1 (keep the facing it has) when the game has no
// facing or it sees none.
int endFacing(const Battle& b, const Unit& u);

// ---- ai_score.cpp ----
double typeValue(const Database& db, int type);
// What a unit is worth to the computer: its type's value, a little more for a unit that has earned a rank (docs/AI.md "Unit ranks"); typeValue itself in a game without a rank table.
double unitValue(const Battle& b, const Unit& u);
// Score of attacking `target` with `weapon` from `from`; <= 0 means "not worth it".
double scoreAttack(const Battle& b, const Unit& u, const Unit& target, int weapon, Cell from);
// The same for an area weapon aimed at the square `aim` (ai_score.cpp).
double scoreAreaAttack(const Battle& b, const Unit& u, int weapon, Cell aim, Cell from);
// Score of starting/continuing a capture of the structure on cell `at` with `u`.
double scoreCapture(const Battle& b, const Unit& u, Cell at);
double terrainDefense(const Battle& b, Cell c);
// Height rules: what shooting at `target` from `from` gains for being higher (0 without the rules).
double highGroundBonus(const Battle& b, Cell from, Cell target);

// ---- ai_behaviour.cpp (docs/AI.md): per-unit behaviour; a unit with the default behaviour gets an empty Stance ----
struct Stance {
  const AiBehaviour* beh = nullptr;  // null = the default behaviour
  bool idle = false;                 // does nothing this turn (waiting for its start turn, or asleep)
  bool stay = false;                 // never changes square (hold)
  bool noAdvance = false;            // moves only to act, never to approach the enemy
  bool seeking = false;              // heads for `field`'s goal instead of the nearest enemy
  std::vector<int> field;            // seeking: distance to the goal for every square
  int seekTarget = -1;               // seeking a unit: its id
  int bestGoal = kFar;               // seeking: the smallest goal distance among the squares it may stop on
  // Scripted-style computer players (ai_scripted.cpp): a patrol walks `field` (distance to its next route square); `ignored` marks (per unit id) the
  // hostile units it never chases or attacks, and `goalField` is then the distance to the nearest hostile unit it does not ignore (ownGoals).
  bool patrolling = false;
  std::vector<char> ignored;
  bool ownGoals = false;
  std::vector<int> goalField;
};
// At the start of an AI step: sleeping guards of the active side that have a reason wake up (their groups with them).
void updateWakes(Battle& b);
Stance stanceFor(Context& cx, const Unit& u, const std::vector<Cell>& stops);
// False = this attack is not allowed to the unit; otherwise `score` may be raised (a seeker's target, a favourite target).
bool adjustAttack(const Battle& b, const Stance& st, const Unit& u, const Unit& target, Cell from, double& score);
void considerSeek(const Stance& st, Context& cx, const Unit& u, const std::vector<Cell>& stops, Order& best);

// ---- ai_scripted.cpp (docs/AI.md "Scripted-style computer players") ----
// The unit that acts next when rules.ai.turnOrder is "order" (lowest `order`, then lowest unit id, among the side's ready units); -1 when the
// game plays "best" (the default) or nobody is ready.
int orderedUnit(const Battle& b, int side);
// Fills st.ignored / st.goalField for a unit with an "ignores" list; st.patrolling / st.field (distance to its next route square) for a patrol.
void fillIgnores(const Battle& b, const Unit& u, Stance& st);
void fillPatrol(const Battle& b, const Unit& u, Stance& st);
bool ignoredTarget(const Stance& st, int target);
// The walk of a patrol toward its next route square.
void considerPatrol(const Stance& st, const Battle& b, const Unit& u, const std::vector<Cell>& stops, Order& best);
// guardArea "measure": "moveCost": is a hostile unit it knows of within `within` of path cost from the unit?
bool hostileWithinMoveCost(const Battle& b, const Unit& u, int within);

// ---- ai_weights.cpp (docs/AI.md "Attack weights") ----
// The unit's best attack by the weight set `w` (its behaviour's rules.ai.weights set, or its side's faction's); false when it has none to make
// (nothing scores above 0, or attackAtZero is off).
bool weightedAttack(const Battle& b, const Unit& u, const AiWeightSet& w, const Stance& st, const Reach& reach, const std::vector<Cell>& stops, Order& out);

// ---- ai_retreat.cpp (docs/AI.md "Retreat to heal") ----
// The order of a unit that is retreating: a healing item, a step toward the terrain it falls back to (or away from the enemy), else Wait.
Order retreatOrder(Context& cx, const Unit& u, const std::vector<Cell>& stops);
// The stop that gets the unit closest along `field` (a distanceField), strictly closer than where it stands; equal: better terrain. kNoCell =
// nothing closer. `bestVal` = the field's value there (or where it stands). Shared by retreat, escape (ai_loot.cpp).
Cell stepToward(const Battle& b, const Unit& u, const std::vector<int>& field, const std::vector<Cell>& stops, int& bestVal);
// The stop where the fewest known enemies could hit it next turn (equal: farther from the nearest, then better terrain); kNoCell = where it stands.
Cell safestStop(const Battle& b, const Unit& u, const std::vector<Cell>& stops);

// ---- ai_loot.cpp (docs/AI.md "Looting and escaping") ----
// At the start of every AI step: a looter of the side to act that has nothing left it could take starts escaping, for good.
void updateLooters(Battle& b);
// Does the unit escape now (mode "escape", or a looter that has nothing left)?
bool escapesNow(const Unit& u);
// The order of an escaping unit: leave by the exit it stands on, else walk (or step onto) the nearest exit, else keep away from the enemy.
Order escapeOrder(Context& cx, const Unit& u, const std::vector<Cell>& stops);
// ai_places.cpp: could the unit still open or visit something it goes for anywhere it can walk to (units in the way do not count; with "lootBlockedBy": "units" hostile units do)?
bool lootLeft(const Battle& b, const Unit& u);

// ---- ai_order.cpp ----
Order bestOrder(Context& cx, const Unit& u);
// ai.cpp: carries out an order: the walk, the action, and a Wait when it did not work (so the unit's turn always ends).
void playOrder(Battle& battle, int unit, Order order, std::vector<Event>& events);
// Takes the charged weapons (speed turn order) out of a list of attack options: they are not instant attacks; ai_speed.cpp scores them.
inline void dropCharged(const Battle& b, std::vector<AttackOption>& options) {
  if (!b.speedGame()) return;
  options.erase(std::remove_if(options.begin(), options.end(), [&](const AttackOption& o) { return b.weaponCharges(o.weapon); }), options.end());
}

// The score a square costs a unit that does something there while enemies could gang up on it (an attack's own terms).
double exposureCost(const Battle& b, const Unit& u, Cell c);

// ---- ai_strike.cpp (docs/AI.md "Abilities that strike") ----
// The strike abilities the unit could use from `s` now (usable, a target in reach); `charged` picks those that charge (speed games) or the others.
void strikeAbilitiesAt(const Battle& b, const Unit& u, Cell s, bool charged, std::vector<int>& out);
// The options of strike ability `ability` from `s` that using it would accept: abilityStrikeOptions kept to the targets abilityTargets gives (its "if"
// conditions on the target included), so a chosen order is never refused.
void strikeOptionsFor(const Battle& b, const Unit& u, int ability, Cell s, std::vector<AttackOption>& out);
// A strike ability's option scored as the attack it is (no counter-attack, so none is feared), a little less when it has a price ("spend").
double scoreStrike(const Battle& b, const Unit& u, int ability, const AttackOption& o, Cell s);
// Every instant strike ability from `s` (its travel cost `travel`): replaces `best` with an Ability order when one beats it.
void considerStrikes(const Battle& b, const Unit& u, Cell s, int travel, const Stance& st, Order& best);

// ---- ai_conditions.cpp (docs/AI.md "Status abilities") ----
// What putting `condition` on `t` is worth to `user` at `chance` percent: a share of t's value, 0 on the wrong side for its kind (a helpful one
// on a foe, a harmful one on its own team) and 0 where it would not take hold (immune, or on already for as long).
double conditionWorth(const Battle& b, const Unit& user, const Unit& t, int condition, int turns, int chance);
// What a cure ability is worth used on `t`: a share of t's value (once) when it would take off a harmful condition of its own team or a helpful
// one of a foe; 0 otherwise (so a blessing is never cured from an ally, a curse never from a foe).
double cureWorth(const Battle& b, const Unit& user, const Unit& t, const AbilityDef& d);
// What the ability's "inflict" entries are worth on `t` (conditionWorth of each, by its own chance, summed).
double inflictsWorth(const Battle& b, const Unit& user, const Unit& t, int ability, Cell from);
// What a strike ability's own "inflict" adds to the option's score (each unit it would hit and not defeat, times the strike's hit chance).
double strikeInflictWorth(const Battle& b, const Unit& u, int ability, const AttackOption& o, Cell s);
// Every instant status ability (only "inflict") and cure ability from `s`: replaces `best` with an Ability order when one beats it.
void considerStatusAbilities(const Battle& b, const Unit& u, Cell s, int travel, Order& best);

// ---- ai_ability_area.cpp (docs/AI.md "Abilities with an area") ----
// Every instant heal / cure / status ability with an "area" from `s`: each square it may be aimed at is scored by the sum over the units it would act on
// (a heal and a cure count for its own team and against a foe, a condition by its kind); a total of 0 or less is never used. Replaces `best` when it beats it.
void considerAreaAbilities(const Battle& b, const Unit& u, Cell s, int travel, Order& best);

// ---- ai_revive.cpp (docs/AI.md "Reviving downed units") ----
// What standing `t` up with `hp` is worth to `user` (negative for a foe: never revived): (0.5 + the share of its maximum) times its value.
double reviveWorth(const Battle& b, const Unit& user, const Unit& t, int hp);
// Every single-target revive ability and revive item from `s` on a downed unit of its team: replaces `best` when one beats it. Nothing without rules.downed.
void considerRevives(const Battle& b, const Unit& u, Cell s, int travel, Order& best);

// ---- ai_staves.cpp (docs/SRPG.md section 8 "AI"; docs/AI.md sections 9 and 10) ----
// What a unit's behaviour asks of its staves ("heals", "staffPick", "inflictOnlyOn"). A default policy changes nothing. `keys` (per kind of staff: heal, afflict, cure)
// = the unit placed last that a staff of the kind could serve this turn (-1 = none: such a staff is then not used), filled by lastPlacedKeys when a rule picks it.
struct StaffPolicy {
  bool healsRetreating = false, lastPlaced = false, castersOnly = false;
  int keys[3] = {-1, -1, -1};
};
StaffPolicy staffPolicyOf(const AiBehaviour* beh);
void lastPlacedKeys(const Battle& b, const Unit& u, const std::vector<Cell>& stops, StaffPolicy& pol);
// Does the policy allow this forecast use of the staff item (the staff must be one the computer uses at all)?
bool staffUseAllowed(const Battle& b, const Unit& u, int item, const StaffForecast& f, const StaffPolicy& pol);
// What the usual scoring says a use is worth (heals, cures, afflictions, pulls of allies), before the exposure and travel terms.
double staffUseValue(const Battle& b, const Unit& user, const StaffForecast& f);
// Considers every usable staff of the unit from the square `s` (its travel cost `travel`) and replaces `best` when a use beats it.
void considerStaves(const Battle& b, const Unit& u, Cell s, int travel, Order& best, const StaffPolicy& pol);
// ---- ai_staff_first.cpp (docs/AI.md section 10) ----
// The use "staffFirst" would make: the first staff carried with a legal use from a square in `stops`, on its pick of target. No randomness here: playOrder rolls.
bool staffFirstOrder(const Battle& b, const Unit& u, const Reach& reach, const std::vector<Cell>& stops, const StaffPolicy& pol, Order& out);

// ---- ai_places.cpp (docs/SRPG.md section 10 "Computer opponent") ----
// A unit the map marked ("goesFor") may replace `order` by opening a chest / door or visiting / destroying a place, or by walking toward the
// nearest one. Unmarked units do nothing here.
void considerPlaces(Context& cx, const Unit& u, const Reach& reach, const std::vector<Cell>& stops, Order& best);
// ai_walls.cpp: a unit marked "walls" attacks a breakable door that blocks its way to every enemy, or walks to where it can.
void considerWalls(Context& cx, const Unit& u, const Reach& reach, const std::vector<Cell>& stops, Order& best);

// ---- ai_items.cpp ----
// Slot of the first usable item that heals its carrier and would do something now (-1 = none).
int healingItemSlot(const Battle& b, const Unit& u);
// Turns an Attack or Wait order of a badly hurt unit into drinking its healing item (same square), when it has one.
void considerHealing(const Battle& b, const Unit& u, Order& order);
// A cure item (itself, or a unit next to it when the unit stays where it is); used instead of a plain Wait or a hit that does not finish its target.
void considerCuring(const Battle& b, const Unit& u, Order& order);

// ---- ai_supply.cpp ----
// Repair and supply (docs/ECONOMY.md): may replace `order` by a retreat to a repairing structure or a supply
// point, or by a Supply action of a supplier unit. `stops` are the squares the unit may end on (from `reach`).
void considerSupply(const Battle& b, const Unit& u, const Reach& reach, const std::vector<Cell>& stops, Order& order);

// ---- ai_srpg.cpp (docs/SRPG.md "AI"; step AI1) ----
// May replace `order` by an action of the strategy-RPG features: an offered promotion, a summon or a heal ability.
void considerSrpg(const Battle& b, const Unit& u, Order& order);
// ---- ai_abilities.cpp ---- (called by considerSrpg) summon and heal abilities; the only ones the AI uses.
void considerAbilities(const Battle& b, const Unit& u, Order& order);
// ---- ai_again.cpp ---- A unit that has used its action but has movement left ("move again"): the order for the rest
// of its turn, a step away from the enemies that could reach it, or a plain Wait.
Order moveAgainOrder(const Battle& b, const Unit& u, const std::vector<Cell>& stops);

// ---- ai_build.cpp ----
bool chooseBuild(const Battle& b, Cell& cell, int& unitType);

// ---- ai_economy.cpp ----
// The technology to start now (cheapest it can pay while still affording a build), if any.
bool chooseResearch(const Battle& b, int& tech);
// A structure to upgrade now (only with more than twice the cost to spare), if any.
bool chooseUpgrade(const Battle& b, int& structure, int& to);


// ---- ai_commander.cpp (docs/AI.md "Commanders") ----
// Uses one of the active side's commander powers when it can pay for it and the power is worth it (the meter is full, or the fighting is about to
// start, or the heal / refresh pays off); true when it applied one (the caller's step is then over). A side without a commander: false at once.
bool usePowerIfWorthIt(Battle& b, std::vector<Event>& events);

}  // namespace ai
}  // namespace ote
