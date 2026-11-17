// Docs/SRPG.md "More skill blocks", second part: follow-up control, the triangle multiplier, weapon experience, "move again but not
// after attacking", carrying, effectiveness, the counterattack / inventory tests and "ignore the foe's defence" as a formula.
// The fixture tests/fixtures/srpg-skillblocks: scout (0, blue, speed 8) next to guard (1, red, speed 3), rider (2, red, far),
// lancer (3, blue, horse spear), mage (4, blue, a magic tome, range 1-2). "strikes" doubles at +5 speed less `a.followShift`.
#include "srpg_ranks_fixture.h"
#include "srpg_skills_fixture.h"

using namespace ote;
using skilltest::Copy;
using skilltest::World;
using skilltest::saysIn;

namespace {

const char* kFix = "srpg-skillblocks";

std::string skill(const std::string& id, const std::string& when, const std::string& dos, const std::string& cond = "") {
  return "{ \"id\": \"" + id + "\", \"blocks\": [ { \"when\": \"" + when + "\"" + (cond.empty() ? "" : ", \"if\": [" + cond + "]") +
         ", \"do\": [" + dos + "] } ] }";
}

// A world on the fixture with these skills (JSON list body) defined; `give` hands skill ids to unit ids afterwards.
struct SB {
  Copy c;
  World w;
  static std::string dir(Copy& c, const std::string& skills, const std::string& damage) {
    c.skills(skills);
    if (!damage.empty()) c.swap("game.json", "\"damage\": \"a.str + w.might * vs - b.def\"", "\"damage\": \"" + damage + "\"");
    return c.dir;
  }
  explicit SB(const std::string& skills, const std::string& damage = "") : c("sb_more", kFix), w("field", dir(c, skills, damage)) {}
  Battle& b() { return w.b(); }
  Unit& u(int id) { return b().units[(size_t)id]; }
  void give(int unit, const char* skillId) { u(unit).learned.push_back(w.skill(skillId)); }
  Forecast fc(int from, int to, int fromX = -1, int fromY = -1) {
    Forecast f;
    const Cell at = fromX < 0 ? u(from).cell : b().grid.at(fromX, fromY);
    CHECK(b().forecast(from, to, b().typeOf(u(from)).weapons[0], at, f));
    return f;
  }
};

}  // namespace

// ---- 4. follow-up control ----

TEST(skillblocks_followup_force_and_forbid) {
  SB s(skill("never", "combat", "{ \"followUp\": \"forbid\" }") + "," + skill("always", "combat", "{ \"followUp\": \"force\" }") + "," +
       skill("deny_foe", "combat", "{ \"followUp\": \"forbid\", \"to\": \"foe\" }"));
  CHECK_EQ(s.fc(0, 1).attack.strikes, 2);  // scout speed 8 against guard speed 3: the formula doubles
  s.give(0, "never");
  CHECK_EQ(s.fc(0, 1).attack.strikes, 1);
  s.u(0).learned.clear();
  s.give(1, "deny_foe");  // the guard forbids the scout's follow-up (aimed at its foe)
  CHECK_EQ(s.fc(0, 1).attack.strikes, 1);  // aimed at its foe: the attacker's follow-up is forbidden in this fight
  s.u(1).learned.clear();
  s.give(0, "deny_foe");
  CHECK_EQ(s.fc(0, 1).attack.strikes, 2);  // the scout aims it at the guard: the guard's own count (1) cannot drop below 1
  CHECK_EQ(s.fc(0, 1).counter.strikes, 1);
  s.u(0).learned.clear();
  s.give(1, "always");  // the guard (speed 3) is forced a follow-up in its counter
  CHECK_EQ(s.fc(0, 1).counter.strikes, 2);
  s.give(1, "never");  // both: forbid wins
  CHECK_EQ(s.fc(0, 1).counter.strikes, 1);
}

