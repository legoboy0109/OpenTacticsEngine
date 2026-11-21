// Step K1 of docs/SRPG.md "Skills": the loader of data/skills.json (the whole format, every refusal), of the skill
// keys on unit types, weapons and items, and of the story's teachSkill / forgetSkill / hasSkill.
#include "core/script/formulas.h"
#include "srpg_skills_fixture.h"

using namespace ote;
using skilltest::Copy;
using skilltest::oneBlock;
using skilltest::saysIn;

namespace {

struct Case {
  const char* block;   // one block, JSON text
  const char* needle;  // what the message must contain
};

// Each block goes into a skill "x" of a fixture copy; the game must be refused with the needle.
void refuseBlocks(const char* name, const Case* cases, size_t count) {
  Copy c(name);
  for (size_t i = 0; i < count; ++i) {
    c.skills(oneBlock(cases[i].block));
    CHECK(saysIn(c.gameError(), cases[i].needle));
  }
}
#define REFUSE(name, ...)                               \
  do {                                                  \
    static const Case cases[] = {__VA_ARGS__};          \
    refuseBlocks(name, cases, sizeof cases / sizeof *cases); \
  } while (0)

}  // namespace

TEST(skills_loader_the_fixture_loads_with_every_field) {
  Database db;
  std::string err;
  CHECK(loadGame(ote_test::sourceDir() + "/tests/fixtures/srpg-skills", db, err));
  CHECK_EQ(db.skills.size(), (size_t)16);
  const SkillDef& heavy = db.skills[(size_t)db.findSkill("heavy_blow")];
  CHECK_STR(heavy.name, "Heavy Blow");
  CHECK_STR(heavy.description, "A chance of double damage.");
  CHECK_EQ(heavy.blocks.size(), (size_t)1);
  CHECK(heavy.blocks[0].when == SkillWhen::Strike && heavy.blocks[0].hasChance && heavy.blocks[0].chanceExpr >= 0);
  CHECK_EQ(heavy.blocks[0].effects[0].value, 200);
  const SkillDef& bold = db.skills[(size_t)db.findSkill("bold")];
  CHECK(bold.blocks[0].conds[0].kind == SkillCond::Kind::Hp && bold.blocks[0].conds[0].op == CmpOp::Le);
  CHECK_EQ(bold.blocks[0].conds[0].value, 50);
  CHECK_EQ(db.skills[(size_t)db.findSkill("tough")].blocks[0].effects[0].stats[(size_t)db.findStat("hp")], 5);
  CHECK_STR(db.skills[(size_t)db.findSkill("steady_recovery")].name, "Steady Recovery");
  // The sources: unit type, learns, weapon, accessories; the character's lists are resolved by the characters loader.
  const UnitTypeDef& hero = db.unitTypes[(size_t)db.findUnitType("hero")];
  CHECK_EQ(hero.skills.size(), (size_t)1);
  CHECK_EQ(hero.learns.size(), (size_t)2);
  CHECK_EQ(hero.learns[0].skill, db.findSkill("light_step"));
  CHECK_EQ(db.weapons[(size_t)db.findWeapon("sword")].skills[0], db.findSkill("blade_focus"));
  CHECK_EQ(db.items[(size_t)db.findItem("charm")].skills[0], db.findSkill("lucky_charm"));
  CHECK_EQ(db.characters[(size_t)db.findCharacter("eirik")].skills[0], db.findSkill("bold"));
  CHECK(db.turnFlagSkills);  // strike_and_move has "moveAgain"
}

