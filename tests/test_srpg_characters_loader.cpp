// Step C1 of docs/SRPG.md "Characters": loading data/characters.json and the "character" key of map units.
// Every refusal of the contract, each with its words; the happy files.
#include "srpg_chars_fixture.h"

using namespace ote;
using charstest::Copy;
using charstest::saysIn;

namespace {

// The fixture with data/characters.json replaced; the game's load message.
std::string charsError(const char* name, const std::string& text) {
  Copy c(name);
  c.put("data/characters.json", text);
  return c.gameError();
}

// A one-character file with `extra` keys after the required ones.
std::string one(const std::string& extra) { return R"({ "characters": [ { "id": "x", "type": "hero")" + extra + " } ] }"; }

const char* kPrefix = "character \"x\": ";

}  // namespace

TEST(chars_the_fixture_loads_with_every_field) {
  Database db;
  std::string err;
  CHECK(loadGame(ote_test::sourceDir() + "/tests/fixtures/srpg-chars", db, err));
  CHECK_EQ((int)db.characters.size(), 3);
  const CharacterDef& e = db.characters[(size_t)db.findCharacter("eirik")];
  CHECK_STR(e.name, "Eirik");
  CHECK_STR(e.description, "Captain of the Blue Company.");
  CHECK_EQ(e.type, db.findUnitType("hero"));
  CHECK_EQ(e.level, 3);
  CHECK_EQ(e.stats[(size_t)db.findStat("def")], -1);  // negative is allowed
  CHECK_EQ(e.growths[(size_t)db.findStat("hp")], 10);
  CHECK(e.hasItems && e.items.size() == 1);
  const CharacterDef& l = db.characters[(size_t)db.findCharacter("lyra")];
  CHECK_STR(l.name, "lyra");  // no name: the id
  CHECK_EQ(l.level, 1);
  CHECK(!l.hasItems);
  CHECK_EQ(db.findCharacter("nobody"), -1);
}

TEST(chars_a_game_without_the_file_has_no_characters) {
  Copy c("chars_nofile");
  c.drop("data/characters.json");
  c.put("maps/field.json", c.read("maps/plain.json"));
  Database db;
  std::string err;
  CHECK(loadGame(c.dir, db, err));
  CHECK(db.characters.empty());
}

TEST(chars_an_empty_list_is_fine) { CHECK_STR(charsError("chars_empty", R"({ "characters": [] })"), ""); }

TEST(chars_refusals_of_the_file) {
  CHECK(saysIn(charsError("chars_r0", R"({ "nothing": [] })"), "\"characters\""));
  CHECK_STR(charsError("chars_r1", one("")), "");  // sanity: the helper's own file loads
  CHECK(saysIn(charsError("chars_dup", R"({ "characters": [ { "id": "x", "type": "hero" }, { "id": "x", "type": "hero" } ] })"),
               std::string(kPrefix) + "duplicate id"));
  CHECK(saysIn(charsError("chars_badid", R"({ "characters": [ { "id": "a b", "type": "hero" } ] })"),
               "\"id\" must be letters, digits, \"_\" and \"-\""));
  CHECK(saysIn(charsError("chars_noid", R"({ "characters": [ { "type": "hero" } ] })"), "missing required \"id\""));
  CHECK(saysIn(charsError("chars_notype", R"({ "characters": [ { "id": "x" } ] })"), std::string(kPrefix) + "missing required \"type\""));
  CHECK(saysIn(charsError("chars_badtype", R"({ "characters": [ { "id": "x", "type": "wizard" } ] })"),
               std::string(kPrefix) + "unknown unit type \"wizard\""));
  CHECK(saysIn(charsError("chars_key", one(R"(, "mood": 3)")), std::string(kPrefix) + "unknown key \"mood\""));
}

TEST(chars_refusals_of_level_stats_and_lists) {
  CHECK(saysIn(charsError("chars_lv0", one(R"(, "level": 0)")), std::string(kPrefix) + "\"level\" must be from 1 to 20"));
  CHECK(saysIn(charsError("chars_lv21", one(R"(, "level": 21)")), "\"level\" must be from 1 to 20"));
  CHECK_STR(charsError("chars_lv20", one(R"(, "level": 20)")), "");
  CHECK(saysIn(charsError("chars_stat", one(R"(, "stats": { "luck": 1 })")), std::string(kPrefix) + "\"stats\" names unknown stat \"luck\""));
  CHECK(saysIn(charsError("chars_growth", one(R"(, "growths": { "luck": 1 })")), "\"growths\" names unknown stat \"luck\""));
  CHECK(saysIn(charsError("chars_skill", one(R"(, "skills": ["quick_study"])")),
               std::string(kPrefix) + "\"skills\" names unknown skill \"quick_study\" (data/skills.json)"));
  CHECK(saysIn(charsError("chars_learn1", one(R"(, "learns": [ { "level": 1, "skill": "a" } ])")),
               std::string(kPrefix) + "\"learns\" entry 1: \"level\" must be from 2 to 20"));
  CHECK(saysIn(charsError("chars_learn2", one(R"(, "learns": [ { "level": 5, "skill": "a" }, { "level": 99, "skill": "a" } ])")),
               "\"learns\" entry 2: \"level\" must be from 2 to 20"));
  CHECK(saysIn(charsError("chars_learnsk", one(R"(, "learns": [ { "level": 5, "skill": "heavy_blow" } ])")),
               "\"learns\" entry 1 names unknown skill \"heavy_blow\" (data/skills.json)"));
  CHECK(saysIn(charsError("chars_abil", one(R"(, "abilities": ["shove"])")),
               std::string(kPrefix) + "\"abilities\" names unknown ability \"shove\" (data/abilities.json)"));
  CHECK(saysIn(charsError("chars_item", one(R"(, "items": ["spear"])")), std::string(kPrefix)));
  CHECK(saysIn(charsError("chars_items4", one(R"(, "items": ["potion","potion","potion","potion"])")), std::string(kPrefix)));
}

