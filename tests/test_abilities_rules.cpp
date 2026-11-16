// Step A1 of docs/SRPG.md "Legality" and "Cost, uses, cooldown": who has an ability, the menu and its reasons, costs,
// uses, cooldowns, the once-per-turn rule of free abilities, conditions, healing.
#include "abilities_fixture.h"

using namespace ote;
using namespace abtest;

namespace {

// Blue soldier 0 at (2,1), blue archer 1 at (3,1), red soldier 2 at (3,2), red soldier 3 far away at (8,1).
struct Pair {
  AbWorld w;
  Pair() : w(Square, false, {{"soldier", 0, 2, 1, false}, {"archer", 0, 3, 1, false}, {"soldier", 1, 3, 2, false}, {"soldier", 1, 8, 1, false}},
             fogtest::field(9, 4)) {}
};

}  // namespace

TEST(abilities_a_unit_has_its_types_abilities_once_in_order) {
  Pair p;
  const int a = p.w.give("soldier", ability("a", AbilityTarget::Enemy, AbilityEffect::Push, 1));
  const int b = p.w.give("soldier", ability("b", AbilityTarget::Ally, AbilityEffect::Heal, 5));
  p.w.db.unitTypes[0].abilities.push_back(a);  // listed twice: still one entry
  CHECK(p.w.begin());
  std::vector<int> list;
  p.w.battle.abilitiesOf(p.w.un(0), list);
  CHECK(list.size() == 2 && list[0] == a && list[1] == b);
  p.w.battle.abilitiesOf(p.w.un(1), list);  // the archer has none
  CHECK(list.empty());
}

TEST(abilities_the_menu_lists_every_ability_with_a_plain_reason) {
  Pair p;
  const int shove = p.w.give("soldier", ability("shove", AbilityTarget::Enemy, AbilityEffect::Push, 1));
  const int mend = p.w.give("soldier", ability("mend", AbilityTarget::Ally, AbilityEffect::Heal, 5));
  CHECK(p.w.begin());
  std::vector<AbilityOption> menu;
  p.w.battle.abilityOptions(0, p.w.cellOf(0), menu);
  CHECK_EQ(menu.size(), (size_t)2);
  CHECK(menu[0].ability == shove && !menu[0].ok && menu[0].why == "nothing to target");  // the enemy is not next to it
  CHECK(menu[1].ability == mend && !menu[1].ok && menu[1].why == "nothing to target");   // the archer is at full health
  // Standing east of the enemy (as if it had moved there) the push is on offer.
  p.w.battle.abilityOptions(0, p.w.at(4, 2), menu);
  CHECK(menu[0].ok && menu[0].why.empty());
  std::vector<int> us;
  std::vector<Cell> cs;
  p.w.battle.abilityTargets(0, shove, p.w.at(4, 2), us, cs);
  CHECK(us.size() == 1 && us[0] == 2 && cs.empty());
  p.w.battle.abilityTargets(1, shove, p.w.cellOf(1), us, cs);  // the archer does not have it
  CHECK(us.empty() && cs.empty());
}

TEST(abilities_refusals_name_the_reason) {
  Pair p;
  const int shove = p.w.give("soldier", ability("shove", AbilityTarget::Enemy, AbilityEffect::Push, 1));
  CHECK(p.w.begin());
  // The enemy at (3,2) is diagonal to (2,1): two steps away.
  CHECK_STR(p.w.why(0, shove, 2), "that unit is out of range");
  CHECK_STR(p.w.why(0, shove, 1), "that ability works on an enemy");  // the archer is an ally
  CHECK_STR(p.w.why(0, shove, 0), "a unit cannot use that on itself");
  CHECK_STR(p.w.why(0, shove, 9), "that is not a unit on the board");
  CHECK_STR(p.w.why(1, shove, 2), "that unit does not have that ability");
  CHECK_STR(p.w.why(0, 5, 2), "there is no such ability");
  CHECK_STR(p.w.why(2, shove, 0), "that unit cannot act now");  // red's unit on blue's turn
  CHECK(!p.w.act(Action::useAbility(0, shove, 2)));
  CHECK_EQ(p.w.count(EventType::AbilityUsed), 0);
}