TEST(skills_loader_a_game_without_the_file_has_no_skills) {
  Copy c("skills_nofile");
  c.drop("data/skills.json");
  c.put("data/characters.json", R"({ "characters": [ { "id": "gorm", "type": "grunt" } ] })");
  c.put("data/units.json", R"({ "units": [ { "id": "grunt", "moveType": "foot", "stats": { "hp": 8 }, "weapons": ["sword"] } ] })");
  c.put("data/weapons.json", R"({ "weapons": [ { "id": "sword" } ] })");
  c.put("data/items.json", R"({ "items": [ { "id": "potion", "kind": "usable", "heal": 5 } ] })");
  c.put("maps/field.json", R"({ "name": "F", "grid": { "type": "square", "width": 2, "height": 1 }, "legend": { ".": "plain" },
     "tiles": [".."], "sides": [ { "id": "a", "controller": "human" }, { "id": "b", "controller": "ai" } ],
     "units": [ { "type": "grunt", "side": "a", "x": 0, "y": 0 }, { "type": "grunt", "side": "b", "x": 1, "y": 0 } ] })");
  CHECK_STR(c.gameError(), "");
  Database db;
  std::string err;
  CHECK(loadGame(c.dir, db, err) && db.skills.empty() && !db.turnFlagSkills);
  c.put("data/skills.json", R"({ "skills": [] })");  // an empty list is fine too
  CHECK_STR(c.gameError(), "");
}

TEST(skills_loader_refusals_of_the_file) {
  Copy c("skills_file");
  c.put("data/skills.json", R"({ "skills": [], "extra": 1 })");
  CHECK(saysIn(c.gameError(), "unknown key \"extra\" (expected \"skills\")"));
  c.put("data/skills.json", "{}");
  CHECK(saysIn(c.gameError(), "missing required \"skills\""));
  c.put("data/skills.json", "[]");
  CHECK(!c.gameError().empty());
  c.skills(oneBlock(R"({ "when": "always", "do": [ { "pass": "enemies" } ] })") + "," + oneBlock(R"({ "when": "always", "do": [ { "pass": "enemies" } ] })"));
  CHECK(saysIn(c.gameError(), "skill \"x\": duplicate id"));
  c.skills(R"({ "blocks": [] })");
  CHECK(saysIn(c.gameError(), "missing required \"id\""));
  c.skills(R"({ "id": "", "blocks": [] })");
  CHECK(saysIn(c.gameError(), "\"id\" must not be empty"));
  c.skills(R"({ "id": "x" })");
  CHECK(saysIn(c.gameError(), "skill \"x\": missing required \"blocks\""));
  c.skills(R"({ "id": "x", "blocks": [] })");
  CHECK(saysIn(c.gameError(), "\"blocks\" needs at least one block"));
  c.skills(R"({ "id": "x", "blocks": [ { "when": "always", "do": [ { "pass": "enemies" } ] } ], "color": 1 })");
  CHECK(saysIn(c.gameError(), "skill \"x\": unknown key \"color\""));
  c.skills(R"({ "id": "x", "name": 5, "blocks": [ { "when": "always", "do": [ { "pass": "enemies" } ] } ] })");
  CHECK(saysIn(c.gameError(), "\"name\" must be a string"));
  c.skills(R"({ "id": "x", "blocks": [ 5 ] })");
  CHECK(saysIn(c.gameError(), "skill \"x\": block 1: must be an object"));
  // Limits: 8 blocks per skill, 500 skills.
  std::string many = R"({ "id": "x", "blocks": [)";
  for (int i = 0; i < 9; ++i) many += std::string(i ? "," : "") + R"({ "when": "always", "do": [ { "pass": "enemies" } ] })";
  c.skills(many + "] }");
  CHECK(saysIn(c.gameError(), "too many blocks (at most 8)"));
  std::string lots;
  for (int i = 0; i < 501; ++i)
    lots += std::string(i ? "," : "") + "{ \"id\": \"s" + std::to_string(i) + R"(", "blocks": [ { "when": "always", "do": [ { "pass": "enemies" } ] } ] })";
  c.skills(lots);
  CHECK(saysIn(c.gameError(), "too many skills (at most 500)"));
}

