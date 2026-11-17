// Docs/SRPG.md "Skill blocks, third set", part one: an aura that asks about its receiver, the strike number of a fight, what a staff just healed,
// the terrain test by number, cancelEffective limited to tags, and a staff that gives stats while carried. Fixture tests/fixtures/srpg-skillblocks2
// (carried iron swords, 100 % hit, one strike): "thief" (blue rogue, str 5, 0,1) attacking "mark" (red guard, def 4, 1,1) with an iron sword (might 3)
// does 5 + 3 - 4 = 4; "pal" (0,2) and "medic" (0,0) are blue allies. Part two (steal, recoil, light, the computer opponent): test_srpg_skillblocks3_more.cpp.
#include <functional>
#include <memory>

#include "srpg_skills_fixture.h"

using namespace ote;
using skilltest::Copy;
using skilltest::World;
using skilltest::saysIn;

namespace {

const char* kFix = "srpg-skillblocks2";

// One skill with one block: `extra` is any further keys of the block ("if": [...], "chance": 50, "aura": {...}).
std::string skill(const std::string& id, const std::string& when, const std::string& dos, const std::string& extra = "") {
  return "{ \"id\": \"" + id + "\", \"blocks\": [ { \"when\": \"" + when + "\"," + extra + " \"do\": [" + dos + "] } ] }";
}

// The fixture with these skills, a few files changed, and the units' learned skills (story id -> skill id).
struct Game {
  Copy c;
  std::unique_ptr<World> w;
  Game(const char* name, const std::string& skills, const std::vector<std::pair<const char*, const char*>>& learn,
       const std::function<void(Copy&)>& tweak = nullptr)
      : c(name, kFix) {
    c.skills(skills);
    if (tweak) tweak(c);
    w.reset(new World("field", c.dir));
    for (const auto& l : learn) w->unit(l.first).learned.push_back(w->skill(l.second));
  }
  int weapon(const char* unit) { return w->b().restingWeapon(w->unit(unit)); }
  Forecast forecast(const char* from, const char* to) {
    Forecast f;
    CHECK(w->b().forecast(w->id(from), w->id(to), weapon(from), w->unit(from).cell, f));
    return f;
  }
  // The damage each strike of `from` did in the last fight (the real events), in order.
  std::vector<int> strikes(const char* from) {
    std::vector<int> out;
    for (const Event& e : w->events)
      if (e.type == EventType::Attack && e.a == w->id(from)) out.push_back(e.d);
    return out;
  }
  bool fight(const char* from, const char* to) { return w->apply(Action::attack(w->id(from), w->id(to), weapon(from))); }
};

void twoStrikes(Copy& c) { c.swap("game.json", "\"strikes\": \"1\"", "\"strikes\": \"2\""); }

}  // namespace

// ---- 1. an aura that asks about the receiver ----

TEST(aura_receiver_weapon_group_decides_who_gets_it_forecast_equals_the_fight) {
  Game g("sb3_aura_weapon", skill("banner", "combat", "{ \"damage\": 3 }", " \"aura\": { \"to\": \"allies\", \"within\": 3, \"weapon\": \"sword\" },") + "," +
                                skill("stick_banner", "combat", "{ \"damage\": 3 }", " \"aura\": { \"to\": \"allies\", \"within\": 3, \"weapon\": \"stick\" },"),
         {{"pal", "banner"}});
  CHECK_EQ(g.forecast("thief", "mark").attack.damage, 7);  // the thief wields a sword: 4 + 3
  CHECK(g.fight("thief", "mark"));
  CHECK_EQ(g.strikes("thief")[0], 7);
  g.w->unit("pal").learned = {g.w->skill("stick_banner")};
  CHECK_EQ(g.forecast("thief", "mark").attack.damage, 4);  // a sword is not a stick
  g.w->unit("thief").done = false;
  CHECK(g.fight("thief", "mark"));
  CHECK_EQ(g.strikes("thief")[0], 4);
}

