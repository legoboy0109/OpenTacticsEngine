// Fog of war: which teams saw each event (Event::seen and Event::seenSteps; the rules are listed at the top of
// engine/core/battle/event_visibility.cpp), and that without fog nothing changes for anybody.
#include "core/ai/ai.h"
#include "fog_fixture.h"
#include "save_helpers.h"

using namespace ote;
using namespace fogtest;

namespace {

bool seenBy(const Event& e, int team) { return ((e.seen >> team) & 1u) != 0; }

// First event of a type (a copy; the test fails loudly when there is none).
Event first(const FogWorld& w, EventType t, size_t nth = 0) {
  std::vector<Event> all = w.all(t);
  if (nth >= all.size()) {
    std::printf("  no event number %zu of type %d\n", nth, (int)t);
    CHECK(false);
    return Event();
  }
  return all[nth];
}

// Three teams on a long row: blue (team 0), red (1) and green (2), one soldier each, at the given columns.
struct Three : FogWorld {
  Three(int blueX, int redX, int greenX)
      : FogWorld(field(16, 3), {{"soldier", 0, blueX, 1, false}, {"soldier", 1, redX, 1, false}}) {
    addSide("green", "green");
    addUnit("soldier", 2, greenX, 1);
  }
};

}  // namespace

TEST(fog_events_public_events_are_seen_by_every_team) {
  Three w(0, 7, 15);
  CHECK(w.play(R"([{"id":"intro","when":{"battle":"start"},"do":[{"say":"Begin."},
                     {"objective":{"text":"Hold on","win":[{"round":50}]}}]}])"));
  CHECK_EQ(w.battle.teamCount, 3);
  CHECK(seenBy(first(w, EventType::TurnStarted), 0) && seenBy(first(w, EventType::TurnStarted), 1) && seenBy(first(w, EventType::TurnStarted), 2));
  Event say = first(w, EventType::Dialogue), goal = first(w, EventType::ObjectiveChanged);
  CHECK(say.seen == kSeenByAll);
  CHECK(goal.seen == kSeenByAll);
}

TEST(fog_events_a_move_is_seen_by_a_team_that_sees_any_square_of_it_and_the_steps_say_which) {
  Three w(0, 5, 15);
  CHECK(w.begin());
  w.events.clear();
  CHECK(w.act(Action::move(0, w.at(3, 1))));                 // (0,1) (1,1) (2,1) (3,1)
  Event e = first(w, EventType::UnitMoved);
  CHECK(seenBy(e, 0));                                       // the mover's team saw all of it
  CHECK(seenBy(e, 1));                                       // red at (5,1) sees (3,1) (distance 2)
  CHECK(!seenBy(e, 2));                                      // green is far away
  CHECK_EQ(e.seenSteps.size(), size_t(6));                   // two entries per team
  CHECK(e.seenSteps[0] == 0 && e.seenSteps[1] == 3);         // blue: from the first to the last square
  CHECK(e.seenSteps[2] == 3 && e.seenSteps[3] == 3);         // red: only the last
  CHECK(e.seenSteps[4] == -1 && e.seenSteps[5] == -1);       // green: never
}

TEST(fog_events_a_unit_walking_through_sight_and_out_again_has_first_and_last_seen_steps) {
  FogWorld w({"..............", "..............", ".............."}, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 5, 0, false}});
  w.map.units[0].stats[3] = 9;
  CHECK(w.begin());
  w.events.clear();
  CHECK(w.act(Action::move(0, w.at(9, 1))));                 // passes red at (5,0): row 1 squares x = 4..6 are within 2
  Event e = first(w, EventType::UnitMoved);
  CHECK_EQ(e.data.size(), size_t(10));
  CHECK(e.seenSteps[2] == 4 && e.seenSteps[3] == 6);         // red saw it from the fifth square to the seventh
  CHECK(e.seenSteps[0] == 0 && e.seenSteps[1] == 9);
}

TEST(fog_events_combat_is_seen_when_either_fighter_is_seen) {
  Three w(5, 6, 15);
  w.addSide("amber", "amber");
  w.addUnit("soldier", 3, 8, 1);                             // amber sees red (2 away) but not blue (3 away)
  w.addSide("grey", "grey");
  w.addUnit("soldier", 4, 14, 0);                            // grey is far from the fight
  CHECK(w.begin());
  w.events.clear();
  CHECK(w.act(Action::attack(0, 1, 0)));
  Event a = first(w, EventType::Attack);
  CHECK(seenBy(a, 0) && seenBy(a, 1));                       // the fighters' own teams
  CHECK(seenBy(a, 3));                                       // amber sees red
  CHECK(!seenBy(a, 2) && !seenBy(a, 4));
  for (const Event& e : w.all(EventType::Attack)) CHECK(e.seen == a.seen);  // the counter, too
}