TEST(skills_loader_refusals_of_blocks_and_effects) {
  REFUSE("skills_blocks",
         {R"({ "when": "sometimes", "do": [ { "crit": 1 } ] })", "\"when\" must be one of always, turnStart, combat"},
         {R"({ "do": [ { "crit": 1 } ] })", "missing required \"when\""},
         {R"({ "when": "combat", "do": [ { "crit": 1 } ], "zzz": 1 })", "skill \"x\": block 1: unknown key \"zzz\""},
         {R"({ "when": "combat" })", "missing required \"do\""},
         {R"({ "when": "combat", "do": [] })", "\"do\" needs at least one effect"},
         {R"({ "when": "combat", "do": [ {"hit":1},{"hit":1},{"hit":1},{"hit":1},{"hit":1},{"hit":1},{"hit":1},{"hit":1},{"hit":1} ] })",
          "too many effects (at most 8)"},
         {R"({ "when": "combat", "if": [ {"round":{"value":1}},{"round":{"value":1}},{"round":{"value":1}},{"round":{"value":1}},{"round":{"value":1}},
              {"round":{"value":1}},{"round":{"value":1}},{"round":{"value":1}},{"round":{"value":1}} ], "do": [ { "hit": 1 } ] })",
          "too many conditions (at most 8)"},
         {R"({ "when": "always", "chance": 30, "do": [ { "pass": "enemies" } ] })", "\"chance\" cannot be used in an \"always\" block"},
         {R"({ "when": "combat", "chance": 30, "do": [ { "hit": 1 } ] })", "\"chance\" cannot be used in a \"combat\" block"},
         {R"({ "when": "strike", "chance": true, "do": [ { "hit": 1 } ] })", "\"chance\" must be a percent or a formula"},
         {R"({ "when": "strike", "chance": 101, "do": [ { "hit": 1 } ] })", "skill \"x\": block 1: \"chance\" must be a percent from 0 to 100"},
         {R"({ "when": "strike", "chance": -5, "do": [ { "hit": 1 } ] })", "skill \"x\": block 1: \"chance\" must be a percent from 0 to 100"},
         {R"({ "when": "strike", "aura": { "to": "allies" }, "do": [ { "hit": 1 } ] })",
          "\"aura\" cannot be used in a \"strike\" block (it belongs to: combat)"},
         {R"({ "when": "combat", "aura": { "to": "friends" }, "do": [ { "hit": 1 } ] })", "\"aura\".\"to\" must be \"allies\" or \"enemies\""},
         {R"({ "when": "combat", "aura": { "to": "allies", "within": 11 }, "do": [ { "hit": 1 } ] })", "\"within\" must be a whole number from 1 to 10"},
         {R"({ "when": "combat", "aura": { "to": "allies" }, "do": [ { "strikes": 1 } ] })", "\"strikes\" cannot be used in an aura block"},
         {R"({ "when": "combat", "aura": { "to": "allies" }, "do": [ { "hit": 1, "to": "foe" } ] })", "\"to\" cannot be used with \"hit\" in an aura block"},
         {R"({ "when": "combat", "do": [ { "drain": 50 } ] })",
          "skill \"x\": block 1: \"drain\" cannot be used in a \"combat\" block (it belongs to: strike)"},
         {R"({ "when": "always", "do": [ { "heal": 5 } ] })", "\"heal\" cannot be used in an \"always\" block (it belongs to: turnStart, defeat, afterCombat, staffUse)"},
         {R"({ "when": "always", "if": [ { "unit": "hero" } ], "do": [ { "stats": { "hp": 2 } } ] })",
          "skill \"x\": block 1: a bonus to \"hp\" cannot have a condition"},
         {R"({ "when": "always", "do": [ { "stats": { "hp": 2 }, "growths": { "hp": 1 } } ] })", "has two effects"},
         {R"({ "when": "always", "do": [ { "bogus": 1 } ] })", "is not an effect; it needs one of stats, growths"},
         {R"({ "when": "always", "do": [ 5 ] })", "must be an object such as"},
         {R"({ "when": "always", "do": [ { "stats": { "zzz": 1 } } ] })", "\"stats\" names unknown stat \"zzz\""},
         {R"({ "when": "always", "do": [ { "stats": {} } ] })", "\"stats\" must name at least one stat"},
         {R"({ "when": "always", "do": [ { "stats": { "atk": 1.5 } } ] })", "must be a whole number from -1000 to 1000"},
         {R"({ "when": "always", "do": [ { "xpPercent": 0 } ] })", "\"xpPercent\" must be a whole number from 1 to 1000"},
         {R"({ "when": "always", "do": [ { "maxMoveCost": 0 } ] })", "\"maxMoveCost\" must be a whole number from 1 to 100"},
         {R"({ "when": "combat", "do": [ { "multiStrike": 5 } ] })", "\"multiStrike\" must be a whole number from 2 to 4"},
         {R"({ "when": "combat", "do": [ { "strikes": 8 } ] })", "\"strikes\" must be a whole number from -7 to 7"},
         {R"({ "when": "strike", "do": [ { "drain": 0 } ] })", "\"drain\" must be a whole number from 1 to 100"},
         {R"({ "when": "strike", "do": [ { "damagePercent": 1001 } ] })", "\"damagePercent\" must be a whole number from 0 to 1000"},
         {R"({ "when": "strike", "do": [ { "drain": 50, "to": "foe" } ] })", "\"to\" cannot be used with \"drain\""},
         {R"({ "when": "strike", "do": [ { "hit": 5, "to": "foe" } ] })", "\"to\": \"foe\" belongs to \"combat\" blocks"},
         {R"({ "when": "combat", "do": [ { "hit": 5, "to": "moon" } ] })", "\"to\" must be \"foe\""},
         {R"({ "when": "turnStart", "do": [ { "heal": 5, "to": "foe" } ] })", "\"to\" must be \"self\" or \"allies\""},
         {R"({ "when": "turnStart", "do": [ { "heal": 5, "within": 2 } ] })", "\"within\" belongs to \"to\": \"allies\" or \"enemies\""},
         {R"({ "when": "turnStart", "do": [ { "heal": 5, "to": "allies", "within": 0 } ] })", "\"within\" must be a whole number from 1 to 10"},
         {R"({ "when": "turnStart", "do": [ { "hurt": 5, "to": "foe" } ] })", "\"to\": \"foe\" belongs to \"afterCombat\" blocks"},
         {R"({ "when": "afterCombat", "do": [ { "hurt": 5, "to": "enemies" } ] })", "\"to\": \"enemies\" belongs to \"turnStart\" blocks"},
         {R"({ "when": "turnStart", "do": [ { "heal": 0 } ] })", "\"heal\" must be a whole number of 1 or more"},
         {R"({ "when": "turnStart", "do": [ { "heal": { "percent": 101 } } ] })", "\"percent\" must be a whole number from 1 to 100"},
         {R"({ "when": "always", "do": [ { "moveAgain": false } ] })", "\"moveAgain\" must be true"},
         {R"({ "when": "always", "do": [ { "pass": "allies" } ] })", "\"pass\" must be \"enemies\""},
         {R"({ "when": "combat", "do": [ { "stats": { "mov": 1 } } ] })", "a combat bonus cannot change the hit points or the movement stat"},
         {R"({ "when": "combat", "do": [ { "stats": { "hp": 1 } } ] })", "a combat bonus cannot change the hit points or the movement stat"},
         {R"({ "when": "always", "if": [ { "round": { "value": 1 } } ], "do": [ { "growths": { "atk": 1 } } ] })", "\"growths\" cannot have a condition"},
         {R"({ "when": "always", "if": [ { "round": { "value": 1 } } ], "do": [ { "ability": "shove" } ] })", "\"ability\" cannot have a condition"});
}

