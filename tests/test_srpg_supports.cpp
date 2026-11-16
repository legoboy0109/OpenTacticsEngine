// Supports in a battle (docs/SRPG.md section 6, step S1): each source of points and its once-per rules, levels
// that are gained at once or by a conversation, maxLevels, the Support action. The combat bonus is in
// test_srpg_supports_bonus.cpp, the story in test_srpg_supports_story.cpp.
#include "core/battle/support_rules.h"
#include "srpg_supports_fixture.h"

using namespace suptest;

namespace {

const Lv kPlain{10, {}, {}};                                         // a level with no conversation
const Lv kTalk{10, bonus(10, 5), {"eirik: You never miss.", "lyra: I miss plenty."}};

}  // namespace

// ---- ending a turn side by side ----

TEST(supports_end_turn_adjacent_pays_once_per_round_to_pairs_next_to_each_other) {
  SupWorld w = trio();
  w.points(5);
  const int pl = w.pair("eirik", "lyra", {kPlain});  // (1,1) and (2,1): neighbours
  const int pg = w.pair("eirik", "gorm", {kPlain});  // (1,1) and (0,2): diagonal, not neighbours
  CHECK(w.begin());
  CHECK_EQ(w.pts(pl), 0);
  w.events.clear();
  CHECK(w.act(Action::endTurn()));  // blue ends its turn
  CHECK_EQ(w.pts(pl), 5);
  CHECK_EQ(w.pts(pg), 0);
  std::vector<Event> pts = w.of(EventType::SupportPoints);
  CHECK_EQ(pts.size(), (size_t)1);
  if (pts.size() == 1) {
    CHECK_EQ(pts[0].a, w.unitOf("eirik"));
    CHECK_EQ(pts[0].b, w.unitOf("lyra"));
    CHECK_EQ(pts[0].c, pl);
    CHECK_EQ(pts[0].d, 5);
    CHECK_EQ(pts[0].flags & kReady, 0);
  }
  CHECK(story::find(w.events, EventType::SupportPoints) < story::find(w.events, EventType::TurnStarted));
  CHECK(w.act(Action::endTurn()));  // red ends its turn: the pair is blue's, nothing more
  CHECK_EQ(w.pts(pl), 5);
  CHECK_EQ(w.level(pl), 0);
}

TEST(supports_a_level_without_conversation_is_gained_at_once) {
  SupWorld w = trio();
  w.points(5);
  const int p = w.pair("eirik", "lyra", {kPlain, {20, {}, {}}});
  CHECK(w.begin());
  w.round();
  CHECK_EQ(w.level(p), 0);
  w.events.clear();
  CHECK(w.act(Action::endTurn()));  // 10 points: level 1 at once
  CHECK_EQ(w.level(p), 1);
  std::vector<Event> got = w.of(EventType::SupportReached);
  CHECK_EQ(got.size(), (size_t)1);
  if (got.size() == 1) {
    CHECK_EQ(got[0].c, p);
    CHECK_EQ(got[0].d, 1);
    CHECK_EQ(got[0].a, w.unitOf("eirik"));
  }
  CHECK_EQ(w.of(EventType::SupportPoints)[0].flags & kReady, 0);  // nothing to wait for: not "ready"
  CHECK_EQ(w.level(p), 1);
}

TEST(supports_a_level_with_conversation_waits_and_the_gain_that_makes_it_ready_says_so) {
  SupWorld w = trio();
  w.points(5);
  const int p = w.pair("eirik", "lyra", {kTalk});
  CHECK(w.begin());
  w.round();
  w.events.clear();
  CHECK(w.act(Action::endTurn()));
  CHECK_EQ(w.pts(p), 10);
  CHECK_EQ(w.level(p), 0);  // not yet: the conversation has to be played
  CHECK_EQ(w.of(EventType::SupportReached).size(), (size_t)0);
  CHECK((w.of(EventType::SupportPoints)[0].flags & kReady) != 0);
  w.round();  // more points do not repeat the flag
  CHECK_EQ(w.level(p), 0);
}

