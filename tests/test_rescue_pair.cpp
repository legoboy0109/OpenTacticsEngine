// Step R1 of docs/SRPG.md: rescue in "pairUp" mode. PairUp, SwitchPair, Drop, the pair bonus, a pair as one actor,
// and the front unit falling.
#include <algorithm>

#include "rescue_fixture.h"

using namespace ote;
using namespace rescuetest;

namespace {

RescueWorld pairs() { return line(RescueMode::PairUp); }

}  // namespace

TEST(rescue_pair_up_puts_the_unit_behind_its_partner) {
  RescueWorld w = pairs();
  CHECK(w.begin());
  CHECK(w.act(Action::pairUp(1, 0)));  // the archer goes behind the soldier
  CHECK(w.carriedBy(1, 0));
  CHECK_EQ(w.un(1).hp, 8);
  const Event* e = w.last(EventType::UnitRescued);
  CHECK(e && e->a == 0 && e->b == 1 && e->c == w.at(2, 1) && e->flags == kPaired);
  CHECK(w.un(1).done);   // it ends the turn of the unit that goes behind
  CHECK(!w.un(0).done);  // the one in front can still act: a pair moves and acts once, as one actor
  CHECK(w.act(Action::move(0, w.at(1, 0))));
  CHECK(w.act(Action::wait(0)));
  CHECK(w.stands(0, 1, 0));
  CHECK(w.carriedBy(1, 0));  // the partner went along
}

TEST(rescue_pair_up_refuses_with_a_reason) {
  RescueWorld w = pairs();
  CHECK(w.begin());
  CHECK(w.refused(Action::pairUp(0, 0), "a unit cannot pair up with itself"));
  CHECK(w.refused(Action::pairUp(0, 4), "only a unit of the same side can be paired with"));
  CHECK(w.refused(Action::pairUp(0, 3), "that unit is not next to it"));
  CHECK(w.refused(Action::pairUp(0, 99), "that unit is not on the board"));
  CHECK(w.refused(Action::rescue(0, 1), "this game uses pair up, not carrying"));
  CHECK(w.refused(Action::giveUnit(0, 2), "this game uses pair up, not carrying"));
  CHECK(w.refused(Action::takeUnit(0, 2), "this game uses pair up, not carrying"));
  CHECK(w.refused(Action::switchPair(0), "that unit has nobody behind it"));
  CHECK(w.refused(Action::drop(0, w.at(0, 1)), "that unit has nobody behind it"));
  CHECK(w.act(Action::pairUp(1, 0)));
  CHECK(w.refused(Action::pairUp(2, 0), "that unit is already in a pair"));  // the target is in a pair
  CHECK(w.refused(Action::pairUp(2, 1), "that unit is not on the board"));
  CHECK(w.refused(Action::switchPair(1), "that unit cannot act now"));  // the unit behind is out of the game
  w.pass();
  CHECK(w.refused(Action::pairUp(0, 2), "that unit is already in a pair"));  // the actor is in a pair
}

TEST(rescue_pair_up_canCarry_reads_the_front_unit_as_the_carrier) {
  RescueWorld w = pairs();
  w.canCarry("a.atk >= b.atk");  // soldier 5, archer 4: the front unit must be at least as strong
  CHECK(w.begin());
  CHECK(w.refused(Action::pairUp(0, 1), "that unit cannot take it behind"));  // the soldier behind the archer: 4 < 5
  CHECK(w.battle.validate(Action::pairUp(1, 0)));
  std::vector<int> out;
  w.battle.rescueTargets(1, w.un(1).cell, out);  // the archer's possible partners
  CHECK(out == std::vector<int>{0});
  w.battle.rescueTargets(0, w.un(0).cell, out);
  CHECK(std::find(out.begin(), out.end(), 1) == out.end());
}

