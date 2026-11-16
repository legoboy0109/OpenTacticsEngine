// Step C1 of docs/SRPG.md "Characters", in a battle: units dressed from characters (stats, items, name, level),
// the map's overrides, growths, identity through a roster and a save, addUnit of a character, and the data hash.
#include "core/battle/save.h"
#include "srpg_chars_fixture.h"

using namespace ote;
using charstest::Copy;
using savetest::Rig;

namespace {

struct Chars {
  Rig rig;
  explicit Chars(const char* map = "field", const std::string& dir = "") {
    CHECK(rig.load(dir.empty() ? ote_test::sourceDir() + "/tests/fixtures/srpg-chars" : dir, map));
  }
  Battle& b() { return rig.battle; }
  const Unit& unit(const char* id) { return b().units[(size_t)b().unitByStoryId(id)]; }
  int stat(const Unit& u, const char* s) { return b().stat(u, rig.db.findStat(s)); }
};

// The map file of `c` with the units list replaced, then loaded into a battle.
void useUnits(Copy& c, const std::string& units) {
  c.put("maps/field.json",
        R"({ "name": "Field", "grid": { "type": "square", "width": 6, "height": 3 }, "legend": { ".": "plain" },
             "tiles": ["......", "......", "......"],
             "sides": [ { "id": "blue", "controller": "human" }, { "id": "red", "controller": "ai" } ],
             "units": )" + units + " }");
}

}  // namespace

TEST(chars_stats_are_the_type_plus_the_character) {
  Chars w;
  const Unit& e = w.unit("eirik");  // hero 20 6 3 4, character +4 +1 -1
  CHECK_EQ(w.stat(e, "hp"), 24);
  CHECK_EQ(w.stat(e, "atk"), 7);
  CHECK_EQ(w.stat(e, "def"), 2);
  CHECK_EQ(w.stat(e, "mov"), 4);
  CHECK_EQ(e.hp, 24);
  CHECK_STR(e.name, "Eirik");
  CHECK_EQ(e.level, 3);
  CHECK(e.leader);
  CHECK_EQ(e.character, w.rig.db.findCharacter("eirik"));
  CHECK_STR(e.storyId, "eirik");
  // A stat never goes below 0: grunt def 1 + (-5).
  CHECK_EQ(w.stat(w.unit("gorm"), "def"), 0);
  CHECK_EQ(w.stat(w.unit("gorm"), "atk"), 3);
}

TEST(chars_a_character_with_nothing_but_a_type_is_the_type) {
  Chars w;
  const Unit& l = w.unit("lyra");
  CHECK_EQ(l.level, 1);
  CHECK_EQ(w.stat(l, "atk"), 6);
  CHECK_EQ(l.character, w.rig.db.findCharacter("lyra"));
  CHECK_EQ(l.items.size(), (size_t)1);  // the hero's starting ring
  CHECK_STR(l.name, "lyra");
  // A plain unit has no character.
  CHECK_EQ(w.b().units[3].character, -1);
}

TEST(chars_items_map_then_character_then_type) {
  Chars w;
  int potion = w.rig.db.findItem("potion"), ring = w.rig.db.findItem("ring");
  CHECK_EQ(w.unit("eirik").items.size(), (size_t)1);
  CHECK_EQ(w.unit("eirik").items[0].item, potion);  // the character's items replace the type's
  CHECK_EQ(w.unit("lyra").items[0].item, ring);     // none given: the type's
  Copy c("chars_items_map");
  useUnits(c, R"([ { "character": "eirik", "side": "blue", "x": 0, "y": 0, "items": [ "ring", "ring" ] },
                   { "character": "gorm", "side": "red", "x": 5, "y": 0, "items": [] } ])");
  Chars m("field", c.dir);
  CHECK_EQ(m.unit("eirik").items.size(), (size_t)2);  // the map wins
  CHECK_EQ(m.unit("eirik").items[0].item, ring);
  CHECK(m.unit("gorm").items.empty());                 // an empty list is a choice too
}