TEST(supports_a_pair_off_the_board_or_on_another_team_earns_nothing) {
  SupWorld w = trio();
  w.points(5);
  const int pb = w.pair("lyra", "bram", {kPlain});  // bram is red
  const int pl = w.pair("eirik", "lyra", {kPlain});
  CHECK(w.begin());
  w.battle.units[(size_t)w.unitOf("eirik")].alive = false;  // defeated
  w.battle.occupant[(size_t)w.at(1, 1)] = -1;
  w.battle.units[(size_t)w.unitOf("eirik")].cell = kNoCell;
  w.act(Action::endTurn());
  CHECK_EQ(w.pts(pl), 0);
  CHECK_EQ(w.pts(pb), 0);
}

// ---- fighting together ----

TEST(supports_fight_together_pays_each_fighter_s_partners_within_range_once_per_combat) {
  SupWorld w = trio();
  w.points(0, 3);
  const int pl = w.pair("eirik", "lyra", {kPlain});
  const int pg = w.pair("lyra", "gorm", {kPlain});  // gorm stands 4 squares from where lyra fights
  CHECK(w.begin());
  CHECK(w.act(Action::move(w.unitOf("lyra"), w.at(3, 1))));
  w.events.clear();
  CHECK(w.act(Action::attack(w.unitOf("lyra"), w.unitOf("bram"), 0)));  // bram counters: two strikes in all
  CHECK_EQ(w.pts(pl), 3);  // eirik is 2 squares away
  CHECK_EQ(w.pts(pg), 0);
  CHECK_EQ(w.of(EventType::SupportPoints).size(), (size_t)1);
}

TEST(supports_a_fighter_that_fell_earns_nothing_but_the_other_still_does) {
  SupWorld w = trio();
  w.points(0, 3);
  const int pl = w.pair("eirik", "lyra", {kPlain});
  const int pb = w.pair("bram", "gorm", {kPlain});  // opposite teams: never
  CHECK(w.begin());
  CHECK(w.act(Action::move(w.unitOf("lyra"), w.at(3, 1))));
  w.battle.units[(size_t)w.unitOf("lyra")].hp = 1;  // bram's counter defeats it
  CHECK(w.act(Action::attack(w.unitOf("lyra"), w.unitOf("bram"), 0)));
  CHECK(!w.battle.units[(size_t)w.unitOf("lyra")].alive);
  CHECK_EQ(w.pts(pl), 0);
  CHECK_EQ(w.pts(pb), 0);
}

// ---- healing ----

TEST(supports_heal_pays_the_pair_when_one_heals_the_other) {
  SupWorld w = trio();
  fixture::addItems(w);
  fixture::carry(w, 0, {fixture::items::Salve});  // eirik carries a salve for allies
  w.points(0, 0, 4);
  const int p = w.pair("eirik", "lyra", {kPlain});
  const int pg = w.pair("eirik", "gorm", {kPlain});
  CHECK(w.begin());
  w.battle.units[(size_t)w.unitOf("lyra")].hp = 3;
  w.events.clear();
  CHECK(w.act(Action::useItem(w.unitOf("eirik"), 0, w.unitOf("lyra"))));
  CHECK_EQ(w.pts(p), 4);
  CHECK_EQ(w.pts(pg), 0);
  CHECK_EQ(w.of(EventType::SupportPoints).size(), (size_t)1);
}

// ---- maxLevels, start, names ----

TEST(supports_max_levels_holds_a_ready_level_back_until_there_is_room) {
  SupWorld w = trio();
  w.points(5);
  w.db.supports.rules.maxLevels = 1;
  const int a = w.pair("eirik", "lyra", {kPlain, {20, {}, {}}});
  const int b = w.pair("eirik", "gorm", {{5, {}, {}}}, 100);
  CHECK(w.begin());
  // eirik already holds one level (pair b, from its start points): its cap is reached.
  CHECK_EQ(w.level(b), 1);
  w.round();
  w.round();
  CHECK_EQ(w.pts(a), 10);
  CHECK_EQ(w.level(a), 0);
  CHECK(!supportReady(w.db, w.battle.supports, a));
}

TEST(supports_start_points_and_level_names) {
  SupWorld w = trio();
  const int p = w.pair("eirik", "lyra", {{10, {}, {}}, {50, {}, {"eirik: Hello."}}}, 25);
  CHECK(w.begin());
  CHECK_EQ(w.pts(p), 25);
  CHECK_EQ(w.level(p), 1);  // the first level needs no conversation and its points are reached at the start
  w.db.supports.rules.levelNames = {"C", "B"};
  CHECK_STR(supportLevelName(w.db.supports.rules, 2), "B");
  CHECK_STR(supportLevelName(SupportRules(), 3), "3");
}

