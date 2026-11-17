// Docs/SRPG.md "Skill and ability blocks, second set": the skill effects nullifySkills, "staffUse" blocks, staffRange, the terrain
// list and carryBonus. Fixture tests/fixtures/srpg-skillblocks2 (carried iron swords, 100 % hit, one strike): "thief" (blue rogue,
// str 5) attacking "mark" (red guard, def 4) with an iron sword (might 3) does 5 + 3 - 4 = 4.
#include "srpg_skills_fixture.h"

using namespace ote;
using skilltest::Copy;
using skilltest::World;
using skilltest::saysIn;

namespace {

const char* kFix = "srpg-skillblocks2";

std::string skill(const std::string& id, const std::string& when, const std::string& dos, const std::string& extra = "") {
  return "{ \"id\": \"" + id + "\"," + extra + " \"blocks\": [ { \"when\": \"" + when + "\", \"do\": [" + dos + "] } ] }";
}

// The fixture with these skills; `learn` = story id -> skill ids the unit has learned.
struct Duel {
  Copy c;
  std::unique_ptr<World> w;
  Duel(const char* name, const std::string& skills, const std::vector<std::pair<const char*, const char*>>& learn) : c(name, kFix) {
    c.skills(skills);
    w.reset(new World("field", c.dir));
    for (const auto& l : learn) w->unit(l.first).learned.push_back(w->skill(l.second));
  }
  int weapon(const char* unit) { return w->b().restingWeapon(w->unit(unit)); }
  // The forecast damage of `from` attacking `to` and what the real fight did to `to`.
  int forecast(const char* from, const char* to, bool counter = false) {
    Forecast f;
    CHECK(w->b().forecast(w->id(from), w->id(to), weapon(from), w->unit(from).cell, f));
    return counter ? f.counter.damage : f.attack.damage;
  }
  int fight(const char* from, const char* to, bool counter = false) {
    const int hp = counter ? w->unit(from).hp : w->unit(to).hp;
    CHECK(w->apply(Action::attack(w->id(from), w->id(to), weapon(from))));
    return hp - (counter ? w->unit(from).hp : w->unit(to).hp);
  }
};

const std::string kBase = skill("edge", "combat", "{ \"damage\": 2 }", " \"tags\": [\"bonus\"],") + "," +
                          skill("edge2", "combat", "{ \"damage\": 5 }") + "," +
                          skill("silencer", "combat", "{ \"nullifySkills\": true }") + "," +
                          skill("silence_edge", "combat", "{ \"nullifySkills\": [\"edge\"] }") + "," +
                          skill("silence_tag", "combat", "{ \"nullifySkills\": { \"tag\": \"bonus\" } }") + "," +
                          skill("thick", "always", "{ \"stats\": { \"str\": 3 } }");

}  // namespace

// ---- 4. switch off the foe's skills in a fight ----

TEST(nullify_all_skills_of_the_foe_forecast_equals_the_fight) {
  Duel d("sb2_null_all", kBase, {{"thief", "edge"}});
  CHECK_EQ(d.forecast("thief", "mark"), 6);  // 4 + the edge
  d.w->unit("mark").learned.push_back(d.w->skill("silencer"));
  CHECK_EQ(d.forecast("thief", "mark"), 4);  // the edge is switched off for this fight
  CHECK_EQ(d.fight("thief", "mark"), 4);     // the real strike agrees
}

TEST(nullify_the_counterattack_too_and_only_for_that_fight) {
  Duel d("sb2_null_counter", kBase, {{"mark", "edge2"}, {"thief", "silencer"}});
  CHECK_EQ(d.forecast("thief", "mark", true), 6);  // mark strikes back: 5 + 3 - 2 = 6; its edge2 (+5) is off
  CHECK_EQ(d.fight("thief", "mark", true), 6);
  d.w->unit("thief").learned.clear();
  CHECK_EQ(d.forecast("mark", "thief"), 11);  // switched on again once the nullifier is gone
}

