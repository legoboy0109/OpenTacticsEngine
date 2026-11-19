// See ai_field_cache.h for why reuse across AI steps is result-identical.
#include "core/ai/ai_field_cache.h"

#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

namespace {

// Bounds on memory. Hitting either only means a fresh set is started (sets already handed out stay alive through
// their shared_ptr); it never changes a result.
const size_t kMaxSets = 24;
const size_t kMaxFieldsPerSet = 2048;

// Per thread: the cache is a pure content-keyed memo, but threads must not share its containers.
std::vector<std::shared_ptr<GoalFieldSet>>& sets() {
  thread_local std::vector<std::shared_ptr<GoalFieldSet>> s;
  return s;
}

}  // namespace

std::shared_ptr<GoalFieldSet> goalFieldSetFor(const Battle& b, int moveType) {
  const Grid& g = b.grid;
  std::vector<int> costs((size_t)g.cellCount());
  for (Cell c = 0; c < g.cellCount(); ++c) costs[(size_t)c] = b.enterCost(c, moveType);

  std::vector<std::shared_ptr<GoalFieldSet>>& all = sets();
  for (size_t i = 0; i < all.size(); ++i) {
    GoalFieldSet& s = *all[i];
    if (s.moveType != moveType || s.gridType != g.type() || s.layout != g.layout() || s.width != g.width() ||
        s.height != g.height() || s.costs != costs)
      continue;
    if (s.fields.size() >= kMaxFieldsPerSet) {  // full: replace it by an empty twin
      all.erase(all.begin() + (long)i);
      break;
    }
    std::shared_ptr<GoalFieldSet> hit = all[i];
    all.erase(all.begin() + (long)i);  // keep most recently used last
    all.push_back(hit);
    return hit;
  }
  std::shared_ptr<GoalFieldSet> fresh = std::make_shared<GoalFieldSet>();
  fresh->gridType = g.type();
  fresh->layout = g.layout();
  fresh->width = g.width();
  fresh->height = g.height();
  fresh->moveType = moveType;
  fresh->costs.swap(costs);
  if (all.size() >= kMaxSets) all.erase(all.begin());  // least recently used
  all.push_back(fresh);
  return fresh;
}

const std::vector<int>& goalFieldIn(const Battle& b, GoalFieldSet& set, Cell goal) {
  std::vector<int>& f = set.fields[goal];
  if (f.empty()) distanceField(b, set.moveType, std::vector<Cell>(1, goal), f);
  return f;
}

}  // namespace ai
}  // namespace ote
