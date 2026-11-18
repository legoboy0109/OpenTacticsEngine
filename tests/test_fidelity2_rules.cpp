// "Fidelity 2": an ability with no effect of its own, the staged fight story command, a range of condition damage, summon follow-ups
// (docs/SRPG.md sections 4 and 9, docs/EVENTS.md "A staged fight"). Game: tests/fidelity2_fixture.h.
#include "fidelity2_fixture.h"

using namespace ote;
using namespace f2;

namespace {

std::string abilityError(const char* name, const std::string& body) {
  Opts o;
  o.extraAbility = "{\"id\":\"x\",\"target\":\"ally\"," + body + "}";
  return error(make(name, o));
}

// A map whose start event runs the commands (a JSON list body).
std::string withStart(const char* name, const std::string& commands, const char* extraGame = "") {
  Opts o;
  o.gameExtra = extraGame;
  o.events = "{\"id\":\"go\",\"when\":{\"battle\":\"start\"},\"do\":[" + commands + "]}";
  return make(name, o);
}

std::string fightJson(const std::string& body) { return "{\"fight\":{\"attacker\":\"hero\",\"defender\":\"bandit\"" + body + "}}"; }

}  // namespace

// ---- item 1: an ability with only inflict ----

TEST(fid2_ability_with_only_inflict_loads) {
  Database db;
  std::string err;
  CHECK(loadGame(make("ab_load"), db, err));
  const AbilityDef& a = db.abilities[(size_t)db.findAbility("inspire")];
  CHECK(a.effect == AbilityEffect::None);
  CHECK(a.inflictCond == db.findCondition("boost") && a.inflictTurns == 4 && a.xp == 20);
  CHECK(a.target == AbilityTarget::Ally);
}

TEST(fid2_ability_needs_an_effect_or_an_inflict) {
  CHECK(says(abilityError("ab_none", "\"xp\":5"), "missing required \"effect\""));
  CHECK(says(abilityError("ab_none2", "\"cost\":\"free\""), "missing required \"effect\""));
  CHECK(says(abilityError("ab_cell", "\"inflict\":{\"condition\":\"boost\"},\"target\":\"cell\""), "needs a target unit"));
  CHECK(says(abilityError("ab_unknown", "\"inflict\":{\"condition\":\"nope\"}"), "nope"));
  CHECK_STR(abilityError("ab_ok", "\"inflict\":{\"condition\":\"boost\"}"), "");
}

TEST(fid2_ability_with_only_inflict_acts_pays_xp_and_rolls_nothing) {
  World w(make("ab_use"));
  const int user = w.id("hero"), target = w.id("pal"), ab = w.ability("inspire");
  const Rng before = w.b().rng;
  CHECK_STR(w.why(Action::useAbility(user, ab, target)), "");
  CHECK(w.run(Action::useAbility(user, ab, target)));
  CHECK(w.sameRng(before));
  CHECK_EQ(w.count(EventType::AbilityUsed), 1);
  CHECK_EQ(w.count(EventType::ConditionApplied), 1);
  CHECK(w.b().hasCondition(w.unit("pal"), w.cond("boost")));
  CHECK_EQ(w.count(EventType::XpGained), 1);
  CHECK_EQ(w.of(EventType::XpGained)[0].b, 20);
  CHECK(w.b().units[(size_t)user].done);  // an ordinary action: it ends the turn
  // The same ally cannot be given what it already has for as long (a shorter one it may be given again).
  w.unit("pal").conditions[0].left = 10;
  w.round();
  CHECK(says(w.why(Action::useAbility(user, ab, target)), "already has"));
  // An enemy is not an ally.
  CHECK(says(w.why(Action::useAbility(user, ab, w.id("bandit"))), "ally"));
}

// ---- item 2: the staged fight ----