TEST(nullify_listed_skills_and_tagged_skills) {
  {
    Duel d("sb2_null_ids", kBase, {{"thief", "edge"}, {"mark", "silence_edge"}});
    d.w->unit("thief").learned.push_back(d.w->skill("thick"));
    // edge (listed) is off; thick is "always", part of the unit's shown stats, and stays: str 5 + 3 = 8, 8 + 3 - 4 = 7.
    CHECK_EQ(d.forecast("thief", "mark"), 7);
    CHECK_EQ(d.fight("thief", "mark"), 7);
  }
  {
    Duel d("sb2_null_other", kBase, {{"thief", "edge2"}, {"mark", "silence_edge"}});
    CHECK_EQ(d.forecast("thief", "mark"), 9);  // edge2 is not listed: it stays (4 + 5)
  }
  {
    Duel d("sb2_null_tag", kBase, {{"thief", "edge"}, {"mark", "silence_tag"}});
    d.w->unit("thief").learned.push_back(d.w->skill("edge2"));
    CHECK_EQ(d.forecast("thief", "mark"), 9);  // edge (tag "bonus") is off, edge2 (no tag) stays
  }
}

TEST(nullify_when_both_have_it_each_loses_everything_but_the_nullifier) {
  Duel d("sb2_null_both", kBase, {{"thief", "silencer"}, {"mark", "silencer"}});
  d.w->unit("thief").learned.push_back(d.w->skill("edge"));
  d.w->unit("mark").learned.push_back(d.w->skill("edge2"));
  CHECK_EQ(d.forecast("thief", "mark"), 4);
  CHECK_EQ(d.forecast("thief", "mark", true), 6);  // mark strikes back: str 5 + might 3 - def 2, no edge2
  CHECK_EQ(d.fight("thief", "mark"), 4);
}

TEST(nullify_never_switches_off_a_condition_or_an_unrelated_third_unit) {
  Duel d("sb2_null_hidden", kBase, {{"thief", "edge"}, {"mark", "silencer"}});
  ConditionState rage;
  rage.condition = d.w->db().findCondition("rage");
  rage.left = 3;
  d.w->unit("thief").conditions.push_back(rage);
  CHECK_EQ(d.forecast("thief", "mark"), 7);  // the rage's own +3 stays, the edge goes (4 + 3)
  CHECK_EQ(d.fight("thief", "mark"), 7);
}

TEST(nullify_loader_refusals) {
  auto say = [](const std::string& effect, const char* when = "combat") {
    Copy c("sb2_null_bad", kFix);
    c.skills(skill("x", when, effect));
    return c.gameError();
  };
  CHECK(saysIn(say("{ \"nullifySkills\": [\"nobody\"] }"), "\"nullifySkills\" names unknown skill \"nobody\""));
  CHECK(saysIn(say("{ \"nullifySkills\": { \"tag\": \"nothing\" } }"), "\"nullifySkills\" names no skill tag \"nothing\""));
  CHECK(saysIn(say("{ \"nullifySkills\": 3 }"), "\"nullifySkills\" must be true"));
  CHECK(saysIn(say("{ \"nullifySkills\": [] }"), "\"nullifySkills\" must be true"));
  CHECK(saysIn(say("{ \"nullifySkills\": true }", "strike"), "cannot be used in a \"strike\" block (it belongs to: combat)"));
  CHECK(saysIn(say("{ \"nullifySkills\": true }", "always"), "cannot be used in an \"always\" block"));
  Copy c("sb2_null_aura", kFix);
  c.put("data/skills.json", "{ \"skills\": [ { \"id\": \"x\", \"blocks\": [ { \"when\": \"combat\", \"aura\": { \"to\": \"enemies\", \"within\": 2 }, \"do\": [ { \"nullifySkills\": true } ] } ] } ] }");
  CHECK(saysIn(c.gameError(), "cannot be used in an aura block"));
  c.put("data/skills.json", "{ \"skills\": [ { \"id\": \"x\", \"tags\": \"bonus\", \"blocks\": [ { \"when\": \"combat\", \"do\": [ { \"damage\": 1 } ] } ] } ] }");
  CHECK(!c.gameError().empty());  // "tags" must be a list
  CHECK(say("{ \"nullifySkills\": true }").empty());
}

