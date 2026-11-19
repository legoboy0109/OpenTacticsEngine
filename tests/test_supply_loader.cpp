// Loading the repair and supply data (docs/ECONOMY.md "Repair and supply"): game.json "rules.supply",
// structures "repairs", units "fuel" / "fuelPerTurn" / "supplies", weapons "ammo". The good case and one test
// per refusal, each checking that the message names what is wrong.
#include "posix_compat.h"

#include <cstdio>

#include "core/base/json.h"
#include "core/data/loader.h"
#include "test.h"

using namespace ote;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

struct Parts {
  std::string rules = R"("rules": { "supply": { "repair": { "amount": 20, "costShare": 50 }, "structures": true } },)";
  std::string weapons = R"({ "weapons": [ {"id":"sword"}, {"id":"cannon","ammo":6} ] })";
  std::string units =
      R"({ "units": [
        {"id":"grunt","moveType":"foot","stats":{"hp":5,"str":2,"mov":3},"weapons":["sword"],"tags":["infantry"]},
        {"id":"plane","moveType":"foot","stats":{"hp":5,"str":2,"mov":3},"weapons":["cannon"],"fuel":60,"fuelPerTurn":5,"tags":["air"]},
        {"id":"truck","moveType":"foot","stats":{"hp":5,"str":0,"mov":4},"fuel":40,"supplies":true} ] })";
  std::string structures =
      R"({ "structures": [ { "id": "depot" }, { "id": "airfield", "repairs": ["air", "truck"] }, { "id": "none", "repairs": [] } ] })";
  std::string items;  // data/items.json, empty = no file
};