TEST(fid2_fight_loader_refusals) {
  CHECK(says(error(withStart("f_r1", fightJson(",\"strikes\":[{\"by\":\"nobody\",\"result\":\"hit\"}]"))), "\"by\" must be \"attacker\" or \"defender\""));
  CHECK(says(error(withStart("f_r2", fightJson(",\"strikes\":[{\"by\":\"attacker\",\"result\":\"graze\"}]"))), "\"result\" must be \"hit\", \"miss\" or \"crit\""));
  CHECK(says(error(withStart("f_r3", fightJson(",\"strikes\":[{\"by\":\"attacker\",\"result\":\"miss\",\"damage\":3}]"))), "a miss does no damage"));
  CHECK(says(error(withStart("f_r4", fightJson(",\"strikes\":[{\"by\":\"attacker\",\"result\":\"hit\",\"damage\":-1}]"))), "\"damage\" must be a whole number from 0 to 9999"));
  CHECK(says(error(withStart("f_r5", fightJson(",\"strikes\":[]"))), "\"strikes\" must be a list of 1 to 24"));
  CHECK(says(error(withStart("f_r6", fightJson(",\"strikes\":[{\"by\":\"attacker\"}]"))), "result"));
  CHECK(says(error(withStart("f_r7", fightJson(",\"weapon\":{\"attacker\":\"nope\"}"))), "unknown weapon \"nope\""));
  CHECK(says(error(withStart("f_r8", "{\"fight\":{\"attacker\":\"hero\",\"defender\":\"hero\"}}")), "cannot fight itself"));
  CHECK(says(error(withStart("f_r9", "{\"fight\":{\"attacker\":\"hero\",\"defender\":\"ghost\"}}")), "unknown unit \"ghost\""));
  CHECK(says(error(withStart("f_r10", "{\"fight\":{\"attacker\":\"hero\"}}")), "missing required \"defender\""));
  CHECK(says(error(withStart("f_r11", fightJson(",\"sparkle\":true"))), "unknown key \"sparkle\""));
  CHECK(says(error(withStart("f_r12", fightJson(",\"lethal\":\"no\""))), "lethal"));
  CHECK_STR(error(withStart("f_ok", fightJson(",\"strikes\":[{\"by\":\"attacker\",\"result\":\"crit\",\"damage\":0}],\"lethal\":false,\"xp\":true,\"wear\":true,"
                                                "\"weapon\":{\"attacker\":\"sword\",\"defender\":\"axe\"}"))), "");
}

TEST(fid2_staged_fight_takes_every_strike_as_written_and_rolls_nothing) {
  // hero hits for 7 normally (atk 8 - def 1); a crit is 21 (x3). bandit has 20 HP.
  World w(withStart("f_exact", fightJson(",\"strikes\":[{\"by\":\"attacker\",\"result\":\"hit\",\"damage\":5},{\"by\":\"defender\",\"result\":\"miss\"},"
                                         "{\"by\":\"defender\",\"result\":\"hit\"},{\"by\":\"attacker\",\"result\":\"hit\",\"damage\":0},"
                                         "{\"by\":\"attacker\",\"result\":\"hit\"}]")),
          false);
  const Rng before = w.b().rng;
  w.b().start(w.events);
  CHECK(w.sameRng(before));
  const std::vector<Event> hits = w.of(EventType::Attack);
  CHECK_EQ((int)hits.size(), 5);
  CHECK(hits[0].a == w.id("hero") && hits[0].b == w.id("bandit") && hits[0].d == 5 && (hits[0].flags & kHit) && !(hits[0].flags & kCounter));
  CHECK(hits[1].a == w.id("bandit") && (hits[1].flags & kCounter) && !(hits[1].flags & kHit) && hits[1].d == 0);
  CHECK(hits[2].a == w.id("bandit") && (hits[2].flags & kHit) && hits[2].d == 3);  // atk 5 - def 2: the normal damage
  CHECK(hits[3].d == 0 && (hits[3].flags & kHit));                                 // damage 0 is allowed
  CHECK(hits[4].d == 7 && !(hits[4].flags & kKill));                               // 20 - 5 - 0 - 7 = 8 left
  CHECK_EQ(w.unit("bandit").hp, 8);
  CHECK_EQ(w.unit("hero").hp, 27);
  CHECK_EQ(w.count(EventType::XpGained), 0);  // no experience unless "xp": true
  // no wear: the iron sword still has its 3 uses
  CHECK_EQ(w.unit("hero").items[0].uses, 3);
  CHECK(w.unit("bandit").alive);
}

