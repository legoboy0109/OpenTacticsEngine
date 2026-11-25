// Step A1 of docs/SRPG.md "Menu abilities and summoning": the loader of data/abilities.json (the whole format, every
// refusal), of "abilities" on unit types and characters, and of the skill-granted ability.
#include "abilities_files_fixture.h"
#include "core/battle/save_hash.h"

using namespace ote;
using abfiles::Copy;
using abfiles::saysIn;

namespace {

struct Case {
  const char* body;    // the inside of one ability "x" (JSON members after "id")
  const char* needle;  // what the message must contain
};

// Each body goes into an ability "x" of a fixture copy; the game must be refused with `ability "x": ` + the needle.
void refuse(const char* name, const Case* cases, size_t count) {
  Copy c(name);
  for (size_t i = 0; i < count; ++i) {
    c.abilities(std::string("{ \"id\": \"x\", ") + cases[i].body + " }");
    const std::string err = c.gameError();
    CHECK(saysIn(err, "data/abilities.json: ability \"x\": "));  // a message in the loader's usual form...
    CHECK(saysIn(err, cases[i].needle));                          // ...saying what is wrong
  }
}

// The same for what is only checked once everything is loaded: the fixture's own abilities stay, "x" is added.
void refuseLate(const char* name, const char* body, const char* needle) {
  Copy c(name);
  c.swap("data/abilities.json", "{ \"abilities\": [", std::string("{ \"abilities\": [ { \"id\": \"x\", ") + body + " },");
  const std::string err = c.gameError();
  CHECK(saysIn(err, "data/abilities.json: ability \"x\": "));
  CHECK(saysIn(err, needle));
}
#define REFUSE(name, ...)                           \
  do {                                              \
    static const Case cases[] = {__VA_ARGS__};      \
    refuse(name, cases, sizeof cases / sizeof *cases); \
  } while (0)

}  // namespace

TEST(abilities_loader_the_fixture_loads_with_every_field) {
  Database db;
  std::string err;
  CHECK(loadGame(abfiles::source(), db, err));
  CHECK_EQ(db.abilities.size(), (size_t)8);
  const AbilityDef& shove = db.abilities[(size_t)db.findAbility("shove")];
  CHECK_STR(shove.name, "Shove");
  CHECK_STR(shove.description, "Pushes a neighbour one square away.");
  CHECK(shove.target == AbilityTarget::Unit && shove.effect == AbilityEffect::Push && shove.amount == 1);
  CHECK(shove.minRange == 1 && shove.maxRange == 1 && !shove.free && shove.uses == 2 && shove.cooldown == 3 && shove.afterMove);
  CHECK(shove.conds.size() == 1 && shove.conds[0].kind == SkillCond::Kind::Hp && shove.conds[0].foe);
  CHECK(shove.conds[0].op == CmpOp::Gt && shove.conds[0].value == 50);
  const AbilityDef& yank = db.abilities[(size_t)db.findAbility("yank")];
  CHECK(yank.target == AbilityTarget::Enemy && yank.effect == AbilityEffect::Pull && yank.free);
  CHECK(db.abilities[(size_t)db.findAbility("trade")].effect == AbilityEffect::Swap);
  CHECK(!db.abilities[(size_t)db.findAbility("trade")].afterMove);
  CHECK(db.abilities[(size_t)db.findAbility("flank")].effect == AbilityEffect::MoveBehind);
  const AbilityDef& mend = db.abilities[(size_t)db.findAbility("mend")];
  CHECK(mend.effect == AbilityEffect::Heal && mend.amount == 10 && mend.maxRange == 2);
  const AbilityDef& rest = db.abilities[(size_t)db.findAbility("rest")];
  CHECK(rest.target == AbilityTarget::Self && rest.amount == kHealFull && rest.cooldown == 2);
  const AbilityDef& hound = db.abilities[(size_t)db.findAbility("call_hound")];
  CHECK(hound.target == AbilityTarget::Cell && hound.effect == AbilityEffect::Summon && hound.maxRange == 2);
  CHECK(hound.summon.type == db.findUnitType("hound") && hound.summon.level == 0 && hound.summon.limit == 1);
  CHECK(hound.summon.vanish && hound.summon.lifetime == 0 && !hound.summon.canAct && hound.summon.givesXp);
  const AbilityDef& wisp = db.abilities[(size_t)db.findAbility("call_wisp")];
  CHECK(wisp.summon.level == 4 && wisp.summon.limit == 2 && !wisp.summon.vanish && wisp.summon.lifetime == 3);
  CHECK(wisp.summon.canAct && !wisp.summon.givesXp && wisp.minRange == 1 && wisp.maxRange == 1);
  // Who has them: unit types, characters, skills.
  const UnitTypeDef& hero = db.unitTypes[(size_t)db.findUnitType("hero")];
  CHECK(hero.abilities == (std::vector<int>{db.findAbility("shove"), db.findAbility("call_hound"), db.findAbility("call_wisp")}));
  const CharacterDef& mira = db.characters[(size_t)db.findCharacter("mira")];
  CHECK(mira.abilities == (std::vector<int>{db.findAbility("trade"), db.findAbility("mend")}));
}