// ---- the "character" key on map units ----

namespace {

// field.json with the units list replaced.
std::string fieldWith(Copy& c, const std::string& units) {
  c.put("maps/field.json",
        R"({ "name": "Field", "grid": { "type": "square", "width": 6, "height": 3 }, "legend": { ".": "plain" },
             "tiles": ["......", "......", "......"],
             "sides": [ { "id": "blue", "controller": "human" }, { "id": "red", "controller": "ai" } ],
             "units": )" + units + " }");
  return c.mapError("field");
}

}  // namespace

TEST(chars_map_unit_refusals) {
  Copy c("chars_map");
  CHECK(saysIn(fieldWith(c, R"([ { "character": "eirik", "id": "e2", "side": "blue", "x": 0, "y": 0 } ])"),
               "units[0] (hero \"Eirik\" at 0,0): a unit with a \"character\" is known to events by the character's id; remove \"id\""));
  CHECK(saysIn(fieldWith(c, R"([ { "type": "hero", "side": "blue", "x": 0, "y": 0 }, { "type": "hero", "side": "blue", "x": 1, "y": 0 },
                                  { "type": "hero", "id": "eirik", "side": "blue", "x": 2, "y": 0 } ])"),
               "units[2] (hero at 2,0): \"id\" \"eirik\" is a character (data/characters.json); write \"character\": \"eirik\""));
  CHECK(saysIn(fieldWith(c, R"([ { "character": "eirik", "side": "blue", "x": 0, "y": 0 }, { "character": "eirik", "side": "blue", "x": 1, "y": 0 } ])"),
               "units[1] (hero \"Eirik\" at 1,0): \"character\" \"eirik\" is already placed by units[0]"));
  CHECK(saysIn(fieldWith(c, R"([ { "type": "hero", "side": "blue", "x": 0, "y": 0 }, { "type": "hero", "side": "blue", "x": 1, "y": 0 },
                                  { "character": "x", "side": "blue", "x": 2, "y": 0 } ])"),
               "units[2]: unknown character \"x\" (data/characters.json)"));
  CHECK(saysIn(fieldWith(c, R"([ { "character": "eirik", "type": "wizard", "side": "blue", "x": 0, "y": 0 } ])"), "unknown unit type \"wizard\""));
  CHECK(saysIn(fieldWith(c, R"([ { "character": "eirik", "side": "green", "x": 0, "y": 0 } ])"), "unknown side \"green\""));
}

TEST(chars_a_character_key_in_a_game_without_characters) {
  Copy c("chars_map_nofile");
  c.drop("data/characters.json");
  CHECK(saysIn(c.mapError("field"), "units[0]: \"character\" is given but the game has no characters (data/characters.json)"));
}

TEST(chars_map_unit_defaults_and_overrides_load) {
  Copy c("chars_map_ok");
  CHECK_STR(fieldWith(c, R"([ { "character": "eirik", "type": "grunt", "name": "Eirik the Grey", "level": 7, "side": "blue", "x": 0, "y": 0,
                                "stats": { "hp": 99 }, "items": [] } ])"), "");
}

TEST(chars_add_unit_takes_a_character) {
  Copy c("chars_add");
  c.swap("maps/field.json", R"("units": [)",
         R"("events": [ { "id": "late", "when": { "battle": "start" },
                           "do": [ { "addUnit": { "character": "lyra", "side": "blue", "x": 3, "y": 1 } },
                                   { "addUnit": { "character": "lyra", "side": "blue", "x": 3, "y": 2 } } ] } ],
             "units": [)");
  CHECK_STR(c.mapError("field"), "");
  // The same character in "id" form and "character" form together is the plain refusal.
  c.swap("maps/field.json", R"("addUnit": { "character": "lyra", "side": "blue", "x": 3, "y": 1 })",
         R"("addUnit": { "character": "lyra", "id": "l", "side": "blue", "x": 3, "y": 1 })");
  CHECK(saysIn(c.mapError("field"), "remove \"id\""));
}
