// The combat bonus of supports (docs/SRPG.md section 6 "The bonus"): only after the conversation was seen, only
// while the partner is near, partners add up, a pair-up partner is always near, forecast and fight agree.
#include "srpg_supports_fixture.h"

using namespace suptest;

namespace {

// hit +10, avoid +5, damage +2 once the conversation was seen.
const Lv kTalk{10, bonus(10, 5, 2), {"eirik: Well met."}};
const Lv kNoTalk{1, bonus(0, 0, 2), {}};

}  // namespace

TEST(supports_the_bonus_starts_after_the_conversation_was_seen_and_shows_in_the_forecast) {
  SupWorld w = trio();
  w.points(10);
  const int p = w.pair("eirik", "lyra", {kTalk});
  CHECK(w.begin());
  const int lyra = w.unitOf("lyra"), bram = w.unitOf("bram");
  Forecast f;
  CHECK(w.battle.forecast(lyra, bram, 0, w.at(3, 1), f));  // as if from (3,1)
  CHECK_EQ(f.attack.damage, 3);
  CHECK_EQ(f.counter.hit, 100);
  w.act(Action::endTurn());
  w.act(Action::endTurn());  // 10 points: the conversation is ready but not seen
  CHECK_EQ(w.level(p), 0);
  CHECK(w.battle.forecast(lyra, bram, 0, w.at(3, 1), f));
  CHECK_EQ(f.attack.damage, 3);  // ready is not enough
  CHECK(w.supportAction("lyra", "eirik"));
  CHECK_EQ(w.level(p), 1);
  CHECK(w.battle.forecast(lyra, bram, 0, w.at(3, 1), f));  // as if from (3,1), 2 squares from eirik
  CHECK_EQ(f.attack.damage, 5);                            // +2 damage
  CHECK_EQ(f.counter.hit, 95);                             // +5 avoid against the answer
  // The fight does what the forecast said.
  CHECK(w.act(Action::move(lyra, w.at(3, 1))));
  const int hp = w.battle.units[(size_t)bram].hp;
  CHECK(w.act(Action::attack(lyra, bram, 0)));
  CHECK_EQ(hp - w.battle.units[(size_t)bram].hp, 5);
}

TEST(supports_the_bonus_needs_the_partner_within_range_of_where_the_unit_fights_from) {
  SupWorld w = trio();
  w.pair("eirik", "lyra", {kNoTalk}, 5);  // level 1 from the start
  CHECK(w.begin());
  const int lyra = w.unitOf("lyra"), bram = w.unitOf("bram");
  Forecast f;
  CHECK(w.battle.forecast(lyra, bram, 0, w.at(3, 1), f));
  CHECK_EQ(f.attack.damage, 5);  // eirik at (1,1) is 2 squares from (3,1)
  CHECK(w.battle.forecast(lyra, bram, 0, w.at(4, 2), f));
  CHECK_EQ(f.attack.damage, 3);  // 4 squares from (4,2): out of the range of 3
}

TEST(supports_the_range_is_the_rules_range) {
  SupWorld w = trio();
  w.db.supports.rules.range = 2;
  w.pair("eirik", "lyra", {kNoTalk}, 5);
  CHECK(w.begin());
  const int lyra = w.unitOf("lyra"), bram = w.unitOf("bram");
  Forecast f;
  CHECK(w.battle.forecast(lyra, bram, 0, w.at(3, 1), f));
  CHECK_EQ(f.attack.damage, 5);  // 2 squares: near
  CHECK(w.battle.forecast(lyra, bram, 0, w.at(4, 2), f));
  CHECK_EQ(f.attack.damage, 3);  // 4 squares: not near
  CHECK(w.battle.forecast(lyra, bram, 0, w.at(4, 0), f));
  CHECK_EQ(f.attack.damage, 3);
}

TEST(supports_the_bonuses_of_several_partners_add_up_and_each_pair_uses_its_current_level) {
  SupWorld w({{"soldier", 0, 1, 1, false}, {"soldier", 0, 2, 1, false}, {"soldier", 0, 2, 2, false}, {"soldier", 1, 4, 1, false}},
             {"eirik", "lyra", "gorm", "bram"});
  const int a = w.pair("lyra", "eirik", {{1, bonus(0, 0, 2), {}}, {2, bonus(0, 0, 9), {"lyra: More."}}}, 5);
  w.pair("lyra", "gorm", {{1, bonus(3, 0, 1), {}}}, 5);
  CHECK(w.begin());
  CHECK_EQ(w.level(a), 1);  // level 2 has a conversation: not seen, so level 1's bonus counts
  const int lyra = w.unitOf("lyra"), bram = w.unitOf("bram");
  Forecast f;
  CHECK(w.battle.forecast(lyra, bram, 0, w.at(3, 1), f));
  CHECK_EQ(f.attack.damage, 3 + 2 + 1);  // eirik's level 1 and gorm's level 1: not the next level, not summed over levels
}

TEST(supports_a_unit_that_defends_gets_its_partners_bonus_too) {
  SupWorld w = trio();
  w.pair("eirik", "lyra", {{1, bonus(0, 10, 0), {}}}, 5);  // avoid +10 for whoever stands next to the other
  CHECK(w.begin());
  w.act(Action::endTurn());  // red: bram walks up and attacks lyra
  CHECK(w.battle.activeSide == 1);
  const int bram = w.unitOf("bram"), lyra = w.unitOf("lyra");
  Forecast f;
  CHECK(w.battle.forecast(bram, lyra, 0, w.at(3, 1), f));
  CHECK_EQ(f.attack.hit, 90);
}

TEST(supports_a_pair_up_partner_is_always_near_but_a_carried_unit_is_not) {
  SupWorld w = trio(RescueMode::PairUp);
  w.db.supports.rules.range = 1;
  w.pair("eirik", "lyra", {kNoTalk}, 5);
  CHECK(w.begin());
  const int eirik = w.unitOf("eirik"), lyra = w.unitOf("lyra"), bram = w.unitOf("bram");
  Forecast f;
  CHECK(w.battle.forecast(eirik, bram, 0, w.at(4, 2), f));
  CHECK_EQ(f.attack.damage, 3);  // lyra stands 3 squares away, range 1
  CHECK(w.act(Action::pairUp(lyra, eirik)));  // lyra goes behind eirik
  CHECK(w.battle.forecast(eirik, bram, 0, w.at(4, 2), f));
  CHECK_EQ(f.attack.damage, 5);  // the partner behind is near whatever the range
}

TEST(supports_a_carried_unit_gives_no_bonus) {
  SupWorld w = trio(RescueMode::Carry);
  w.pair("eirik", "lyra", {kNoTalk}, 5);
  CHECK(w.begin());
  const int eirik = w.unitOf("eirik"), lyra = w.unitOf("lyra"), bram = w.unitOf("bram");
  CHECK(w.act(Action::rescue(eirik, lyra)));  // eirik carries lyra: she is off the board
  Forecast f;
  CHECK(w.battle.forecast(eirik, bram, 0, w.at(3, 1), f));
  CHECK_EQ(f.attack.damage, 3);
}