std::string makeGame(const char* name, const Parts& p) {
  std::string root = ote_test::tempDir() + "/sup_" + name;
  mkdir(root.c_str(), 0755);
  mkdir((root + "/data").c_str(), 0755);
  mkdir((root + "/maps").c_str(), 0755);
  CHECK(writeTextFile(root + "/game.json",
                      "{ \"formatVersion\": 1, \"name\": \"G\", \"stats\": [{\"id\":\"hp\"},{\"id\":\"str\"},{\"id\":\"mov\"}],"
                      " \"roles\": {\"hp\":\"hp\",\"move\":\"mov\"}, \"moveTypes\": [{\"id\":\"foot\"}],"
                      " \"resources\": [{\"id\":\"gold\"}], \"formulas\": {\"damage\":\"a.str\"}, " + p.rules +
                      " \"maps\": [{\"id\":\"m\",\"file\":\"maps/m.json\"}] }"));
  CHECK(writeTextFile(root + "/data/terrain.json", "{ \"terrain\": [ {\"id\":\"plain\"} ] }"));
  CHECK(writeTextFile(root + "/data/weapons.json", p.weapons));
  CHECK(writeTextFile(root + "/data/units.json", p.units));
  std::string sp = root + "/data/structures.json", ip = root + "/data/items.json";
  if (p.structures.empty()) std::remove(sp.c_str());
  else CHECK(writeTextFile(sp, p.structures));
  if (p.items.empty()) std::remove(ip.c_str());
  else CHECK(writeTextFile(ip, p.items));
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

Parts withRules(const char* supplyBody) {
  Parts p;
  p.rules = std::string("\"rules\": { \"supply\": ") + supplyBody + " },";
  return p;
}
Parts withUnits(const char* unitBody) {
  Parts p;
  p.units = std::string(R"({ "units": [ {"id":"grunt","moveType":"foot","stats":{"hp":5,"str":2,"mov":3},"weapons":["sword"],)") +
            unitBody + " } ] }";
  return p;
}
Parts withWeapon(const char* weaponBody) {
  Parts p;
  p.weapons = std::string(R"({ "weapons": [ {"id":"sword",)") + weaponBody + "} ] }";
  p.units = R"({ "units": [ {"id":"grunt","moveType":"foot","stats":{"hp":5,"str":2,"mov":3},"weapons":["sword"]} ] })";
  return p;
}
Parts withStructures(const char* text) {
  Parts p;
  p.structures = text;
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

TEST(supply_loader_reads_everything) {
  Database db;
  std::string err;
  CHECK(loadGame(makeGame("ok", Parts()), db, err));
  CHECK(err.empty());
  const SupplyRules& s = db.rules.supply;
  CHECK(s.repair && s.structures);
  CHECK_EQ(s.repairAmount, 20);
  CHECK_EQ(s.repairCostShare, 50);
  CHECK_EQ(db.weapons[(size_t)db.findWeapon("cannon")].ammo, 6);
  CHECK_EQ(db.weapons[(size_t)db.findWeapon("sword")].ammo, 0);  // unlimited
  const UnitTypeDef& plane = db.unitTypes[(size_t)db.findUnitType("plane")];
  CHECK_EQ(plane.fuel, 60);
  CHECK_EQ(plane.fuelPerTurn, 5);
  CHECK(!plane.supplies);
  const UnitTypeDef& truck = db.unitTypes[(size_t)db.findUnitType("truck")];
  CHECK(truck.supplies);
  CHECK_EQ(truck.fuel, 40);
  CHECK_EQ(truck.fuelPerTurn, 0);
  CHECK_EQ(db.unitTypes[0].fuel, 0);
  const StructureDef& depot = db.structures[(size_t)db.findStructure("depot")];
  CHECK(depot.repairsAll && depot.repairs.empty());
  const StructureDef& airfield = db.structures[(size_t)db.findStructure("airfield")];
  CHECK(!airfield.repairsAll);
  CHECK_EQ(airfield.repairs.size(), size_t(2));  // a tag and a unit type id
  CHECK(!db.structures[(size_t)db.findStructure("none")].repairsAll);  // [] = repairs nothing
}

TEST(supply_loader_a_game_without_any_of_it_has_nothing) {
  Parts p;
  p.rules = "";
  p.weapons = R"({ "weapons": [ {"id":"sword"} ] })";
  p.units = R"({ "units": [ {"id":"grunt","moveType":"foot","stats":{"hp":5,"str":2,"mov":3},"weapons":["sword"]} ] })";
  p.structures = "";
  Database db;
  std::string err;
  CHECK(loadGame(makeGame("none", p), db, err));
  CHECK(!db.rules.supply.repair && !db.rules.supply.structures);
  CHECK_EQ(db.rules.supply.repairCostShare, 100);
}

TEST(supply_loader_cost_share_defaults_to_100_and_structures_alone_is_fine) {
  Database db;
  std::string err;
  CHECK(loadGame(makeGame("share", withRules(R"({ "repair": { "amount": 100 } })")), db, err));
  CHECK_EQ(db.rules.supply.repairCostShare, 100);
  CHECK(db.rules.supply.repair && !db.rules.supply.structures);
  CHECK(loadGame(makeGame("only", withRules(R"({ "structures": true })")), db, err));
  CHECK(!db.rules.supply.repair && db.rules.supply.structures);
  CHECK(loadGame(makeGame("zero", withRules(R"({ "repair": { "amount": 1, "costShare": 0 } })")), db, err));
  CHECK_EQ(db.rules.supply.repairCostShare, 0);
}

// ---- rules.supply ----
REFUSES(supply_loader_refuses_amount_zero, withRules(R"({ "repair": { "amount": 0 } })"), "rules.supply.repair", "\"amount\"", "1 to 100")
REFUSES(supply_loader_refuses_amount_over_100, withRules(R"({ "repair": { "amount": 101 } })"), "\"amount\"", "1 to 100")
REFUSES(supply_loader_refuses_amount_fraction, withRules(R"({ "repair": { "amount": 2.5 } })"), "\"amount\"", "whole number")
REFUSES(supply_loader_refuses_missing_amount, withRules(R"({ "repair": { "costShare": 10 } })"), "rules.supply.repair", "\"amount\"")
REFUSES(supply_loader_refuses_cost_share_over_100, withRules(R"({ "repair": { "amount": 10, "costShare": 101 } })"), "\"costShare\"", "0 to 100")
REFUSES(supply_loader_refuses_negative_cost_share, withRules(R"({ "repair": { "amount": 10, "costShare": -1 } })"), "\"costShare\"")
REFUSES(supply_loader_refuses_unknown_supply_key, withRules(R"({ "repairs": { "amount": 10 } })"), "rules.supply", "unknown key \"repairs\"")
REFUSES(supply_loader_refuses_unknown_repair_key, withRules(R"({ "repair": { "amount": 10, "speed": 3 } })"), "rules.supply.repair", "unknown key \"speed\"")
REFUSES(supply_loader_refuses_repair_not_an_object, withRules(R"({ "repair": 20 })"), "\"repair\"", "object")
REFUSES(supply_loader_refuses_structures_not_a_bool, withRules(R"({ "structures": "yes" })"), "\"structures\"", "true or false")
REFUSES(supply_loader_refuses_supply_not_an_object, withRules("true"), "\"supply\"", "object")

// ---- structures.json "repairs" ----
REFUSES(supply_loader_refuses_unknown_repairs_entry,
        withStructures(R"({ "structures": [ { "id": "dock", "repairs": ["ship"] } ] })"), "dock", "\"repairs\"", "\"ship\"", "unit type id nor a tag")
REFUSES(supply_loader_refuses_repairs_not_a_list,
        withStructures(R"({ "structures": [ { "id": "dock", "repairs": "air" } ] })"), "dock", "\"repairs\"", "list")
REFUSES(supply_loader_refuses_repairs_entry_not_text,
        withStructures(R"({ "structures": [ { "id": "dock", "repairs": [3] } ] })"), "dock", "\"repairs\"", "string")

// ---- units.json ----
REFUSES(supply_loader_refuses_fuel_zero, withUnits(R"("fuel": 0)"), "\"fuel\"", "1 to 999")
REFUSES(supply_loader_refuses_fuel_over_999, withUnits(R"("fuel": 1000)"), "\"fuel\"", "1 to 999")
REFUSES(supply_loader_refuses_fuel_not_whole, withUnits(R"("fuel": 10.5)"), "\"fuel\"", "whole number")
REFUSES(supply_loader_refuses_fuel_per_turn_without_fuel, withUnits(R"("fuelPerTurn": 3)"), "\"fuelPerTurn\"", "needs \"fuel\"")
REFUSES(supply_loader_refuses_fuel_per_turn_zero, withUnits(R"("fuel": 10, "fuelPerTurn": 0)"), "\"fuelPerTurn\"", "1 to 999")
REFUSES(supply_loader_refuses_fuel_per_turn_that_empties_the_tank, withUnits(R"("fuel": 10, "fuelPerTurn": 10)"), "\"fuelPerTurn\"", "less than \"fuel\"")
REFUSES(supply_loader_refuses_supplies_not_a_bool, withUnits(R"("supplies": 1)"), "\"supplies\"", "true or false")

// ---- weapons.json ----
REFUSES(supply_loader_refuses_ammo_zero, withWeapon(R"("ammo": 0)"), "sword", "\"ammo\"", "1 to 99")
REFUSES(supply_loader_refuses_ammo_over_99, withWeapon(R"("ammo": 100)"), "sword", "\"ammo\"", "1 to 99")
REFUSES(supply_loader_refuses_ammo_not_whole, withWeapon(R"("ammo": "six")"), "sword", "\"ammo\"", "whole number")

TEST(supply_loader_refuses_ammo_on_a_weapon_carried_as_an_item) {
  Parts p;
  p.rules = R"("rules": { "items": { "weapons": "carried" }, "supply": { "structures": true } },)";
  p.items = R"({ "items": [ { "id": "blade", "kind": "weapon", "weapon": "cannon", "uses": 5 } ] })";
  std::string err = problem("ammo_item", p);
  CHECK(!err.empty());
  CHECK(has(err, "cannon"));
  CHECK(has(err, "\"ammo\""));
  CHECK(has(err, "blade"));
  CHECK(has(err, "uses"));
  // The same weapon without ammo is fine as an item.
  p.weapons = R"({ "weapons": [ {"id":"sword"}, {"id":"cannon"} ] })";
  p.structures = "";
  p.units = R"({ "units": [ {"id":"grunt","moveType":"foot","stats":{"hp":5,"str":2,"mov":3},"weapons":["sword"]} ] })";
  CHECK_STR(problem("ammo_item_ok", p), "");
}