TEST(fid2_staged_fight_a_critical_default_and_a_fall) {
  World w(withStart("f_crit", fightJson(",\"strikes\":[{\"by\":\"attacker\",\"result\":\"crit\"},{\"by\":\"defender\",\"result\":\"hit\"}]")), false);
  w.b().start(w.events);
  const std::vector<Event> hits = w.of(EventType::Attack);
  CHECK_EQ((int)hits.size(), 1);  // 21 damage fells the bandit (20 HP): the answer is not played
  CHECK(hits[0].d == 20 && (hits[0].flags & kCrit) && (hits[0].flags & kKill));
  CHECK(!w.unit("bandit").alive);
  CHECK_EQ(w.count(EventType::UnitDied), 1);
}

TEST(fid2_staged_fight_can_spare_pay_experience_and_wear_weapons) {
  World w(withStart("f_flags", fightJson(",\"strikes\":[{\"by\":\"attacker\",\"result\":\"crit\"},{\"by\":\"attacker\",\"result\":\"hit\"}],"
                                          "\"lethal\":false,\"xp\":true,\"wear\":true")),
          false);
  w.b().start(w.events);
  CHECK(w.unit("bandit").alive);
  CHECK_EQ(w.unit("bandit").hp, 1);  // spared at 1 HP
  CHECK_EQ(w.count(EventType::UnitDied), 0);
  CHECK_EQ(w.count(EventType::XpGained), 1);
  CHECK_EQ(w.unit("hero").items[0].uses, 1);  // both strikes wore the sword
}

TEST(fid2_a_fight_without_strikes_uses_the_real_rolls) {
  World w(withStart("f_real", fightJson("")), false);
  const Rng before = w.b().rng;
  w.b().start(w.events);
  CHECK(w.count(EventType::Attack) >= 1);
  CHECK(!w.sameRng(before));  // the dice were rolled
  CHECK(w.of(EventType::Attack)[0].a == w.id("hero"));
  CHECK_EQ(w.count(EventType::XpGained), 0);
  CHECK_EQ(w.unit("hero").items[0].uses, 3);
}

TEST(fid2_a_fight_does_not_set_off_the_fight_trigger_or_end_the_attackers_turn) {
  Opts o;
  o.events = "{\"id\":\"go\",\"when\":{\"battle\":\"start\"},\"do\":[" + fightJson(",\"strikes\":[{\"by\":\"attacker\",\"result\":\"hit\"}]") + "]},"
             "{\"id\":\"watch\",\"when\":{\"fight\":{\"unit\":\"hero\"}},\"do\":[{\"setSwitch\":\"seen\",\"to\":true}]}";
  o.gameExtra = "\"switches\":[{\"id\":\"seen\"}],";
  World w(make("f_trig", o));
  CHECK_EQ(w.b().storyState.switches[0], 0);
  CHECK(!w.unit("hero").done);
}

// ---- item 5: a range of condition damage ----

TEST(fid2_condition_damage_range_loader) {
  Opts o;
  auto with = [&](const char* name, const char* hurt) {
    Opts p = o;
    p.conditions = std::string("{\"id\":\"c\",\"turns\":3,\"blocks\":[{\"when\":\"turnStart\",\"do\":[{\"hurt\":") + hurt + ",\"to\":\"self\"}]}]},{\"id\":\"boost\",\"turns\":3,\"blocks\":[{\"when\":\"always\",\"do\":[{\"stats\":{\"atk\":1}}]}]}";
    return error(make(name, p));
  };
  CHECK_STR(with("c_ok", "[1,3]"), "");
  CHECK_STR(with("c_ok2", "{\"amount\":[2,9],\"lethal\":false}"), "");
  CHECK(says(with("c_r1", "[3,1]"), "highest above the lowest"));
  CHECK(says(with("c_r2", "[2,2]"), "highest above the lowest"));
  CHECK(says(with("c_r3", "[0,3]"), "whole numbers from 1 up"));
  CHECK(says(with("c_r4", "[1,2,3]"), "[lowest, highest]"));
  CHECK(says(with("c_r5", "{\"amount\":5,\"lethal\":1}"), "\"lethal\" must be true or false"));
  CHECK(says(with("c_r6", "{\"amount\":5,\"lethal\":true,\"canKill\":true}"), "not both"));
}

