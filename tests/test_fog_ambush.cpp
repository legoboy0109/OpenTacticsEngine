// The ambush rule (game.json "rules.fog.ambush", docs/ECONOMY.md "Fog of war"): "endsTurn" (the default) is the old
// behaviour; "canAct" leaves the stopped unit marked as moved but not done, so it may fight, wait, capture... from the
// square where it was stopped. The move itself, its fuel and the story's "reach" are the same in both settings.
#include "core/ai/ai.h"
#include "fog_fixture.h"
#include "save_helpers.h"

using namespace ote;
using namespace fogtest;

namespace {

// Blue soldier at (0,1); red soldier hiding in a forest at (3,1), unseen until the blue one stands next to it. An order
// to walk to (3,1) (planned as if empty) stops on (2,1). `blind`: the blue unit has vision 0 and does not notice the
// enemy even when next to it (forest-free field; with enemyX 1 the very first step is blocked).
struct Ambush : FogWorld {
  explicit Ambush(bool canAct, bool blind = false, int enemyX = 3)
      : FogWorld(blind ? field(8, 3) : std::vector<std::string>{"........", "...f....", "........"},
                 {{blind ? "blind" : "soldier", 0, 0, 1, false}, {"soldier", 1, enemyX, 1, false}}) {
    db.rules.ambush = canAct ? AmbushRule::CanAct : AmbushRule::EndsTurn;
    if (!blind) {
      forestHides();
      db.terrains[(size_t)db.findTerrain("forest")].moveCost = {1};
    }
  }
};

}  // namespace

TEST(ambush_default_ends_the_turn_exactly_as_before) {
  Ambush w(false);
  CHECK(w.db.rules.ambush == AmbushRule::EndsTurn);
  CHECK(w.begin());
  CHECK(w.act(Action::move(0, w.at(3, 1))));
  CHECK_EQ(w.battle.units[0].cell, w.at(2, 1));
  CHECK(w.battle.units[0].moved && w.battle.units[0].done);
  CHECK(w.refused(Action::attack(0, 1, 0)).size() > 0);
}

TEST(ambush_can_act_the_unit_is_moved_but_not_done_and_may_attack_the_revealed_enemy) {
  Ambush w(true);
  CHECK(w.begin());
  w.events.clear();
  CHECK(w.act(Action::move(0, w.at(3, 1))));
  CHECK_EQ(w.battle.units[0].cell, w.at(2, 1));
  CHECK(w.battle.units[0].moved);
  CHECK(!w.battle.units[0].done);
  CHECK_EQ(w.all(EventType::MoveBlocked).size(), size_t(1));  // same events as with "endsTurn"
  CHECK_EQ(w.all(EventType::UnitMoved).size(), size_t(1));
  CHECK(w.seesUnit(0, 1));
  CHECK_STR(w.refused(Action::attack(0, 1, 0)), "");
  CHECK(w.refused(Action::move(0, w.at(1, 1))).size() > 0);   // it has moved: no second move
  CHECK(w.act(Action::attack(0, 1, 0)));
  CHECK(w.battle.units[0].done);
  CHECK(w.battle.units[1].hp < 10);
}

TEST(ambush_can_act_the_unit_may_simply_wait) {
  Ambush w(true);
  CHECK(w.begin());
  CHECK(w.act(Action::move(0, w.at(3, 1))));
  CHECK_STR(w.refused(Action::wait(0)), "");
  CHECK(w.act(Action::wait(0)));
  CHECK(w.battle.units[0].done);
}

TEST(ambush_can_act_blocked_at_the_first_step_the_unit_keeps_its_turn) {
  Ambush w(true, true, 1);
  CHECK(w.begin());
  w.events.clear();
  CHECK(w.act(Action::move(0, w.at(1, 1))));
  CHECK_EQ(w.battle.units[0].cell, w.at(0, 1));
  CHECK(w.battle.units[0].moved && !w.battle.units[0].done);
  CHECK_EQ(w.all(EventType::UnitMoved).size(), size_t(0));
  CHECK_EQ(w.all(EventType::MoveBlocked).size(), size_t(1));
  CHECK(w.refused(Action::move(0, w.at(0, 2))).size() > 0);   // zero squares walked still counts as the move
  CHECK(w.refused(Action::attack(0, 1, 0)).size() > 0);       // vision 0: the neighbour is still not seen, so no attack
  CHECK(w.act(Action::wait(0)));                              // but the unit keeps its turn and may do other things
}

TEST(ambush_can_act_fuel_is_spent_for_the_squares_walked) {
  Ambush w(true);
  w.db.unitTypes[0].fuel = 10;
  CHECK(w.begin());
  CHECK(w.act(Action::move(0, w.at(3, 1))));
  CHECK_EQ(w.battle.fuel(0), 8);
  CHECK(w.act(Action::attack(0, 1, 0)));
  CHECK_EQ(w.battle.fuel(0), 8);
}

