// Docs/SRPG.md "More skill blocks": the general blocks added after the first skill set. Each section: the rule, the forecast equal to
// the real fight where it matters, the loader's refusals. A duel (tests/srpg_skills_duel.h): ua attacks ub for 4 damage, 100 % hit.
#include "core/ai/ai.h"
#include "core/script/formulas.h"
#include "srpg_skills_duel.h"

using namespace ote;
using namespace duel;
using skilltest::Copy;
using skilltest::oneBlock;
using skilltest::saysIn;

namespace {

std::string C(const std::string& effects, const std::string& cond = "") { return block("combat", effects, cond); }

// A one-block skill "x" in a fixture copy: the game's load message.
std::string refusal(const std::string& blk, const char* name = "sb_refuse") {
  Copy c(name);
  c.skills(oneBlock(blk));
  return c.gameError();
}

}  // namespace

// ---- 1a. auras limited to a kind of unit; character tags ----

TEST(skillblocks_aura_match_reaches_only_units_with_the_tag) {
  Options o;
  o.aTags = "\"scout\"";
  o.cTags = "\"scout\"";
  o.extraUnits = ", { \"type\": \"uc\", \"side\": \"blue\", \"x\": 0, \"y\": 0 }";
  o.aSkills = "";
  Duel d(skill("rally", "{ \"when\": \"combat\", \"aura\": { \"to\": \"allies\", \"within\": 2, \"match\": \"scout\" }, "
                        "\"do\": [ { \"damage\": 3 } ] }") +
             "," + skill("none", C("{ \"damage\": 0 }")),
         o);
  // uc (unit 2) owns the aura: give it the skill after loading (its type has none), then ua (a scout) is helped.
  d.u(2).learned.push_back(d.skill("rally"));
  CHECK_EQ(d.forecast().attack.damage, 4 + 3);
  CHECK_EQ(d.forecast().counter.damage, 4);  // the enemy is not an ally
  // Real fight equals the forecast.
  const int hpBefore = d.hp(1);
  CHECK(d.attack());
  CHECK_EQ(hpBefore - d.hp(1), 7);
}

TEST(skillblocks_aura_match_skips_units_without_the_tag) {
  Options o;
  o.cTags = "\"scout\"";  // the owner has the tag, ua does not: "match" is about the receiver
  o.extraUnits = ", { \"type\": \"uc\", \"side\": \"blue\", \"x\": 0, \"y\": 0 }";
  Duel d(skill("rally", "{ \"when\": \"combat\", \"aura\": { \"to\": \"allies\", \"within\": 2, \"match\": \"scout\" }, "
                        "\"do\": [ { \"damage\": 3 } ] }"),
         o);
  d.u(2).learned.push_back(d.skill("rally"));
  CHECK_EQ(d.forecast().attack.damage, 4);
}

TEST(skillblocks_character_tags_match_like_type_tags) {
  Copy c("sb_chartags");
  c.swap("data/characters.json", "{ \"id\": \"gorm\", \"name\": \"Gorm\", \"type\": \"grunt\" }",
         "{ \"id\": \"gorm\", \"name\": \"Gorm\", \"type\": \"grunt\", \"tags\": [\"dwarf\"] }");
  c.skills(skill("tagged", C("{ \"damage\": 2 }", "{ \"unit\": \"dwarf\", \"who\": \"foe\" }")));
  CHECK_STR(c.gameError(), "");
  // The resolved reference check accepts a character tag.
  c.skills(skill("tagged", C("{ \"damage\": 2 }", "{ \"unit\": \"no_such_tag\" }")));
  CHECK(saysIn(c.gameError(), "names no unit type id or tag"));
}

TEST(skillblocks_aura_match_refusals) {
  CHECK(saysIn(refusal("{ \"when\": \"combat\", \"aura\": { \"to\": \"allies\", \"match\": \"\" }, \"do\": [ { \"damage\": 1 } ] }"),
               "\"aura\".\"match\" must be a unit type id or tag"));
  CHECK(saysIn(refusal("{ \"when\": \"combat\", \"aura\": { \"to\": \"allies\", \"match\": \"nobody\" }, \"do\": [ { \"damage\": 1 } ] }"),
               "\"aura\".\"match\" names no unit type id or tag \"nobody\""));
}

