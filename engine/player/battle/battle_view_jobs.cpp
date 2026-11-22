// View hooks for what a unit learns and pays with (docs/JOBS.md "Player UI", docs/SRPG.md "Unit resources"). Each floats a line over the unit; nothing
// waits: the entry is over the moment the text is on its way (a unit out of sight gets nothing).
//   JpGained      "+10 JP", and "Job Lv 2!" under it when the job level rose
//   PointsGained  "+10 AP" (the game's name for ability points), and "Mastered: First Aid" under it when the entry became mastered
//   PoolChanged   "-6 MP" when paid, "+2 MP" when regenerated
#include "battle/battle_view.h"
#include "core/base/log.h"
#include "core/jobs/mastery.h"
#include "ui/text.h"

namespace ote {

bool BattleView::beginJobs(const Battle& b) {
  const Event& e = cur_.ev;
  if (e.type != EventType::JpGained && e.type != EventType::PointsGained && e.type != EventType::PoolChanged) return false;
  cur_.dur = 0;
  if (e.a < 0 || (size_t)e.a >= units.size() || (size_t)e.a >= b.units.size() || !units[(size_t)e.a].visible) return true;
  const UnitView& v = units[(size_t)e.a];
  const float x = v.px + tile_ / 2.0f;
  if (e.type == EventType::PoolChanged) {
    const std::string& name = b.db->unitResources[(size_t)e.b].name;
    addFloat((e.c >= 0 ? "+" : "") + intStr(e.c) + " " + name, x, v.py - 2, e.c >= 0 ? Color(140, 200, 255) : Color(190, 160, 255), 1.0f);
    return true;
  }
  if (e.type == EventType::PointsGained) {
    addFloat("+" + intStr(e.b) + " " + b.db->rules.abilityPoints.name, x, v.py - 2, Color(170, 235, 190), 1.1f);
    if (e.flags & kMastered) {
      const std::string name = teachEntryName(*b.db, e.c);
      addFloat("Mastered: " + name, x, v.py + tile_ / 3.0f, Color(255, 225, 110), 1.6f);
      OTE_INFO("mastered: %s masters %s", b.units[(size_t)e.a].name.c_str(), name.c_str());
    }
    return true;
  }
  addFloat("+" + intStr(e.b) + " JP", x, v.py - 2, Color(150, 225, 255), 1.1f);
  if (e.flags & kJobLevelUp) {
    addFloat("Job Lv " + intStr(e.d) + "!", x, v.py + tile_ / 3.0f, Color(255, 225, 110), 1.6f);
    OTE_INFO("job level up: %s is Lv %d in %s", b.units[(size_t)e.a].name.c_str(), e.d, b.db->jobs[(size_t)e.c].name.c_str());
  }
  return true;
}

}  // namespace ote
