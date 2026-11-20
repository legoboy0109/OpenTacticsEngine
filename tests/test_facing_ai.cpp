// docs/AI.md "Facing": the computer opponent ends its move looking at the nearest foe, picks the side or back of a foe when the game's numbers make that
// better (it scores through the forecast, which carries the arc from the square it would attack from), and a game between two computer sides with BOTH
// the height rules and facing finishes the same way every time. Fixture tests/fixtures/facing (numbers: tests/facing_fixture.h).
#include "core/ai/ai.h"
#include "facing_fixture.h"

using namespace ote;
using facingtest::Scene;
using facingtest::U;

namespace {

int aiTurn(Battle& b, std::vector<Event>& events, int limit = 150) {
  const int side = b.activeSide, round = b.round;
  int steps = 0;
  while (!b.over && b.activeSide == side && b.round == round && steps < limit) {
    ++steps;
    aiTakeStep(b, events);
  }
  return steps;
}

// The facing game with the height rules added as well (a jump of 1 everywhere).
void withHeights(skilltest::Copy& c) { c.swap("game.json", "\"facing\": { \"arcs\"", "\"height\": { \"jump\": { \"default\": 1 } }, \"facing\": { \"arcs\""); }

}  // namespace

TEST(a_computer_unit_that_ends_its_move_looks_at_the_nearest_foe) {
  Scene s("fcai_end", facingtest::plain(12, 5), {{"lancer", "blue", 0, 2, "lan"}, {"dummy", "red", 9, 2, "foe", "east"}});
  s.w->apply(Action::endTurn());  // blue's turn is over
  s.w->events.clear();
  aiTurn(s.b(), s.w->events);
  const Unit& foe = s.unit("foe");
  CHECK(foe.cell != s.at(9, 2));  // it walked toward the lancer
  CHECK_EQ(foe.facing, directionToward(s.b().grid, foe.cell, s.unit("lan").cell));
  CHECK_EQ(foe.facing, 3);  // west
  CHECK(s.count(EventType::UnitFaced) >= 1);
}

TEST(the_computer_attacks_from_the_back_when_the_formula_pays_for_it) {
  // The blue dummy at (4,2) looks west; the red lancer at (6,3) can reach (5,2) (behind the dummy) or (4,3) (beside it) in two steps: 13 damage
  // against 10, the rest equal: it goes behind.
  Scene s("fcai_back", facingtest::plain(10, 5), {{"dummy", "blue", 4, 2, "dum", "west"}, {"lancer", "red", 6, 3, "lan", "west"}});
  s.w->apply(Action::endTurn());
  s.w->events.clear();
  aiTurn(s.b(), s.w->events);
  CHECK(s.unit("lan").cell == s.at(5, 2));
  int damage = 0;
  for (const Event& e : s.w->events)
    if (e.type == EventType::Attack && e.a == s.id("lan") && !(e.flags & kCounter)) damage = e.d;
  CHECK_EQ(damage, 13);
  // The same board in a game without facing: the formula has no arc and the attack is the plain 8.
  Scene t("fcai_back_off", facingtest::plain(10, 5), {{"dummy", "blue", 4, 2, "dum"}, {"lancer", "red", 6, 3, "lan"}}, false, facingtest::dropFacing);
  t.w->apply(Action::endTurn());
  t.w->events.clear();
  aiTurn(t.b(), t.w->events);
  int plain = 0;
  for (const Event& e : t.w->events)
    if (e.type == EventType::Attack && e.a == t.id("lan") && !(e.flags & kCounter)) plain = e.d;
  CHECK_EQ(plain, 8);
}

TEST(two_computer_sides_with_height_rules_and_facing_finish_the_same_battle_twice) {
  auto game = [](const char* name) {
    std::vector<U> units = {{"lancer", "blue", 0, 2, "b1"}, {"archer", "blue", 0, 3, "b2"}, {"assassin", "blue", 1, 3, "b3"}, {"gunner", "blue", 0, 4, "b4"},
                            {"lancer", "red", 9, 2, "r1"},  {"archer", "red", 9, 3, "r2"},  {"sentry", "red", 8, 3, "r3"},   {"bomber", "red", 9, 4, "r4"}};
    Scene s(name, facingtest::plain(10, 7), units, true, withHeights, "square",
            {"0000000000", "0001221000", "0123332100", "0123332100", "0123332100", "0001221000", "0000000000"});
    std::vector<Event> events;
    while (!s.b().over && s.b().round <= 40) {
      if (aiTurn(s.b(), events) >= 150) break;
    }
    CHECK(s.b().over);
    int faced = 0;
    for (const Event& e : events) faced += e.type == EventType::UnitFaced;
    CHECK(faced > 10);
    return std::make_pair(savetest::stateText(s.b()), events);
  };
  const auto a = game("fcai_full1"), b = game("fcai_full2");
  CHECK_STR(a.first, b.first);
  CHECK(savetest::sameEvents(a.second, b.second));
}

TEST(a_hexagon_game_with_facing_plays_through_without_stalling) {
  std::vector<U> units = {{"lancer", "blue", 0, 1, "b1"}, {"archer", "blue", 0, 2, "b2"}, {"lancer", "red", 7, 1, "r1"}, {"archer", "red", 7, 2, "r2"}};
  Scene s("fcai_hex", facingtest::plain(8, 4), units, true, nullptr, "hex");
  std::vector<Event> events;
  while (!s.b().over && s.b().round <= 40) {
    if (aiTurn(s.b(), events) >= 150) break;
  }
  CHECK(s.b().over);
}
