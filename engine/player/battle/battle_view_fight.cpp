// The view's side of the battle scene (docs/BATTLE_SCENE.md): picking the run of events of one fight out of the queue, and
// playing it into the view's state once the scene has shown it. The scene itself is fight_scene*.cpp.
#include <algorithm>

#include "battle/battle_view.h"
#include "battle/fight_script.h"

namespace ote {

// cur_.ev has just been taken off the queue. When it begins a fight (a strike, or the skills that fire just before one),
// the whole run goes to `fight` and the view waits; returns true.
bool BattleView::takeFightRun(const Battle& b) {
  const Event head = cur_.ev;
  if (head.type != EventType::Attack && head.type != EventType::SkillActivated) return false;
  int u0 = -1, u1 = -1;
  if (head.type == EventType::Attack) {
    u0 = head.a;
    u1 = head.b;
  } else {  // skills fire before the strike they belong to: look for the strike behind them
    for (const Event& e : queue_) {
      if (e.type == EventType::SkillActivated) continue;
      if (e.type == EventType::Attack) {
        u0 = e.a;
        u1 = e.b;
      }
      break;
    }
    if (u0 < 0 || (head.a != u0 && head.a != u1)) return false;
  }
  auto onMap = [&](int u) { return u >= 0 && (size_t)u < units.size() && units[(size_t)u].visible && (size_t)u < b.units.size(); };
  if (!onMap(u0) || !onMap(u1)) return false;  // a fight out of the viewer's sight, or on units that are not drawn: no scene
  std::vector<Event> run;
  run.push_back(head);
  while (!queue_.empty() && belongsToFight(queue_.front(), u0, u1) && !unseenByViewer(queue_.front())) {
    run.push_back(queue_.front());
    queue_.pop_front();
  }
  fight.levelsGained.clear();
  fight.hpGained.clear();
  for (const Event& e : queue_) {  // what follows the fight, up to the next strike
    if (e.type == EventType::Attack || e.type == EventType::TurnStarted || e.type == EventType::UnitMoved) break;
    if (e.type != EventType::LevelUp || (e.a != u0 && e.a != u1)) continue;
    ++fight.levelsGained[e.a];
    if ((size_t)b.db->rules.statHp < e.data.size()) fight.hpGained[e.a] += e.data[(size_t)b.db->rules.statHp];
  }
  fight.events.swap(run);
  fight.active = true;
  ++fight.serial;
  cur_.active = true;  // held: update() waits while fight.active
  cur_.dur = 1e9f;
  cur_.t = 0;
  hasFocus = false;
  return true;
}

// The scene has played (or been skipped): the run goes into the view's state in one go, as if the map had played it and been
// skipped, without sounds, and without the floating numbers (the scene showed them).
void BattleView::fightFinished(const Battle& b) {
  if (!fight.active) return;
  std::vector<Event> run;
  run.swap(fight.events);
  fight.active = false;
  cur_.active = false;
  const size_t floatsBefore = floats.size();
  const std::string noticeBefore = notice;
  quiet_ = true;
  for (const Event& e : run) {
    cur_.ev = e;
    cur_.active = true;
    begin(b);
    finish(b);  // (also clears cur_.active)
  }
  quiet_ = false;
  if (floats.size() > floatsBefore) floats.resize(floatsBefore);
  notice = noticeBefore;
  cur_.active = false;
  hasFocus = false;
}

}  // namespace ote