TEST(chars_map_overrides_type_name_level_and_stats) {
  Copy c("chars_overrides");
  useUnits(c, R"([ { "character": "eirik", "type": "grunt", "name": "Eirik the Grey", "level": 7, "side": "blue", "x": 0, "y": 0,
                     "stats": { "hp": 99 } },
                   { "character": "gorm", "side": "red", "x": 5, "y": 0, "level": 4 } ])");
  Chars w("field", c.dir);
  const Unit& e = w.unit("eirik");
  CHECK_EQ(e.type, w.rig.db.findUnitType("grunt"));  // the class it has on THIS map
  CHECK_STR(e.name, "Eirik the Grey");
  CHECK_EQ(e.level, 7);
  CHECK_EQ(w.stat(e, "hp"), 99);                      // the map's own value replaces the sum
  CHECK_EQ(w.stat(e, "atk"), 4);                      // grunt 3 + the character's 1
  CHECK_EQ(w.stat(e, "def"), 0);                      // grunt 1 - 1
  CHECK_EQ(w.unit("gorm").level, 4);
  CHECK_STR(w.unit("gorm").name, "Gorm");
}

TEST(chars_growths_add_up_and_never_go_below_zero) {
  Chars w;
  int hp = w.rig.db.findStat("hp"), atk = w.rig.db.findStat("atk"), def = w.rig.db.findStat("def");
  CHECK_EQ(w.b().growthOf(w.unit("eirik"), hp), 110);   // 100 + 10
  CHECK_EQ(w.b().growthOf(w.unit("eirik"), atk), 45);   // 40 + 5
  CHECK_EQ(w.b().growthOf(w.unit("eirik"), def), 0);
  CHECK_EQ(w.b().growthOf(w.unit("gorm"), hp), 0);      // 50 - 80 counts as 0
  CHECK_EQ(w.b().growthOf(w.unit("gorm"), atk), 20);
  // No personal growths = exactly the type's, so a level up rolls the same dice as for a plain unit.
  for (int s = 0; s < (int)w.rig.db.stats.size(); ++s) CHECK_EQ(w.b().growthOf(w.unit("lyra"), s), w.b().growthOf(w.b().units[3], s));
}

TEST(chars_a_level_up_uses_the_personal_growth) {
  Copy c("chars_levelup");
  useUnits(c, R"([ { "character": "eirik", "side": "blue", "x": 0, "y": 0 }, { "type": "grunt", "side": "red", "x": 1, "y": 0 } ])");
  Chars w("field", c.dir);
  std::vector<Event> ev;
  w.b().start(ev);
  int e = w.b().unitByStoryId("eirik");
  w.b().units[(size_t)e].xp = 99;
  int hp0 = w.b().units[(size_t)e].stats[0];
  CHECK(w.b().apply(Action::attack(e, 1, 0), ev));
  bool up = false;
  for (const Event& x : ev)
    if (x.type == EventType::LevelUp && x.a == e) {
      up = true;
      CHECK(x.data[0] >= 1);  // growth 110: one point is certain
      CHECK_EQ(w.b().units[(size_t)e].stats[0], hp0 + x.data[0]);
    }
  CHECK(up);
}

TEST(chars_identity_survives_a_save) {
  Chars w;
  Json saved, parsed;
  std::string err;
  saveBattleState(w.b(), saved);
  CHECK(saved.dump(2).find("character") == std::string::npos);  // nothing about characters is written
  CHECK(Json::parse(saved.dump(2), parsed, err));
  Rig fresh;
  CHECK(fresh.load(w.rig.gameDir, "field", 999));
  CHECK(restoreBattleState(fresh.battle, parsed, err));
  int e = fresh.battle.unitByStoryId("eirik");
  CHECK(e >= 0);
  CHECK_EQ(fresh.battle.units[(size_t)e].character, fresh.db.findCharacter("eirik"));
  CHECK_EQ(fresh.battle.units[3].character, -1);
  CHECK_STR(savetest::stateText(w.b()), savetest::stateText(fresh.battle));
}

