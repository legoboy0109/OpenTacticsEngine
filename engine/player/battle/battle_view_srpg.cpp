// How the battle view plays the strategy-RPG entries of the event stream (docs/SRPG.md "Events", "Player UI"):
// SkillLearned, SkillActivated, UnitHurt, UnitPromoted, AbilityUsed, UnitRescued, UnitDropped, UnitHanded,
// PairSwitched, SupportPoints, SupportReached.
//
// This file only dispatches: one file per area (battle_view_skills / _promotion / _abilities / _rescue /
// _supports.cpp) holds that area's begin / progress / finish hooks, empty until its step (U1..U5) fills them. An
// entry no area took is played as a no-op with one [info] log line, so a game that already emits these events plays
// through (nothing to wait for) and the windowless tests can see them.
//
// The one thing done here for everybody is the picture of who is on the board: a unit that is carried is not drawn
// and one that is dropped is. (The view re-syncs from the rules when the queue drains, which does the same; this
// keeps the picture right for the events in between, e.g. a second event that mentions the carrier.)
#include "battle/battle_view.h"
#include "core/base/log.h"

namespace ote {
namespace {

bool isSrpgEvent(EventType t) {
  switch (t) {
    case EventType::SkillLearned: case EventType::SkillActivated: case EventType::UnitHurt:
    case EventType::UnitPromoted: case EventType::AbilityUsed:
    case EventType::UnitRescued: case EventType::UnitDropped: case EventType::UnitHanded: case EventType::PairSwitched:
    case EventType::SupportPoints: case EventType::SupportReached: return true;
    default: return false;
  }
}

const char* srpgEventName(EventType t) {
  switch (t) {
    case EventType::SkillLearned: return "SkillLearned";
    case EventType::SkillActivated: return "SkillActivated";
    case EventType::UnitHurt: return "UnitHurt";
    case EventType::UnitPromoted: return "UnitPromoted";
    case EventType::AbilityUsed: return "AbilityUsed";
    case EventType::UnitRescued: return "UnitRescued";
    case EventType::UnitDropped: return "UnitDropped";
    case EventType::UnitHanded: return "UnitHanded";
    case EventType::PairSwitched: return "PairSwitched";
    case EventType::SupportPoints: return "SupportPoints";
    case EventType::SupportReached: return "SupportReached";
    default: return "?";
  }
}

}  // namespace

bool BattleView::beginSrpg(const Battle& b) {
  const Event& e = cur_.ev;
  if (!isSrpgEvent(e.type)) return false;
  if (beginSkills(b) || beginPromotion(b) || beginAbilities(b) || beginRescue(b) || beginSupports(b)) return true;
  OTE_INFO("srpg event (not shown yet): %s a=%d b=%d c=%d d=%d flags=%d", srpgEventName(e.type), e.a, e.b, e.c, e.d, e.flags);
  cur_.dur = 0;
  return true;
}

bool BattleView::progressSrpg(const Battle& b) {
  if (!isSrpgEvent(cur_.ev.type)) return false;
  (void)(progressSkills(b) || progressPromotion(b) || progressAbilities(b) || progressRescue(b) || progressSupports(b));
  return true;
}

bool BattleView::finishSrpg(const Battle& b) {
  const Event& e = cur_.ev;
  if (!isSrpgEvent(e.type)) return false;
  if (finishSkills(b) || finishPromotion(b) || finishAbilities(b) || finishRescue(b) || finishSupports(b)) return true;
  auto valid = [&](int id) { return id >= 0 && (size_t)id < units.size() && (size_t)id < b.units.size(); };
  switch (e.type) {
    case EventType::UnitRescued:  // a stays, b leaves the board
      if (valid(e.b)) units[(size_t)e.b].visible = false;
      break;
    case EventType::UnitDropped:  // b stands on square c again
      if (valid(e.b) && b.grid.valid(e.c)) {
        UnitView& v = units[(size_t)e.b];
        v.visible = b.units[(size_t)e.b].alive;
        v.hp = b.units[(size_t)e.b].hp;
        cellPos(b, e.c, v.px, v.py);
      }
      break;
    case EventType::PairSwitched:  // a went behind, b is in front on square c
      if (valid(e.a)) units[(size_t)e.a].visible = false;
      if (valid(e.b) && b.grid.valid(e.c)) {
        UnitView& v = units[(size_t)e.b];
        v.visible = b.units[(size_t)e.b].alive;
        v.hp = b.units[(size_t)e.b].hp;
        cellPos(b, e.c, v.px, v.py);
      }
      break;
    default: break;
  }
  return true;
}

}  // namespace ote
