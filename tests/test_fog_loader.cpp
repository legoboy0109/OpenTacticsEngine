// Loading the fog of war data (docs/ECONOMY.md "Fog of war"): the map's "fog", units.json "vision", terrain.json and
// structures.json "visionBonus" and "hides", and the story condition "sees". The good case and one test per refusal,
// each checking that the message names what is wrong.
#include <sys/stat.h>

#include <cstdio>

#include "core/base/json.h"
#include "core/data/loader.h"
#include "fog_fixture.h"

using namespace ote;
using namespace fogtest;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

struct Parts {
  std::string terrain = R"({ "terrain": [ {"id":"plain"}, {"id":"hill","visionBonus":3}, {"id":"wood","hides":true,"visionBonus":0} ] })";
  std::string units =
      R"({ "units": [
        {"id":"grunt","moveType":"foot","stats":{"hp":5,"str":2,"mov":3}},
        {"id":"scout","moveType":"foot","stats":{"hp":5,"str":2,"mov":3},"vision":6},
        {"id":"blind","moveType":"foot","stats":{"hp":5,"str":2,"mov":3},"vision":0} ] })";
  std::string structures =
      R"({ "structures": [ { "id": "tower", "visionBonus": 4, "hides": true }, { "id": "wall" } ] })";
  std::string fog = ", \"fog\": true";  // the map's extra key (with its leading comma), "" = none
  std::string sides = R"([{"id":"a"},{"id":"b"}])";
};

std::string makeGame(const char* name, const Parts& p) {
  std::string root = ote_test::tempDir() + "/fog_" + name;
  mkdir(root.c_str(), 0755);
  mkdir((root + "/data").c_str(), 0755);
  mkdir((root + "/maps").c_str(), 0755);
  CHECK(writeTextFile(root + "/game.json",
                      R"({ "formatVersion": 1, "name": "G", "stats": [{"id":"hp"},{"id":"str"},{"id":"mov"}],
                           "roles": {"hp":"hp","move":"mov"}, "moveTypes": [{"id":"foot"}],
                           "resources": [{"id":"gold"}], "formulas": {"damage":"a.str"},
                           "maps": [{"id":"m","file":"maps/m.json"}] })"));
  CHECK(writeTextFile(root + "/data/terrain.json", p.terrain));
  CHECK(writeTextFile(root + "/data/weapons.json", R"({ "weapons": [ {"id":"sword"} ] })"));
  CHECK(writeTextFile(root + "/data/units.json", p.units));
  CHECK(writeTextFile(root + "/data/structures.json", p.structures));
  CHECK(writeTextFile(root + "/maps/m.json",
                      "{ \"grid\": {\"type\":\"square\",\"width\":3,\"height\":1}, \"legend\": {\".\":\"plain\"}, \"tiles\": [\"...\"],"
                      " \"sides\": " + p.sides + ", \"units\": [{\"type\":\"grunt\",\"side\":\"a\",\"x\":0,\"y\":0},"
                      "{\"type\":\"grunt\",\"side\":\"b\",\"x\":2,\"y\":0}]" + p.fog + " }"));
  return root;
}

// "" = loaded fine (game and map), else the loader's message.
std::string problem(const char* name, const Parts& p) {
  Database db;
  MapDef map;
  std::string err;
  if (!loadGame(makeGame(name, p), db, err)) return err;
  return loadMap(db, "m", map, err) ? "" : err;
}

Parts withUnit(const char* extra) {
  Parts p;
  p.units = std::string(R"({ "units": [ {"id":"grunt","moveType":"foot","stats":{"hp":5,"str":2,"mov":3},)") + extra + " } ] }";
  return p;
}
Parts withTerrain(const char* extra) {
  Parts p;
  p.terrain = std::string(R"({ "terrain": [ {"id":"plain",)") + extra + " } ] }";
  return p;
}
Parts withStructure(const char* extra) {
  Parts p;
  p.structures = std::string(R"({ "structures": [ {"id":"tower",)") + extra + " } ] }";
  return p;
}