TEST(aura_receiver_weapon_id_and_the_defender_that_has_no_weapon_in_the_fight) {
  Game g("sb3_aura_wid", skill("banner", "combat", "{ \"damage\": 3 }", " \"aura\": { \"to\": \"allies\", \"within\": 3, \"weapon\": \"iron_sword\" },"), {{"pal", "banner"}});
  CHECK_EQ(g.forecast("thief", "mark").attack.damage, 7);  // by the weapon's id
  // An aura for enemies reaches the foe that answers with a weapon; a foe that cannot answer has no weapon in this fight, so a weapon test is false.
  Game h("sb3_aura_foe", skill("pall", "combat", "{ \"damage\": -1 }", " \"aura\": { \"to\": \"enemies\", \"within\": 3, \"weapon\": \"sword\" },"), {{"pal", "pall"}});
  const Forecast f = h.forecast("thief", "mark");
  CHECK_EQ(f.attack.damage, 4);   // the thief is an ally of the owner: nothing for it
  CHECK_EQ(f.counter.damage, 5 + 3 - 2 - 1);  // mark's counter (iron sword) is weakened by the aura of an enemy of its side
}

TEST(aura_receiver_damage_kind_and_a_formula_test) {
  auto kinds = [](Copy& c) { c.swap("data/weapons.json", "\"group\": \"sword\", \"props\": { \"might\": 3 }", "\"group\": \"sword\", \"damageKind\": \"magic\", \"props\": { \"might\": 3 }"); };
  Game g("sb3_aura_kind", skill("kind", "combat", "{ \"damage\": 3 }", " \"aura\": { \"to\": \"allies\", \"within\": 3, \"weaponKind\": \"magic\" },") + "," +
                              skill("tested", "combat", "{ \"hit\": -10 }", " \"aura\": { \"to\": \"allies\", \"within\": 3, \"test\": \"b.str >= 5 and b.tags.thief\" },") + "," +
                              skill("failing", "combat", "{ \"crit\": 50 }", " \"aura\": { \"to\": \"allies\", \"within\": 3, \"test\": \"b.str >= 6\" },"),
         {{"pal", "kind"}}, kinds);
  CHECK_EQ(g.forecast("thief", "mark").attack.damage, 7);
  g.w->unit("pal").learned = {g.w->skill("tested"), g.w->skill("failing")};
  Forecast f = g.forecast("thief", "mark");
  CHECK_EQ(f.attack.hit, 90);   // the test holds for the thief (str 5, a "thief")
  CHECK_EQ(f.attack.crit, 0);   // and not the other: str 5 is below 6
  CHECK_EQ(f.attack.damage, 4);  // the damage aura is not on any more
}

TEST(aura_receiver_tests_loader_refusals) {
  auto say = [](const std::string& aura) {
    Copy c("sb3_aura_bad", kFix);
    c.skills(skill("x", "combat", "{ \"damage\": 1 }", " \"aura\": { \"to\": \"allies\", \"within\": 2" + aura + " },"));
    return c.gameError();
  };
  CHECK(say(", \"weapon\": \"sword\"").empty());
  CHECK(saysIn(say(", \"weapon\": \"lance\""), "\"aura\".\"weapon\" names no weapon id or group \"lance\""));
  CHECK(saysIn(say(", \"weapon\": \"\""), "\"aura\".\"weapon\" must be a weapon id or group"));
  CHECK(saysIn(say(", \"weaponKind\": \"magic\""), "\"aura\".\"weaponKind\" names no weapon damage kind \"magic\""));
  CHECK(saysIn(say(", \"test\": 3"), "\"aura\".\"test\" must be the text of a formula"));
  CHECK(saysIn(say(", \"other\": 1"), "unknown key \"other\""));
  CHECK(say(", \"test\": \"b.str >= 1\"").empty());
}

// ---- 2. which strike of the fight ----

TEST(strike_first_and_followup_blocks_forecast_each_strike_and_the_fight_agrees) {
  Game g("sb3_strike", skill("opener", "strike", "{ \"damage\": 5 }", " \"if\": [ { \"strike\": \"first\" } ],") + "," +
                           skill("follow", "strike", "{ \"damage\": 2 }", " \"if\": [ { \"strike\": \"followUp\" } ],"),
         {{"thief", "opener"}}, twoStrikes);
  Forecast f = g.forecast("thief", "mark");
  CHECK_EQ(f.attack.strikes, 2);
  CHECK_EQ(f.attack.damage, 9);  // the headline number is the first strike's
  CHECK_EQ((int)f.attack.each.size(), 2);
  if (f.attack.each.size() == 2) CHECK(f.attack.each[0].damage == 9 && f.attack.each[1].damage == 4);
  CHECK_EQ(f.attack.totalDamage(), 13);
  CHECK(f.counter.each.empty());  // mark's strikes are all alike
  CHECK(g.fight("thief", "mark"));
  const std::vector<int> d = g.strikes("thief");
  CHECK(d.size() == 2 && d[0] == 9 && d[1] == 4);
  CHECK_EQ(g.w->unit("mark").hp, 30 - 13);
  // The count starts over in the next fight.
  g.w->unit("thief").done = false;
  g.w->unit("thief").learned = {g.w->skill("follow")};
  f = g.forecast("thief", "mark");
  CHECK(f.attack.damage == 4 && f.attack.each.size() == 2 && f.attack.each[1].damage == 6);
  CHECK(g.fight("thief", "mark"));
  const std::vector<int> d2 = g.strikes("thief");
  CHECK(d2.size() == 2 && d2[0] == 4 && d2[1] == 6);
}

