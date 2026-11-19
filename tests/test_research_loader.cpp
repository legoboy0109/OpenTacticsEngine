// Loading upgrades, "research": true and data/research.json (docs/ECONOMY.md): the good case and
// one test per refusal, each checking the message names the thing that is wrong.
#include "posix_compat.h"

#include <cstdio>

#include "core/base/json.h"
#include "core/data/loader.h"
#include "test.h"

using namespace ote;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

struct Parts {
  std::string rules;  // the inside of game.json "rules": { ... }, or empty for none
  std::string research =
      R"({ "technologies": [
        { "id": "tools", "name": "Tools", "description": "Better hammers.", "cost": {"gold": 100}, "turns": 1,
          "bonus": { "types": ["grunt"], "stats": { "str": 1 } } },
        { "id": "masonry", "cost": {"gold": 200}, "turns": 2, "requires": ["tools"] },
        { "id": "armor", "cost": {"gold": 300}, "turns": 0, "unlocks": { "units": ["tank"] } } ] })";
  std::string structures =
      R"({ "structures": [
        { "id": "lab", "research": true },
        { "id": "city", "capturePoints": 10, "income": {"gold": 100},
          "upgrades": [ { "to": "town", "cost": {"gold": 300}, "requires": "masonry" }, { "to": "keep" } ] },
        { "id": "town", "capturePoints": 20 },
        { "id": "keep", "width": 1, "height": 1 },
        { "id": "castle", "width": 2, "height": 2 } ] })";
};

std::string makeGame(const char* name, const Parts& p) {
  std::string root = ote_test::tempDir() + "/res_" + name;
  mkdir(root.c_str(), 0755);
  mkdir((root + "/data").c_str(), 0755);
  mkdir((root + "/maps").c_str(), 0755);
  CHECK(writeTextFile(root + "/game.json",
                      "{ \"formatVersion\": 1, \"name\": \"G\", \"stats\": [{\"id\":\"hp\"},{\"id\":\"str\"},{\"id\":\"mov\"}],"
                      " \"roles\": {\"hp\":\"hp\",\"move\":\"mov\"}, \"moveTypes\": [{\"id\":\"foot\"}],"
                      " \"resources\": [{\"id\":\"gold\"}], \"formulas\": {\"damage\":\"a.str\"},"
                      " \"switches\": [{\"id\":\"s\"}]," + (p.rules.empty() ? std::string() : " \"rules\": {" + p.rules + "},") +
                      " \"maps\": [{\"id\":\"m\",\"file\":\"maps/m.json\"}] }"));
  CHECK(writeTextFile(root + "/data/terrain.json", "{ \"terrain\": [ {\"id\":\"plain\"} ] }"));
  CHECK(writeTextFile(root + "/data/weapons.json", "{ \"weapons\": [{\"id\":\"sword\"}] }"));
  CHECK(writeTextFile(root + "/data/units.json",
                      "{ \"units\": [{\"id\":\"grunt\",\"moveType\":\"foot\",\"stats\":{\"hp\":5,\"str\":2,\"mov\":3},\"weapons\":[\"sword\"]},"
                      "{\"id\":\"tank\",\"moveType\":\"foot\",\"stats\":{\"hp\":9,\"str\":4,\"mov\":4},\"weapons\":[\"sword\"]}] }"));
  std::string rp = root + "/data/research.json", sp = root + "/data/structures.json";
  if (p.research.empty()) std::remove(rp.c_str());
  else CHECK(writeTextFile(rp, p.research));
  if (p.structures.empty()) std::remove(sp.c_str());
  else CHECK(writeTextFile(sp, p.structures));
  CHECK(writeTextFile(root + "/maps/m.json",
                      "{ \"grid\": {\"type\":\"square\",\"width\":3,\"height\":1}, \"legend\": {\".\":\"plain\"}, \"tiles\": [\"...\"],"
                      " \"sides\": [{\"id\":\"a\"},{\"id\":\"b\"}],"
                      " \"units\": [{\"type\":\"grunt\",\"side\":\"a\",\"x\":0,\"y\":0},{\"type\":\"grunt\",\"side\":\"b\",\"x\":2,\"y\":0}] }"));
  return root;
}