TEST(skills_loader_refusals_of_conditions) {
  REFUSE("skills_conds",
         {R"({ "when": "combat", "if": [ 5 ], "do": [ { "hit": 1 } ] })", "condition 1: must be an object"},
         {R"({ "when": "combat", "if": [ { "hp": { "percent": 5 }, "unit": "hero" } ], "do": [ { "hit": 1 } ] })", "has two conditions"},
         {R"({ "when": "combat", "if": [ { "bogus": 1 } ], "do": [ { "hit": 1 } ] })", "is not a condition; it needs one of hp, near, weapon"},
         {R"({ "when": "always", "if": [ { "hp": { "percent": 5 }, "who": "foe" } ], "do": [ { "pass": "enemies" } ] })",
          "\"who\": \"foe\" cannot be used in an \"always\" block"},
         {R"({ "when": "turnStart", "if": [ { "hp": { "percent": 5 }, "who": "foe" } ], "do": [ { "heal": 1 } ] })",
          "\"who\": \"foe\" cannot be used in a \"turnStart\" block"},
         {R"({ "when": "always", "if": [ { "initiating": true } ], "do": [ { "pass": "enemies" } ] })",
          "\"initiating\" cannot be used in an \"always\" block (there is no combat)"},
         {R"({ "when": "turnStart", "if": [ { "distance": { "value": 1 } } ], "do": [ { "heal": 1 } ] })",
          "\"distance\" cannot be used in a \"turnStart\" block"},
         {R"({ "when": "always", "if": [ { "formula": "a.atk > 1" } ], "do": [ { "pass": "enemies" } ] })",
          "\"formula\" cannot be used in an \"always\" block"},
         {R"({ "when": "combat", "aura": { "to": "allies" }, "if": [ { "initiating": true } ], "do": [ { "hit": 1 } ] })",
          "\"initiating\" cannot be used in an aura block"},
         {R"({ "when": "combat", "aura": { "to": "allies" }, "if": [ { "hp": { "percent": 5 }, "who": "foe" } ], "do": [ { "hit": 1 } ] })",
          "\"who\": \"foe\" cannot be used in an aura block"},
         {R"({ "when": "combat", "if": [ { "near": "ally", "who": "foe" } ], "do": [ { "hit": 1 } ] })", "\"near\" cannot have \"who\""},
         {R"({ "when": "combat", "if": [ { "hp": { "percent": 5 }, "who": "owner" } ], "do": [ { "hit": 1 } ] })", "\"who\" must be \"foe\""},
         {R"({ "when": "combat", "if": [ { "hp": { "op": "~", "percent": 5 } } ], "do": [ { "hit": 1 } ] })", "\"op\" must be one of = != < <= > >="},
         {R"({ "when": "combat", "if": [ { "hp": { "percent": 101 } } ], "do": [ { "hit": 1 } ] })", "\"percent\" must be a whole number from 0 to 100"},
         {R"({ "when": "combat", "if": [ { "hp": { "op": "<=" } } ], "do": [ { "hit": 1 } ] })", "missing required \"percent\""},
         {R"({ "when": "combat", "if": [ { "hp": 50 } ], "do": [ { "hit": 1 } ] })", "\"hp\" must be { \"op\""},
         {R"({ "when": "combat", "if": [ { "near": "friend" } ], "do": [ { "hit": 1 } ] })", "\"near\" must be \"ally\" or \"enemy\""},
         {R"({ "when": "combat", "if": [ { "near": "ally", "within": 11 } ], "do": [ { "hit": 1 } ] })", "\"within\" must be a whole number from 1 to 10"},
         {R"({ "when": "combat", "if": [ { "near": "ally", "color": 1 } ], "do": [ { "hit": 1 } ] })", "unknown key \"color\""},
         {R"({ "when": "combat", "if": [ { "weapon": "sword", "zzz": 1 } ], "do": [ { "hit": 1 } ] })", "unknown key \"zzz\""},
         {R"({ "when": "combat", "if": [ { "weapon": 5 } ], "do": [ { "hit": 1 } ] })", "\"weapon\" must be an id or a group"},
         {R"({ "when": "combat", "if": [ { "structure": 5 } ], "do": [ { "hit": 1 } ] })", "\"structure\" must be true, false or a structure id or tag"},
         {R"({ "when": "combat", "if": [ { "carrying": "yes" } ], "do": [ { "hit": 1 } ] })", "\"carrying\" must be true or false"},
         {R"({ "when": "combat", "if": [ { "round": 5 } ], "do": [ { "hit": 1 } ] })", "\"round\" must be { \"op\""},
         {R"({ "when": "combat", "if": [ { "distance": { "op": ">=" } } ], "do": [ { "hit": 1 } ] })", "missing required \"value\""},
         {R"({ "when": "combat", "if": [ { "formula": "" } ], "do": [ { "hit": 1 } ] })", "\"formula\" must be the text of a formula"});
}

