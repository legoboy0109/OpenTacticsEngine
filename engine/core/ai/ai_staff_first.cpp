// "staffFirst" (docs/AI.md section 10): before a unit's usual choice, if it carries a staff it can use now on a legal target from a square it can reach,
// the battle rolls 0..99 and the unit uses the staff (and is done) when the roll is at most the key's number.
//
// bestOrder only FINDS the use (staffFirstOrder, no randomness: the AI's choices are queries, many of them thrown away) and hangs it on the order it
// chose; playOrder (ai.cpp) rolls, once, for the unit that really acts. So the random sequence depends only on which unit acts, never on how often the
// AI looked, and a game without the key draws nothing.
#include <algorithm>
#include <cmath>

#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

bool staffFirstOrder(const Battle& b, const Unit& u, const Reach& reach, const std::vector<Cell>& stops, const StaffPolicy& pol, Order& out) {
  if (b.db->staves.empty() || u.items.empty()) return false;
  struct Pick {
    bool found = false;
    double value = 0;
    int travel = 0, target = -1;
    Cell stop = kNoCell;
    bool needsPick = true;
  };
  std::vector<Pick> picks(u.items.size());  // per carried slot: its best legal use over the squares and targets
  std::vector<StaffOption> options;
  std::vector<int> targets;
  for (Cell s : stops) {  // (in the move search's order: an equal use keeps the earlier square)
    b.staffOptions(u.id, s, options);
    const int travel = s == u.cell ? 0 : reach.cost[s];
    for (const StaffOption& o : options) {
      if (!o.ok) continue;
      b.staffTargets(u.id, o.slot, s, targets);
      for (int t : targets) {
        StaffForecast f;
        if (!b.staffForecast(u.id, o.slot, t, s, f) || !staffUseAllowed(b, u, o.item, f, pol)) continue;
        const double value = staffUseValue(b, u, f);
        Pick& p = picks[(size_t)o.slot];
        // The staff's own pick first (the unit placed last when asked; else the most valuable), then the shortest walk.
        const bool better = !p.found || (pol.lastPlaced ? travel < p.travel : value > p.value + 1e-9 || (std::fabs(value - p.value) <= 1e-9 && travel < p.travel));
        if (!better) continue;
        p.found = true;
        p.value = value;
        p.travel = travel;
        p.stop = s;
        p.target = t;
        p.needsPick = f.needsPick;
      }
    }
  }
  for (size_t slot = 0; slot < picks.size(); ++slot) {  // carried order: the first staff with a use
    const Pick& p = picks[slot];
    if (!p.found) continue;
    out = Order();
    out.score = 0;
    out.kind = ActionType::UseStaff;
    out.moveTo = p.stop == u.cell ? kNoCell : p.stop;
    out.slot = (int)slot;
    out.target = p.needsPick ? p.target : -1;
    return true;
  }
  return false;
}

}  // namespace ai
}  // namespace ote