std::string problem(const char* name, const Parts& p) {
  Database db;
  std::string err;
  return loadGame(makeGame(name, p), db, err) ? "" : err;
}

Parts withResearch(const char* text) {
  Parts p;
  p.structures = R"({ "structures": [ { "id": "lab", "research": true } ] })";  // the default ones name technologies
  p.research = text;
  return p;
}
Parts withStructures(const char* text) {
  Parts p;
  p.structures = text;
  return p;
}

// Checks the refusal and that the message points at what is wrong.
#define REFUSES(testName, parts, ...)                      \
  TEST(testName) {                                         \
    std::string err = problem(#testName, parts);           \
    CHECK(!err.empty());                                   \
    const char* needles[] = {__VA_ARGS__};                 \
    for (const char* n : needles)                          \
      if (!has(err, n)) {                                  \
        std::printf("  message lacks \"%s\": %s\n", n, err.c_str()); \
        CHECK(false);                                      \
      }                                                    \
  }

}  // namespace

TEST(research_loader_reads_everything) {
  Database db;
  std::string err;
  CHECK(loadGame(makeGame("ok", Parts()), db, err));
  CHECK(err.empty());
  CHECK_EQ(db.research.size(), size_t(3));
  const TechnologyDef& tools = db.research[0];
  CHECK_STR(tools.id, "tools");
  CHECK_STR(tools.name, "Tools");
  CHECK_STR(tools.description, "Better hammers.");
  CHECK_EQ(tools.cost.size(), size_t(1));
  CHECK_EQ(tools.cost[0].amount, 100);
  CHECK_EQ(tools.turns, 1);
  CHECK_EQ(tools.bonusTypes.size(), size_t(1));
  CHECK_EQ(tools.bonusTypes[0], db.findUnitType("grunt"));
  CHECK_EQ(tools.bonus.size(), db.stats.size());
  CHECK_EQ(tools.bonus[(size_t)db.findStat("str")], 1);
  const TechnologyDef& masonry = db.research[1];
  CHECK_STR(masonry.name, "masonry");  // the id when there is no name
  CHECK_EQ(masonry.needs.size(), size_t(1));
  CHECK_EQ(masonry.needs[0], 0);
  CHECK(masonry.bonus.empty() && masonry.unlocksUnits.empty());
  CHECK_EQ(db.research[2].turns, 0);
  CHECK_EQ(db.research[2].unlocksUnits[0], db.findUnitType("tank"));
  CHECK_EQ(db.findTechnology("armor"), 2);
  CHECK_EQ(db.findTechnology("nope"), -1);
  // Structures.
  CHECK(db.structures[(size_t)db.findStructure("lab")].research);
  CHECK(!db.structures[(size_t)db.findStructure("city")].research);
  const StructureDef& city = db.structures[(size_t)db.findStructure("city")];
  CHECK_EQ(city.upgrades.size(), size_t(2));
  CHECK_EQ(city.upgrades[0].to, db.findStructure("town"));
  CHECK_EQ(city.upgrades[0].cost[0].amount, 300);
  CHECK_EQ(city.upgrades[0].needs, db.findTechnology("masonry"));
  CHECK(city.upgrades[1].cost.empty());  // a free upgrade is allowed
  CHECK_EQ(city.upgrades[1].needs, -1);
}

TEST(research_loader_a_game_without_the_file_has_no_research) {
  Parts p;
  p.research = "";
  p.structures = R"({ "structures": [ { "id": "lab" } ] })";
  Database db;
  std::string err;
  CHECK(loadGame(makeGame("none", p), db, err));
  CHECK(db.research.empty());
}