TEST(skills_loader_names_in_conditions_and_effects_must_exist) {
  REFUSE("skills_names",
         {R"({ "when": "combat", "if": [ { "weapon": "axe" } ], "do": [ { "hit": 1 } ] })", "skill \"x\": block 1: condition 1: \"weapon\" names no weapon id or group \"axe\""},
         {R"({ "when": "combat", "if": [ { "terrain": "lava" } ], "do": [ { "hit": 1 } ] })", "\"terrain\" names no terrain id or tag \"lava\""},
         {R"({ "when": "combat", "if": [ { "structure": "castle" } ], "do": [ { "hit": 1 } ] })", "\"structure\" names no structure id or tag \"castle\""},
         {R"({ "when": "combat", "if": [ { "unit": "dragon" } ], "do": [ { "hit": 1 } ] })", "\"unit\" names no unit type id or tag \"dragon\""},
         {R"({ "when": "combat", "if": [ { "near": "ally", "match": "dragon" } ], "do": [ { "hit": 1 } ] })", "\"match\" names no unit type id or tag \"dragon\""},
         {R"({ "when": "always", "do": [ { "ability": "shove" } ] })", "block 1: effect 1: \"ability\" names unknown ability \"shove\" (data/abilities.json)"});
  // And the good ones: ids, groups and tags all count.
  Copy c("skills_names_ok");
  c.skills(oneBlock(R"({ "when": "combat", "if": [ { "weapon": "blade" }, { "weapon": "bow" }, { "terrain": "cover" }, { "terrain": "wall" },
        { "structure": "fort" }, { "structure": "stronghold" }, { "structure": true }, { "structure": false }, { "unit": "lord" },
        { "unit": "grunt" }, { "near": "enemy", "match": "lord", "within": 3, "op": ">", "count": 0 }, { "carrying": false },
        { "round": { "op": ">=", "value": 2 } }, { "initiating": true }, { "distance": { "value": 1 } }, { "formula": "a.atk > b.def" } ],
        "do": [ { "hit": 1 } ] })"));
  CHECK(saysIn(c.gameError(), "too many conditions"));  // 16 > 8: the limit counts
  c.skills(oneBlock(R"({ "when": "combat", "if": [ { "weapon": "blade" }, { "weapon": "bow" }, { "terrain": "cover" }, { "terrain": "wall" },
        { "structure": "fort" }, { "structure": "stronghold" }, { "structure": true }, { "structure": false } ], "do": [ { "hit": 1 } ] })") + "," +
         R"({ "id": "y", "blocks": [ { "when": "strike", "if": [ { "unit": "lord" }, { "unit": "grunt" },
        { "near": "enemy", "match": "lord", "within": 3, "op": ">", "count": 0 }, { "carrying": false }, { "round": { "op": ">=", "value": 2 } },
        { "initiating": true }, { "distance": { "value": 1 } }, { "formula": "a.atk > b.def" } ], "do": [ { "hit": 1 } ] } ] })");
  CHECK_STR(c.gameError(), "");
}

