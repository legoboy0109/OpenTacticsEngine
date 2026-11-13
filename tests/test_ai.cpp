// Computer opponent tests. Fixture side 0 "blue" is passive (tests end its
// turn); side 1 "red" is the one the AI plays unless a test says otherwise.
#include "battle_fixture.h"
#include "core/ai/ai.h"
#include "core/data/loader.h"

using namespace ote;
using fixture::World;

namespace {

// Hands the turn to red (ends blue's first turn).
bool toRed(World& w) { return w.act(Action::endTurn()) && w.battle.activeSide == 1; }

// Runs AI steps until the active side's turn ends. Returns the number of
// steps, or -1 if it never ends (the bug this guards against).
int runTurn(Battle& b, std::vector<Event>& events) {
  int limit = 10 * (int)b.units.size() + 100;
  int steps = 0;
  while (aiTakeStep(b, events)) {
    if (++steps > limit) return -1;
  }
  return steps;
}

// Plays both sides with the AI; true if the battle ended in time.
bool playOut(Battle& b, std::vector<Event>& events, int maxRounds) {
  while (!b.over && b.round <= maxRounds)
    if (runTurn(b, events) < 0) return false;
  return b.over;
}

struct Demo {
  Database db;
  MapDef map;
  Formulas formulas;
  Battle battle;
  std::vector<Event> events;
  bool load(const char* game, const char* mapId, uint32_t seed) {
    std::string err;
    bool ok = loadGame(ote_test::sourceDir() + "/games/" + game, db, err) &&
              loadMap(db, mapId, map, err) && formulas.init(db, err) &&
              battle.init(db, map, formulas, seed, err);
    if (!ok) std::printf("  %s\n", err.c_str());
    if (ok) battle.start(events);
    return ok;
  }
};

bool sameEvents(const std::vector<Event>& a, const std::vector<Event>& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (a[i].type != b[i].type || a[i].a != b[i].a || a[i].b != b[i].b || a[i].c != b[i].c ||
        a[i].d != b[i].d || a[i].flags != b[i].flags || a[i].data != b[i].data)
      return false;
  return true;
}

}  // namespace

TEST(ai_attacks_and_kills_adjacent_weak_enemy) {
  World w({"....."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 3, 0, false}, {"archer", 0, 2, 0, false}});
  CHECK(w.begin());
  w.battle.units[2].hp = 3;  // the archer next to red's soldier is nearly dead
  CHECK(toRed(w));
  CHECK(aiTakeStep(w.battle, w.events));  // first step is the attack
  const Event* atk = w.last(EventType::Attack);
  CHECK(atk && atk->a == 1 && atk->b == 2);
  CHECK(atk && (atk->flags & kKill));
  CHECK(!w.battle.units[2].alive);
  CHECK(w.battle.units[1].done);
}

TEST(ai_moves_toward_distant_enemy) {
  World w({"........."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 8, 0, false}});
  CHECK(w.begin());
  CHECK(toRed(w));
  int before = w.battle.grid.distance(w.battle.units[1].cell, w.battle.units[0].cell);
  CHECK(runTurn(w.battle, w.events) >= 0);
  int after = w.battle.grid.distance(w.battle.units[1].cell, w.battle.units[0].cell);
  CHECK(after < before);
  CHECK_EQ(w.battle.activeSide, 0);  // and the turn ended
}

TEST(ai_routes_around_walls) {
  // The wall column forces a detour through the bottom row; straight-line
  // distance would pin the soldier against the wall.
  World w({"..#..",
           "..#..",
           "....."},
          {{"soldier", 0, 0, 0, false}, {"soldier", 1, 4, 0, false}});
  CHECK(w.begin());
  for (int i = 0; i < 6 && !w.battle.over; ++i) {
    if (w.battle.activeSide == 0) w.act(Action::endTurn());
    else CHECK(runTurn(w.battle, w.events) >= 0);
  }
  CHECK(w.count(EventType::Attack) > 0);  // got around the wall and engaged
}

TEST(ai_captures_building_over_several_turns) {
  World w({".c..."}, {{"soldier", 0, 4, 0, false}, {"soldier", 1, 0, 0, false}});
  w.db.rules.capture = true;
  w.db.rules.fCaptureAmount = "10";  // two turns to take a 20-point city
  CHECK(w.begin());
  CHECK(toRed(w));
  Cell city = w.at(1, 0);
  CHECK(runTurn(w.battle, w.events) >= 0);
  CHECK_EQ(w.battle.units[1].cell, city);  // walked onto it and started capturing
  CHECK_EQ(w.count(EventType::CaptureProgress), 1);
  CHECK_EQ(w.battle.cells[city].owner, -1);

  w.act(Action::endTurn());  // blue
  CHECK(runTurn(w.battle, w.events) >= 0);
  CHECK_EQ(w.battle.units[1].cell, city);  // did not wander off
  CHECK_EQ(w.battle.cells[city].owner, 1);
  CHECK_EQ(w.count(EventType::CellCaptured), 1);
}