TEST(skillblocks_followup_forecast_equals_the_fight) {
  SB s(skill("always", "combat", "{ \"followUp\": \"force\" }"));
  s.give(1, "always");
  const Forecast f = s.fc(0, 1);
  CHECK_EQ(f.attack.strikes, 2);
  CHECK_EQ(f.counter.strikes, 2);
  const int hp0 = s.u(0).hp, hp1 = s.u(1).hp;
  CHECK(s.w.apply(Action::attack(0, 1, 0)));
  CHECK_EQ(hp1 - s.u(1).hp, 2 * f.attack.damage);
  CHECK_EQ(hp0 - s.u(0).hp, 2 * f.counter.damage);
}

TEST(skillblocks_follow_shift_and_the_triangle_multiplier_reach_formulas) {
  SB s(skill("quick", "combat", "{ \"followShift\": 4 }") + "," + skill("tri", "combat", "{ \"trianglePercent\": 300 }") + "," +
           skill("tri_half", "combat", "{ \"trianglePercent\": 50 }, { \"followShift\": 1 }"),
       "a.str + w.might * vs - b.def + (a.trianglePercent - 100) / 50");
  // The mage (speed 4) against the guard (speed 3): 4 >= 3 + 5 - shift needs a shift of 4.
  CHECK_EQ(s.fc(4, 1, 1, 0).attack.strikes, 1);
  s.give(4, "quick");
  CHECK_EQ(s.fc(4, 1, 1, 0).attack.strikes, 2);
  s.u(4).learned.clear();
  // The triangle multiplier: damage 4 + 3 - 4 = 3, plus (300 - 100) / 50 = 4.
  const int base = s.fc(4, 1, 1, 0).attack.damage;
  s.give(4, "tri");
  CHECK_EQ(s.fc(4, 1, 1, 0).attack.damage, base + 4);
  CHECK_EQ(s.fc(4, 1, 1, 0).counter.damage, s.fc(4, 1, 1, 0).counter.damage);
  // Both sides have it: each fighter's strikes read its own value (the guard's counter gets the 50 % malus).
  s.give(1, "tri_half");
  const Forecast f = s.fc(4, 1, 1, 0);
  CHECK_EQ(f.attack.damage, base + 4);
  CHECK_EQ(f.counter.damage, 5 + 3 - 1 + (50 - 100) / 50);  // guard: str 5 + might 3 - mage def 1 - 1
}

TEST(skillblocks_follow_up_refusals) {
  Copy c("sb_follow", kFix);
  c.skills(skill("x", "combat", "{ \"followUp\": \"sometimes\" }"));
  CHECK(saysIn(c.gameError(), "\"followUp\" must be \"force\""));
  c.skills(skill("x", "always", "{ \"followUp\": \"force\" }"));
  CHECK(saysIn(c.gameError(), "cannot be used in an \"always\" block"));
  c.skills(skill("x", "combat", "{ \"followShift\": 500 }"));
  CHECK(saysIn(c.gameError(), "from -100 to 100"));
  c.skills(skill("x", "combat", "{ \"followUp\": \"force\", \"to\": \"foe\" }"));
  CHECK_STR(c.gameError(), "");
}

// ---- 6. weapon experience ----

TEST(skillblocks_weapon_xp_percent_multiplies_the_experience_of_a_fight) {
  // Weapon ranks need carried weapons: the ranks fixture (kell, a fighter with a sword, next to the grunt g1).
  auto gained = [](bool skillOn) {
    rankstest::World w("sb_wxp", "duel", [](rankstest::Copy& c) {
      c.put("data/skills.json", "{ \"skills\": [ { \"id\": \"studious\", \"blocks\": [ { \"when\": \"always\", \"do\": [ { \"weaponXpPercent\": 200 } ] } ] } ] }");
    });
    const int g = w.group("sword");
    if (skillOn) w.unit("kell").learned.push_back(w.rig.db.findSkill("studious"));
    const int before = w.unit("kell").weaponXp[(size_t)g];
    CHECK(w.run(Action::attack(w.id("kell"), w.id("g1"), w.weapon("steel_sword"))));
    return w.unit("kell").weaponXp[(size_t)g] - before;
  };
  CHECK_EQ(gained(false), 2);  // the steel sword's "rankXp"
  CHECK_EQ(gained(true), 4);
  Copy c("sb_wxp_bad", "srpg-skillblocks");
  c.skills(skill("x", "always", "{ \"weaponXpPercent\": 0 }"));
  CHECK(saysIn(c.gameError(), "from 1 to 1000"));
}