TEST(abilities_loader_who_has_what_type_character_then_skills_each_once) {
  savetest::Rig rig;
  CHECK(rig.load(abfiles::source(), "field"));
  rig.start();
  const Database& db = rig.db;
  Battle& b = rig.battle;
  auto idsOf = [&](int unit) {
    std::vector<int> list;
    b.abilitiesOf(b.units[(size_t)unit], list);
    std::string out;
    for (int a : list) out += (out.empty() ? "" : " ") + db.abilities[(size_t)a].id;
    return out;
  };
  // eirik: hero's own three, then his character's flank, then the skill's yank.
  CHECK_STR(idsOf(b.unitByStoryId("eirik")), "shove call_hound call_wisp flank yank");
  CHECK_STR(idsOf(b.unitByStoryId("mira")), "mend rest trade");  // archer: mend rest; mira: trade, (mend once)
  CHECK_STR(idsOf(b.unitByStoryId("gorm")), "");
}

TEST(abilities_loader_a_game_without_the_file_has_none_and_the_hash_notices_the_file) {
  Copy c("abilities_hash");
  c.put("data/units.json", R"({ "units": [ { "id": "hero", "moveType": "foot", "stats": {"hp":10,"atk":3,"def":1,"mov":3}, "weapons": ["sword"] } ] })");
  c.put("data/characters.json", R"({ "characters": [] })");
  c.drop("data/abilities.json");
  c.drop("data/skills.json");
  c.put("maps/field.json", R"({ "name": "Field", "grid": { "type": "square", "width": 3, "height": 1 }, "legend": { ".": "plain" },
    "tiles": ["..."], "sides": [ { "id": "blue", "controller": "human" }, { "id": "red", "controller": "ai" } ],
    "units": [ { "type": "hero", "side": "blue", "x": 0, "y": 0 }, { "type": "hero", "side": "red", "x": 2, "y": 0 } ] })");
  auto hashOf = [&]() {
    Database db;
    std::string err, out;
    if (!loadGame(c.dir, db, err) || !computeDataHash(db, "field", out, err)) return "error: " + err;
    return out;
  };
  const std::string without = hashOf();
  CHECK(without.rfind("error", 0) != 0);
  c.put("data/abilities.json", R"({ "abilities": [ { "id": "rest", "target": "self", "effect": { "heal": 5 } } ] })");
  const std::string with = hashOf();
  CHECK(with.rfind("error", 0) != 0 && with != without);
  c.put("data/abilities.json", R"({ "abilities": [ { "id": "rest", "name": "Rest", "target": "self", "effect": { "heal": 6 } } ] })");
  CHECK(hashOf() != with);  // what an ability does is part of the rules
  c.drop("data/abilities.json");
  CHECK_STR(hashOf(), without);  // removing the file restores the hash
}

TEST(abilities_loader_target_and_range) {
  REFUSE("abilities_target",
         {R"("target": "ground", "effect": { "heal": 5 })", R"("target" must be "self", "ally", "enemy", "unit" or "cell")"},
         {R"("effect": { "heal": 5 })", "missing required \"target\""},
         {R"("target": "unit", "range": [0, 1], "effect": { "push": 1 })", R"("push" moves a neighbour: "range" must be [1, 1])"},
         {R"("target": "unit", "range": [2, 1], "effect": { "push": 1 })", R"("range" must be [min, max])"},
         {R"("target": "unit", "range": 1, "effect": { "push": 1 })", R"("range" must be [min, max])"},
         {R"("target": "self", "range": [1, 1], "effect": { "heal": 5 })", R"("range" cannot be used with "target": "self")"});
}