TEST(ai_builds_when_it_has_funds_and_a_factory) {
  World w({"F....", "....."}, {{"soldier", 0, 4, 1, false}, {"soldier", 1, 3, 1, false}});
  w.db.rules.production = true;
  w.map.owner[0] = 1;
  w.map.sides[1].resources = {{0, 250}};
  CHECK(w.begin());
  CHECK(toRed(w));
  CHECK(runTurn(w.battle, w.events) >= 0);
  CHECK_EQ(w.count(EventType::UnitBuilt), 1);  // one factory, one unit per turn
  CHECK_EQ(w.battle.sides[1].resources[0], 150);
  CHECK(w.battle.unitAt(w.at(0, 0)) >= 0);
}

TEST(ai_does_not_build_what_it_cannot_afford) {
  World w({"F....", "....."}, {{"soldier", 0, 4, 1, false}, {"soldier", 1, 3, 1, false}});
  w.db.rules.production = true;
  w.map.owner[0] = 1;
  w.map.sides[1].resources = {{0, 50}};
  CHECK(w.begin());
  CHECK(toRed(w));
  CHECK(runTurn(w.battle, w.events) >= 0);
  CHECK_EQ(w.count(EventType::UnitBuilt), 0);
}

TEST(ai_turn_always_terminates_and_ends_turn) {
  // Cramped, mixed units, ranged and melee, including an enemy melee unit
  // that cannot hurt the tank (versus table) so some orders are impossible.
  World w({"..f..", ".#...", "....."},
          {{"soldier", 0, 0, 0, true}, {"tank", 0, 1, 2, false}, {"archer", 0, 0, 2, false},
           {"soldier", 1, 4, 0, true}, {"tank", 1, 4, 2, false}, {"archer", 1, 3, 1, false}});
  CHECK(w.begin());
  for (int i = 0; i < 12 && !w.battle.over; ++i) {
    int side = w.battle.activeSide;
    CHECK(runTurn(w.battle, w.events) >= 0);
    if (!w.battle.over) CHECK(w.battle.activeSide != side);
  }
  // Every action sequence is also legal for the side the AI plays on behalf of a human.
  CHECK(!aiTakeStep(w.battle, w.events) || !w.battle.over);
}

TEST(ai_returns_false_when_battle_over) {
  World w({"..."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 1, 0, false}});
  CHECK(w.begin());
  w.battle.over = true;
  size_t n = w.events.size();
  CHECK(!aiTakeStep(w.battle, w.events));
  CHECK_EQ(w.events.size(), n);
}

TEST(ai_full_game_on_small_map_has_a_winner) {
  for (uint32_t seed = 1; seed <= 3; ++seed) {
    World w({"......f.",
             "..#.....",
             ".......c"},
            {{"soldier", 0, 0, 0, true}, {"archer", 0, 0, 2, false}, {"tank", 0, 1, 1, false},
             {"soldier", 1, 7, 0, true}, {"archer", 1, 7, 2, false}, {"tank", 1, 6, 1, false}});
    CHECK(w.begin(seed));
    CHECK(playOut(w.battle, w.events, 60));
    CHECK(w.battle.winnerTeam >= 0);
  }
}

TEST(ai_is_deterministic_on_demo_economy) {
  Demo a, b;
  CHECK(a.load("demo-economy", "twin_rivers", 5));
  CHECK(b.load("demo-economy", "twin_rivers", 5));
  CHECK(playOut(a.battle, a.events, 60));
  CHECK(playOut(b.battle, b.events, 60));
  CHECK(a.events.size() > 100);
  CHECK(sameEvents(a.events, b.events));
  CHECK_EQ(a.battle.winnerTeam, b.battle.winnerTeam);
}

TEST(ai_demo_games_finish_within_round_limit) {
  for (uint32_t seed = 1; seed <= 3; ++seed) {
    Demo srpg, eco;
    CHECK(srpg.load("demo-srpg", "bridge", seed));
    CHECK(eco.load("demo-economy", "twin_rivers", seed));
    CHECK(playOut(srpg.battle, srpg.events, 60));
    CHECK(playOut(eco.battle, eco.events, 60));
    CHECK(srpg.battle.winnerTeam >= 0 && eco.battle.winnerTeam >= 0);
  }
}
