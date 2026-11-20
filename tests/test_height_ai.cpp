// docs/AI.md "Height rules": the computer opponent obeys the jump and the height range through the same functions the player's rules use,
// walks around the cliffs it cannot climb, and a game between two computer sides with height rules finishes the same way every time.
// Fixture tests/fixtures/height-rules (numbers: tests/height_fixture.h).
#include "core/ai/ai.h"
#include "height_fixture.h"

using namespace ote;
using heighttest::Scene;
using heighttest::U;

namespace {

// The active side's AI steps until its turn is over; returns the number of steps (a runaway turn stops at `limit`).
int aiTurn(Battle& b, std::vector<Event>& events, int limit = 150) {
  const int side = b.activeSide, round = b.round;
  int steps = 0;
  while (!b.over && b.activeSide == side && b.round == round && steps < limit) {
    ++steps;
    aiTakeStep(b, events);
  }
  return steps;
}

int attacksBy(const std::vector<Event>& events, int unit) {
  int n = 0;
  for (const Event& e : events) n += e.type == EventType::Attack && e.a == unit && !(e.flags & kCounter);
  return n;
}

// Both sides computer-controlled: plays whole rounds until the battle is over or `rounds` have passed; the whole event stream as the result.
std::vector<Event> playRounds(Battle& b, int rounds) {
  std::vector<Event> events;
  while (!b.over && b.round <= rounds) {
    if (aiTurn(b, events) >= 150) break;  // a turn that never ends (the test then fails on `over`)
  }
  return events;
}

}  // namespace

TEST(the_computer_never_fires_at_a_target_its_weapon_cannot_reach_in_height) {
  // The red slinger at level 1 beside a plateau of level 3 it cannot climb; blue's dummy stands up there. A sling cannot fire upwards.
  auto turnOf = [](bool rules) {
    Scene s(rules ? "hrai_sling" : "hrai_sling_off", {"01300", "01300", "01300"}, {{"dummy", "blue", 2, 1, "x"}, {"slinger", "red", 1, 1, "sl"}}, false,
            [rules](skilltest::Copy& c) {
              if (!rules) heighttest::dropHeights(c);
            });
    s.w->apply(Action::endTurn());  // blue's turn is over
    s.w->events.clear();
    const int steps = aiTurn(s.b(), s.w->events);
    CHECK(steps < 20);
    return attacksBy(s.w->events, s.id("sl"));
  };
  CHECK_EQ(turnOf(true), 0);
  CHECK(turnOf(false) >= 1);  // the same board without the rules: it shoots
}

TEST(the_computer_walks_round_a_cliff_it_cannot_climb_to_a_ramp) {
  // A ridge of level 3 across the map (column 4) with a ramp of single steps in the bottom row; blue's lancer is on the left, red's dummy behind
  // the ridge. Both sides are computer sides. The lancer must find the ramp (the straight way is a cliff) and fight within a few rounds.
  const std::vector<std::string> rows = {"00003000", "00003000", "00003000", "00003000", "00012210"};
  Scene s("hrai_ramp", rows, {{"lancer", "blue", 1, 1, "lan"}, {"dummy", "red", 6, 1, "foe"}}, true);
  std::vector<Event> events;
  int fought = -1;
  for (int round = 1; round <= 8 && !s.b().over && fought < 0; ++round) {
    aiTurn(s.b(), events);  // blue
    aiTurn(s.b(), events);  // red
    if (attacksBy(events, s.id("lan")) > 0) fought = round;
  }
  CHECK(fought > 0);
  // It never stood on a square it could not have reached.
  CHECK(s.b().heightAt(s.unit("lan").cell) <= 2);
}

TEST(the_computer_prefers_the_higher_square_to_shoot_from_when_all_else_is_equal) {
  // Two squares from which the archer hits the dummy at distance 2: one level 0, one level 2 (reachable by single steps). Same defence, same cost.
  Scene s("hrai_high", {"0000000", "0012100", "0000000"}, {{"archer", "blue", 2, 0, "arc"}, {"dummy", "red", 5, 1, "foe"}}, true);
  std::vector<Event> events;
  aiTurn(s.b(), events);
  const Unit& arc = s.unit("arc");
  CHECK_EQ(attacksBy(events, arc.id), 1);
  CHECK_EQ(s.b().grid.distance(arc.cell, s.unit("foe").cell) >= 2 ? 1 : 0, 1);
  CHECK(s.b().heightAt(arc.cell) >= s.b().heightAt(s.unit("foe").cell));  // it did not shoot from a hole
}

TEST(two_computer_sides_with_height_rules_finish_the_same_battle_twice) {
  auto game = [](const char* name) {
    std::vector<U> units = {{"lancer", "blue", 0, 2, "b1"}, {"archer", "blue", 0, 3, "b2"}, {"goat", "blue", 1, 3, "b3"}, {"gunner", "blue", 0, 4, "b4"},
                            {"lancer", "red", 9, 2, "r1"},  {"archer", "red", 9, 3, "r2"},  {"hawk", "red", 8, 3, "r3"}, {"bomber", "red", 9, 4, "r4"}};
    Scene s(name, {"0000000000", "0001221000", "0123332100", "0123332100", "0123332100", "0001221000", "0000000000"}, units, true);
    std::vector<Event> events = playRounds(s.b(), 40);
    CHECK(s.b().over);
    return std::make_pair(savetest::stateText(s.b()), events);
  };
  const auto a = game("hrai_full1"), b = game("hrai_full2");
  CHECK_STR(a.first, b.first);
  CHECK(savetest::sameEvents(a.second, b.second));
  CHECK(a.second.size() > 20);
}

TEST(a_hexagon_game_with_height_rules_plays_through_without_stalling) {
  std::vector<U> units = {{"lancer", "blue", 0, 1, "b1"}, {"archer", "blue", 0, 2, "b2"}, {"lancer", "red", 7, 1, "r1"}, {"archer", "red", 7, 2, "r2"}};
  Scene s("hrai_hex", {"00000000", "00123200", "00123200", "00000000"}, units, true, nullptr, std::vector<std::string>(), "hex");
  std::vector<Event> events = playRounds(s.b(), 40);
  CHECK(s.b().over);
}