TEST(research_loader_a_unit_unlocked_by_two_technologies_loads) {
  Parts p = withResearch(
      R"({ "technologies": [ { "id": "a", "unlocks": {"units": ["tank"]} }, { "id": "b", "unlocks": {"units": ["tank"]} } ] })");
  CHECK_STR(problem("twounlocks", p), "");
}

TEST(research_loader_turns_at_the_limits_load) {
  CHECK_STR(problem("turns0", withResearch(R"({ "technologies": [ { "id": "a", "turns": 0 } ] })")), "");
  CHECK_STR(problem("turns99", withResearch(R"({ "technologies": [ { "id": "a", "turns": 99 } ] })")), "");
}

// ---- research.json ----
REFUSES(research_loader_refuses_a_file_that_is_not_an_object, withResearch("[]"), "research.json")
REFUSES(research_loader_refuses_unknown_top_level_keys, withResearch(R"({ "technologies": [ {"id":"a"} ], "extras": 1 })"), "unknown key \"extras\"")
REFUSES(research_loader_refuses_a_missing_technologies_list, withResearch("{}"), "technologies")
REFUSES(research_loader_refuses_an_empty_list, withResearch(R"({ "technologies": [] })"), "empty")
REFUSES(research_loader_refuses_an_entry_that_is_not_an_object, withResearch(R"({ "technologies": [ 5 ] })"), "technologies[0]", "object")
REFUSES(research_loader_refuses_a_missing_id, withResearch(R"({ "technologies": [ {} ] })"), "\"id\"")
REFUSES(research_loader_refuses_an_empty_id, withResearch(R"({ "technologies": [ {"id":""} ] })"), "must not be empty")
REFUSES(research_loader_refuses_duplicate_ids, withResearch(R"({ "technologies": [ {"id":"a"}, {"id":"a"} ] })"), "technology \"a\"", "duplicate")
REFUSES(research_loader_refuses_unknown_keys_in_a_technology, withResearch(R"({ "technologies": [ {"id":"a","price":5} ] })"), "technology \"a\"", "unknown key \"price\"")
REFUSES(research_loader_refuses_an_unknown_resource_in_the_cost, withResearch(R"({ "technologies": [ {"id":"a","cost":{"mana":5}} ] })"), "technology \"a\"", "mana")
REFUSES(research_loader_refuses_a_negative_cost, withResearch(R"({ "technologies": [ {"id":"a","cost":{"gold":-5}} ] })"), "negative")
REFUSES(research_loader_refuses_a_cost_that_is_not_a_number, withResearch(R"({ "technologies": [ {"id":"a","cost":{"gold":"lots"}} ] })"), "whole number")
REFUSES(research_loader_refuses_turns_below_zero, withResearch(R"({ "technologies": [ {"id":"a","turns":-1} ] })"), "\"turns\"", "0 to 99")
REFUSES(research_loader_refuses_turns_above_99, withResearch(R"({ "technologies": [ {"id":"a","turns":100} ] })"), "\"turns\"", "0 to 99")
REFUSES(research_loader_refuses_fractional_turns, withResearch(R"({ "technologies": [ {"id":"a","turns":1.5} ] })"), "\"turns\"", "whole number")
REFUSES(research_loader_refuses_unknown_prerequisites, withResearch(R"({ "technologies": [ {"id":"a","requires":["zzz"]} ] })"), "technology \"a\"", "\"requires\"", "zzz")
REFUSES(research_loader_refuses_a_prerequisite_listed_twice, withResearch(R"({ "technologies": [ {"id":"a"}, {"id":"b","requires":["a","a"]} ] })"), "twice")
REFUSES(research_loader_refuses_a_technology_that_requires_itself, withResearch(R"({ "technologies": [ {"id":"a","requires":["a"]} ] })"), "loop", "a -> a")
REFUSES(research_loader_refuses_prerequisite_loops, withResearch(R"({ "technologies": [ {"id":"a","requires":["c"]}, {"id":"b","requires":["a"]}, {"id":"c","requires":["b"]} ] })"),
        "loop", "a -> c -> b -> a")
