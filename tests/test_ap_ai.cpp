// Docs/AI.md "Action points": the computer opponent spends the shared pool where it buys most, never loops, ends its turn when nothing
// worthwhile is affordable, and a game between two computer sides finishes the same way every time. Fixture tests/fixtures/action-points.
#include "ap_fixture.h"
#include "core/ai/ai.h"

using namespace ote;
using aptest::Scene;
using aptest::plain;
using aptest::U;

namespace {

// The active side's AI steps until its turn is over; returns the number of steps (a runaway turn stops at `limit`).
int aiTurn(Battle& b, std::vector<Event>& events, int limit = 120) {
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

}  // namespace

TEST(the_computer_walks_up_and_attacks_with_what_the_pool_pays_for) {
  Scene s("apai_basic", plain(12, 5), {{"lancer", "blue", 1, 2, "lan"}, {"dummy", "red", 5, 2, "foe"}}, true);
  s.w->events.clear();
  const int steps = aiTurn(s.b(), s.w->events);
  CHECK(steps < 20);
  CHECK(s.b().activeSide == 1);              // its turn is over
  CHECK_EQ(attacksBy(s.w->events, s.id("lan")), 1);
  CHECK(s.unit("foe").hp < 40);
  CHECK_EQ(s.ap(0), 20 - 3 - 2);             // three squares to be next to it, then the attack
  CHECK_EQ(s.unit("lan").apAttacks, 1);
}

TEST(the_computer_picks_the_attack_that_buys_most_for_its_ap) {
  // Two foes in reach, one pool of 2 AP: the poker's spear costs 1 AP and the archer's bow 2, and the spear does 5 damage against the bow's 6,
  // so the spear buys more per AP. (Both shoot from two squares away: nothing answers, so the scores compare damage alone.)
  Scene s("apai_value", plain(12, 5), {{"archer", "blue", 1, 2, "arc"}, {"poker", "blue", 1, 3, "pok"}, {"dummy", "red", 3, 2, "a"}, {"dummy", "red", 3, 3, "b"}},
          true, [](skilltest::Copy& c) { c.swap("game.json", "\"perTurn\": 20", "\"perTurn\": 2"); });
  s.w->events.clear();
  aiTurn(s.b(), s.w->events);
  CHECK_EQ(attacksBy(s.w->events, s.id("pok")), 1);
  CHECK_EQ(attacksBy(s.w->events, s.id("arc")), 0);  // nothing left for the archer (1 AP < 2)
  CHECK(s.ap(0) <= 1 && s.ap(0) >= 0);
}

TEST(the_computer_ends_its_turn_when_nothing_worthwhile_is_affordable) {
  Scene s("apai_poor", plain(12, 5), {{"lancer", "blue", 1, 2, "lan"}, {"dummy", "red", 10, 2, "foe"}}, true,
          [](skilltest::Copy& c) { c.swap("game.json", "\"perTurn\": 20", "\"perTurn\": 2"); });
  s.w->events.clear();
  const int steps = aiTurn(s.b(), s.w->events);
  CHECK(steps < 6);
  CHECK(s.b().activeSide == 1);
  CHECK_EQ(s.ap(0), 0);                       // it walked two squares for its two AP and stopped
  CHECK_EQ(s.b().grid.distance(s.unit("lan").cell, s.at(1, 2)), 2);
  // With nothing at all to pay a step it still ends the turn at once.
  Scene t("apai_zero", plain(12, 5), {{"lancer", "blue", 1, 2, "lan"}, {"dummy", "red", 10, 2, "foe"}}, true,
          [](skilltest::Copy& c) { c.swap("game.json", "\"perTurn\": 20", "\"perTurn\": 0"); });
  t.w->events.clear();
  CHECK(aiTurn(t.b(), t.w->events) <= 2);
  CHECK(t.b().activeSide == 1);
}

TEST(the_computer_builds_with_a_cost_that_lists_ap_and_keeps_to_its_pool) {
  Scene s("apai_build", aptest::factoryMap(), [](skilltest::Copy& c) { c.swap("maps/field.json", "\"controller\": \"human\"", "\"controller\": \"ai\""); });
  s.w->events.clear();
  aiTurn(s.b(), s.w->events);
  int built = 0;
  for (const Event& e : s.w->events) built += e.type == EventType::UnitBuilt;
  CHECK(built >= 1);
  CHECK(s.ap(0) >= 0 && s.gold(0) >= 0);
}

TEST(a_game_between_two_computer_sides_finishes_the_same_way_every_time) {
  auto playOut = [](const char* name, std::string* state, size_t* events, int* rounds, bool* over) {
    skilltest::Copy c(name, "action-points");
    std::string aiMap;
    readTextFile(ote_test::sourceDir() + "/tests/fixtures/action-points/maps/ai.json", aiMap);
    // No factories: with them both sides keep building and the standoff has no end (the fixture's sim says "stalled"); without, one side wins.
    const size_t from = aiMap.find("\"structures\"");
    aiMap.replace(from, aiMap.find("]", from) - from + 1, "\"structures\": []");
    c.put("maps/ai.json", aiMap);
    skilltest::World w("ai", c.dir);
    int worst = 0;
    for (int guard = 0; guard < 60 && !w.b().over; ++guard) {
      const int steps = aiTurn(w.b(), w.events, 200);
      worst = std::max(worst, steps);
    }
    *state = savetest::stateText(w.b());
    *events = w.events.size();
    *rounds = w.b().round;
    *over = w.b().over;
    return worst;
  };
  std::string a, b;
  size_t ea = 0, eb = 0;
  int ra = 0, rb = 0;
  bool overA = false, overB = false;
  const int worstA = playOut("apai_full1", &a, &ea, &ra, &overA);
  const int worstB = playOut("apai_full2", &b, &eb, &rb, &overB);
  CHECK(worstA < 200 && worstB < 200);  // no turn ran away
  CHECK_STR(a, b);
  CHECK_EQ(ea, eb);
  CHECK_EQ(ra, rb);
  CHECK(overA && overB);  // a side was beaten before the guard of 60 turns
}

TEST(no_turn_of_the_computer_runs_away_whatever_the_pool_and_the_prices) {
  // Pools from nothing to plenty, free and dear walking, many attacks per turn, no splitting: every turn of both sides ends in a bounded number of steps.
  struct Variant { const char* from; const char* to; };
  const std::vector<std::vector<Variant>> variants = {
      {{"\"perTurn\": 20", "\"perTurn\": 0"}},
      {{"\"perTurn\": 20", "\"perTurn\": 1"}},
      {{"\"perTurn\": 20", "\"perTurn\": 7"}},
      {{"\"perTurn\": 20", "\"perTurn\": 90"}, {"\"costPerStep\": 1", "\"costPerStep\": 0"}},
      {{"\"attacksPerTurn\": 1", "\"attacksPerTurn\": 5"}, {"\"cost\": 2", "\"cost\": 0"}},
      {{"\"splitMoves\": true", "\"splitMoves\": false"}},
      {{"\"softCap\": 4", "\"softCap\": 0"}, {"\"softCapPenaltyPercent\": 100", "\"softCapPenaltyPercent\": 400"}},
  };
  int n = 0;
  for (const std::vector<Variant>& v : variants) {
    skilltest::Copy c("apai_runaway" + std::to_string(n++), "action-points");
    for (const Variant& x : v) c.swap("game.json", x.from, x.to);
    std::string aiMap;
    readTextFile(ote_test::sourceDir() + "/tests/fixtures/action-points/maps/ai.json", aiMap);
    c.put("maps/ai.json", aiMap);
    skilltest::World w("ai", c.dir);
    int worst = 0;
    for (int turn = 0; turn < 24 && !w.b().over; ++turn) worst = std::max(worst, aiTurn(w.b(), w.events, 300));
    CHECK(worst < 300);
  }
}
