// View hook for JpGained (docs/JOBS.md "Player UI"): "+10 JP" floats over the unit, and "Job Lv 2!" under it when the job level rose. Nothing waits: the
// entry is over the moment the text is on its way (a unit out of sight gets nothing).
#include "battle/battle_view.h"
#include "core/base/log.h"
#include "ui/text.h"

namespace ote {

bool BattleView::beginJobs(const Battle& b) {
  const Event& e = cur_.ev;
  if (e.type != EventType::JpGained) return false;
  cur_.dur = 0;
  if (e.a < 0 || (size_t)e.a >= units.size() || (size_t)e.a >= b.units.size() || !units[(size_t)e.a].visible) return true;
  const UnitView& v = units[(size_t)e.a];
  addFloat("+" + intStr(e.b) + " JP", v.px + tile_ / 2.0f, v.py - 2, Color(150, 225, 255), 1.1f);
  if (e.flags & kJobLevelUp) {
    addFloat("Job Lv " + intStr(e.d) + "!", v.px + tile_ / 2.0f, v.py + tile_ / 3.0f, Color(255, 225, 110), 1.6f);
    OTE_INFO("job level up: %s is Lv %d in %s", b.units[(size_t)e.a].name.c_str(), e.d, b.db->jobs[(size_t)e.c].name.c_str());
  }
  return true;
}

}  // namespace ote