REFUSES(research_loader_refuses_unlocking_an_unknown_unit_type, withResearch(R"({ "technologies": [ {"id":"a","unlocks":{"units":["dragon"]}} ] })"), "\"unlocks\"", "dragon")
REFUSES(research_loader_refuses_unknown_keys_in_unlocks, withResearch(R"({ "technologies": [ {"id":"a","unlocks":{"structures":["x"]}} ] })"), "\"unlocks\"", "unknown key \"structures\"")
REFUSES(research_loader_refuses_a_unit_listed_twice_in_unlocks, withResearch(R"({ "technologies": [ {"id":"a","unlocks":{"units":["tank","tank"]}} ] })"), "twice")
REFUSES(research_loader_refuses_a_bonus_for_an_unknown_type, withResearch(R"({ "technologies": [ {"id":"a","bonus":{"types":["dragon"],"stats":{"str":1}}} ] })"), "\"bonus\"", "dragon")
REFUSES(research_loader_refuses_a_bonus_to_an_unknown_stat, withResearch(R"({ "technologies": [ {"id":"a","bonus":{"stats":{"luck":1}}} ] })"), "\"bonus\"", "luck")
REFUSES(research_loader_refuses_a_bonus_without_stats, withResearch(R"({ "technologies": [ {"id":"a","bonus":{"types":["grunt"]}} ] })"), "\"bonus\"", "stats")
REFUSES(research_loader_refuses_a_bonus_that_changes_nothing, withResearch(R"({ "technologies": [ {"id":"a","bonus":{"stats":{"str":0}}} ] })"), "at least one stat")
REFUSES(research_loader_refuses_unknown_keys_in_a_bonus, withResearch(R"({ "technologies": [ {"id":"a","bonus":{"stats":{"str":1},"tags":["x"]}} ] })"), "\"bonus\"", "unknown key \"tags\"")
REFUSES(research_loader_refuses_a_non_text_name, withResearch(R"({ "technologies": [ {"id":"a","name":5} ] })"), "\"name\"")

// ---- structures.json ----
REFUSES(upgrade_loader_refuses_research_that_is_not_true_or_false, withStructures(R"({ "structures": [ {"id":"lab","research":"yes"} ] })"), "\"research\"", "true or false")
REFUSES(upgrade_loader_refuses_upgrades_that_are_not_a_list, withStructures(R"({ "structures": [ {"id":"a","upgrades":{}} ] })"), "\"upgrades\"", "list")
REFUSES(upgrade_loader_refuses_an_entry_that_is_not_an_object, withStructures(R"({ "structures": [ {"id":"a","upgrades":["town"]} ] })"), "structure \"a\"", "entry 1", "object")
REFUSES(upgrade_loader_refuses_a_missing_to, withStructures(R"({ "structures": [ {"id":"a","upgrades":[{"cost":{"gold":5}}]} ] })"), "\"to\"")
REFUSES(upgrade_loader_refuses_an_unknown_target, withStructures(R"({ "structures": [ {"id":"a","upgrades":[{"to":"zzz"}]} ] })"), "structure \"a\"", "unknown structure \"zzz\"")
REFUSES(upgrade_loader_refuses_upgrading_to_itself, withStructures(R"({ "structures": [ {"id":"a","upgrades":[{"to":"a"}]} ] })"), "to itself")
REFUSES(upgrade_loader_refuses_a_different_footprint, withStructures(R"({ "structures": [ {"id":"a","upgrades":[{"to":"big"}]}, {"id":"big","width":2,"height":2} ] })"),
        "structure \"a\"", "\"big\" is 2 x 2", "keeps the footprint")