TEST(fid2_condition_damage_range_rolls_each_time_and_a_number_does_not) {
  World w(make("c_roll"));
  Unit& hero = w.unit("hero");
  ConditionState cs;
  cs.condition = w.cond("poison");
  cs.left = 50;
  hero.conditions.push_back(cs);
  int seen[4] = {0, 0, 0, 0};
  for (int round = 0; round < 12; ++round) {
    const int hp = w.unit("hero").hp;
    const Rng before = w.b().rng;
    w.round();
    const int lost = hp - w.unit("hero").hp;
    CHECK(lost >= 1 && lost <= 3);
    if (lost >= 1 && lost <= 3) ++seen[lost];
    (void)before;
  }
  CHECK(seen[1] + seen[2] + seen[3] == 12);
  CHECK((seen[1] > 0) + (seen[2] > 0) + (seen[3] > 0) >= 2);  // it varies
  // A plain number takes no roll: the generator is where it was after the turn change alone.
  World p(make("c_fixed"));
  ConditionState fx;
  fx.condition = p.cond("fixed");
  fx.left = 50;
  p.unit("hero").conditions.push_back(fx);
  World q(make("c_fixed2"));  // the same game without the condition: the same turn changes
  const Rng start = p.b().rng;
  p.round();
  q.round();
  CHECK(p.b().rng.state == q.b().rng.state);
  CHECK_EQ(p.unit("hero").hp, 28);
  (void)start;
}

TEST(fid2_condition_damage_never_kills_unless_lethal) {
  World gentle(make("c_gentle"));
  ConditionState g;
  g.condition = gentle.cond("gentle");
  g.left = 50;
  gentle.unit("hero").hp = 3;
  gentle.unit("hero").conditions.push_back(g);
  gentle.round();
  CHECK(gentle.unit("hero").alive && gentle.unit("hero").hp == 1);  // the default: it leaves 1 HP
  World deadly(make("c_deadly"));
  ConditionState d;
  d.condition = deadly.cond("deadly");
  d.left = 50;
  deadly.unit("hero").hp = 3;
  deadly.unit("hero").conditions.push_back(d);
  deadly.round();
  CHECK(!deadly.unit("hero").alive);  // "lethal": true defeats it
}

// ---- item 6: summon follow-ups ----

TEST(fid2_summon_loader) {
  auto with = [&](const char* name, const char* summon) {
    Opts p;
    p.extraAbility = std::string("{\"id\":\"x\",\"target\":\"cell\",\"effect\":{\"summon\":") + summon + "}}";
    return error(make(name, p));
  };
  CHECK_STR(with("s_ok", "{\"type\":\"hound\",\"level\":\"user\",\"replace\":true,\"items\":[{\"item\":\"fang\",\"minLevel\":3}],\"pick\":\"random\"}"), "");
  CHECK(says(with("s_r1", "{\"type\":\"hound\",\"items\":[]}"), "\"items\" must be a list of 1 to 8"));
  CHECK(says(with("s_r2", "{\"type\":\"hound\",\"items\":[{\"item\":\"nope\"}]}"), "unknown item \"nope\""));
  CHECK(says(with("s_r3", "{\"type\":\"hound\",\"items\":[{\"item\":\"fang\",\"minLevel\":0}]}"), "\"minLevel\" must be a whole number from 1 to"));
  CHECK(says(with("s_r4", "{\"type\":\"hound\",\"pick\":\"highest\"}"), "\"pick\" goes with \"items\""));
  CHECK(says(with("s_r5", "{\"type\":\"hound\",\"items\":[{\"item\":\"fang\"}],\"pick\":\"best\"}"), "\"pick\" must be \"highest\""));
  CHECK(says(with("s_r6", "{\"type\":\"hound\",\"replace\":\"yes\"}"), "replace"));
  CHECK(says(with("s_r7", "{\"type\":\"hound\",\"items\":[{\"item\":\"fang\",\"colour\":1}]}"), "unknown key \"colour\""));
}