// ---- 5. a "staff used" moment for skills ----

namespace {

ItemStack giveStaff(World& w, const char* item) {
  ItemStack s;
  s.item = w.db().findItem(item);
  s.uses = w.db().items[(size_t)s.item].uses;
  return s;
}

}  // namespace

TEST(staffuse_heals_the_user_after_the_staff_works) {
  Duel d("sb2_staffuse", skill("mender", "staffUse", "{ \"heal\": 3 }") + "," + skill("pct", "staffUse", "{ \"heal\": { \"percent\": 25 } }") + "," +
                             skill("calc", "staffUse", "{ \"heal\": \"a.maxhp / 8\" }"),
         {{"medic", "mender"}});
  World& w = *d.w;
  const int thief = w.id("thief"), medic = w.id("medic");
  w.unit("thief").hp = 10;
  w.unit("medic").hp = 5;
  CHECK(w.apply(Action::useStaff(medic, 0, thief)));
  CHECK_EQ(w.unit("thief").hp, 20);  // the staff's own heal
  CHECK_EQ(w.unit("medic").hp, 8);   // 5 + 3 from the skill
  CHECK(w.unit("medic").done);
  // The order of events: StaffUsed ... UnitHealed (the thief) ... SkillActivated, UnitHealed (the medic).
  int staff = -1, activated = -1;
  for (size_t i = 0; i < w.events.size(); ++i) {
    if (w.events[i].type == EventType::StaffUsed) staff = (int)i;
    if (w.events[i].type == EventType::SkillActivated) activated = (int)i;
  }
  CHECK(staff >= 0 && activated > staff);
  CHECK(w.events[(size_t)activated + 1].type == EventType::UnitHealed);
  CHECK_EQ(w.events[(size_t)activated + 1].a, medic);
}

TEST(staffuse_percent_formula_and_the_staffs_group) {
  Duel d("sb2_staffuse2",
         skill("pct", "staffUse", "{ \"heal\": { \"percent\": 25 } }") + "," + skill("calc", "staffUse", "{ \"heal\": \"a.maxhp / 8\" }") + "," +
             "{ \"id\": \"wandonly\", \"blocks\": [ { \"when\": \"staffUse\", \"if\": [ { \"weapon\": \"wand\" } ], \"do\": [ { \"heal\": 7 } ] } ] },"
             "{ \"id\": \"swordonly\", \"blocks\": [ { \"when\": \"staffUse\", \"if\": [ { \"weapon\": \"sword\" } ], \"do\": [ { \"heal\": 7 } ] } ] }",
         {{"medic", "pct"}});
  World& w = *d.w;
  const int thief = w.id("thief"), medic = w.id("medic");
  w.unit("thief").hp = 10;
  w.unit("medic").hp = 2;
  CHECK(w.apply(Action::useStaff(medic, 0, thief)));
  CHECK_EQ(w.unit("medic").hp, 2 + 4);  // 25 % of 16
  w.b().units[(size_t)medic].learned.clear();
  w.b().units[(size_t)medic].learned.push_back(w.skill("calc"));
  w.unit("medic").done = false;
  w.unit("medic").hp = 2;
  w.unit("thief").hp = 10;
  CHECK(w.apply(Action::useStaff(medic, 0, thief)));
  CHECK_EQ(w.unit("medic").hp, 2 + 2);  // 16 / 8
  // The staff's group is what "weapon" tests: a wand triggers "wandonly", not "swordonly".
  w.unit("medic").learned = {w.skill("wandonly"), w.skill("swordonly")};
  w.unit("medic").items = {giveStaff(w, "wand")};
  w.unit("medic").done = false;
  w.unit("medic").hp = 1;
  w.unit("thief").hp = 10;
  CHECK(w.apply(Action::useStaff(medic, 0, thief)));
  CHECK_EQ(w.unit("medic").hp, 1 + 7);
  CHECK_EQ(w.count(EventType::SkillActivated), 1);
}