TEST(skills_loader_a_formula_is_compiled_with_its_place_in_the_message) {
  Copy c("skills_formula");
  c.skills(oneBlock(R"({ "when": "strike", "chance": "a.skl +", "do": [ { "hit": 1 } ] })"));
  Database db;
  std::string err;
  CHECK(loadGame(c.dir, db, err));  // the syntax is checked when the formulas are compiled, like every formula
  Formulas f;
  CHECK(!f.init(db, err));
  CHECK(saysIn(err, "data/skills.json: skill \"x\": block 1: \"chance\": "));
  c.skills(oneBlock(R"({ "when": "combat", "if": [ { "round": { "value": 1 } }, { "formula": "a.atk >" } ], "do": [ { "hit": 1 } ] })"));
  Database db2;
  CHECK(loadGame(c.dir, db2, err));
  CHECK(!f.init(db2, err));
  CHECK(saysIn(err, "data/skills.json: skill \"x\": block 1: condition 2: \"formula\": "));
  c.skills(oneBlock(R"({ "when": "strike", "chance": "a.skl * 2", "if": [ { "formula": "a.level >= 1 and b.tags.flier" } ], "do": [ { "hit": 1 } ] })"));
  Database db3;
  CHECK(loadGame(c.dir, db3, err));
  CHECK(f.init(db3, err));
}