#define REFUSES(testName, parts, ...)                                              \
  TEST(testName) {                                                                 \
    std::string err = problem(#testName, parts);                                   \
    CHECK(!err.empty());                                                           \
    const char* needles[] = {__VA_ARGS__};                                         \
    for (const char* n : needles)                                                  \
      if (!has(err, n)) {                                                          \
        std::printf("  message lacks \"%s\": %s\n", n, err.c_str());               \
        CHECK(false);                                                              \
      }                                                                            \
  }

}  // namespace

TEST(fog_loader_reads_everything) {
  Database db;
  MapDef map;
  std::string err;
  CHECK(loadGame(makeGame("ok", Parts()), db, err));
  CHECK(loadMap(db, "m", map, err));
  CHECK(map.fog);
  CHECK_EQ(db.unitTypes[(size_t)db.findUnitType("grunt")].vision, 2);   // the default
  CHECK_EQ(db.unitTypes[(size_t)db.findUnitType("scout")].vision, 6);
  CHECK_EQ(db.unitTypes[(size_t)db.findUnitType("blind")].vision, 0);   // 0 is allowed
  const TerrainDef& hill = db.terrains[(size_t)db.findTerrain("hill")];
  CHECK_EQ(hill.visionBonus, 3);
  CHECK(!hill.hides);
  CHECK(db.terrains[(size_t)db.findTerrain("wood")].hides);
  CHECK_EQ(db.terrains[0].visionBonus, 0);
  const StructureDef& tower = db.structures[(size_t)db.findStructure("tower")];
  CHECK_EQ(tower.visionBonus, 4);
  CHECK(tower.hides);
  CHECK(!db.structures[(size_t)db.findStructure("wall")].hides);
}

TEST(fog_loader_fog_defaults_to_off_and_off_can_be_written) {
  Parts none;
  none.fog = "";
  Database db;
  MapDef map;
  std::string err;
  CHECK(loadGame(makeGame("nofog", none), db, err) && loadMap(db, "m", map, err));
  CHECK(!map.fog);
  Parts off;
  off.fog = ", \"fog\": false";
  CHECK(loadGame(makeGame("fogoff", off), db, err) && loadMap(db, "m", map, err));
  CHECK(!map.fog);
}

TEST(fog_loader_the_limits_themselves_are_fine) {
  Parts p = withUnit("\"vision\": 20");
  CHECK_STR(problem("max_vision", p), "");
  p = withTerrain("\"visionBonus\": 10");
  CHECK_STR(problem("max_bonus", p), "");
}

REFUSES(fog_loader_refuses_vision_above_20, withUnit("\"vision\": 21"), "units.json", "grunt", "\"vision\"", "0 to 20")
REFUSES(fog_loader_refuses_negative_vision, withUnit("\"vision\": -1"), "\"vision\"", "0 to 20")
REFUSES(fog_loader_refuses_fractional_vision, withUnit("\"vision\": 2.5"), "\"vision\"", "whole number")
REFUSES(fog_loader_refuses_vision_that_is_text, withUnit("\"vision\": \"far\""), "\"vision\"", "whole number")
REFUSES(fog_loader_refuses_terrain_bonus_above_10, withTerrain("\"visionBonus\": 11"), "terrain.json", "\"visionBonus\"", "0 to 10")
REFUSES(fog_loader_refuses_negative_terrain_bonus, withTerrain("\"visionBonus\": -1"), "\"visionBonus\"", "0 to 10")
REFUSES(fog_loader_refuses_terrain_bonus_that_is_not_whole, withTerrain("\"visionBonus\": 1.5"), "\"visionBonus\"", "whole number")
REFUSES(fog_loader_refuses_terrain_hides_that_is_not_true_or_false, withTerrain("\"hides\": \"yes\""), "\"hides\"", "true or false")
REFUSES(fog_loader_refuses_structure_bonus_above_10, withStructure("\"visionBonus\": 12"), "structures.json", "tower", "\"visionBonus\"", "0 to 10")
REFUSES(fog_loader_refuses_negative_structure_bonus, withStructure("\"visionBonus\": -3"), "\"visionBonus\"", "0 to 10")
REFUSES(fog_loader_refuses_structure_hides_that_is_not_true_or_false, withStructure("\"hides\": 1"), "\"hides\"", "true or false")