// ---- 1b. the kind of damage a weapon deals ----

TEST(skillblocks_weapon_kind_tests_own_and_foe_weapon) {
  Options o;
  o.extraWeapons = "{ \"id\": \"fire_tome\", \"name\": \"Fire Tome\", \"damageKind\": \"magic\" },";
  o.aWeapon = "fire_tome";
  Options p = o;
  p.aSkills = "\"mage\"";
  p.dSkills = "\"ward\"";
  Duel e(skill("mage", C("{ \"damage\": 5 }", "{ \"weaponKind\": \"magic\" }")) + "," +
             skill("ward", C("{ \"damageTaken\": -2 }", "{ \"weaponKind\": \"magic\", \"who\": \"foe\" }")),
         p);
  const Forecast f = e.forecast();
  CHECK_EQ(f.attack.damage, 4 + 5 - 2);  // its own magic bonus, the defender's ward against a magic foe
  CHECK_EQ(f.counter.damage, 4);         // the defender's sword has no kind
  const int before = e.hp(1);
  CHECK(e.attack());
  CHECK_EQ(before - e.hp(1), 7);
}

TEST(skillblocks_weapon_kind_is_visible_to_formulas_and_refused_when_unknown) {
  CHECK(saysIn(refusal(C("{ \"damage\": 1 }", "{ \"weaponKind\": \"nothing\" }")), "names no weapon damage kind \"nothing\""));
  CHECK(saysIn(refusal(C("{ \"damage\": 1 }", "{ \"weaponKind\": 3 }")), "must be a damage kind"));
  Options o;
  o.extraWeapons = "{ \"id\": \"fire_tome\", \"name\": \"Fire Tome\", \"damageKind\": \"magic\" },";
  o.aWeapon = "fire_tome";
  o.aSkills = "\"f\"";
  Duel d(skill("f", C("{ \"damage\": \"w.damageKind == 'magic' and 6 or 0\" }")), o);
  CHECK_EQ(d.forecast().attack.damage, 10);
}

// ---- 2. turn-number tests and `turn` in formulas ----

TEST(skillblocks_turn_tests) {
  struct Row { const char* cond; int turn; bool holds; };
  const Row rows[] = {
      {"{ \"turn\": { \"odd\": true } }", 1, true},        {"{ \"turn\": { \"odd\": true } }", 2, false},
      {"{ \"turn\": { \"even\": true } }", 2, true},       {"{ \"turn\": { \"even\": true } }", 3, false},
      {"{ \"turn\": { \"from\": 3, \"to\": 5 } }", 2, false}, {"{ \"turn\": { \"from\": 3, \"to\": 5 } }", 3, true},
      {"{ \"turn\": { \"from\": 3, \"to\": 5 } }", 5, true},  {"{ \"turn\": { \"from\": 3, \"to\": 5 } }", 6, false},
      {"{ \"turn\": { \"from\": 4 } }", 99, true},         {"{ \"turn\": { \"to\": 2 } }", 3, false},
      {"{ \"turn\": { \"every\": 3 } }", 6, true},         {"{ \"turn\": { \"every\": 3 } }", 4, false},
      {"{ \"turn\": { \"every\": 3, \"offset\": 1 } }", 4, true}, {"{ \"turn\": { \"every\": 3, \"offset\": 1 } }", 1, true},
      {"{ \"turn\": { \"every\": 3, \"offset\": 1 } }", 3, false},
  };
  for (const Row& r : rows) {
    Options o;
    o.aSkills = "\"t\"";
    Duel d(skill("t", C("{ \"damage\": 3 }", r.cond)), o);
    d.b().round = r.turn;
    const int want = r.holds ? 7 : 4;
    CHECK_EQ(d.forecast().attack.damage, want);
    if (d.forecast().attack.damage != want) std::printf("  %s at turn %d\n", r.cond, r.turn);
  }
}

TEST(skillblocks_turn_in_the_defender_and_strike_blocks) {
  Options o;
  o.dSkills = "\"s\"";
  Duel d(skill("s", block("struck", "{ \"damageTaken\": -1 }", "{ \"turn\": { \"even\": true } }")), o);
  d.b().round = 2;
  CHECK_EQ(d.forecast().attack.damage, 3);
  d.b().round = 3;
  CHECK_EQ(d.forecast().attack.damage, 4);
}