TEST(fog_events_a_death_is_seen_by_the_victims_team_and_those_who_saw_it_fall) {
  Three w(5, 6, 8);                          // green sees red (distance 2), not blue (3)
  CHECK(w.begin());
  w.battle.units[1].hp = 1;
  w.events.clear();
  CHECK(w.act(Action::attack(0, 1, 0)));
  Event d = first(w, EventType::UnitDied);
  CHECK(seenBy(d, 0) && seenBy(d, 1) && seenBy(d, 2));
}

TEST(fog_events_a_death_nobody_else_sees_is_private_to_the_two_teams) {
  Three w(5, 6, 15);
  CHECK(w.begin());
  w.battle.units[1].hp = 1;
  w.events.clear();
  CHECK(w.act(Action::attack(0, 1, 0)));
  Event d = first(w, EventType::UnitDied);
  CHECK(seenBy(d, 0) && seenBy(d, 1) && !seenBy(d, 2));
}

TEST(fog_events_experience_and_other_bookkeeping_belongs_to_the_units_own_team) {
  Three w(5, 6, 15);
  w.db.rules.progression = true;
  w.db.rules.fXpHit = "10";
  w.db.rules.fXpKill = "20";
  CHECK(w.begin());
  w.events.clear();
  CHECK(w.act(Action::attack(0, 1, 0)));
  Event xp = first(w, EventType::XpGained);
  CHECK(seenBy(xp, 0) && !seenBy(xp, 1) && !seenBy(xp, 2));  // red stands right next to it and still does not see this
}

TEST(fog_events_money_is_only_known_to_the_side_it_belongs_to) {
  FogWorld w({"..F.....", "........", "........"}, {{"soldier", 0, 7, 0, false}, {"soldier", 1, 3, 1, false}});
  w.own(2, 0, 0);
  w.map.sides[0].resources = {{0, 1000}};
  CHECK(w.begin());
  w.events.clear();
  CHECK(w.act(Action::build(w.at(2, 0), 0)));
  Event pay = first(w, EventType::ResourceChanged), built = first(w, EventType::UnitBuilt);
  CHECK(seenBy(pay, 0) && !seenBy(pay, 1));
  CHECK(seenBy(built, 0) && seenBy(built, 1));               // red's soldier at (3,1) sees the square (distance 2)
}

TEST(fog_events_income_at_the_start_of_a_turn_is_private_too) {
  FogWorld w({"c.......", "........", "........"}, {{"soldier", 0, 7, 0, false}, {"soldier", 1, 3, 1, false}});
  w.own(0, 0, 1);                                            // red's city: income for red
  CHECK(w.begin());
  w.events.clear();
  CHECK(w.act(Action::endTurn()));
  Event income = first(w, EventType::ResourceChanged);
  CHECK_EQ(income.a, 1);
  CHECK(seenBy(income, 1) && !seenBy(income, 0));
}

TEST(fog_events_capture_progress_is_seen_only_by_teams_that_see_the_capturer) {
  FogWorld w({"..........c.", "............", "............"}, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 10, 0, false}});
  w.db.rules.fCaptureAmount = "5";
  CHECK(w.begin());
  CHECK(w.act(Action::endTurn()));
  w.events.clear();
  CHECK(w.act(Action::capture(1)));
  Event p = first(w, EventType::CaptureProgress);
  CHECK(seenBy(p, 1) && !seenBy(p, 0));                      // blue, far away, does not know a capture started
}

TEST(fog_events_a_finished_capture_is_seen_by_both_owners_and_whoever_watched) {
  FogWorld w({"...H........", "............", "............"}, {{"soldier", 0, 3, 0, false}, {"soldier", 1, 11, 2, false}});
  w.addSide("green", "green");
  w.addUnit("soldier", 2, 7, 2);
  w.own(3, 0, 1);                                            // red's headquarters, far from red's only unit
  CHECK(w.begin());
  w.events.clear();
  CHECK(w.act(Action::capture(0)));
  Event c = first(w, EventType::StructureCaptured);
  CHECK(seenBy(c, 0));                                       // the new owner
  CHECK(seenBy(c, 1));                                       // the old owner is told it lost it
  CHECK(!seenBy(c, 2));                                      // green is four squares away
}