TEST(skills_loader_refusals_of_the_skill_lists_on_types_weapons_and_items) {
  Copy c("skills_lists");
  c.swap("data/units.json", "\"skills\": [\"steady_recovery\"]", "\"skills\": [\"zzz\"]");
  CHECK(saysIn(c.gameError(), "unit \"hero\": \"skills\" names unknown skill \"zzz\" (data/skills.json)"));
  c.swap("data/units.json", "\"skills\": [\"zzz\"]", "\"skills\": [\"steady_recovery\", 5]");
  CHECK(saysIn(c.gameError(), "unit \"hero\": \"skills\" entry 2 must be a string"));
  c.swap("data/units.json", "\"skills\": [\"steady_recovery\", 5]", "\"skills\": \"steady_recovery\"");
  CHECK(saysIn(c.gameError(), "unit \"hero\": \"skills\" must be a list"));
  c.swap("data/units.json", "\"skills\": \"steady_recovery\"", "\"skills\": [\"steady_recovery\"]");
  CHECK_STR(c.gameError(), "");

  const char* learns[] = {R"([ { "level": 1, "skill": "light_step" } ])",    R"([ { "level": 21, "skill": "light_step" } ])",
                          R"([ { "level": 3, "skill": "zzz" } ])",            R"([ { "level": 3 } ])",
                          R"([ { "skill": "light_step" } ])",                 R"([ { "level": 3, "skill": "light_step", "x": 1 } ])",
                          R"([ 5 ])"};
  const char* wanted[] = {"unit \"hero\": \"learns\" entry 1: \"level\" must be from 2 to 20",
                          "\"learns\" entry 1: \"level\" must be from 2 to 20",
                          "\"learns\" entry 1 names unknown skill \"zzz\" (data/skills.json)",
                          "missing required \"skill\"",
                          "missing required \"level\"",
                          "\"learns\" entry 1: unknown key \"x\"",
                          "\"learns\" entry 1 must be an object"};
  for (size_t i = 0; i < 7; ++i) {
    Copy u("skills_learns");
    u.swap("data/units.json", "[ { \"level\": 3, \"skill\": \"light_step\" }, { \"level\": 2, \"skill\": \"fast_learner\" } ]", learns[i]);
    CHECK(saysIn(u.gameError(), wanted[i]));
  }

  Copy w("skills_weapon");
  w.swap("data/weapons.json", "\"skills\": [\"blade_focus\"]", "\"skills\": [\"zzz\"]");
  CHECK(saysIn(w.gameError(), "weapon \"sword\": \"skills\" names unknown skill \"zzz\""));

  Copy i("skills_item");
  i.swap("data/items.json", "\"skills\": [\"lucky_charm\"]", "\"skills\": [\"zzz\"]");
  CHECK(saysIn(i.gameError(), "\"skills\" names unknown skill \"zzz\""));
  // An accessory needs a bonus or skills (or both); skills belong to accessories only.
  Copy a("skills_item_kinds");
  a.swap("data/items.json", "\"skills\": [\"lucky_charm\"]", "\"skills\": []");
  CHECK(saysIn(a.gameError(), "an accessory needs a \"bonus\" that changes at least one stat, or \"skills\""));
  Copy k("skills_item_usable");
  k.swap("data/items.json", "\"id\": \"potion\"", "\"id\": \"potion\", \"skills\": [\"tough\"]");
  CHECK(saysIn(k.gameError(), "\"skills\" belongs to items of kind \"accessory\""));
}