// ---- the Support action ----

TEST(supports_the_action_plays_the_conversation_gains_the_level_and_is_free) {
  SupWorld w = trio();
  w.points(10);
  const int p = w.pair("eirik", "lyra", {kTalk});
  CHECK(w.begin());
  CHECK_STR(w.whyNot("eirik", "lyra"), "those two have no support conversation waiting");
  w.act(Action::endTurn());
  w.act(Action::endTurn());
  CHECK_EQ(w.level(p), 0);
  std::vector<int> options;
  const int e = w.unitOf("eirik"), l = w.unitOf("lyra");
  w.battle.supportOptions(e, w.battle.units[(size_t)e].cell, options);
  CHECK(options == std::vector<int>{l});
  w.battle.supportOptions(l, w.battle.units[(size_t)l].cell, options);  // either of the two may start it
  CHECK(options == std::vector<int>{e});
  CHECK(w.supportAction("lyra", "eirik"));
  CHECK_EQ(w.level(p), 1);
  std::vector<Event> lines = w.of(EventType::Dialogue);
  CHECK_EQ(lines.size(), (size_t)2);
  if (lines.size() == 2) {
    CHECK_EQ(lines[0].a, e);  // speaker = the unit when it is on the board
    CHECK_STR(w.battle.storyText(lines[0].b), "You never miss.");
    CHECK_STR(w.battle.storyText(lines[1].b), "I miss plenty.");
    CHECK_EQ(lines[1].a, l);
  }
  CHECK(story::find(w.events, EventType::Dialogue) < story::find(w.events, EventType::SupportReached));
  CHECK_EQ(w.of(EventType::SupportReached)[0].d, 1);
  // Free: nobody is done, the turn goes on, and the same conversation cannot be played twice.
  CHECK(!w.battle.units[(size_t)e].done && !w.battle.units[(size_t)l].done && w.battle.activeSide == 0);
  CHECK(!w.supportAction("eirik", "lyra"));
  CHECK(w.act(Action::move(e, w.at(1, 0))));  // still free to act
}

TEST(supports_the_action_is_refused_with_a_plain_reason) {
  SupWorld w = trio();
  w.points(20);
  w.pair("eirik", "lyra", {kTalk});
  w.pair("eirik", "gorm", {kTalk}, 50);  // ready from the start, but gorm is not next to eirik
  CHECK(w.begin());
  CHECK_STR(w.whyNot("eirik", "gorm"), "those two are not next to each other");
  CHECK_STR(w.whyNot("eirik", "bram"), "those two are not a support pair");
  CHECK_STR(w.whyNot("eirik", "eirik"), "that is not a unit to talk to");
  CHECK_STR(w.whyNot("bram", "eirik"), "that unit cannot act now");  // red's unit on blue's turn
  w.act(Action::wait(w.unitOf("eirik")));
  CHECK_STR(w.whyNot("eirik", "lyra"), "that unit has already acted");
}

TEST(supports_conversations_only_in_camp_are_never_played_in_battle) {
  SupWorld w = trio();
  w.points(10);
  w.db.supports.rules.conversations = SupportTalks::Camp;
  const int p = w.pair("eirik", "lyra", {kTalk, {40, {}, {}}});
  const int q = w.pair("eirik", "gorm", {kPlain});
  CHECK(w.begin());
  w.act(Action::endTurn());
  w.act(Action::endTurn());
  CHECK_STR(w.whyNot("eirik", "lyra"), "support conversations are played between chapters in this game");
  CHECK_EQ(w.level(p), 0);
  CHECK_EQ(w.pts(p), 10);  // the points still grow
  (void)q;
}

TEST(supports_a_game_without_pairs_does_nothing) {
  SupWorld w = trio();
  CHECK(w.begin());
  CHECK_STR(w.whyNot("eirik", "lyra"), "this game has no supports");
  w.act(Action::endTurn());
  CHECK(w.battle.supports.points.empty() && w.of(EventType::SupportPoints).empty());
}