TEST(skillblocks_turn_tests_are_refused_when_wrong) {
  CHECK(saysIn(refusal(C("{ \"damage\": 1 }", "{ \"turn\": { } }")), "needs exactly one of"));
  CHECK(saysIn(refusal(C("{ \"damage\": 1 }", "{ \"turn\": { \"odd\": true, \"even\": true } }")), "needs exactly one of"));
  CHECK(saysIn(refusal(C("{ \"damage\": 1 }", "{ \"turn\": { \"odd\": false } }")), "must be true"));
  CHECK(saysIn(refusal(C("{ \"damage\": 1 }", "{ \"turn\": { \"from\": 5, \"to\": 2 } }")), "\"from\" is later than \"to\""));
  CHECK(saysIn(refusal(C("{ \"damage\": 1 }", "{ \"turn\": { \"every\": 3, \"offset\": 3 } }")), "\"offset\" must be less than \"every\""));
  CHECK(saysIn(refusal(C("{ \"damage\": 1 }", "{ \"turn\": { \"offset\": 1 } }")), "\"offset\" needs \"every\""));
  CHECK(saysIn(refusal(C("{ \"damage\": 1 }", "{ \"turn\": { \"every\": 1 } }")), "from 2 to 1000"));
  CHECK(saysIn(refusal(C("{ \"damage\": 1 }", "{ \"turn\": { \"odd\": true }, \"who\": \"foe\" }")), "cannot have \"who\""));
  CHECK_STR(refusal(block("always", "{ \"stats\": { \"atk\": 1 } }", "{ \"turn\": { \"odd\": true } }")), "");  // allowed in every block
}

// ---- 5. formula-sized combat bonuses (and `turn` as a formula variable) ----

TEST(skillblocks_formula_amounts_scale_with_the_turn_and_the_foe) {
  Options o;
  o.aSkills = "\"grow\"";
  Duel d(skill("grow", C("{ \"damage\": \"turn\" }, { \"hit\": \"-b.atk\" }, { \"crit\": \"a.atk - b.def\" }")), o);
  d.b().round = 3;
  Forecast f = d.forecast();
  CHECK_EQ(f.attack.damage, 4 + 3);
  CHECK_EQ(f.attack.hit, 100 - 6);
  CHECK_EQ(f.attack.crit, 4);
  d.b().round = 6;
  CHECK_EQ(d.forecast().attack.damage, 4 + 6);
}

TEST(skillblocks_formula_amounts_for_every_combat_number) {
  Options o;
  o.aSkills = "\"all\"";
  o.dSkills = "\"def\"";
  Duel d(skill("all", C("{ \"damagePercent\": \"100 + a.atk * 10\" }, { \"stats\": { \"atk\": \"turn\" } }")) + "," +
             skill("def", C("{ \"avoid\": \"b.skl / 4\" }, { \"critAvoid\": \"2\" }, { \"damageTaken\": \"-1\" }, { \"damageTakenPercent\": \"50 + 50\" }")),
         o);
  d.b().round = 2;
  const Forecast f = d.forecast();
  // atk 6 + 2 = 8, minus def 2 = 6, -1 taken = 5, then 100 + 6 * 10 = 160 % -> 8 (the formula reads atk without this combat's bonus: 6).
  CHECK_EQ(f.attack.damage, 8);
  CHECK_EQ(f.attack.hit, 100 - 5);  // the defender's avoid = the attacker's skl 20 / 4
  CHECK_EQ(f.attack.crit, 0);
}

TEST(skillblocks_formula_amounts_in_strike_blocks_and_auras_equal_the_fight) {
  Options o;
  o.aSkills = "\"sk\"";
  o.extraUnits = ", { \"type\": \"uc\", \"side\": \"blue\", \"x\": 0, \"y\": 0 }";
  Duel d(skill("sk", block("strike", "{ \"damage\": \"turn * 2\" }")) + "," +
             skill("aura", "{ \"when\": \"combat\", \"aura\": { \"to\": \"allies\", \"within\": 2 }, \"do\": [ { \"hit\": \"-b.atk\" } ] }"),
         o);
  d.u(2).learned.push_back(d.skill("aura"));
  d.b().round = 3;
  const Forecast f = d.forecast();
  CHECK_EQ(f.attack.damage, 4 + 6);  // a strike block without a chance is part of the forecast
  CHECK_EQ(f.attack.hit, 100 - 6);   // the aura's formula sees the receiving fighter as `b`: ua's atk 6
  const int before = d.hp(1);
  CHECK(d.attack());
  CHECK_EQ(before - d.hp(1), f.attack.damage);
}

