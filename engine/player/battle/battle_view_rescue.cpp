// How the battle view plays UnitRescued, UnitDropped, UnitHanded and PairSwitched (docs/SRPG.md "Player UI > Rescue /
// pair up", step U4). Same contract as the story hooks: each function returns true for these four entries.
//
//   UnitRescued   the unit that leaves slides from its square onto the one that stays and fades out; hidden at the end.
//   UnitDropped   the unit slides from its carrier's spot onto the square (a fall: the carrier has just died, the unit
//                 comes out of where it lay). Blocked (fog): it slides out half way and back, "Blocked!" shows over the
//                 carrier and the unit in the way is revealed, like an ambush; it stays carried.
//   UnitHanded    the carried unit, drawn for the moment, slides from the one that gave to the one that took.
//   PairSwitched  the two sprites cross-fade on the shared square.
//
// The Battle is already final when this plays, so every position is read off the view (a unit's `px` / `py` stay where
// it last was, also when it is hidden) and the end state is set from the rules in finishRescue.
#include <algorithm>
#include <cmath>

#include "battle/battle_view.h"

namespace ote {
namespace {

constexpr float kSlideDur = 0.28f;
constexpr float kSwitchDur = 0.3f;
constexpr float kBlockedDur = 0.5f;

bool isRescueEvent(EventType t) {
  return t == EventType::UnitRescued || t == EventType::UnitDropped || t == EventType::UnitHanded || t == EventType::PairSwitched;
}

}  // namespace

bool BattleView::beginRescue(const Battle& b) {
  const Event& e = cur_.ev;
  if (!isRescueEvent(e.type)) return false;
  auto ok = [&](int id) { return id >= 0 && (size_t)id < units.size() && (size_t)id < b.units.size(); };
  cur_.dur = 0;
  switch (e.type) {
    case EventType::UnitRescued:  // a stays, b leaves from square c
      if (!ok(e.a) || !ok(e.b) || !b.grid.valid(e.c)) return true;
      units[(size_t)e.b].visible = true;
      units[(size_t)e.b].alpha = 1;
      cellPos(b, e.c, units[(size_t)e.b].px, units[(size_t)e.b].py);
      cur_.dur = kSlideDur;
      break;
    case EventType::UnitDropped:  // carrier a puts b down on square c
      if (!ok(e.a) || !ok(e.b) || !b.grid.valid(e.c)) return true;
      units[(size_t)e.b].visible = true;
      units[(size_t)e.b].alpha = 1;
      units[(size_t)e.b].px = units[(size_t)e.a].px;
      units[(size_t)e.b].py = units[(size_t)e.a].py;
      if (e.flags & kBlocked) {
        UnitView& carrier = units[(size_t)e.a];
        addFloat("Blocked!", carrier.px + tile_ / 2.0f, carrier.py - 2, Color(255, 120, 100), 1.0f);
        if (ok(e.d) && b.units[(size_t)e.d].alive) {  // whoever stands there is shown, until the run of events is over
          UnitView& blocker = units[(size_t)e.d];
          blocker.visible = true;
          cellPos(b, b.units[(size_t)e.d].cell, blocker.px, blocker.py);
        }
        cur_.dur = kBlockedDur;
      } else {
        cur_.dur = kSlideDur;
      }
      break;
    case EventType::UnitHanded:  // a gives the unit c to b
      if (!ok(e.a) || !ok(e.b) || !ok(e.c)) return true;
      units[(size_t)e.c].visible = true;
      units[(size_t)e.c].alpha = 1;
      units[(size_t)e.c].px = units[(size_t)e.a].px;
      units[(size_t)e.c].py = units[(size_t)e.a].py;
      cur_.dur = kSlideDur;
      break;
    case EventType::PairSwitched:  // a goes behind, b comes to the front on square c
      if (!ok(e.a) || !ok(e.b) || !b.grid.valid(e.c)) return true;
      cellPos(b, e.c, units[(size_t)e.a].px, units[(size_t)e.a].py);
      units[(size_t)e.a].visible = true;
      units[(size_t)e.a].alpha = 1;
      cellPos(b, e.c, units[(size_t)e.b].px, units[(size_t)e.b].py);
      units[(size_t)e.b].hp = b.units[(size_t)e.b].hp;
      units[(size_t)e.b].visible = true;
      units[(size_t)e.b].alpha = 0;
      cur_.dur = kSwitchDur;
      break;
    default: break;
  }
  return true;
}

bool BattleView::progressRescue(const Battle& b) {
  const Event& e = cur_.ev;
  if (!isRescueEvent(e.type)) return false;
  auto ok = [&](int id) { return id >= 0 && (size_t)id < units.size() && (size_t)id < b.units.size(); };
  const float p = cur_.dur > 0 ? std::min(1.0f, cur_.t / cur_.dur) : 1.0f;
  auto follow = [&](float x, float y) {
    hasFocus = true;
    focusX = x + tile_ / 2.0f;
    focusY = y + tile_ / 2.0f;
  };
  switch (e.type) {
    case EventType::UnitRescued: {
      if (!ok(e.a) || !ok(e.b) || !b.grid.valid(e.c)) break;
      UnitView& v = units[(size_t)e.b];
      const UnitView& stays = units[(size_t)e.a];
      float x0, y0;
      cellPos(b, e.c, x0, y0);
      v.px = x0 + (stays.px - x0) * p;
      v.py = y0 + (stays.py - y0) * p;
      v.alpha = p < 0.6f ? 1.0f : 1.0f - (p - 0.6f) / 0.4f;  // gone as it arrives
      follow(stays.px, stays.py);
      break;
    }
    case EventType::UnitDropped: {
      if (!ok(e.a) || !ok(e.b) || !b.grid.valid(e.c)) break;
      UnitView& v = units[(size_t)e.b];
      const UnitView& from = units[(size_t)e.a];
      float x1, y1;
      cellPos(b, e.c, x1, y1);
      // Blocked: half way out, then back (the sine rises and falls once over the event).
      const float t = (e.flags & kBlocked) ? 0.5f * std::sin(p * 3.14159265f) : p;
      v.px = from.px + (x1 - from.px) * t;
      v.py = from.py + (y1 - from.py) * t;
      follow(from.px + (x1 - from.px) * t, from.py + (y1 - from.py) * t);
      break;
    }
    case EventType::UnitHanded: {
      if (!ok(e.a) || !ok(e.b) || !ok(e.c)) break;
      UnitView& v = units[(size_t)e.c];
      const UnitView &from = units[(size_t)e.a], &to = units[(size_t)e.b];
      v.px = from.px + (to.px - from.px) * p;
      v.py = from.py + (to.py - from.py) * p;
      follow(v.px, v.py);
      break;
    }
    case EventType::PairSwitched:
      if (!ok(e.a) || !ok(e.b) || !b.grid.valid(e.c)) break;
      units[(size_t)e.a].alpha = 1 - p;
      units[(size_t)e.b].alpha = p;
      follow(units[(size_t)e.b].px, units[(size_t)e.b].py);
      break;
    default: break;
  }
  return true;
}

bool BattleView::finishRescue(const Battle& b) {
  const Event& e = cur_.ev;
  if (!isRescueEvent(e.type)) return false;
  auto ok = [&](int id) { return id >= 0 && (size_t)id < units.size() && (size_t)id < b.units.size(); };
  // On the board afterwards = what the rules say: a dropped unit is alive and uncarried, a blocked drop keeps it carried.
  auto settle = [&](int id, bool setPos) {
    UnitView& v = units[(size_t)id];
    const Unit& u = b.units[(size_t)id];
    v.visible = u.alive && u.carriedBy < 0;
    v.alpha = 1;
    v.hp = u.hp;
    if (setPos && u.alive && b.grid.valid(e.c)) cellPos(b, e.c, v.px, v.py);
  };
  switch (e.type) {
    case EventType::UnitRescued:
      if (ok(e.b)) {
        units[(size_t)e.b].visible = false;  // (the rules agree: it is off the board and carried)
        units[(size_t)e.b].alpha = 1;
      }
      break;
    case EventType::UnitDropped:
      if (ok(e.b)) settle(e.b, true);
      break;
    case EventType::UnitHanded:
      if (ok(e.c)) {
        units[(size_t)e.c].visible = false;
        units[(size_t)e.c].alpha = 1;
      }
      break;
    case EventType::PairSwitched:
      if (ok(e.a)) {
        units[(size_t)e.a].visible = false;
        units[(size_t)e.a].alpha = 1;
      }
      if (ok(e.b)) settle(e.b, true);
      break;
    default: break;
  }
  return true;
}

}  // namespace ote