TEST(chars_a_roster_entry_wins_and_keeps_the_identity) {
  Chars w;
  auto roster = std::make_shared<Roster>();
  RosterUnit r;
  r.id = "eirik";
  r.type = w.rig.db.findUnitType("grunt");
  r.name = "Eirik, veteran";
  r.level = 9;
  r.xp = 40;
  r.stats = {30, 8, 5, 4};
  roster->push_back(r);
  w.b().setStartingRoster(roster);
  const Unit& e = w.unit("eirik");
  CHECK_EQ(e.type, w.rig.db.findUnitType("grunt"));
  CHECK_STR(e.name, "Eirik, veteran");
  CHECK_EQ(e.level, 9);
  CHECK_EQ(w.stat(e, "hp"), 30);
  CHECK_EQ(e.character, w.rig.db.findCharacter("eirik"));  // still the same person
  CHECK(e.items.empty());                                  // what the roster stored (nothing): not the character's potion
  // Not in the roster yet: the file and the map say.
  CHECK_EQ(w.stat(w.unit("lyra"), "atk"), 6);
}

TEST(chars_add_unit_of_a_character_already_in_the_battle_adds_nothing) {
  Copy c("chars_addtwice");
  useUnits(c, R"([ { "character": "eirik", "side": "blue", "x": 0, "y": 0 }, { "type": "grunt", "side": "red", "x": 5, "y": 2 } ])");
  c.swap("maps/field.json", R"("units": [)",
         R"("events": [ { "id": "late", "when": { "battle": "start" },
                           "do": [ { "addUnit": { "character": "lyra", "side": "blue", "x": 3, "y": 1 } },
                                   { "addUnit": { "character": "lyra", "side": "blue", "x": 3, "y": 2 } },
                                   { "addUnit": { "character": "eirik", "side": "blue", "x": 4, "y": 2 } } ] } ],
             "units": [)");
  Chars w("field", c.dir);
  std::vector<Event> ev;
  w.b().start(ev);
  CHECK_EQ((int)w.b().units.size(), 3);  // eirik, the grunt, one lyra
  const Unit& l = w.unit("lyra");
  CHECK_EQ(l.character, w.rig.db.findCharacter("lyra"));
  CHECK_EQ(w.b().grid.x(l.cell), 3);
  CHECK_EQ(w.b().grid.y(l.cell), 1);
}

TEST(chars_a_built_or_plain_unit_never_has_a_character) {
  Chars w("plain");
  for (const Unit& u : w.b().units) CHECK_EQ(u.character, -1);
}

// ---- data hash and compatibility ----

namespace {
std::string hashOf(const Copy& c, const char* map) {
  Database db;
  std::string err, out;
  if (!loadGame(c.dir, db, err) || !computeDataHash(db, map, out, err)) return err;
  return out;
}
}  // namespace

TEST(chars_the_hash_changes_only_when_the_file_exists) {
  Copy c("chars_hash");
  std::string with = hashOf(c, "plain");
  CHECK(with.size() == 16 || with.size() > 8);
  c.drop("data/characters.json");
  std::string without = hashOf(c, "plain");
  CHECK(with != without);
  // Restoring the file restores the hash.
  Copy back("chars_hash_back");
  CHECK_STR(hashOf(back, "plain"), with);
  // Wording is cosmetic: a rename or a new description leaves the hash alone; a stat does not.
  back.swap("data/characters.json", "\"Captain of the Blue Company.\"", "\"Someone else.\"");
  back.swap("data/characters.json", "\"name\": \"Gorm\"", "\"name\": \"Gormund\"");
  CHECK_STR(hashOf(back, "plain"), with);
  back.swap("data/characters.json", "\"hp\": 4,", "\"hp\": 5,");
  CHECK(hashOf(back, "plain") != with);
}