TEST(strike_number_n_the_formula_variable_and_struck_blocks) {
  Game g("sb3_strike_n", skill("second", "strike", "{ \"damage\": 5 }", " \"if\": [ { \"strike\": 2 } ],") + "," +
                             skill("calc", "strike", "{ \"damage\": \"strikeNo == 1 and 3 or 0\" }") + "," +
                             skill("late", "strike", "{ \"hit\": -50 }", " \"if\": [ { \"strike\": { \"op\": \">=\", \"value\": 2 } } ],") + "," +
                             skill("guard1", "struck", "{ \"damageTaken\": -2 }", " \"if\": [ { \"strike\": \"first\" } ],") + "," +
                             skill("cond", "strike", "{ \"damage\": 1 }", " \"if\": [ { \"formula\": \"strikeNo == 2\" } ],"),
         {{"thief", "second"}, {"mark", "guard1"}}, twoStrikes);
  Forecast f = g.forecast("thief", "mark");
  CHECK_EQ(f.attack.damage, 2);  // the first strike: 4, and mark's struck block takes 2 off the first one
  CHECK(f.attack.each.size() == 2 && f.attack.each[1].damage == 9 && f.attack.each[1].hit == 100);
  g.w->unit("thief").learned = {g.w->skill("calc"), g.w->skill("cond"), g.w->skill("late")};
  f = g.forecast("thief", "mark");
  CHECK(f.attack.each.size() == 2 && f.attack.each[0].damage == 4 + 3 - 2 && f.attack.each[1].damage == 4 + 1);
  CHECK_EQ(f.attack.each[1].hit, 50);
  CHECK_EQ(f.attack.each[0].hit, 100);
}

TEST(strike_notes_list_an_activation_only_a_later_strike_can_fire_once) {
  Game g("sb3_strike_notes", skill("late_surge", "strike", "{ \"damage\": 5 }", " \"chance\": 40, \"if\": [ { \"strike\": \"followUp\" } ],") + "," +
                                 skill("always_surge", "strike", "{ \"damage\": 1 }", " \"chance\": 25,"),
         {{"thief", "late_surge"}}, twoStrikes);
  g.w->unit("thief").learned.push_back(g.w->skill("always_surge"));
  const Forecast f = g.forecast("thief", "mark");
  int late = 0, always = 0;
  for (const ForecastNote& n : f.notes) {
    if (n.skill == g.w->skill("late_surge")) { ++late; CHECK_EQ(n.chance, 40); }
    if (n.skill == g.w->skill("always_surge")) { ++always; CHECK_EQ(n.chance, 25); }
  }
  CHECK_EQ(late, 1);
  CHECK_EQ(always, 1);
}

TEST(strike_number_extra_strikes_count_and_a_game_without_it_is_unchanged) {
  // "strikeAgain" gives an extra strike that is the next number: a first-strike bonus is not given twice.
  Game g("sb3_strike_extra", skill("opener", "strike", "{ \"damage\": 5 }", " \"if\": [ { \"strike\": \"first\" } ],") + "," +
                                 skill("again", "strike", "{ \"strikeAgain\": true }", " \"if\": [ { \"strike\": \"first\" } ],"),
         {{"thief", "opener"}});
  g.w->unit("thief").learned.push_back(g.w->skill("again"));
  CHECK(g.fight("thief", "mark"));
  const std::vector<int> d = g.strikes("thief");
  CHECK(d.size() == 2 && d[0] == 9 && d[1] == 4);
  // A game that never asks: the forecast has no per-strike list at all.
  Game h("sb3_strike_none", skill("plain", "strike", "{ \"damage\": 5 }"), {{"thief", "plain"}}, twoStrikes);
  const Forecast f = h.forecast("thief", "mark");
  CHECK(f.attack.each.empty());
  CHECK_EQ(f.attack.totalDamage(), 18);
}

