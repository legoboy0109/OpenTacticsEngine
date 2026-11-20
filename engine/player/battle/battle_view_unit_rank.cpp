// View hooks for UnitRankUp (docs/SRPG.md "Unit ranks", "Player"): the unit's badge changes, "Veteran!" floats over it and the level-up box
// (the one levelling uses) lists the stat gains and the skills the rank brings, for a short while: it closes by itself for everybody, since a rank-up
// needs no choice. A unit out of sight just gets its badge.
#include "battle/battle_view.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {
namespace {
constexpr float kBoxSecs = 1.2f;
}

void BattleView::pushUnitRank(const Event& e) {
  // Like a promotion's old type: the badge the unit had stays on the map until its UnitRankUp entry plays.
  if (e.type == EventType::UnitRankUp && e.a >= 0 && (size_t)e.a < units.size() && units[(size_t)e.a].rank == kRankOwn) units[(size_t)e.a].rank = e.c;
}

bool BattleView::beginUnitRank(const Battle& b) {
  const Event& e = cur_.ev;
  if (e.type != EventType::UnitRankUp) return false;
  if (e.a < 0 || (size_t)e.a >= units.size() || (size_t)e.a >= b.units.size()) {
    cur_.dur = 0;
    return true;
  }
  UnitView& v = units[(size_t)e.a];
  v.rank = e.b;
  if (!v.visible) {  // out of sight (fog) or off the board: only the badge
    cur_.dur = 0;
    return true;
  }
  const Unit& u = b.units[(size_t)e.a];
  const UnitRankDef& rank = b.db->rules.unitRanks.levels[(size_t)e.b];
  addFloat(rank.name + "!", v.px + tile_ / 2.0f, v.py - 2, Color(255, 225, 110), 1.4f);
  levelUp.active = true;
  levelUp.timer = 0;
  levelUp.autoContinue = true;
  levelUp.title = rank.name + "!";  // short: at UI scale 150 the box is only a few characters wide
  levelUp.lines.clear();
  levelUp.lines.push_back(u.name);
  for (size_t s = 0; s < e.data.size() && s < b.db->stats.size(); ++s)
    if (e.data[s] != 0) levelUp.lines.push_back((e.data[s] > 0 ? "+" : "") + intStr(e.data[s]) + " " + b.db->stats[s].name);
  const std::vector<int>* before = e.c >= 0 ? &b.db->rules.unitRanks.levels[(size_t)e.c].skills : nullptr;
  for (int s : rank.skills) {
    bool had = false;
    if (before)
      for (int o : *before) had = had || o == s;
    if (!had && !b.db->skills[(size_t)s].hidden) levelUp.lines.push_back("New skill: " + b.db->skills[(size_t)s].name);
  }
  levelUp.ticks = 0;
  levelUp.ticksSent = 0;
  OTE_INFO("rank up box: %s - %s (%s)", u.name.c_str(), rank.name.c_str(), levelUp.lines.size() > 1 ? levelUp.lines[1].c_str() : "no changes");
  cur_.dur = 1e9f;  // closed by progressUnitRank
  return true;
}

bool BattleView::progressUnitRank(const Battle&) {
  if (cur_.ev.type != EventType::UnitRankUp) return false;
  levelUp.timer = cur_.t;
  if (cur_.t >= kBoxSecs) cur_.dur = 0;  // ends this frame
  return true;
}

bool BattleView::finishUnitRank(const Battle&) {
  if (cur_.ev.type != EventType::UnitRankUp) return false;
  levelUp.active = false;
  return true;
}

}  // namespace ote
