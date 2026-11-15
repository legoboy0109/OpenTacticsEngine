// The story condition { "sees": <unit filter>, "team": "<team>" } (docs/EVENTS.md): true while the team sees at least
// one living unit that matches; judged on the team's sight, so a hidden or distant unit does not count. Without fog a
// team "sees" every living unit.
#include "fog_fixture.h"

using namespace ote;
using namespace fogtest;

namespace {

// blue (0,1) "eirik", red "gorm" at (gormX, 1); an event on every blue turn from round 2 flips the switch "alarm" when
// the condition holds.
struct Watch : FogWorld {
  explicit Watch(int gormX) : FogWorld(field(12, 3), {{"soldier", 0, 0, 1, false}, {"soldier", 1, gormX, 1, false}}, {"eirik", "gorm"}) {}
  bool ok(const std::string& condition) {
    return play("[{\"id\":\"watch\",\"when\":{\"turn\":2},\"if\":[" + condition + "],\"do\":[{\"setSwitch\":\"alarm\",\"to\":true}]}]");
  }
  // Does an event at the start of the battle with this condition run?
  bool cond(const std::string& condition) {
    std::string e = load("[{\"id\":\"c\",\"when\":{\"battle\":\"start\"},\"if\":[" + condition +
                         "],\"do\":[{\"setSwitch\":\"saved\",\"to\":true}]}]");
    if (!e.empty()) std::printf("  %s\n", e.c_str());
    return e.empty() && begin() && sw("saved");
  }
};

}  // namespace

TEST(fog_sees_is_true_for_a_unit_in_sight_and_false_for_one_out_of_it) {
  Watch near(2), far(9);
  CHECK(near.cond(R"({"sees":"gorm","team":"blue"})"));
  CHECK(!far.cond(R"({"sees":"gorm","team":"blue"})"));
}

TEST(fog_sees_is_judged_for_the_team_named) {
  Watch w(2);
  CHECK(w.cond(R"({"sees":"eirik","team":"red"})"));        // red's soldier two squares away sees blue's
  Watch far(9);
  CHECK(!far.cond(R"({"sees":"eirik","team":"red"})"));
  CHECK(far.cond(R"({"sees":"eirik","team":"blue"})"));     // a team always sees its own units
}

TEST(fog_sees_follows_the_battle_as_units_move) {
  Watch w(5);
  CHECK(w.ok(R"({"sees":"gorm","team":"blue"})"));
  CHECK(!w.sw("alarm"));
  CHECK(w.act(Action::move(0, w.at(3, 1))));                // gorm at (5,1) is now two squares away
  CHECK(w.nextTurn());                                      // round 2 begins: the event looks
  CHECK(w.sw("alarm"));
}

TEST(fog_sees_a_unit_in_a_hideout_is_not_seen_until_somebody_is_next_to_it) {
  Watch w(2);
  w.forestHides();
  w.map.terrain[(size_t)(1 * 12 + 2)] = w.db.findTerrain("forest");
  CHECK(w.cond(R"({"sees":"gorm","team":"blue"})") == false);
  Watch next(1);
  next.forestHides();
  next.map.terrain[(size_t)(1 * 12 + 1)] = next.db.findTerrain("forest");
  CHECK(next.cond(R"({"sees":"gorm","team":"blue"})"));
}

TEST(fog_sees_works_with_every_kind_of_filter) {
  Watch w(2);
  CHECK(w.cond(R"({"sees":{"side":"red"},"team":"blue"})"));
  CHECK(w.cond(R"({"sees":{"team":"red"},"team":"blue"})"));
  CHECK(w.cond(R"({"sees":{"type":"soldier"},"team":"blue"})"));
  CHECK(!w.cond(R"({"sees":{"type":"tank"},"team":"blue"})"));
  CHECK(w.cond(R"({"sees":{},"team":"blue"})"));            // any unit: its own counts
}

TEST(fog_sees_a_dead_unit_is_not_seen) {
  Watch w(1);
  w.addUnit("soldier", 1, 11, 0);                           // red has another unit, so the battle goes on
  CHECK(w.ok(R"({"sees":"gorm","team":"blue"})"));
  w.battle.units[1].hp = 1;
  CHECK(w.act(Action::attack(0, 1, 0)));
  CHECK(!w.battle.units[1].alive);
  CHECK(w.nextTurn());
  CHECK(!w.sw("alarm"));
}

TEST(fog_sees_allied_eyes_count_for_the_team) {
  Watch w(9);
  w.addAlly("blue");
  w.addUnit("soldier", 2, 8, 1);
  CHECK(w.cond(R"({"sees":"gorm","team":"blue"})"));
}

TEST(fog_sees_without_fog_a_team_sees_every_living_unit) {
  Watch w(11);
  w.map.fog = false;
  CHECK(w.cond(R"({"sees":"gorm","team":"blue"})"));
}
