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
  ActionType kind = ActionType::Wait;  // Attack, Capture, UseItem or Wait
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

// ---- ai_build.cpp ----
bool chooseBuild(const Battle& b, Cell& cell, int& unitType);

// ---- ai_economy.cpp ----
// The technology to start now (cheapest it can pay while still affording a build), if any.
bool chooseResearch(const Battle& b, int& tech);
// A structure to upgrade now (only with more than twice the cost to spare), if any.
bool chooseUpgrade(const Battle& b, int& structure, int& to);

}  // namespace ai
}  // namespace ote
