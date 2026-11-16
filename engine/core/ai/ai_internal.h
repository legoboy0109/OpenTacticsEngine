// Internals shared by the AI's source files. Not a public interface:
// everything the rest of the engine may call is declared in ai.h.
#pragma once
#include <map>
#include <vector>

#include "core/battle/battle.h"

namespace ote {
namespace ai {

constexpr int kFar = 1 << 28;  // "unreachable" in a distance field

// One unit's chosen turn: optionally move, then do `kind`.
struct Order {
  double score = 0;
  Cell moveTo = kNoCell;               // kNoCell = act from the current cell
  ActionType kind = ActionType::Wait;  // Attack, Capture, UseItem, Supply or Wait
  int target = -1, weapon = -1;        // Attack only
  int slot = -1;                       // UseItem only: the inventory slot (target = the unit itself)
};

// Cached data shared by all unit evaluations of ONE aiTakeStep call.
// Nothing survives between calls (the AI is stateless on purpose: the same
// Battle always yields the same decision, and copies of a Battle just work).
struct Context {
  explicit Context(const Battle& battle);
  const Battle& b;
  int side;
  std::vector<std::vector<int>> enemyField;  // per move type; empty = not built yet
  std::vector<int> captureTargets;           // capturable structures not claimed by a friendly capturer
  std::vector<int> claimant;                 // per structure: friendly unit allowed to capture it now, or -1
  std::vector<Cell> assigned;                // per unit id: square of a capture target to walk to, or kNoCell
  std::map<std::pair<int, Cell>, std::vector<int>> goalFields;  // (moveType, goal) -> field

  // One claim per structure: a second capturer standing on another square of the
  // same structure would only reset the first one's progress.
  bool mayCapture(const Unit& u, Cell at) const {
    int s = b.structureIndexAt(at);
    return s >= 0 && s < (int)claimant.size() && (claimant[(size_t)s] < 0 || claimant[(size_t)s] == u.id);
  }
  const std::vector<int>& enemyFieldFor(int moveType);
  const std::vector<int>& goalFieldFor(int moveType, Cell goal);
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
// Score of starting/continuing a capture of the structure on cell `at` with `u`.
double scoreCapture(const Battle& b, const Unit& u, Cell at);
double terrainDefense(const Battle& b, Cell c);

// ---- ai_order.cpp ----
Order bestOrder(Context& cx, const Unit& u);

// ---- ai_items.cpp ----
// Turns an Attack or Wait order of a badly hurt unit into drinking its healing item (same square), when it has one.
void considerHealing(const Battle& b, const Unit& u, Order& order);

// ---- ai_supply.cpp ----
// Repair and supply (docs/ECONOMY.md): may replace `order` by a retreat to a repairing structure or a supply
// point, or by a Supply action of a supplier unit. `stops` are the squares the unit may end on (from `reach`).
void considerSupply(const Battle& b, const Unit& u, const Reach& reach, const std::vector<Cell>& stops, Order& order);

// ---- ai_srpg.cpp (docs/SRPG.md "AI"; filled and called by step AI1) ----
// May replace `order` by an action of the strategy-RPG features: an offered promotion, a summon or a heal ability.
// `stops` are the squares the unit may end on (from `reach`).
void considerSrpg(const Battle& b, const Unit& u, const Reach& reach, const std::vector<Cell>& stops, Order& order);

// ---- ai_build.cpp ----
bool chooseBuild(const Battle& b, Cell& cell, int& unitType);

// ---- ai_economy.cpp ----
// The technology to start now (cheapest it can pay while still affording a build), if any.
bool chooseResearch(const Battle& b, int& tech);
// A structure to upgrade now (only with more than twice the cost to spare), if any.
bool chooseUpgrade(const Battle& b, int& structure, int& to);

}  // namespace ai
}  // namespace ote