// ---- 7. single blocks ----

TEST(skillblocks_move_again_but_not_after_attacking) {
  SB s(skill("hit_and_run", "always", "{ \"moveAgain\": { \"notAfterAttack\": true } }") + "," + skill("plain_again", "always", "{ \"moveAgain\": true }"));
  s.give(0, "hit_and_run");
  CHECK(s.w.apply(Action::attack(0, 1, 0)));
  CHECK(s.u(0).done && !s.u(0).acted);  // a fight ends the turn
  // Another action (rescuing a neighbour) leaves the movement that is left.
  SB t(skill("hit_and_run", "always", "{ \"moveAgain\": { \"notAfterAttack\": true } }"));
  t.give(0, "hit_and_run");
  t.u(3).cell = t.b().grid.at(0, 0);
  CHECK(t.w.apply(Action::rescue(0, 3)));
  CHECK(t.u(0).acted && !t.u(0).done);
  // The plain form allows it after a fight too.
  SB p(skill("plain_again", "always", "{ \"moveAgain\": true }"));
  p.give(0, "plain_again");
  CHECK(p.w.apply(Action::attack(0, 1, 0)));
  CHECK(p.u(0).acted && !p.u(0).done);
}

TEST(skillblocks_no_carry_penalty) {
  SB s(skill("steady", "always", "{ \"noCarryPenalty\": true }"));
  const int spd = s.w.db().findStat("spd");
  CHECK_EQ(s.b().stat(s.u(0), spd), 8);
  CHECK(s.w.apply(Action::rescue(0, 3)));
  CHECK_EQ(s.b().stat(s.u(0), spd), 4);  // the game halves the carrier's speed
  SB t(skill("steady", "always", "{ \"noCarryPenalty\": true }"));
  t.give(0, "steady");
  CHECK(t.w.apply(Action::rescue(0, 3)));
  CHECK_EQ(t.b().stat(t.u(0), spd), 8);
}

TEST(skillblocks_cancel_effectiveness) {
  SB s(skill("armoured", "always", "{ \"cancelEffective\": true }"));
  // The lancer's horse spear is 3 times effective against "mounted": 5 + 2 * 3 - 2.
  CHECK_EQ(s.fc(3, 2, 6, 0).attack.damage, 9);
  s.give(2, "armoured");
  CHECK_EQ(s.fc(3, 2, 6, 0).attack.damage, 5);  // x1: 5 + 2 - 2
  // Against a unit with no effectiveness entry nothing changes (the spear's default is 1).
  CHECK_EQ(s.fc(3, 1, 2, 1).attack.damage, 5 + 2 - 4);
}