TEST(staffuse_loader_refusals) {
  auto say = [](const std::string& effect) {
    Copy c("sb2_staffuse_bad", kFix);
    c.skills(skill("x", "staffUse", effect));
    return c.gameError();
  };
  CHECK(say("{ \"heal\": 3 }").empty());
  CHECK(say("{ \"cure\": \"all\" }").empty());
  CHECK(saysIn(say("{ \"damage\": 3 }"), "\"damage\" cannot be used in a \"staffUse\" block"));
  CHECK(saysIn(say("{ \"heal\": 3, \"to\": \"allies\", \"within\": 1 }"), "not allowed in a \"staffUse\" block"));
  CHECK(saysIn(say("{ \"hurt\": 3 }"), "\"hurt\" cannot be used in a \"staffUse\" block"));
  Copy c("sb2_staffuse_when", kFix);
  c.skills(skill("x", "staffuse", "{ \"heal\": 3 }"));
  CHECK(saysIn(c.gameError(), "\"when\" must be one of"));
}

// ---- 6. staff range bonus ----

TEST(staffrange_raises_the_maximum_in_every_form) {
  Duel d("sb2_staffrange", skill("long_arm", "always", "{ \"staffRange\": 2 }") + "," +
                              skill("wand_arm", "always", "{ \"staffRange\": { \"amount\": 3, \"group\": \"wand\" } }"),
         {});
  World& w = *d.w;
  const int medic = w.id("medic");
  auto reach = [&](const char* item) {
    int lo = 0, hi = 0;
    w.b().staffRange(w.unit("medic"), w.db().findItem(item), lo, hi);
    return lo * 100 + hi;
  };
  CHECK_EQ(reach("mend_staff"), 101);
  CHECK_EQ(reach("far_staff"), 102);
  CHECK_EQ(reach("calc_staff"), 101);
  // The pal at (0,2) is two squares from the medic at (0,0): out of reach, then in reach.
  w.unit("medic").items = {giveStaff(w, "mend_staff")};
  w.unit("pal").hp = 5;
  CHECK(!w.b().validate(Action::useStaff(medic, 0, w.id("pal"))));
  w.unit("medic").learned.push_back(w.skill("long_arm"));
  CHECK_EQ(reach("mend_staff"), 103);  // number form: 1 + 2
  CHECK_EQ(reach("far_staff"), 104);   // [1, 2] form: the maximum goes up
  CHECK_EQ(reach("calc_staff"), 103);  // formula form: its result plus 2
  CHECK_EQ(reach("wand"), 103);
  CHECK(w.b().validate(Action::useStaff(medic, 0, w.id("pal"))));
  // The group form only helps staves of that group.
  w.unit("medic").learned = {w.skill("wand_arm")};
  CHECK_EQ(reach("mend_staff"), 101);
  CHECK_EQ(reach("wand"), 104);
  std::vector<StaffOption> opts;
  w.unit("medic").items = {giveStaff(w, "wand")};
  w.b().staffOptions(medic, w.unit("medic").cell, opts);
  CHECK(!opts.empty() && opts[0].ok);
}

TEST(staffrange_loader_refusals) {
  auto say = [](const std::string& effect) {
    Copy c("sb2_staffrange_bad", kFix);
    c.skills(skill("x", "always", effect));
    return c.gameError();
  };
  CHECK(say("{ \"staffRange\": 2 }").empty());
  CHECK(say("{ \"staffRange\": { \"amount\": 2, \"group\": \"wand\" } }").empty());
  CHECK(saysIn(say("{ \"staffRange\": 0 }"), "\"staffRange\" must be a whole number from 1 to 10"));
  CHECK(saysIn(say("{ \"staffRange\": { \"group\": \"wand\" } }"), "\"staffRange\" must be a whole number from 1 to 10"));
  CHECK(saysIn(say("{ \"staffRange\": { \"amount\": 2, \"group\": \"flute\" } }"), "\"staffRange\" names no staff group \"flute\""));
  CHECK(saysIn(say("{ \"staffRange\": { \"amount\": 2, \"kind\": \"x\" } }"), "unknown key"));
}

// ---- 7. terrain tests ----

