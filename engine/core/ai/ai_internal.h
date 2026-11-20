// Internals shared by the AI's source files. Not a public interface:
// everything the rest of the engine may call is declared in ai.h.
#pragma once
#include <map>
#include <memory>
#include <vector>

#include "core/ai/ai_field_cache.h"
#include "core/battle/battle.h"

namespace ote {

// ai_ap.cpp: the computer opponent's turn in a game with action points (docs/AI.md "Action points"); aiTakeStep hands over to it.
bool aiTakeApStep(Battle& battle, std::vector<Event>& events);

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
  GoalFieldSet& goalSetFor(int moveType);  // this move type's cached fields, looked up once per Context
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
void distanceField(const Battle& b, int moveType, const std::vector<Cell>& goals,
                   std::vector<int>& out);
// Fills captureTargets and assigns each ready capturer a distinct structure:
// the unit walks to the nearest of that structure's squares it can enter and
// that is free.
void assignCaptureGoals(Context& cx);

// ---- ai_score.cpp ----
double typeValue(const Database& db, int type);
// Score of attacking `target` with `weapon` from `from`; <= 0 means "not worth it".
double scoreAttack(const Battle& b, const Unit& u, const Unit& target, int weapon, Cell from);
// The same for an area weapon aimed at the square `aim` (ai_score.cpp).
double scoreAreaAttack(const Battle& b, const Unit& u, int weapon, Cell aim, Cell from);
// Score of starting/continuing a capture of the structure on cell `at` with `u`.
double scoreCapture(const Battle& b, const Unit& u, Cell at);
double terrainDefense(const Battle& b, Cell c);

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

// ---- ai_order.cpp ----
Order bestOrder(Context& cx, const Unit& u);

// The score a square costs a unit that does something there while enemies could gang up on it (an attack's own terms).
double exposureCost(const Battle& b, const Unit& u, Cell c);

// ---- ai_staves.cpp (docs/SRPG.md section 8 "AI") ----
// Considers every usable staff of the unit from the square `s` (its travel cost `travel`) and replaces `best` when a use
// beats it. `healKey`: kHealAnyone (the default AI), or for a unit with "heals": "retreating" what lastRetreatingHeal returned: its staves
// with a heal effect then serve only a use whose last-placed healed unit is that id (kNoHealKey: none of them is used).
constexpr int kHealAnyone = -2, kNoHealKey = -1;
void considerStaves(const Battle& b, const Unit& u, Cell s, int travel, Order& best, int healKey = kHealAnyone);
// "heals": "retreating": the highest unit id among the retreating, hurt allies (never the user) a healing staff of the unit could reach from any of
// `stops`, or kNoHealKey.
int lastRetreatingHeal(const Battle& b, const Unit& u, const std::vector<Cell>& stops);

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

}  // namespace ai
}  // namespace ote