TEST(fid2_summon_replace_makes_the_oldest_vanish) {
  World w(make("s_replace"));
  const int user = w.id("hero"), ab = w.ability("call_pet");
  CHECK(w.run(Action::useAbility(user, ab, -1, w.at(1, 0))));
  CHECK_EQ(w.count(EventType::UnitAdded), 1);
  const int first = w.of(EventType::UnitAdded)[0].a;
  w.round();
  CHECK_STR(w.why(Action::useAbility(user, ab, -1, w.at(1, 2))), "");  // at its limit, but "replace" allows it
  CHECK(w.run(Action::useAbility(user, ab, -1, w.at(1, 2))));
  CHECK_EQ(w.count(EventType::UnitRemoved), 1);
  CHECK_EQ(w.of(EventType::UnitRemoved)[0].a, first);
  CHECK_EQ(w.count(EventType::UnitAdded), 1);
  CHECK_EQ(w.b().summonCount(user, ab), 1);
  CHECK(!w.b().units[(size_t)first].alive);
}

TEST(fid2_summon_without_replace_is_still_refused_at_its_limit) {
  World w(make("s_limit"));
  const int user = w.id("hero"), ab = w.ability("call_plain");
  CHECK(w.run(Action::useAbility(user, ab, -1, w.at(1, 0))));
  w.round();
  CHECK(says(w.why(Action::useAbility(user, ab, -1, w.at(1, 2))), "already has 1 summoned Hound"));
}

TEST(fid2_summon_items_pick_the_highest_that_fits_or_a_random_one) {
  // highest: the summoner's level 1 fits only the fang; at level 6 the big fang (minLevel 5) wins.
  for (int level : {1, 6}) {
    World w(make(level == 1 ? "s_hi1" : "s_hi6"));
    w.unit("hero").level = level;
    const Rng before = w.b().rng;
    CHECK(w.run(Action::useAbility(w.id("hero"), w.ability("call_high"), -1, w.at(1, 0))));
    CHECK(w.sameRng(before));  // "highest" rolls nothing
    const Unit& pet = w.b().units[(size_t)w.of(EventType::UnitAdded)[0].a];
    CHECK_EQ((int)pet.items.size(), 1);
    CHECK_EQ(pet.items[0].item, w.db().findItem(level == 1 ? "fang" : "big_fang"));
    CHECK(!pet.items[0].drop);
  }
  // random: with one candidate that fits there is no roll; with two there is one.
  World one(make("s_r1c"));
  Rng before = one.b().rng;
  CHECK(one.run(Action::useAbility(one.id("hero"), one.ability("call_rand"), -1, one.at(1, 0))));  // level 1: only the fang fits
  CHECK(one.sameRng(before));
  World two(make("s_r2c"));
  two.unit("hero").level = 3;
  before = two.b().rng;
  CHECK(two.run(Action::useAbility(two.id("hero"), two.ability("call_rand"), -1, two.at(1, 0))));
  CHECK(!two.sameRng(before));
  const int item = two.b().units[(size_t)two.of(EventType::UnitAdded)[0].a].items[0].item;
  CHECK(item == two.db().findItem("fang") || item == two.db().findItem("big_fang"));
  // a single candidate with "random": no roll either
  World solo(make("s_solo"));
  before = solo.b().rng;
  CHECK(solo.run(Action::useAbility(solo.id("hero"), solo.ability("call_one"), -1, solo.at(1, 0))));
  CHECK(solo.sameRng(before));
}

TEST(fid2_summon_level_follows_the_summoner) {
  World w(make("s_level"));
  w.unit("hero").level = 7;
  CHECK(w.run(Action::useAbility(w.id("hero"), w.ability("call_pet"), -1, w.at(1, 0))));
  CHECK_EQ(w.b().units[(size_t)w.of(EventType::UnitAdded)[0].a].level, 7);
}