TEST(ambush_can_act_the_story_hears_where_the_unit_ended_and_the_turn_goes_on) {
  Ambush w(true);
  CHECK(w.play(R"([{"id":"near","when":{"reach":{"x":2,"y":1},"by":{"side":"blue"}},"do":[{"setSwitch":"alarm","to":true}]}])"));
  CHECK(w.act(Action::move(0, w.at(3, 1))));
  CHECK(w.sw("alarm"));
  CHECK(!w.battle.units[0].done);
}

TEST(ambush_can_act_a_unit_that_can_capture_may_capture_where_it_stopped) {
  FogWorld w({"..c.....", "........", "........"}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 3, 0, false}});
  w.db.rules.ambush = AmbushRule::CanAct;
  w.db.unitTypes[0].vision = 0;
  w.db.rules.fCaptureAmount = "5";
  CHECK(w.begin());
  CHECK(w.act(Action::move(0, w.at(3, 0))));                  // stops on the city (2,0), the enemy at (3,0) unseen
  CHECK_EQ(w.battle.units[0].cell, w.at(2, 0));
  CHECK(!w.battle.units[0].done);
  CHECK(w.act(Action::capture(0)));
  CHECK_EQ(w.structureAt(2, 0).capture, 15);
}

TEST(ambush_can_act_a_save_in_the_middle_of_the_turn_keeps_the_moved_not_done_unit) {
  Ambush w(true);
  CHECK(w.begin());
  CHECK(w.act(Action::move(0, w.at(3, 1))));
  Json saved, reread;
  saveBattleState(w.battle, saved);
  std::string err;
  CHECK(Json::parse(saved.dump(0), reread, err));
  Battle r;
  CHECK(r.init(w.db, w.map, w.formulas, 1, err));
  CHECK(restoreBattleState(r, reread, err));
  CHECK_STR(savetest::stateText(r), savetest::stateText(w.battle));
  CHECK(r.units[0].moved && !r.units[0].done);
  CHECK_EQ(r.units[0].cell, w.at(2, 1));
  std::string why;
  CHECK(r.validate(Action::attack(0, 1, 0), &why));
  CHECK(!r.validate(Action::move(0, w.at(1, 1)), &why));
}

// ---- the computer side ----

namespace {

// One blue turn by the computer: events of that turn.
std::vector<Event> blueTurn(Battle& b) {
  std::vector<Event> events;
  for (int i = 0; i < 40 && !b.over && b.activeSide == 0; ++i) aiTakeStep(b, events);
  return events;
}

int count(const std::vector<Event>& events, EventType t) {
  int n = 0;
  for (const Event& e : events) n += e.type == t;
  return n;
}

// Blue's "blind" soldier walks toward the red headquarters; a red soldier it cannot see stands on its way.
struct AiAmbush : FogWorld {
  explicit AiAmbush(bool canAct)
      : FogWorld({".........", "...f.....", "........."}, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 3, 1, false}, {"soldier", 1, 8, 1, false}}) {
    db.rules.ambush = canAct ? AmbushRule::CanAct : AmbushRule::EndsTurn;
    forestHides();
    db.terrains[(size_t)db.findTerrain("forest")].moveCost = {1};
    own(8, 1, 1);
  }
};

}  // namespace

TEST(ambush_ai_with_can_act_attacks_the_revealed_enemy_from_where_it_stopped) {
  AiAmbush w(true);
  CHECK(w.begin());
  w.battle.units[1].hp = 1;  // an even fight is not worth it to the AI; a wounded target is
  std::vector<Event> ev = blueTurn(w.battle);
  CHECK_EQ(count(ev, EventType::MoveBlocked), 1);
  CHECK(count(ev, EventType::Attack) >= 1);
  CHECK(w.battle.units[1].hp < 1 || !w.battle.units[1].alive);
}

TEST(ambush_ai_with_ends_turn_does_not_attack_after_a_blocked_move) {
  AiAmbush w(false);
  CHECK(w.begin());
  w.battle.units[1].hp = 1;
  std::vector<Event> ev = blueTurn(w.battle);
  CHECK_EQ(count(ev, EventType::MoveBlocked), 1);
  CHECK_EQ(count(ev, EventType::Attack), 0);
  CHECK_EQ(w.battle.units[1].hp, 1);
}

TEST(ambush_ai_is_deterministic) {
  std::string first;
  for (int run = 0; run < 3; ++run) {
    AiAmbush w(true);
    CHECK(w.begin());
    w.battle.units[1].hp = 1;
    std::vector<Event> ev = blueTurn(w.battle);
    std::string sig;
    for (const Event& e : ev)
      sig += std::to_string((int)e.type) + ":" + std::to_string(e.a) + "," + std::to_string(e.b) + "," + std::to_string(e.c) + ";";
    sig += savetest::stateText(w.battle);
    if (run == 0) first = sig;
    else CHECK_STR(sig, first);
  }
}