TEST(skillblocks_counterattack_test_in_strike_blocks) {
  SB s(skill("sniper", "strike", "{ \"damage\": 3 }", "{ \"counterattack\": false }"));
  s.give(4, "sniper");
  // The tome reaches 2 squares; the guard's sword reaches 1: from two squares away nothing answers.
  const Forecast far = s.fc(4, 1, 3, 1);
  CHECK_EQ(far.attack.damage, 4 + 3 - 4 + 3);  // mage str 4 + might 3 - guard def 4, +3 for no counterattack
  const Forecast near = s.fc(4, 1, 1, 0);
  CHECK_EQ(near.attack.damage, 4 + 3 - 4);
  // The inverse.
  SB t(skill("duelist", "strike", "{ \"damage\": 2 }", "{ \"counterattack\": true }"));
  t.give(4, "duelist");
  CHECK_EQ(t.fc(4, 1, 1, 0).attack.damage, 4 + 3 - 4 + 2);
  CHECK_EQ(t.fc(4, 1, 3, 1).attack.damage, 4 + 3 - 4);
  // Real fight equals the forecast.
  const int before = s.u(1).hp;
  CHECK(s.w.apply(Action::move(4, s.b().grid.at(3, 1))));
  CHECK(s.w.apply(Action::attack(4, 1, s.b().typeOf(s.u(4)).weapons[0])));
  CHECK_EQ(before - s.u(1).hp, 4 + 3 - 4 + 3);
  // A combat block cannot ask it.
  Copy c("sb_counter", kFix);
  c.skills(skill("x", "combat", "{ \"damage\": 1 }", "{ \"counterattack\": false }"));
  CHECK(saysIn(c.gameError(), "can only be asked in \"strike\" and \"struck\" blocks"));
}

TEST(skillblocks_carries_and_inventory_tests) {
  SB s(skill("hoarder", "combat", "{ \"damage\": 2 }", "{ \"carries\": \"potion\" }") + "," +
       skill("full_pack", "combat", "{ \"damage\": 3 }", "{ \"inventory\": \"full\" }") + "," +
       skill("light", "combat", "{ \"damage\": 5 }", "{ \"inventory\": { \"op\": \"<=\", \"value\": 1 } }") + "," +
       skill("greedy", "combat", "{ \"damage\": 7 }", "{ \"carries\": \"gem\", \"who\": \"foe\" }"));
  s.give(0, "hoarder");
  s.give(0, "full_pack");
  s.give(0, "light");
  s.give(0, "greedy");
  const int base = 5 + 3 - 4;
  const int potion = s.w.db().findItem("potion"), gem = s.w.db().findItem("gem");
  CHECK_EQ(s.fc(0, 1).attack.damage, base + 5);  // empty pack: "light" only
  s.u(0).items.push_back(ItemStack{potion, 3, false});
  CHECK_EQ(s.fc(0, 1).attack.damage, base + 2 + 5);  // carries a potion; one item is still "at most 1"
  s.u(0).items.push_back(ItemStack{gem, 1, false});
  s.u(0).items.push_back(ItemStack{gem, 1, false});
  CHECK_EQ(s.fc(0, 1).attack.damage, base + 2 + 3);  // three items = full (inventorySize 3), no longer light
  s.u(1).items.push_back(ItemStack{gem, 1, false});
  CHECK_EQ(s.fc(0, 1).attack.damage, base + 2 + 3 + 7);  // the foe carries a gem
  Copy c("sb_carries", kFix);
  c.skills(skill("x", "combat", "{ \"damage\": 1 }", "{ \"carries\": \"nothing\" }"));
  CHECK(saysIn(c.gameError(), "\"carries\" names unknown item \"nothing\""));
  c.skills(skill("x", "combat", "{ \"damage\": 1 }", "{ \"inventory\": \"half\" }"));
  CHECK(saysIn(c.gameError(), "\"inventory\" must be \"full\""));
}

TEST(skillblocks_ignore_a_share_of_the_foes_defence_with_a_formula) {
  // Half the foe's defence is ignored: a combat bonus of -half to the foe's Defense stat, as a formula (a = owner, b = foe).
  SB s(skill("piercing", "combat", "{ \"stats\": { \"def\": \"-math.floor(b.def * 0.5)\" }, \"to\": \"foe\" }"));
  CHECK_EQ(s.fc(0, 1).attack.damage, 5 + 3 - 4);
  s.give(0, "piercing");
  const Forecast f = s.fc(0, 1);
  CHECK_EQ(f.attack.damage, 5 + 3 - 2);
  const int before = s.u(1).hp;
  CHECK(s.w.apply(Action::attack(0, 1, 0)));
  CHECK_EQ(before - s.u(1).hp, 2 * f.attack.damage);  // the scout also doubles
}
