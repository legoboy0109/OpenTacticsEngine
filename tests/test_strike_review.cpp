// Regression tests for review findings on strike abilities and learning from equipment (docs/SRPG.md "Abilities that strike", docs/JOBS.md "Learning
// from equipment"). Each failed before its fix. Fixture: teaching_fixture.h.
#include <algorithm>

#include "core/ai/ai.h"
#include "core/jobs/mastery.h"
#include "teaching_fixture.h"

using namespace ote;
using skilltest::saysIn;
using teachtest::Duel;

TEST(an_area_strike_ability_follows_its_own_after_move_not_the_weapons) {
  Duel d("strikerev_aftermove", false);
  d.c.swap("data/weapons.json", R"("area": { "shape": "burst" },)", R"("area": { "shape": "burst" }, "afterMove": false,)");
  skilltest::World w("duel", d.c.dir);
  CHECK(w.apply(Action::move(w.id("aldric"), w.at(2, 3))));
  const int flame = w.db().findAbility("flame");
  std::vector<int> units;
  std::vector<Cell> cells;
  w.b().abilityTargets(w.id("aldric"), flame, w.unit("aldric").cell, units, cells);
  CHECK(std::find(cells.begin(), cells.end(), w.unit("foe").cell) != cells.end());
  CHECK(w.apply(Action::useAbility(w.id("aldric"), flame, -1, w.unit("foe").cell)));
}