TEST(fog_loader_refuses_fog_that_is_not_true_or_false) {
  Parts p;
  p.fog = ", \"fog\": \"yes\"";
  std::string err = problem("fog_text", p);
  CHECK(has(err, "maps/m.json") && has(err, "\"fog\"") && has(err, "true or false"));
}

TEST(fog_loader_refuses_fog_on_a_map_with_more_sides_than_an_event_mask_has_bits) {
  Parts p;
  p.sides = "[{\"id\":\"a\"},{\"id\":\"b\"}";
  for (int i = 0; i < 31; ++i) p.sides += ",{\"id\":\"s" + std::to_string(i) + "\"}";
  p.sides += ",{\"id\":\"z\"}]";
  std::string err = problem("many_sides_2", p);
  CHECK(has(err, "fog") && has(err, "at most 32"));
  p.fog = "";
  CHECK_STR(problem("many_sides_nofog", p), "");   // without fog 33 sides are fine
}

// ---- the story condition "sees" ----

namespace {

std::string seesError(const std::string& condition) {
  FogWorld w(field(9, 3), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  return w.load("[{\"id\":\"ev\",\"when\":{\"battle\":\"start\"},\"if\":[" + condition + "],\"do\":[]}]");
}

}  // namespace

TEST(fog_loader_sees_is_read_with_a_filter_and_a_team) {
  FogWorld w(field(9, 3), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  CHECK_STR(w.load(R"([{"id":"a","when":{"battle":"start"},"if":[{"sees":"gorm","team":"blue"},
                       {"sees":{"side":"red"},"team":"blue"},{"sees":{"tag":"x"},"team":"red"},{"sees":{},"team":"red"}],"do":[]}])"), "");
  const Condition& c = w.map.story->events[0].cond[0];
  CHECK(c.kind == Condition::Kind::Sees);
  CHECK_EQ(c.team, 0);
  CHECK(c.filter.kind == UnitFilter::Kind::Unit && c.filter.unit == "gorm");
  CHECK_EQ(w.map.story->events[0].cond[2].team, 1);
}

TEST(fog_loader_sees_accepts_a_side_id_for_the_team_of_that_side) {
  FogWorld w(field(9, 3), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "gorm"});
  w.map.sides[1].team = "reds";
  CHECK_STR(w.load(R"([{"id":"a","when":{"battle":"start"},"if":[{"sees":"eirik","team":"red"},{"sees":"eirik","team":"reds"}],"do":[]}])"), "");
  CHECK_EQ(w.map.story->events[0].cond[0].team, 1);
  CHECK_EQ(w.map.story->events[0].cond[1].team, 1);
}

TEST(fog_loader_sees_refuses_an_unknown_team) {
  std::string e = seesError(R"({"sees":"gorm","team":"purple"})");
  CHECK(has(e, "unknown team") && has(e, "purple"));
}
TEST(fog_loader_sees_refuses_a_missing_team) {
  std::string e = seesError(R"({"sees":"gorm"})");
  CHECK(has(e, "team") && has(e, "missing"));
}
TEST(fog_loader_sees_refuses_a_team_that_is_not_text) {
  std::string e = seesError(R"({"sees":"gorm","team":3})");
  CHECK(has(e, "\"team\"") && has(e, "string"));
}
TEST(fog_loader_sees_refuses_an_unknown_unit) {
  std::string e = seesError(R"({"sees":"nobody","team":"blue"})");
  CHECK(has(e, "unknown unit") && has(e, "nobody"));
}
TEST(fog_loader_sees_refuses_a_bad_filter) {
  std::string e = seesError(R"({"sees":{"colour":"red"},"team":"blue"})");
  CHECK(has(e, "unknown key") && has(e, "colour"));
  e = seesError(R"({"sees":42,"team":"blue"})");
  CHECK(has(e, "unit filter"));
}
TEST(fog_loader_sees_refuses_other_keys_next_to_it) {
  std::string e = seesError(R"({"sees":"gorm","team":"blue","side":"red"})");
  CHECK(has(e, "unknown key") && has(e, "side"));
}
TEST(fog_loader_the_list_of_conditions_in_the_message_mentions_sees) {
  std::string e = seesError(R"({"nothing":1})");
  CHECK(has(e, "sees"));
}
