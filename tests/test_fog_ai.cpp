// Fog of war: the computer side plays by the same rules (docs/ECONOMY.md "Fog of war: AI"). It must never plan on
// something its team cannot see: moving a hidden enemy around out of sight changes none of its decisions. With
// nothing in sight it still advances (toward the last seen enemy structures, else toward unseen ground), and a whole
// battle between two such sides ends.
#include "core/ai/ai.h"
#include "core/ai/ai_internal.h"
#include "fog_fixture.h"
#include "save_helpers.h"

using namespace ote;
using namespace fogtest;

namespace {

// One decision of every blue unit, as text (what a change of hidden information would have to change).
std::string decisions(const Battle& b, int side) {
  ai::Context cx(b);
  ai::assignCaptureGoals(cx);
  std::string out;
  for (const Unit& u : b.units) {
    if (!u.alive || u.side != side || u.done) continue;
    ai::Order o = ai::bestOrder(cx, u);
    out += "u" + std::to_string(u.id) + ":" + std::to_string((int)o.kind) + "@" + std::to_string(o.moveTo) + "#" +
           std::to_string(o.target) + "/" + std::to_string(o.weapon) + "=" + std::to_string((int)(o.score * 100)) + " ";
  }
  return out;
}

// Blue acts: its units, then the end of the turn. The events of that turn.
std::vector<Event> blueTurn(Battle& b) {
  std::vector<Event> events;
  for (int i = 0; i < 40 && !b.over && b.activeSide == 0; ++i) aiTakeStep(b, events);
  return events;
}

// A field with forests on the second row and a red headquarters far to the right.
struct Scene : FogWorld {
  Scene(int hiddenX, int hiddenY)
      : FogWorld({"................", "..f.f.f.f.f.f..H", "................", "..f.f.f.f.f.f...", "................"},
                 {{"soldier", 0, 1, 2, false}, {"archer", 0, 0, 3, false},
                  {"soldier", 1, hiddenX, hiddenY, false}, {"soldier", 1, 15, 4, false}}) {
    own(15, 1, 1);
    forestHides();
    db.terrains[(size_t)db.findTerrain("forest")].moveCost = {1};
  }
};

}  // namespace

TEST(fog_ai_moving_a_hidden_enemy_around_out_of_sight_changes_no_decision) {
  std::string first;
  // (6,1), (8,3), (12,1), (10,3) are hideouts far from blue; (15,0) is plain ground far away (not in sight either).
  for (auto p : std::vector<std::pair<int, int>>{{6, 1}, {8, 3}, {12, 1}, {10, 3}, {15, 0}, {13, 2}}) {
    Scene w(p.first, p.second);
    CHECK(w.begin());
    CHECK(!w.seesUnit(0, 2));
    std::string d = decisions(w.battle, 0);
    CHECK(d.size() > 0);
    if (first.empty()) first = d;
    if (d != first) std::printf("  hidden enemy at %d,%d changed the decisions:\n    %s\n    %s\n", p.first, p.second, d.c_str(), first.c_str());
    CHECK_STR(d, first);
  }
}

TEST(fog_ai_a_hidden_enemy_inside_its_sight_range_in_a_hideout_is_not_used) {
  // Blue's soldier at (1,2) sees two squares: (2,1) is a forest square in range but a hideout. Whether red's soldier
  // stands on it or on another hideout out of range, blue decides the same.
  std::string first;
  for (auto p : std::vector<std::pair<int, int>>{{2, 1}, {2, 3}, {4, 3}, {12, 3}}) {
    Scene w(p.first, p.second);
    CHECK(w.begin());
    CHECK(!w.seesUnit(0, 2));
    std::string d = decisions(w.battle, 0);
    if (first.empty()) first = d;
    CHECK_STR(d, first);
  }
}

TEST(fog_ai_the_first_turn_is_the_same_whatever_a_hidden_enemy_does_elsewhere) {
  std::vector<Event> a, b;
  {
    Scene w(8, 3);
    CHECK(w.begin());
    a = blueTurn(w.battle);
  }
  {
    Scene w(12, 1);
    CHECK(w.begin());
    b = blueTurn(w.battle);
  }
  CHECK(a.size() > 2);
  CHECK(savetest::sameEvents(a, b));
}