TEST(strike_condition_loader_refusals) {
  auto say = [](const std::string& when, const std::string& cond) {
    Copy c("sb3_strike_bad", kFix);
    c.skills(skill("x", when, when == "struck" ? "{ \"damageTaken\": 1 }" : "{ \"damage\": 1 }", " \"if\": [ { \"strike\": " + cond + " } ],"));
    return c.gameError();
  };
  CHECK(say("strike", "\"first\"").empty());
  CHECK(say("struck", "3").empty());
  CHECK(say("strike", "{ \"op\": \"<=\", \"value\": 2 }").empty());
  CHECK(saysIn(say("combat", "\"first\""), "\"strike\" can only be asked in \"strike\" and \"struck\" blocks"));
  CHECK(saysIn(say("always", "\"first\""), "\"strike\" can only be asked in"));
  CHECK(saysIn(say("strike", "\"last\""), "\"strike\" must be \"first\", \"followUp\""));
  CHECK(saysIn(say("strike", "0"), "\"strike\" must be \"first\", \"followUp\""));
  CHECK(saysIn(say("strike", "{ \"op\": \"<=\" }"), "\"value\""));
}

// ---- 3. what a staff just healed ----

TEST(staffuse_formulas_read_the_amount_healed_and_the_target) {
  Game g("sb3_healed", skill("half", "staffUse", "{ \"heal\": \"healed / 2\" }") + "," +
                           skill("big", "staffUse", "{ \"heal\": 3 }", " \"if\": [ { \"formula\": \"healed >= 10\" } ],") + "," +
                           skill("target", "staffUse", "{ \"heal\": \"b.str\" }"),
         {{"medic", "half"}});
  World& w = *g.w;
  const int thief = w.id("thief"), medic = w.id("medic");
  w.unit("thief").hp = 10;
  w.unit("medic").hp = 2;
  CHECK(w.apply(Action::useStaff(medic, 0, thief)));
  CHECK_EQ(w.unit("thief").hp, 20);  // the staff healed 10
  CHECK_EQ(w.unit("medic").hp, 2 + 5);  // half of it
  // Only what the staff really restored counts: the thief was missing 5 hit points.
  w.unit("medic").done = false;
  w.unit("medic").hp = 2;
  w.unit("thief").hp = 15;
  CHECK(w.apply(Action::useStaff(medic, 0, thief)));
  CHECK_EQ(w.unit("medic").hp, 2 + 2);  // 5 / 2, rounded down
  // A condition on the amount, and `b` = the unit the staff was used on.
  w.unit("medic").learned = {w.skill("big"), w.skill("target")};
  w.unit("medic").done = false;
  w.unit("medic").hp = 1;
  w.unit("thief").hp = 5;  // healed = 10 (up to the staff's 10)
  CHECK(w.apply(Action::useStaff(medic, 0, thief)));
  CHECK_EQ(w.unit("medic").hp, 1 + 3 + 5);  // "big" (healed >= 10) and "target" (the thief's strength, 5)
  w.unit("medic").done = false;
  w.unit("medic").hp = 1;
  w.unit("thief").hp = 18;  // healed = 2: "big" does not hold
  CHECK(w.apply(Action::useStaff(medic, 0, thief)));
  CHECK_EQ(w.unit("medic").hp, 1 + 5);
}

TEST(staffuse_healed_is_zero_in_other_blocks_and_loads_everywhere) {
  Copy c("sb3_healed_else", kFix);
  c.skills(skill("x", "combat", "{ \"damage\": \"healed + 2\" }"));
  CHECK(c.gameError().empty());
  World w("field", c.dir);
  w.unit("thief").learned.push_back(w.skill("x"));
  Forecast f;
  CHECK(w.b().forecast(w.id("thief"), w.id("mark"), w.b().restingWeapon(w.unit("thief")), w.unit("thief").cell, f));
  CHECK_EQ(f.attack.damage, 4 + 2);
}

// ---- 8. the terrain test by number ----

