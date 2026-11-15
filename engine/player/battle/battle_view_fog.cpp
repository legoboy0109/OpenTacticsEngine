// How the battle view shows fog of war (docs/ECONOMY.md "Fog of war > Player").
//
// The Battle always holds the truth and is already final when the events are played. A view with a viewer team
// plays only the events whose `seen` bit for that team is set; the others change the view's own copy of the state
// at once, silently (applyUnseen: no camera, no sound, no banner, no time). That is also what keeps the AI's turns
// from leaking: a long turn of an unseen side takes no longer on screen than a short one.
//
// Three more things live here: a walk that is only partly seen (it appears at the first seen square and vanishes
// when it ends unseen), the ambush (MoveBlocked), and "the squares the viewer sees" as drawn: while events play
// they follow the viewer's own units as they walk (cheap: a flood of each unit's vision when one of them has
// changed square), and when the view is idle they are simply the Battle's truth.
#include <algorithm>
#include <cmath>

#include "battle/battle_view.h"
#include "battle/fog_text.h"
#include "core/base/log.h"

namespace ote {

void BattleView::setViewer(const Battle& b, int side, int team) {
  viewerSide_ = side;
  viewerTeam_ = team;
  std::vector<FloatText> keep;
  keep.swap(floats);
  resync(b);
  floats.swap(keep);
}

bool BattleView::unseenByViewer(const Event& e) const {
  return viewerTeam_ >= 0 && viewerTeam_ < 32 && !((e.seen >> viewerTeam_) & 1u);
}

void BattleView::flushUnseen(const Battle& b) {
  if (viewerTeam_ < 0 || cur_.active) return;
  ensureUnits(b);
  startNext(b);
}

// An event the viewer did not see: only the things the view remembers about units change, and only so that
// the next time the unit is seen it stands where it really is with the hit points it really has.
void BattleView::applyUnseen(const Battle& b, const Event& e) {
  ensureUnits(b);
  auto unit = [&](int id) -> UnitView* { return id >= 0 && (size_t)id < units.size() ? &units[(size_t)id] : nullptr; };
  UnitView* v = unit(e.a);
  switch (e.type) {
    case EventType::UnitMoved:
      if (v && !e.data.empty()) {
        cellPos(b, e.data.back(), v->px, v->py);
        v->visible = false;
      }
      break;
    case EventType::UnitBuilt:
    case EventType::UnitAdded:
      if (v) {
        *v = UnitView();
        v->hp = b.units[(size_t)e.a].hp;
        cellPos(b, e.b, v->px, v->py);
        v->visible = false;
      }
      break;
    case EventType::UnitRelocated:
      if (v) {
        cellPos(b, e.c, v->px, v->py);
        v->visible = false;
      }
      break;
    case EventType::UnitDied:
    case EventType::UnitRemoved:
      if (v) {
        v->visible = false;
        v->hp = 0;
        v->alpha = 1;
      }
      break;
    case EventType::Attack:
      if (UnitView* d = unit(e.b)) d->hp = std::max(0, d->hp - e.d);
      break;
    case EventType::UnitHealed:
      if (v) v->hp = std::min(v->hp + e.b, b.maxHp(b.units[(size_t)e.a]));
      break;
    case EventType::UnitRepaired:
      if (v) v->hp = e.c;
      break;
    case EventType::SideChanged:
      if (v) v->side = e.c;
      break;
    default: break;  // everything else is bookkeeping of a side the viewer is not (research, resources, xp ...)
  }
}

bool BattleView::beginFog(const Battle& b) {
  if (viewerTeam_ < 0) return false;
  const Event& e = cur_.ev;
  const float cx = tile_ / 2.0f;
  switch (e.type) {
    case EventType::UnitMoved: {
      // The part of the walk the viewer saw: from the first seen square to the last (its own units: all of it).
      const int n = (int)e.data.size();
      const size_t k = (size_t)viewerTeam_ * 2;
      if (n == 0 || e.seenSteps.size() < k + 2 || e.seenSteps[k] < 0) return false;
      const int first = std::min(e.seenSteps[k], n - 1), last = std::max(first, std::min(e.seenSteps[k + 1], n - 1));
      cur_.path.assign(e.data.begin() + first, e.data.begin() + last + 1);
      cur_.hideAtEnd = last < n - 1;  // it goes on out of sight after the last square seen
      if (first > 0 || last < n - 1)
        OTE_INFO("fog: %s seen on squares %d..%d of its %d-square walk", b.units[(size_t)e.a].name.c_str(), first, last, n - 1);
      UnitView& v = units[(size_t)e.a];
      v.visible = true;
      cellPos(b, cur_.path.front(), v.px, v.py);
      cur_.dur = std::max(0.02f, 0.035f * (float)(cur_.path.size() - 1));
      return true;
    }
    case EventType::Attack:
      // A fight the viewer saw is shown in full, also a hidden attacker (it is hidden again when the run is over).
      units[(size_t)e.a].visible = true;
      units[(size_t)e.b].visible = true;
      return false;
    case EventType::MoveBlocked: {
      UnitView& mover = units[(size_t)e.a];
      UnitView& enemy = units[(size_t)e.b];
      mover.visible = enemy.visible = true;
      cellPos(b, e.c, mover.px, mover.py);
      cellPos(b, e.d, enemy.px, enemy.py);
      addFloat(FogWords::kFloatAmbush, mover.px + cx, mover.py - 2, Color(255, 120, 100), 1.3f);
      if (b.teamOf(b.units[(size_t)e.a]) == viewerTeam_) notice = b.db->rules.ambush == AmbushRule::CanAct ? FogWords::kNoticeCanAct : FogWords::kNoticeStopped;
      OTE_INFO("ambush: %s stopped at %d,%d by %s at %d,%d", b.units[(size_t)e.a].name.c_str(), b.grid.x(e.c), b.grid.y(e.c),
               b.units[(size_t)e.b].name.c_str(), b.grid.x(e.d), b.grid.y(e.d));
      cur_.dur = 0.7f;
      return true;
    }
    default: return false;
  }
}

void BattleView::updateShown(const Battle& b) {
  if (viewerTeam_ < 0) return;
  const size_t n = (size_t)b.grid.cellCount();
  if (!busy()) {
    // Nothing playing: the truth, for the squares and for what stands on them.
    shown_.assign(n, 0);
    for (size_t c = 0; c < n; ++c) shown_[c] = b.visibleTo(viewerTeam_, (Cell)c) ? 1 : 0;
    for (const Unit& u : b.units)
      if ((size_t)u.id < units.size()) units[(size_t)u.id].visible = u.alive && b.unitVisibleTo(viewerTeam_, u);
    shownKey_.clear();
    return;
  }
  // Events playing: the sight of the viewer's own units where they are drawn now (a unit still on its way has not
  // seen what lies ahead yet), plus its structures. Recomputed only when one of the units has changed square.
  std::vector<int> key;
  std::vector<const Unit*> mine;
  for (const Unit& u : b.units) {
    if ((size_t)u.id >= units.size() || b.teamOf(u) != viewerTeam_ || !units[(size_t)u.id].visible) continue;
    const UnitView& v = units[(size_t)u.id];
    Cell c = b.grid.at(std::max(0, std::min((int)std::lround(v.px / tile_), b.grid.width() - 1)),
                       std::max(0, std::min((int)std::lround(v.py / tile_), b.grid.height() - 1)));
    key.push_back(u.id);
    key.push_back((int)c);
    mine.push_back(&u);
  }
  if (shown_.size() == n && key == shownKey_) return;
  shownKey_ = key;
  shown_.assign(n, 0);
  std::vector<Cell> area;
  for (size_t i = 0; i < mine.size(); ++i) {
    const Cell c = (Cell)key[i * 2 + 1];
    int radius = b.typeOf(*mine[i]).vision + b.terrainAt(c).visionBonus;
    if (const StructureDef* sd = b.structureDefAt(c)) radius += sd->visionBonus;
    b.grid.within(c, radius, area);
    for (Cell a : area) shown_[(size_t)a] = 1;
  }
  for (size_t s = 0; s < b.structures.size(); ++s) {
    const int owner = b.structures[s].owner;
    if (owner < 0 || b.sides[(size_t)owner].team != viewerTeam_) continue;
    b.structureCells((int)s, area);
    std::vector<Cell> around;
    for (Cell f : area) {
      b.grid.within(f, 1, around);
      for (Cell a : around) shown_[(size_t)a] = 1;
    }
  }
}

}  // namespace ote
