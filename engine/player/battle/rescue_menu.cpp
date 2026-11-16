#include "battle/rescue_menu.h"

#include "battle/srpg_menu_ids.h"

namespace ote {
namespace {

RescueEntry entry(const char* label, int id, bool enabled = true, const char* why = "") {
  RescueEntry e;
  e.label = label;
  e.id = id;
  e.enabled = enabled;
  e.extra = why;
  return e;
}

// Drop: always offered to a unit that has someone; greyed when no square is free for the other one.
RescueEntry dropEntry(const Battle& b, int unit) {
  std::vector<Cell> cells;
  b.dropCells(unit, b.units[(size_t)unit].cell, cells);
  return cells.empty() ? entry("Drop", kDrop, false, "no room") : entry("Drop", kDrop);
}

}  // namespace

std::vector<RescueEntry> rescueEntries(const Battle& b, int unit) {
  std::vector<RescueEntry> out;
  const RescueMode mode = b.db->rules.rescue.mode;
  if (mode == RescueMode::Off || unit < 0 || unit >= (int)b.units.size()) return out;
  const Unit& u = b.units[(size_t)unit];
  if (!u.alive || u.acted) return out;  // an acted unit gets only Info and Wait (U1)
  std::vector<int> ids;
  const bool has = u.carrying >= 0;
  if (mode == RescueMode::Carry) {
    if (!has) {
      b.rescueTargets(unit, u.cell, ids);
      if (!ids.empty()) out.push_back(entry("Rescue", kRescue));
      b.takeTargets(unit, u.cell, ids);
      if (!ids.empty()) out.push_back(entry("Take", kTake));
    } else {
      out.push_back(dropEntry(b, unit));
      b.giveTargets(unit, u.cell, ids);
      if (!ids.empty()) out.push_back(entry("Give", kGive));
    }
  } else if (!has) {
    b.rescueTargets(unit, u.cell, ids);
    if (!ids.empty()) out.push_back(entry("Pair up", kPairUp));
  } else {
    if (b.canSwitchPair(unit)) out.push_back(entry("Switch", kSwitch));
    out.push_back(dropEntry(b, unit));
  }
  return out;
}

std::vector<int> rescuePickUnits(const Battle& b, int unit, int menuId) {
  std::vector<int> out;
  const Unit& u = b.units[(size_t)unit];
  switch (menuId) {
    case kRescue: case kPairUp: b.rescueTargets(unit, u.cell, out); break;
    case kGive: b.giveTargets(unit, u.cell, out); break;
    case kTake: b.takeTargets(unit, u.cell, out); break;
    default: break;
  }
  return out;
}

std::vector<Cell> rescuePickCells(const Battle& b, int unit) {
  std::vector<Cell> out;
  b.dropCells(unit, b.units[(size_t)unit].cell, out);
  return out;
}

std::string rescueRowText(const Battle& b, const Unit& u, bool hideName) {
  if (u.carrying < 0 || u.carrying >= (int)b.units.size()) return "";
  const char* label = b.db->rules.rescue.mode == RescueMode::PairUp ? "With: " : "Carrying: ";
  return std::string(label) + (hideName ? "someone" : b.units[(size_t)u.carrying].name);
}

}  // namespace ote