TEST(abilities_loader_effect) {
  REFUSE("abilities_effect", {R"("target": "unit")", "missing required \"effect\""},
         {R"("target": "unit", "effect": {})", "\"effect\" needs exactly one of push, pull, swap, moveBehind, heal, summon"},
         {R"("target": "unit", "effect": { "push": 1, "swap": true })", "\"effect\" needs exactly one of push, pull, swap, moveBehind, heal, summon"},
         {R"("target": "unit", "effect": { "teleport": 1 })", "\"effect\": unknown key \"teleport\""},
         {R"("target": "unit", "effect": { "push": 0 })", R"("push" must be a whole number from 1 to 5)"},
         {R"("target": "unit", "effect": { "push": 6 })", R"("push" must be a whole number from 1 to 5)"},
         {R"("target": "unit", "effect": { "pull": false })", R"("pull" must be true)"},
         {R"("target": "ally", "effect": { "heal": 0 })", R"("heal" must be a whole number from 1 to 999, "full")"},
         {R"("target": "ally", "effect": { "heal": "most" })", R"("heal" must be a whole number from 1 to 999, "full")"},
         {R"("target": "unit", "range": [1, 2], "effect": { "push": 1 })", R"("push" moves a neighbour: "range" must be [1, 1])"},
         {R"("target": "unit", "range": [1, 3], "effect": { "pull": true })", R"("pull" moves a neighbour: "range" must be [1, 1])"},
         {R"("target": "unit", "range": [2, 2], "effect": { "swap": true })", R"("swap" moves a neighbour: "range" must be [1, 1])"},
         {R"("target": "unit", "range": [1, 2], "effect": { "moveBehind": true })", R"("moveBehind" moves a neighbour: "range" must be [1, 1])"},
         {R"("target": "cell", "effect": { "push": 1 })", R"("push" needs "target" "ally", "enemy" or "unit")"},
         {R"("target": "self", "effect": { "swap": true })", R"("swap" needs "target" "ally", "enemy" or "unit")"},
         {R"("target": "unit", "effect": { "summon": { "type": "hound" } })", R"("summon" needs "target": "cell")"},
         {R"("target": "enemy", "effect": { "heal": 3 })", R"("heal" cannot target "enemy" or "cell")"},
         {R"("target": "cell", "effect": { "heal": 3 })", R"("heal" cannot target "enemy" or "cell")"});
}

TEST(abilities_loader_cost_uses_cooldown_and_keys) {
  REFUSE("abilities_cost", {R"("target": "self", "effect": { "heal": 3 }, "cost": "bonus")", R"("cost" must be "action" or "free")"},
         {R"("target": "self", "effect": { "heal": 3 }, "uses": 0)", R"("uses" must be from 1 to 99)"},
         {R"("target": "self", "effect": { "heal": 3 }, "uses": 100)", R"("uses" must be from 1 to 99)"},
         {R"("target": "self", "effect": { "heal": 3 }, "cooldown": 0)", R"("cooldown" must be from 1 to 99)"},
         {R"("target": "self", "effect": { "heal": 3 }, "cooldown": 100)", R"("cooldown" must be from 1 to 99)"},
         {R"("target": "self", "effect": { "heal": 3 }, "afterMove": "no")", "\"afterMove\" must be true or false"},
         {R"("target": "self", "effect": { "heal": 3 }, "colour": "red")", R"(unknown key "colour")"});
}

TEST(abilities_loader_summon_type_is_checked_once_unit_types_are_loaded) {
  refuseLate("abilities_late_type", R"("target": "cell", "effect": { "summon": { "type": "ghost" } })", R"("summon": unknown unit type "ghost")");
}