TEST(skillblocks_formula_amount_refusals) {
  CHECK(saysIn(refusal(C("{ \"damage\": \"\" }")), "formula must not be empty"));
  {  // a syntax error is found when the formulas compile, and names the place
    Copy c("sb_syntax");
    c.skills(oneBlock(C("{ \"damage\": \"1 +\" }")));
    Database db;
    std::string err;
    CHECK(loadGame(c.dir, db, err));
    Formulas f;
    CHECK(!f.init(db, err));
    CHECK(saysIn(err, "data/skills.json: skill \"x\": block 1: effect 1: \"damage\""));
  }
  CHECK(saysIn(refusal(block("always", "{ \"stats\": { \"atk\": \"turn\" } }")), "must be a whole number"));
  CHECK_STR(refusal(C("{ \"stats\": { \"atk\": \"turn\" } }")), "");
  CHECK(saysIn(refusal(C("{ \"strikes\": \"1\" }")), "must be a whole number"));  // only combat numbers take a formula
}

// ---- the computer opponent plays whole battles with the new blocks ----

TEST(skillblocks_the_ai_plays_battles_with_every_new_block) {
  const std::string general =
      skill("g1", "{ \"when\": \"combat\", \"if\": [ { \"turn\": { \"even\": true } }, { \"inventory\": \"notFull\" } ], "
                  "\"do\": [ { \"damage\": \"turn\" }, { \"hit\": \"-b.atk\" }, { \"followUp\": \"force\" }, { \"followShift\": 3 }, "
                  "{ \"trianglePercent\": 150 }, { \"stats\": { \"atk\": \"turn\" }, \"to\": \"foe\" } ] }") + "," +
      skill("g2", "{ \"when\": \"combat\", \"aura\": { \"to\": \"allies\", \"within\": 3, \"match\": \"scout\" }, \"do\": [ { \"avoid\": \"turn\" } ] }") + "," +
      skill("g3", "{ \"when\": \"strike\", \"if\": [ { \"counterattack\": false } ], \"do\": [ { \"damage\": 2 } ] }") + "," +
      skill("g4", "{ \"when\": \"combat\", \"if\": [ { \"weaponKind\": \"magic\", \"who\": \"foe\" } ], \"do\": [ { \"followUp\": \"forbid\" } ] }") + "," +
      skill("g5", "{ \"when\": \"always\", \"do\": [ { \"moveAgain\": { \"notAfterAttack\": true } }, { \"cancelEffective\": true }, { \"noCarryPenalty\": true } ] }");
  for (uint32_t seed = 1; seed <= 10; ++seed) {
    Options o;
    o.seed = seed;
    o.aSkills = "\"g1\", \"g3\", \"g5\"";
    o.dSkills = "\"g2\", \"g4\", \"g1\"";
    o.aTags = "\"scout\"";
    o.dTags = "\"scout\"";
    o.extraWeapons = "{ \"id\": \"fire_tome\", \"name\": \"Fire Tome\", \"damageKind\": \"magic\", \"range\": [1, 2] },";
    o.dWeapon = "fire_tome";
    o.extraUnits = ", { \"type\": \"ua\", \"side\": \"blue\", \"x\": 0, \"y\": 0 }, { \"type\": \"ub\", \"side\": \"red\", \"x\": 7, \"y\": 0 }";
    o.gap = 6;
    Duel d(general, o);
    Battle& b = d.b();
    std::vector<Event> events;
    const long limit = 10L * (long)b.units.size() + 100;
    bool hung = false;
    while (!b.over && b.round <= 40 && !hung) {
      long steps = 0;
      while (aiTakeStep(b, events))
        if (++steps > limit) { hung = true; break; }
    }
    CHECK(!hung);
  }
}