TEST(terrain_condition_takes_a_list_of_ids_and_tags) {
  auto damageWith = [](const char* cond, const char* name) {
    Copy c(name, kFix);
    c.skills(std::string("{ \"id\": \"hill\", \"blocks\": [ { \"when\": \"combat\", \"if\": [ { \"terrain\": ") + cond + " } ], \"do\": [ { \"damage\": 2 } ] } ] }");
    c.swap("maps/field.json", "\"tiles\": [\"........\", \"........\", \"........\"]", "\"tiles\": [\"........\", \"w.......\", \"........\"]");
    World w("field", c.dir);
    w.unit("thief").learned.push_back(w.skill("hill"));
    Forecast f;
    CHECK(w.b().forecast(w.id("thief"), w.id("mark"), w.b().restingWeapon(w.unit("thief")), w.unit("thief").cell, f));
    return f.attack.damage;
  };
  CHECK_EQ(damageWith("\"woods\"", "sb2_ter_a"), 6);                 // the old form: an id
  CHECK_EQ(damageWith("\"cover\"", "sb2_ter_b"), 6);                 // a tag
  CHECK_EQ(damageWith("[\"wall\", \"woods\"]", "sb2_ter_c"), 6);     // a list: any one
  CHECK_EQ(damageWith("[\"wall\", \"cover\"]", "sb2_ter_d"), 6);     // ids and tags mixed
  CHECK_EQ(damageWith("[\"wall\", \"plain\"]", "sb2_ter_e"), 4);     // none matches the square the thief stands on
  Copy c("sb2_ter_bad", kFix);
  c.skills("{ \"id\": \"x\", \"blocks\": [ { \"when\": \"combat\", \"if\": [ { \"terrain\": [\"woods\", \"lava\"] } ], \"do\": [ { \"damage\": 2 } ] } ] }");
  CHECK(saysIn(c.gameError(), "\"terrain\" names no terrain id or tag \"lava\""));
  c.skills("{ \"id\": \"x\", \"blocks\": [ { \"when\": \"combat\", \"if\": [ { \"terrain\": [] } ], \"do\": [ { \"damage\": 2 } ] } ] }");
  CHECK(saysIn(c.gameError(), "\"terrain\" must be an id or a tag, or a list of them"));
  c.skills("{ \"id\": \"x\", \"blocks\": [ { \"when\": \"combat\", \"if\": [ { \"terrain\": [\"woods\", 3] } ], \"do\": [ { \"damage\": 2 } ] } ] }");
  CHECK(saysIn(c.gameError(), "\"terrain\" must be an id or a tag, or a list of them"));
}

// ---- 9. carry capacity ----

TEST(carrybonus_is_read_by_the_cancarry_formula) {
  Copy c("sb2_carry", kFix);
  c.skills(skill("strong", "always", "{ \"carryBonus\": 2 }") + "," + skill("weak", "always", "{ \"carryBonus\": -5 }"));
  c.swap("game.json", "\"rules\": {", "\"rules\": { \"rescue\": { \"mode\": \"carry\", \"canCarry\": \"a.carryBonus >= 1\" },");
  World w("field", c.dir);
  const Action rescue = Action::rescue(w.id("thief"), w.id("pal"));
  CHECK_STR(w.refusal(rescue).empty() ? "allowed" : "refused", "refused");  // no skill: a.carryBonus is 0
  w.unit("thief").learned.push_back(w.skill("strong"));
  CHECK_EQ(w.b().carryBonusOf(w.unit("thief")), 2);
  CHECK(w.b().validate(rescue));
  w.unit("thief").learned = {w.skill("weak")};
  CHECK_EQ(w.b().carryBonusOf(w.unit("thief")), -5);
  CHECK(!w.b().validate(rescue));
  Copy bad("sb2_carry_bad", kFix);
  bad.skills(skill("x", "always", "{ \"carryBonus\": 1000 }"));
  CHECK(saysIn(bad.gameError(), "\"carryBonus\" must be a whole number from -100 to 100"));
  bad.skills(skill("x", "combat", "{ \"carryBonus\": 1 }"));
  CHECK(saysIn(bad.gameError(), "cannot be used in a \"combat\" block"));
}