TEST(fog_ai_it_does_use_what_it_sees) {
  Scene far(12, 1);
  Scene near(3, 2);                                          // two squares from blue's soldier at (1,2): in plain sight
  CHECK(far.begin() && near.begin());
  CHECK(!far.seesUnit(0, 2));
  CHECK(near.seesUnit(0, 2));
  CHECK(decisions(far.battle, 0) != decisions(near.battle, 0));
}

TEST(fog_ai_without_an_enemy_in_sight_it_advances_toward_the_last_seen_enemy_structure) {
  FogWorld w({"...............H", "................", "................"}, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 14, 2, false}});
  w.own(15, 0, 1);
  CHECK(w.begin());
  CHECK(!w.seesUnit(0, 1));
  std::vector<Event> events;
  aiTakeStep(w.battle, events);
  CHECK(w.battle.units[0].cell != w.at(0, 1));
  CHECK(w.battle.grid.x(w.battle.units[0].cell) > 0);        // toward the headquarters on the right
  CHECK(w.battle.grid.distance(w.battle.units[0].cell, w.at(15, 0)) < w.battle.grid.distance(w.at(0, 1), w.at(15, 0)));
}

TEST(fog_ai_with_no_known_structure_either_it_explores_the_farthest_unseen_ground) {
  FogWorld w(field(16, 3), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 15, 2, false}});
  CHECK(w.begin());
  std::vector<Event> events;
  aiTakeStep(w.battle, events);
  CHECK(w.battle.grid.x(w.battle.units[0].cell) > 0);
  // The same battle again moves the same way (nothing random in it).
  FogWorld w2(field(16, 3), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 15, 2, false}});
  CHECK(w2.begin());
  std::vector<Event> events2;
  aiTakeStep(w2.battle, events2);
  CHECK_EQ(w.battle.units[0].cell, w2.battle.units[0].cell);
}

TEST(fog_ai_without_fog_it_heads_for_the_enemy_units_as_before) {
  FogWorld w(field(16, 3), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 15, 2, false}});
  w.map.fog = false;
  CHECK(w.begin());
  std::vector<Event> events;
  aiTakeStep(w.battle, events);
  CHECK(w.battle.grid.x(w.battle.units[0].cell) > 0);
  std::vector<Cell> goals;
  ai::enemyGoals(w.battle, 0, goals);
  CHECK_EQ(goals.size(), size_t(1));
  CHECK_EQ(goals[0], w.at(15, 2));
}

TEST(fog_ai_knowledge_helpers_follow_what_the_team_sees) {
  FogWorld w({"...c........", "............", "............"}, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 11, 1, false}, {"soldier", 1, 3, 2, false}});
  CHECK(w.begin());
  CHECK(!ai::knownEnemy(w.battle, 0, w.battle.units[1]));    // far
  CHECK(!ai::knownEnemy(w.battle, 0, w.battle.units[2]));    // (3,2) is 4 away
  CHECK(!ai::knownEnemy(w.battle, 1, w.battle.units[0]));
  CHECK(!ai::knownEnemy(w.battle, 0, w.battle.units[0]));    // never one's own
  CHECK(w.act(Action::move(0, w.at(2, 1))));
  CHECK(ai::knownEnemy(w.battle, 0, w.battle.units[2]));
  std::vector<Cell> goals;
  ai::enemyGoals(w.battle, 0, goals);
  CHECK_EQ(goals.size(), size_t(1));                         // only the one it sees
  CHECK_EQ(goals[0], w.at(3, 2));
}

TEST(fog_ai_plans_a_capture_on_the_owner_it_last_saw_not_on_the_truth) {
  // The city at (9,0) is red's at the start: blue knows that and sends a capturer.
  FogWorld w({".........c..", "............", "............"}, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 11, 2, false}});
  w.own(9, 0, 1);
  CHECK(w.begin());
  ai::Context cx(w.battle);
  ai::assignCaptureGoals(cx);
  CHECK(cx.assigned[0] != kNoCell);
  // The city is blue's at the start, and red takes it out of sight. Blue believes it still owns it, so it plans no
  // capture of it, although the truth says it could.
  FogWorld w2({".........c..", "............", "............"}, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 11, 2, false}});
  w2.own(9, 0, 0);
  CHECK(w2.begin());
  w2.battle.structures[(size_t)w2.structure(9, 0)].owner = 1;  // red took it unseen
  w2.battle.refreshSight();
  CHECK_EQ(w2.battle.seenOwner(0, w2.structure(9, 0)), 0);
  ai::Context cx2(w2.battle);
  ai::assignCaptureGoals(cx2);
  CHECK(cx2.assigned[0] == kNoCell);
}

