// docs/AI.md "Unit ranks": the computer opponent counts a unit that has earned a rank as worth a little more (10% per rank held, at most 50%), and a game
// between two computer sides with a rank table finishes the same way every time. Fixture tests/fixtures/unit-ranks.
#include "core/ai/ai.h"
#include "core/ai/ai_internal.h"
#include "srpg_skills_fixture.h"

using namespace ote;

TEST(a_unit_with_a_rank_is_worth_more_to_the_computer_up_to_a_limit) {
  skilltest::Copy c("urai_value", "unit-ranks");
  skilltest::World w("duel", c.dir, false);
  Battle& b = w.b();
  CHECK_EQ((int)ai::unitValue(b, w.unit("sol")), 1000);  // no rank yet: the type's own value
  CHECK_EQ((int)ai::unitValue(b, w.unit("vet")), 1100);   // Veteran
  CHECK_EQ((int)ai::unitValue(b, w.unit("elite")), 1200); // Elite
  w.unit("elite").xp = 60;
  CHECK_EQ((int)ai::unitValue(b, w.unit("elite")), 1300); // Hero
  // A game without the table values every unit at its type's value.
  skilltest::Copy p("urai_plain", "unit-ranks");
  p.swap("game.json", "\"levels\": [", "\"xpPerLevel\": 15, \"ignored\": [");
  skilltest::World v("field", p.dir, false);
  CHECK_EQ((int)ai::unitValue(v.b(), v.unit("sol")), 1000);
}

namespace {

std::pair<std::string, int> playOut(const char* name) {
  skilltest::Copy c(name, "unit-ranks");
  c.swap("maps/duel.json", "\"id\": \"blue\", \"name\": \"Blue\", \"controller\": \"human\"", "\"id\": \"blue\", \"name\": \"Blue\", \"controller\": \"ai\"");
  skilltest::World w("duel", c.dir);
  std::vector<Event> events;
  int rankUps = 0, guard = 0;
  while (!w.b().over && w.b().round <= 30 && guard++ < 3000) {
    events.clear();
    aiTakeStep(w.b(), events);
    for (const Event& e : events) rankUps += e.type == EventType::UnitRankUp;
  }
  return {savetest::stateText(w.b()), rankUps};
}

}  // namespace

TEST(a_whole_computer_game_with_ranks_is_the_same_every_time_and_ranks_are_earned) {
  const auto a = playOut("urai_game_a"), b = playOut("urai_game_b");
  CHECK_STR(a.first, b.first);
  CHECK_EQ(a.second, b.second);
  CHECK(a.second >= 1);  // somebody fought, and the 10-experience hits crossed the first threshold
}