TEST(abilities_loader_summon) {
  REFUSE("abilities_summon",
         {R"("target": "cell", "effect": { "summon": {} })", "\"summon\": \"type\" must be the id of a unit type"},
         {R"("target": "cell", "effect": { "summon": { "type": "hound", "limit": 0 } })", R"("summon": "limit" must be from 1 to 20)"},
         {R"("target": "cell", "effect": { "summon": { "type": "hound", "limit": 21 } })", R"("summon": "limit" must be from 1 to 20)"},
         {R"("target": "cell", "effect": { "summon": { "type": "hound", "lifetime": 100 } })", R"("summon": "lifetime" must be from 0 to 99)"},
         {R"("target": "cell", "effect": { "summon": { "type": "hound", "lifetime": -1 } })", R"("summon": "lifetime" must be from 0 to 99)"},
         {R"("target": "cell", "effect": { "summon": { "type": "hound", "level": "boss" } })",
          R"("summon": "level" must be "summoner" or a whole number from 1 to 20)"},
         {R"("target": "cell", "effect": { "summon": { "type": "hound", "level": 0 } })",
          R"("summon": "level" must be "summoner" or a whole number from 1 to 20)"},
         {R"("target": "cell", "effect": { "summon": { "type": "hound", "level": 21 } })",
          R"("summon": "level" must be "summoner" or a whole number from 1 to 20)"},
         {R"("target": "cell", "effect": { "summon": { "type": "hound", "vanish": 1 } })", "\"vanish\" must be true or false"},
         {R"("target": "cell", "effect": { "summon": { "type": "hound", "pack": 3 } })", R"("summon": unknown key "pack")"},
         {R"("target": "cell", "effect": { "summon": "hound" })", "\"summon\" must be an object"});
}

TEST(abilities_loader_conditions_use_the_skill_reader) {
  REFUSE("abilities_conds",
         {R"("target": "cell", "effect": { "summon": { "type": "hound" } }, "if": [ { "hp": { "op": ">", "percent": 50 }, "who": "foe" } ])",
          R"("who": "foe" cannot be used in an ability that targets a square)"},
         {R"("target": "unit", "effect": { "push": 1 }, "if": [ { "initiating": true } ])", R"("initiating" cannot be used in an ability)"},
         {R"("target": "unit", "effect": { "push": 1 }, "if": [ { "distance": { "op": ">", "value": 1 } } ])", R"("distance" cannot be used in an ability)"},
         {R"("target": "unit", "effect": { "push": 1 }, "if": [ { "weapon": "sword" }, { "hp": { "op": "<", "percent": 101 } } ])", "condition 2"});
  refuseLate("abilities_late_cond", R"("target": "unit", "effect": { "push": 1 }, "if": [ { "terrain": "lava" } ])", "lava");
  // A formula and the ordinary conditions are fine.
  Copy c("abilities_conds_ok");
  c.abilities(R"({ "id": "x", "target": "unit", "effect": { "push": 1 },
    "if": [ { "formula": "a.atk > b.def" }, { "near": "ally", "within": 2 }, { "terrain": "plain" } ] })");
  c.swap("data/units.json", R"(, "abilities": ["shove", "call_hound", "call_wisp"])", "");
  c.swap("data/units.json", R"("abilities": ["mend", "rest"])", R"("abilities": ["x"])");
  c.put("data/characters.json", R"({ "characters": [ { "id": "eirik", "type": "hero" }, { "id": "mira", "type": "archer" }, { "id": "gorm", "type": "grunt" } ] })");
  c.drop("data/skills.json");
  CHECK_STR(c.gameError(), "");
}

TEST(abilities_loader_the_file_and_the_lists) {
  Copy c("abilities_file");
  c.put("data/abilities.json", R"({ "abilities": [], "extra": 1 })");
  CHECK(saysIn(c.gameError(), "unknown key \"extra\""));
  c.put("data/abilities.json", R"({ "abilities": [ { "id": "a", "target": "self", "effect": {"heal": 1} },
                                                    { "id": "a", "target": "self", "effect": {"heal": 1} } ] })");
  CHECK(saysIn(c.gameError(), "ability \"a\": duplicate id"));
  c.put("data/abilities.json", R"({ "abilities": [ { "target": "self", "effect": {"heal": 1} } ] })");
  CHECK(saysIn(c.gameError(), "missing required \"id\""));
  c.put("data/abilities.json", R"({ "abilities": 5 })");
  CHECK(saysIn(c.gameError(), "\"abilities\" must be a list"));
  c.put("data/abilities.json", R"({ "abilities": [ 5 ] })");
  CHECK(saysIn(c.gameError(), "abilities[0]: must be an object"));
  // The units, characters and skills name abilities that must exist.
  c.put("data/abilities.json", R"({ "abilities": [ { "id": "shove", "target": "unit", "effect": {"push": 1} } ] })");
  CHECK(saysIn(c.gameError(), "\"abilities\" names unknown ability \"call_hound\" (data/abilities.json)"));
}