TEST(fog_events_an_upgrade_is_seen_by_its_owner_and_those_who_see_the_structure) {
  FogWorld w({"..c.....", "........", "........"}, {{"soldier", 0, 7, 2, false}, {"soldier", 1, 3, 1, false}});
  w.own(2, 0, 0);
  StructureDef big = w.db.structures[(size_t)w.db.findStructure("city")];
  big.id = big.name = "megacity";
  w.db.structures.push_back(big);
  UpgradeDef up;
  up.to = (int)w.db.structures.size() - 1;
  w.db.structures[(size_t)w.db.findStructure("city")].upgrades.push_back(up);
  CHECK(w.begin());
  w.events.clear();
  CHECK(w.act(Action::upgrade(w.structure(2, 0), up.to)));
  Event u = first(w, EventType::StructureUpgraded);
  CHECK(seenBy(u, 0) && seenBy(u, 1));                       // red's soldier at (3,1) is next to it
}

TEST(fog_events_story_changes_to_units_are_seen_by_those_who_see_them) {
  Three w(0, 7, 15);
  w.map.units[1].id = "scout";
  CHECK(w.play(R"([
    {"id":"add","when":{"turn":1},"do":[{"addUnit":{"type":"soldier","side":"red","x":12,"y":1}}]},
    {"id":"move","when":{"turn":2},"do":[{"moveUnit":"scout","x":1,"y":1}]},
    {"id":"gone","when":{"turn":3},"do":[{"removeUnit":{"unit":"scout"}}]}])"));
  Event added = first(w, EventType::UnitAdded);
  CHECK(!seenBy(added, 0) && seenBy(added, 1) && !seenBy(added, 2));   // (12,1) is 3 from green at (15,1)
  CHECK(w.act(Action::endTurn()) && w.nextTurn());   // three sides: blue's next turn is three turns away
  Event moved = first(w, EventType::UnitRelocated);
  CHECK(seenBy(moved, 0) && seenBy(moved, 1));               // it lands next to blue
  CHECK(w.act(Action::endTurn()) && w.nextTurn());
  Event removed = first(w, EventType::UnitRemoved);
  CHECK(seenBy(removed, 0) && seenBy(removed, 1));
}

TEST(fog_events_an_ambush_is_seen_by_both_units_teams) {
  Three w(0, 2, 15);
  w.vision("soldier", 1);
  CHECK(w.begin());
  w.events.clear();
  CHECK(w.act(Action::move(0, w.at(3, 1))));                 // the vision-1 soldier walks into red at (2,1)
  Event b = first(w, EventType::MoveBlocked);
  CHECK(seenBy(b, 0) && seenBy(b, 1) && !seenBy(b, 2));
}

TEST(fog_events_the_end_of_the_battle_is_public) {
  Three w(5, 6, 15);
  CHECK(w.begin());
  w.battle.units[1].hp = 1;
  CHECK(w.act(Action::attack(0, 1, 0)));
  CHECK(w.battle.units[1].alive == false);
  Event d = first(w, EventType::SideDefeated);
  CHECK(d.seen == kSeenByAll);
}

TEST(fog_events_without_fog_every_event_has_every_bit_and_no_steps) {
  FogWorld w(field(12, 3), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 11, 1, false}});
  w.map.fog = false;
  CHECK(w.begin());
  for (int i = 0; i < 60 && !w.battle.over; ++i) aiTakeStep(w.battle, w.events);
  CHECK(w.events.size() > 10);
  for (const Event& e : w.events) CHECK(e.seen == kSeenByAll && e.seenSteps.empty());
}

TEST(fog_events_the_same_seed_gives_the_same_events_and_masks) {
  auto play = [](std::vector<Event>& out) {
    FogWorld w(field(14, 5), {{"soldier", 0, 0, 1, false}, {"archer", 0, 0, 3, false}, {"tank", 0, 1, 2, false},
                              {"soldier", 1, 13, 1, false}, {"archer", 1, 13, 3, false}, {"tank", 1, 12, 2, false}});
    CHECK(w.begin(5));
    for (int i = 0; i < 80 && !w.battle.over; ++i) aiTakeStep(w.battle, w.events);
    out = w.events;
  };
  std::vector<Event> a, b;
  play(a);
  play(b);
  CHECK(a.size() > 20);
  CHECK(savetest::sameEvents(a, b));                         // includes `seen` and `seenSteps`
  size_t partial = 0;
  for (const Event& e : a) partial += e.seen != kSeenByAll;
  CHECK(partial > 0);                                        // the masks were really used
}