TEST(terrain_numbers_are_read_by_the_formula_test) {
  auto board = [](bool woods) {
    return [woods](Copy& c) {
      c.put("data/terrain.json", "{ \"terrain\": [ { \"id\": \"plain\", \"name\": \"Plain\", \"props\": { \"defense\": 0 } }, { \"id\": \"wall\", \"name\": \"Wall\", \"moveCost\": { \"foot\": 0 } },"
                                 " { \"id\": \"woods\", \"name\": \"Woods\", \"tags\": [\"cover\"], \"props\": { \"defense\": 1, \"avoid\": 20 } } ] }");
      if (woods) c.swap("maps/field.json", "\"tiles\": [\"........\", \"........\"", "\"tiles\": [\"........\", \"w.......\"");
    };
  };
  const std::string cover = skill("cover", "combat", "{ \"damage\": 3 }", " \"if\": [ { \"formula\": \"(ta.avoid or 0) > 0\" } ],") + "," +
                            skill("defended", "combat", "{ \"hit\": -10 }", " \"if\": [ { \"formula\": \"ta.defense >= 1\" } ],");
  Game woods("sb3_terrain_w", cover, {{"thief", "cover"}}, board(true));
  woods.w->unit("thief").learned.push_back(woods.w->skill("defended"));
  Forecast f = woods.forecast("thief", "mark");
  CHECK_EQ(f.attack.damage, 4 + 3);  // standing on woods (avoid 20)
  CHECK_EQ(f.attack.hit, 90);        // and its defense is 1
  Game plain("sb3_terrain_p", cover, {{"thief", "cover"}}, board(false));
  plain.w->unit("thief").learned.push_back(plain.w->skill("defended"));
  f = plain.forecast("thief", "mark");
  CHECK_EQ(f.attack.damage, 4);      // plain: avoid is not set (`or 0`), defense 0
  CHECK_EQ(f.attack.hit, 100);
  CHECK(plain.fight("thief", "mark"));
  CHECK_EQ(plain.strikes("thief")[0], 4);
  // The foe's square is `tb`: a block can ask about the square the foe stands on.
  Game foe("sb3_terrain_foe", skill("flush", "combat", "{ \"damage\": 2 }", " \"if\": [ { \"formula\": \"(tb.avoid or 0) > 0\" } ],"), {{"thief", "flush"}}, [&](Copy& c) {
    board(false)(c);
    c.swap("maps/field.json", "\"tiles\": [\"........\", \"........\"", "\"tiles\": [\"........\", \".w......\"");
  });
  CHECK_EQ(foe.forecast("thief", "mark").attack.damage, 4 + 2);
}

// ---- 7. cancelEffective limited to tags ----

namespace {

// Weapon "iron_sword" is 3x against "guard" (the unit type) and 2x against "armored" (its tag); "steel_sword" is plain.
void effective(Copy& c) {
  c.swap("data/weapons.json", "\"group\": \"sword\", \"props\": { \"might\": 3 } }", "\"group\": \"sword\", \"props\": { \"might\": 3 }, \"versus\": { \"guard\": 3, \"armored\": 2, \"healer\": 4 } }");
}

}  // namespace

TEST(cancel_effective_with_a_list_cancels_only_those_tags) {
  Game g("sb3_cancel", skill("all", "always", "{ \"cancelEffective\": true }") + "," + skill("armor", "always", "{ \"cancelEffective\": [\"armored\"] }") + "," +
                           skill("kind", "always", "{ \"cancelEffective\": [\"guard\", \"healer\"] }"),
         {}, effective);
  // The "guard" key matches first (the unit type's id): 3 * 4... damage = 5 + 3 * 3 - 4 = 10.
  CHECK_EQ(g.forecast("thief", "mark").attack.damage, 10);
  g.w->unit("mark").learned = {g.w->skill("all")};
  CHECK_EQ(g.forecast("thief", "mark").attack.damage, 4);  // true: every effectiveness
  g.w->unit("mark").learned = {g.w->skill("armor")};
  CHECK_EQ(g.forecast("thief", "mark").attack.damage, 10);  // the 3x came from "guard", not "armored": not cancelled
  g.w->unit("mark").learned = {g.w->skill("kind")};
  CHECK_EQ(g.forecast("thief", "mark").attack.damage, 4);   // "guard" is listed
  CHECK(g.fight("thief", "mark"));
  CHECK_EQ(g.strikes("thief")[0], 4);  // the fight agrees
}