TEST(rescue_pair_up_the_bonus_comes_from_the_unit_behind) {
  RescueWorld w = pairs();
  w.pairBonus({0, 2, 2, 0});  // the game's bonus: atk +2, def +2
  w.pairBonus({0, 3, 0, 1}, "archer");  // an archer behind gives its own, instead
  CHECK(w.begin());
  CHECK_EQ(w.stat(0, Atk), 5);
  CHECK(w.act(Action::pairUp(1, 0)));  // archer behind soldier 0
  CHECK(w.act(Action::pairUp(3, 2)));  // soldier behind soldier 2
  CHECK_EQ(w.stat(0, Atk), 8);  // 5 + 3: the archer's bonus replaces the game's as a whole
  CHECK_EQ(w.stat(0, Def), 2);
  CHECK_EQ(w.stat(0, Mov), 4);
  CHECK_EQ(w.stat(2, Atk), 7);  // 5 + 2: no pairBonus on soldiers, so the game's applies
  CHECK_EQ(w.stat(2, Def), 4);
  CHECK_EQ(w.battle.movePoints(w.un(0)), 4);
  CHECK_EQ(w.stat(4, Atk), 5);  // everybody else is untouched
}

TEST(rescue_pair_up_a_unit_type_can_give_nothing) {
  RescueWorld w = pairs();
  w.pairBonus({0, 2, 2, 0});
  w.pairBonus({0, 0, 0, 0}, "soldier");  // present but zero: replaces the game's bonus as a whole
  CHECK(w.begin());
  CHECK(w.act(Action::pairUp(0, 2)));  // soldier 0 behind soldier 2
  CHECK_EQ(w.stat(2, Atk), 5);
  CHECK_EQ(w.stat(2, Def), 2);
}

TEST(rescue_pair_up_the_health_stat_is_never_changed_and_carrierStats_come_first) {
  RescueWorld w = pairs();
  w.pairBonus({5, 2, 0, 0});  // a hit points bonus (the loader refuses it) is ignored by the rules
  w.carrierStat(Atk, 50, 0);  // the front unit's atk: 5 -> 2, then the bonus: 4
  w.carrierStat(Mov, 100, -1);
  CHECK(w.begin());
  CHECK(w.act(Action::pairUp(1, 0)));
  CHECK_EQ(w.stat(0, Hp), 10);
  CHECK_EQ(w.battle.maxHp(w.un(0)), 10);
  CHECK_EQ(w.stat(0, Atk), 4);
  CHECK_EQ(w.stat(0, Mov), 2);
}

TEST(rescue_pair_up_switch_hands_the_turn_flags_to_the_unit_that_comes_forward) {
  RescueWorld w = pairs();
  w.pairBonus({0, 2, 0, 0});
  CHECK(w.begin());
  CHECK(w.act(Action::pairUp(1, 0)));
  w.pass();
  CHECK(w.battle.canSwitchPair(0));
  CHECK(!w.battle.canSwitchPair(1));  // the unit behind cannot
  CHECK(!w.battle.canSwitchPair(2));  // not in a pair
  CHECK(w.act(Action::move(0, w.at(1, 0))));
  const int spent = w.un(0).moveSpent;
  CHECK(spent > 0 && w.un(0).moved);
  CHECK(w.act(Action::switchPair(0)));
  const Event* e = w.last(EventType::PairSwitched);
  CHECK(e && e->a == 0 && e->b == 1 && e->c == w.at(1, 0));
  CHECK(w.carriedBy(0, 1));  // the soldier is behind now
  CHECK(w.stands(1, 1, 0));
  CHECK_EQ(w.battle.unitAt(w.at(1, 0)), 1);
  CHECK(w.un(1).moved && !w.un(1).done && w.un(1).moveSpent == spent);  // the pair has moved once
  CHECK(w.refused(Action::move(1, w.at(2, 0)), "that unit has already moved"));
  CHECK_EQ(w.stat(1, Atk), 6);  // the archer now gets the soldier's bonus (the game's: atk +2)
  CHECK_EQ(w.stat(0, Atk), 5);  // and the soldier behind is not changed
  // Free: the unit in front may still attack or wait, and may switch back.
  CHECK(w.battle.canSwitchPair(1));
  CHECK(w.act(Action::switchPair(1)));
  CHECK(w.carriedBy(1, 0) && w.stands(0, 1, 0));
  CHECK(w.un(0).moved);
  // An order given ends the turn of the pair: no switching after it.
  CHECK(w.act(Action::wait(0)));
  CHECK(!w.battle.canSwitchPair(0));
  CHECK(w.refused(Action::switchPair(0), "that unit has already acted"));
}