REFUSES(upgrade_loader_refuses_the_same_target_twice, withStructures(R"({ "structures": [ {"id":"a","upgrades":[{"to":"b"},{"to":"b"}]}, {"id":"b"} ] })"), "listed twice")
REFUSES(upgrade_loader_refuses_an_unknown_resource_in_the_cost, withStructures(R"({ "structures": [ {"id":"a","upgrades":[{"to":"b","cost":{"mana":1}}]}, {"id":"b"} ] })"), "mana")
REFUSES(upgrade_loader_refuses_a_negative_cost, withStructures(R"({ "structures": [ {"id":"a","upgrades":[{"to":"b","cost":{"gold":-1}}]}, {"id":"b"} ] })"), "negative")
REFUSES(upgrade_loader_refuses_an_unknown_technology, withStructures(R"({ "structures": [ {"id":"a","upgrades":[{"to":"b","requires":"magic"}]}, {"id":"b"} ] })"), "\"requires\"", "magic")
REFUSES(upgrade_loader_refuses_a_requirement_when_the_game_has_no_research,
        []() {
          Parts p = withStructures(R"({ "structures": [ {"id":"a","upgrades":[{"to":"b","requires":"magic"}]}, {"id":"b"} ] })");
          p.research = "";
          return p;
        }(),
        "magic", "no data/research.json")
REFUSES(upgrade_loader_refuses_a_requirement_that_is_not_text, withStructures(R"({ "structures": [ {"id":"a","upgrades":[{"to":"b","requires":["tools"]}]}, {"id":"b"} ] })"), "\"requires\"")
REFUSES(upgrade_loader_refuses_unknown_keys_in_an_upgrade, withStructures(R"({ "structures": [ {"id":"a","upgrades":[{"to":"b","price":1}]}, {"id":"b"} ] })"), "unknown key \"price\"")

TEST(upgrade_loader_accepts_a_target_that_comes_later_and_chains) {
  CHECK_STR(problem("chain", withStructures(R"({ "structures": [
    {"id":"a","upgrades":[{"to":"b"}]}, {"id":"b","upgrades":[{"to":"c"}]}, {"id":"c"} ] })")), "");
}

// ---- game.json "rules.research" and "rules.fog" (loader_options.cpp) ----

namespace {
Parts withRules(const char* text) {
  Parts p;
  p.rules = text;
  return p;
}
}  // namespace

TEST(option_rules_default_to_no_structure_needed_and_ambush_ends_the_turn) {
  Database db;
  std::string err;
  CHECK(loadGame(makeGame("opt_default", Parts()), db, err));
  CHECK(!db.rules.researchNeedsStructure);
  CHECK(db.rules.ambush == AmbushRule::EndsTurn);
}

TEST(option_rules_are_read) {
  Database db;
  std::string err;
  CHECK(loadGame(makeGame("opt_read", withRules(R"("research": {"needsStructure": true}, "fog": {"ambush": "canAct"})")), db, err));
  CHECK(db.rules.researchNeedsStructure);
  CHECK(db.rules.ambush == AmbushRule::CanAct);
  Database db2;
  CHECK(loadGame(makeGame("opt_read2", withRules(R"("research": {"needsStructure": false}, "fog": {"ambush": "endsTurn"})")), db2, err));
  CHECK(!db2.rules.researchNeedsStructure);
  CHECK(db2.rules.ambush == AmbushRule::EndsTurn);
  Database db3;
  CHECK(loadGame(makeGame("opt_read3", withRules(R"("research": {}, "fog": {})")), db3, err));
}

REFUSES(option_rules_refuse_unknown_research_key, withRules(R"("research": {"needsBuilding": true})"), "rules.research", "needsBuilding")
REFUSES(option_rules_refuse_a_research_flag_that_is_not_a_boolean, withRules(R"("research": {"needsStructure": "yes"})"), "needsStructure")
REFUSES(option_rules_refuse_unknown_fog_key, withRules(R"("fog": {"ambushes": "canAct"})"), "rules.fog", "ambushes")
REFUSES(option_rules_refuse_an_unknown_ambush_value, withRules(R"("fog": {"ambush": "never"})"), "ambush", "endsTurn", "canAct", "never")
REFUSES(option_rules_refuse_a_research_block_that_is_not_an_object, withRules(R"("research": true)"), "research")