TEST(cancel_effective_matches_a_tag_when_the_unit_type_is_not_a_key) {
  Game g("sb3_cancel_tag", skill("armor", "always", "{ \"cancelEffective\": [\"armored\"] }"), {{"mark", "armor"}}, [](Copy& c) {
    c.swap("data/weapons.json", "\"group\": \"sword\", \"props\": { \"might\": 3 } }", "\"group\": \"sword\", \"props\": { \"might\": 3 }, \"versus\": { \"armored\": 2, \"guard\": 1 } }");
  });
  // The type "guard" is a key (1x), so that is what applies: nothing to cancel, 5 + 3 - 4 = 4.
  CHECK_EQ(g.forecast("thief", "mark").attack.damage, 4);
  Game h("sb3_cancel_tag2", skill("armor", "always", "{ \"cancelEffective\": [\"armored\"] }"), {{"mark", "armor"}}, [](Copy& c) {
    c.swap("data/weapons.json", "\"group\": \"sword\", \"props\": { \"might\": 3 } }", "\"group\": \"sword\", \"props\": { \"might\": 3 }, \"versus\": { \"armored\": 2 } }");
  });
  // Only the tag "armored" is a key: 5 + 3 * 2 - 4 = 7 without the skill, 4 with it.
  CHECK_EQ(h.forecast("thief", "mark").attack.damage, 4);
  h.w->unit("mark").learned.clear();
  CHECK_EQ(h.forecast("thief", "mark").attack.damage, 7);
}

TEST(cancel_effective_loader_refusals) {
  auto say = [](const std::string& value) {
    Copy c("sb3_cancel_bad", kFix);
    effective(c);
    c.skills(skill("x", "always", "{ \"cancelEffective\": " + value + " }"));
    return c.gameError();
  };
  CHECK(say("true").empty());
  CHECK(say("[\"armored\", \"healer\"]").empty());
  CHECK(saysIn(say("[\"dragon\"]"), "\"cancelEffective\" names no effectiveness tag \"dragon\" (a key of some weapon's \"versus\")"));
  CHECK(saysIn(say("[]"), "\"cancelEffective\" must be true"));
  CHECK(saysIn(say("false"), "\"cancelEffective\" must be true"));
  CHECK(saysIn(say("[3]"), "\"cancelEffective\" must be true"));
}

// ---- 6. a staff that gives stats while carried ----

TEST(a_staff_can_give_stats_while_it_is_carried) {
  auto bonus = [](Copy& c) {
    c.swap("data/items.json", "{ \"id\": \"mend_staff\", \"name\": \"Mend Staff\", \"kind\": \"staff\",", "{ \"id\": \"mend_staff\", \"name\": \"Mend Staff\", \"kind\": \"staff\", \"bonus\": { \"str\": 2, \"hp\": 4 },");
  };
  Game g("sb3_staff_bonus", "", {}, bonus);
  World& w = *g.w;
  const Unit& medic = w.unit("medic");
  CHECK_EQ(w.stat(medic, "str"), 2 + 2);
  CHECK_EQ(w.b().maxHp(medic), 16 + 4);
  // Used up or dropped, the bonus goes with it; two of the same staff count once, like accessories.
  w.unit("medic").items.push_back(w.unit("medic").items[0]);
  CHECK_EQ(w.stat(w.unit("medic"), "str"), 4);
  w.unit("medic").items.clear();
  CHECK_EQ(w.stat(w.unit("medic"), "str"), 2);
  // The staff works as ever.
  Copy c("sb3_staff_bonus_use", kFix);
  bonus(c);
  World v("field", c.dir);
  v.unit("thief").hp = 10;
  CHECK(v.apply(Action::useStaff(v.id("medic"), 0, v.id("thief"))));
  CHECK_EQ(v.unit("thief").hp, 20);
}

TEST(a_staff_bonus_loader_refusals) {
  auto say = [](const std::string& key, const std::string& extra = "") {
    Copy c("sb3_staff_bonus_bad", kFix);
    c.swap("data/items.json", "{ \"id\": \"mend_staff\", \"name\": \"Mend Staff\", \"kind\": \"staff\",", "{ \"id\": \"mend_staff\", \"name\": \"Mend Staff\", \"kind\": \"staff\", " + key + extra + ",");
    return c.gameError();
  };
  CHECK(say("\"bonus\": { \"str\": 1 }").empty());
  CHECK(saysIn(say("\"bonus\": { \"nerve\": 1 }"), "nerve"));
  CHECK(saysIn(say("\"bonus\": { \"str\": 0 }"), "a staff's \"bonus\" must change at least one stat"));
  Copy c("sb3_weapon_bonus", kFix);
  c.swap("data/items.json", "{ \"id\": \"iron_sword\", \"name\": \"Iron Sword\", \"kind\": \"weapon\",", "{ \"id\": \"iron_sword\", \"name\": \"Iron Sword\", \"kind\": \"weapon\", \"bonus\": { \"str\": 1 },");
  CHECK(saysIn(c.gameError(), "\"bonus\" belongs to items of kind \"accessory\""));
}