TEST(abilities_an_action_ability_ends_the_turn_a_free_one_does_not_and_only_once_a_turn) {
  Pair p;
  AbilityDef act = ability("shove", AbilityTarget::Enemy, AbilityEffect::Push, 1);
  AbilityDef free = ability("poke", AbilityTarget::Self, AbilityEffect::Heal, 1);
  free.free = true;
  const int a = p.w.give("soldier", act), f = p.w.give("soldier", free);
  p.w.map.units[0].x = 3;  // next to the red soldier at (3,2)
  p.w.map.units[0].y = 1;
  p.w.map.units[1].x = 4;
  CHECK(p.w.begin());
  p.w.mut(0).hp = 1;
  CHECK(p.w.use(0, f, -1));
  CHECK(!p.w.un(0).done);  // free: it may still act
  CHECK_STR(p.w.why(0, f, -1), "already used this turn");
  CHECK(p.w.battle.abilityUseOf(p.w.un(0), f)->turn);
  CHECK(p.w.use(0, a, 2));  // an action ability still works after it
  CHECK(p.w.un(0).done);
  p.w.nextRound();
  CHECK(!p.w.battle.abilityUseOf(p.w.un(0), f)->turn);  // a new turn clears it
  CHECK_STR(p.w.why(0, f, -1), "");
}

TEST(abilities_uses_are_per_unit_per_battle) {
  Pair p;
  AbilityDef d = ability("rest", AbilityTarget::Self, AbilityEffect::Heal, 1);
  d.uses = 2;
  const int a = p.w.give("soldier", d);
  CHECK(p.w.begin());
  p.w.mut(0).hp = 1;
  p.w.mut(3).hp = 1;
  CHECK(p.w.use(0, a, -1));
  p.w.nextRound();
  CHECK(p.w.use(0, a, -1));
  CHECK_EQ(p.w.battle.abilityUseOf(p.w.un(0), a)->used, 2);
  p.w.nextRound();
  CHECK_STR(p.w.why(0, a, -1), "no uses left this battle");
  CHECK(p.w.battle.abilityUseOf(p.w.un(3), a) == nullptr);  // another unit has its own count
  std::vector<AbilityOption> menu;
  p.w.battle.abilityOptions(0, p.w.cellOf(0), menu);
  CHECK(!menu[0].ok && menu[0].why == "no uses left this battle");
}

TEST(abilities_a_cooldown_counts_the_units_own_sides_turns) {
  for (int cooldown : {1, 2, 3}) {
    Pair p;
    AbilityDef d = ability("rest", AbilityTarget::Self, AbilityEffect::Heal, 1);
    d.cooldown = cooldown;
    const int a = p.w.give("soldier", d);
    CHECK(p.w.begin());
    p.w.mut(0).hp = 1;
    CHECK(p.w.use(0, a, -1));
    CHECK_EQ(p.w.battle.abilityUseOf(p.w.un(0), a)->wait, cooldown);
    // 1 = ready at the start of the next turn of its side; each round (blue, red) is one turn of blue.
    for (int round = 1; round <= cooldown; ++round) {
      p.w.nextRound();
      const int left = cooldown - round;
      CHECK_STR(p.w.why(0, a, -1), left == 0 ? "" : "ready in " + std::to_string(left) + (left == 1 ? " turn" : " turns"));
    }
  }
}

TEST(abilities_after_move_false_only_works_before_moving) {
  Pair p;
  AbilityDef d = ability("shove", AbilityTarget::Enemy, AbilityEffect::Push, 1);
  d.afterMove = false;
  const int a = p.w.give("soldier", d);
  p.w.map.units[0].x = 3;
  p.w.map.units[0].y = 1;
  p.w.map.units[1].x = 4;
  CHECK(p.w.begin());
  CHECK_STR(p.w.why(0, a, 2), "");
  std::vector<AbilityOption> menu;
  p.w.battle.abilityOptions(0, p.w.at(2, 2), menu);  // as if it had walked to (2,2), still next to the enemy
  CHECK(!menu[0].ok && menu[0].why == "can only be used before moving");
  CHECK(p.w.act(Action::move(0, p.w.at(4, 2))));
  CHECK_STR(p.w.why(0, a, 2), "can only be used before moving");
}