TEST(skills_loader_story_commands_and_condition) {
  Copy c("skills_story");
  auto events = [&](const std::string& ev) {
    c.put("maps/story.json", R"({ "name": "Story", "grid": { "type": "square", "width": 6, "height": 3 }, "legend": { ".": "plain" },
      "tiles": ["......", "......", "......"], "sides": [ { "id": "blue", "controller": "human" }, { "id": "red", "controller": "ai" } ],
      "units": [ { "character": "lyra", "side": "blue", "x": 0, "y": 0 }, { "character": "gorm", "side": "red", "x": 5, "y": 2 } ],
      "events": [ { "id": "ev", "when": { "battle": "start" })" + ev + " } ] }");
    return c.mapError("story");
  };
  CHECK_STR(events(R"(, "do": [ { "teachSkill": "heavy_blow", "to": { "side": "blue" } }, { "forgetSkill": "bold", "from": {} } ])"), "");
  CHECK(saysIn(events(R"(, "do": [ { "teachSkill": "zzz", "to": "lyra" } ])"), "\"teachSkill\" names unknown skill \"zzz\" (data/skills.json)"));
  CHECK(saysIn(events(R"(, "do": [ { "forgetSkill": "zzz", "from": "lyra" } ])"), "\"forgetSkill\" names unknown skill \"zzz\""));
  CHECK(saysIn(events(R"(, "do": [ { "teachSkill": "tough" } ])"), "needs \"to\": which units learn it"));
  CHECK(saysIn(events(R"(, "do": [ { "forgetSkill": "tough" } ])"), "needs \"from\": which units forget it"));
  CHECK(saysIn(events(R"(, "do": [ { "teachSkill": "tough", "from": "lyra" } ])"), "unknown key \"from\""));
  CHECK(saysIn(events(R"(, "do": [ { "teachSkill": "tough", "to": "nobody" } ])"), "unknown unit \"nobody\""));
  CHECK(saysIn(events(R"(, "do": [ { "teachSkill": 5, "to": "lyra" } ])"), "\"teachSkill\" must be a skill id"));
  CHECK_STR(events(R"(, "if": [ { "hasSkill": "tough" } ], "do": [] )"), "");  // unit left out = any unit
  CHECK_STR(events(R"(, "if": [ { "hasSkill": "tough", "unit": { "type": "hero" } } ], "do": [] )"), "");
  CHECK(saysIn(events(R"(, "if": [ { "hasSkill": "zzz" } ], "do": [] )"), "\"hasSkill\" names unknown skill \"zzz\""));
  CHECK(saysIn(events(R"(, "if": [ { "hasSkill": "tough", "who": "lyra" } ], "do": [] )"), "unknown key \"who\""));
  CHECK(saysIn(events(R"(, "if": [ { "hasSkill": "tough", "unit": "nobody" } ], "do": [] )"), "unknown unit \"nobody\""));
  CHECK(saysIn(events(R"(, "if": [ { "hasSkill": "tough" } ], "do": [ { "dance": 1 } ] )"), "is not a command"));
}