TEST(rescue_pair_up_a_unit_behind_cannot_act_be_seen_or_hurt) {
  RescueWorld w = pairs();
  CHECK(w.begin());
  CHECK(w.act(Action::pairUp(1, 0)));
  CHECK(w.refused(Action::wait(1), "that unit cannot act now"));
  w.act(Action::endTurn());
  Forecast f;
  CHECK(!w.battle.forecast(4, 1, 0, w.un(4).cell, f));  // it cannot be targeted
  CHECK_EQ(w.battle.unitAt(w.at(2, 1)), -1);
  w.act(Action::endTurn());
  CHECK(w.un(1).hp == 8 && !w.un(1).alive && w.battle.exists(w.un(1)));
}

TEST(rescue_pair_up_drop_puts_the_unit_behind_back_on_the_board) {
  RescueWorld w = pairs();
  CHECK(w.begin());
  CHECK(w.act(Action::pairUp(1, 0)));
  std::vector<Cell> cells;
  w.battle.dropCells(0, w.un(0).cell, cells);
  CHECK_EQ(cells.size(), (size_t)3);  // (1,2) is taken
  CHECK(w.act(Action::drop(0, w.at(2, 1))));
  CHECK(w.stands(1, 2, 1) && w.un(1).done && w.un(0).done);
  const Event* e = w.last(EventType::UnitDropped);
  CHECK(e && e->a == 0 && e->b == 1 && e->c == w.at(2, 1) && e->flags == 0);
  CHECK(w.un(0).carrying == -1 && w.un(1).carriedBy == -1);
  CHECK_EQ(w.stat(0, Atk), 5);
}

TEST(rescue_pair_up_the_partner_steps_in_when_the_front_unit_falls) {
  RescueWorld w(RescueMode::PairUp, {{"archer", 0, 1, 1, false}, {"soldier", 0, 2, 1, false}, {"tank", 1, 1, 0, false}});
  CHECK(w.begin());
  w.battle.units[0].hp = 3;
  CHECK(w.act(Action::pairUp(1, 0)));  // the soldier behind the archer
  w.act(Action::endTurn());
  CHECK(w.act(Action::attack(2, 0, 2)));
  CHECK(!w.un(0).alive && w.un(0).carrying == -1);
  CHECK(w.stands(1, 1, 1));  // on the square where the front unit fell
  CHECK(w.un(1).done && w.un(1).hp == 10);
  const Event* e = w.last(EventType::UnitDropped);
  CHECK(e && e->a == 0 && e->b == 1 && e->c == w.at(1, 1) && e->flags == kForced);
  CHECK(!w.battle.sides[0].defeated);
}

TEST(rescue_pair_up_the_unit_behind_must_be_able_to_stand_where_it_comes_forward) {
  // A boat behind a soldier on land: it cannot come forward there, and cannot be dropped there either.
  BoatWorld w({{"soldier", 0, 3, 1, false}, {"boat", 0, 4, 1, false}, {"soldier", 1, 0, 0, false}}, {{4, 1}}, {},
              RescueMode::PairUp);
  CHECK(w.begin());
  CHECK(w.act(Action::pairUp(1, 0)));
  w.pass();
  CHECK(w.refused(Action::switchPair(0), "the unit behind could not stand on this square"));
  CHECK(!w.battle.canSwitchPair(0));
  CHECK(w.refused(Action::drop(0, w.at(3, 0)), "the unit could not stand on that square"));
  CHECK(w.battle.validate(Action::drop(0, w.at(4, 1))));  // the water it came from
}