TEST(fog_ai_building_does_not_depend_on_the_types_of_enemies_it_cannot_see) {
  // Blue's factory only builds soldiers, whose sword cannot hurt a tank. Fog off with a tank on the field: nothing is
  // worth building. Fog on with the tank out of sight: it cannot know, so it builds.
  struct Build : FogWorld {
    Build(bool fogOn, int tankX) : FogWorld({"..F.........", "............", "............"}, {{"soldier", 0, 11, 2, false}, {"tank", 1, tankX, 1, false}}) {
      own(2, 0, 0);
      map.fog = fogOn;
      db.rules.capture = false;
      map.sides[0].resources = {{0, 1000}};
    }
  };
  Build off(false, 11);
  CHECK(off.begin());
  Cell cell;
  int type;
  CHECK(!ai::chooseBuild(off.battle, cell, type));            // truth: it would be wasted
  Build hidden(true, 8);
  CHECK(hidden.begin());
  CHECK(!hidden.seesUnit(0, 1));
  CHECK(ai::chooseBuild(hidden.battle, cell, type));          // it does not know about the tank
  Build seen(true, 10);
  CHECK(seen.begin());
  CHECK(seen.seesUnit(0, 1));
  CHECK(!ai::chooseBuild(seen.battle, cell, type));           // in sight: same answer as the truth
}

TEST(fog_ai_two_sides_with_fog_finish_the_battle) {
  int stalled = 0, rounds = 0;
  for (unsigned seed = 1; seed <= 12; ++seed) {
    FogWorld w({"..............", "..f....f......", "......f.......", "..............", "........f....."},
               {{"soldier", 0, 0, 1, true}, {"soldier", 0, 0, 3, false}, {"archer", 0, 1, 2, false}, {"tank", 0, 1, 4, false},
                {"soldier", 1, 13, 1, true}, {"soldier", 1, 13, 3, false}, {"archer", 1, 12, 2, false}, {"tank", 1, 12, 0, false}});
    w.forestHides();
    CHECK(w.begin(seed));
    std::vector<Event> events;
    while (!w.battle.over && w.battle.round <= 60) {
      long steps = 0;
      while (aiTakeStep(w.battle, events) && ++steps < 200) {}
      CHECK(steps < 200);
      events.clear();
    }
    if (!w.battle.over) ++stalled;
    rounds += w.battle.round;
  }
  CHECK_EQ(stalled, 0);
  CHECK(rounds > 12);
}

TEST(fog_ai_never_plans_a_move_the_rules_would_refuse) {
  // Whatever the AI decides under fog, apply() accepts the move (the plan was made on legal ground).
  FogWorld w({"..............", "..f....f......", "......f.......", "..............", "........f....."},
             {{"soldier", 0, 0, 1, false}, {"archer", 0, 1, 2, false}, {"soldier", 1, 13, 3, false}, {"tank", 1, 12, 0, false}});
  w.forestHides();
  CHECK(w.begin());
  std::vector<Event> events;
  size_t refused = 0;
  for (int i = 0; i < 200 && !w.battle.over; ++i) {
    const int side = w.battle.activeSide;
    ai::Context cx(w.battle);
    ai::assignCaptureGoals(cx);
    for (const Unit& u : w.battle.units) {
      if (!u.alive || u.side != side || u.done) continue;
      ai::Order o = ai::bestOrder(cx, u);
      if (o.moveTo != kNoCell && !w.battle.validate(Action::move(u.id, o.moveTo))) ++refused;
    }
    aiTakeStep(w.battle, events);
  }
  CHECK_EQ(refused, size_t(0));
}