TEST(abilities_conditions_judge_the_user_and_the_target) {
  Pair p;
  AbilityDef d = ability("bully", AbilityTarget::Enemy, AbilityEffect::Push, 1);
  SkillCond foeHealthy;  // "who": "foe", hp > 50 percent
  foeHealthy.kind = SkillCond::Kind::Hp;
  foeHealthy.foe = true;
  foeHealthy.op = CmpOp::Gt;
  foeHealthy.value = 50;
  SkillCond meHurt;  // the user: hp <= 50 percent
  meHurt.kind = SkillCond::Kind::Hp;
  meHurt.op = CmpOp::Le;
  meHurt.value = 50;
  d.conds = {foeHealthy, meHurt};
  const int a = p.w.give("soldier", d);
  p.w.map.units[0].x = 3;
  p.w.map.units[0].y = 1;
  p.w.map.units[1].x = 4;
  CHECK(p.w.begin());
  CHECK_STR(p.w.why(0, a, 2), "cannot be used now");  // the user is unhurt
  p.w.mut(0).hp = 4;
  CHECK_STR(p.w.why(0, a, 2), "");
  p.w.mut(2).hp = 3;  // the target is hurt: the foe condition fails for it
  CHECK_STR(p.w.why(0, a, 2), "cannot be used now");
  std::vector<int> us;
  std::vector<Cell> cs;
  p.w.battle.abilityTargets(0, a, p.w.cellOf(0), us, cs);
  CHECK(us.empty());
  std::vector<AbilityOption> menu;
  p.w.battle.abilityOptions(0, p.w.cellOf(0), menu);
  CHECK(!menu[0].ok && menu[0].why == "nothing to target");
  p.w.mut(0).hp = 9;  // the user condition fails: the menu says so
  p.w.battle.abilityOptions(0, p.w.cellOf(0), menu);
  CHECK(!menu[0].ok && menu[0].why == "cannot be used now");
}

TEST(abilities_heal_restores_hit_points_and_refuses_at_full_health) {
  Pair p;
  const int mend = p.w.give("soldier", ability("mend", AbilityTarget::Ally, AbilityEffect::Heal, 4));
  const int rest = p.w.give("soldier", ability("rest", AbilityTarget::Self, AbilityEffect::Heal, kHealFull));
  p.w.map.units[1].x = 3;  // the archer next to the soldier at (2,1)
  CHECK(p.w.begin());
  CHECK_STR(p.w.why(0, mend, 1), "that unit is at full health");
  CHECK_STR(p.w.why(0, rest, -1), "that unit is at full health");
  p.w.mut(1).hp = 3;
  p.w.mut(0).hp = 2;
  CHECK(p.w.use(0, mend, 1));
  CHECK_EQ(p.w.un(1).hp, 7);
  const auto healed = p.w.of(EventType::UnitHealed);
  CHECK(healed.size() == 1 && healed[0].a == 1 && healed[0].b == 4);
  CHECK(p.w.un(0).done);
  p.w.nextRound();
  CHECK(p.w.use(0, rest, 0));  // a "self" ability: -1 or the unit itself
  CHECK_EQ(p.w.un(0).hp, 10);
  CHECK(p.w.of(EventType::UnitHealed).back().b == 8);
  p.w.nextRound();
  p.w.mut(1).hp = 7;
  CHECK(p.w.use(0, mend, 1));  // never past the maximum
  CHECK_EQ(p.w.un(1).hp, 8);
}

TEST(abilities_the_event_names_the_unit_the_target_and_the_square) {
  Pair p;
  const int a = p.w.give("soldier", ability("rest", AbilityTarget::Self, AbilityEffect::Heal, 1));
  CHECK(p.w.begin());
  p.w.mut(0).hp = 5;
  CHECK(p.w.use(0, a, -1));
  const auto used = p.w.of(EventType::AbilityUsed);
  CHECK(used.size() == 1 && used[0].a == 0 && used[0].b == a && used[0].c == 0 && used[0].d == -1);
}
