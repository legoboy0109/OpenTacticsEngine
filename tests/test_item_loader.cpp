// Loading items (docs/ITEMS.md "Data"): data/items.json, rules.items, and the
// item lists of unit types, map units, map sides and structures.
#include "posix_compat.h"

#include <cstdio>

#include "core/base/json.h"
#include "core/data/loader.h"
#include "test.h"

using namespace ote;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

const char* kItems =
    "{ \"items\": ["
    " {\"id\":\"potion\",\"name\":\"Potion\",\"description\":\"Restores 10 HP.\",\"kind\":\"usable\",\"uses\":3,"
    "  \"price\":{\"gold\":300},\"heal\":10},"
    " {\"id\":\"elixir\",\"kind\":\"usable\",\"target\":\"ally\",\"heal\":\"full\",\"raise\":{\"str\":2}},"
    " {\"id\":\"blade\",\"kind\":\"weapon\",\"weapon\":\"sword\",\"uses\":40,\"price\":{\"gold\":500}},"
    " {\"id\":\"ring\",\"kind\":\"accessory\",\"bonus\":{\"str\":1,\"mov\":1}} ] }";

struct Parts {
  std::string rulesItems = "\"weapons\":\"carried\",\"inventorySize\":3,\"storage\":\"leader\"";
  std::string items = kItems;
  std::string unitExtra = ",\"wields\":[\"blade\"],\"items\":[\"blade\",{\"item\":\"potion\",\"uses\":2}]";
  std::string structures = "{ \"structures\": [{\"id\":\"shop\",\"sells\":[\"potion\",\"blade\"]}] }";
  std::string mapUnit0 = ",\"items\":[{\"item\":\"ring\",\"drop\":true}]";
  std::string mapSide0 = ",\"storage\":[\"potion\",{\"item\":\"blade\",\"uses\":7}]";
};

std::string makeGame(const char* name, const Parts& p) {
  std::string root = ote_test::tempDir() + "/item_" + name;
  mkdir(root.c_str(), 0755);
  mkdir((root + "/data").c_str(), 0755);
  mkdir((root + "/maps").c_str(), 0755);
  CHECK(writeTextFile(root + "/game.json",
                      "{ \"formatVersion\": 1, \"name\": \"G\", \"stats\": [{\"id\":\"hp\"},{\"id\":\"str\"},{\"id\":\"mov\"}],"
                      " \"roles\": {\"hp\":\"hp\",\"move\":\"mov\"}, \"moveTypes\": [{\"id\":\"foot\"}],"
                      " \"resources\": [{\"id\":\"gold\"}], \"formulas\": {\"damage\":\"a.str\"}," +
                      std::string(p.rulesItems.empty() ? "" : " \"rules\": {\"items\": {" + p.rulesItems + "}},") +
                      " \"maps\": [{\"id\":\"m\",\"file\":\"maps/m.json\"}] }"));
  CHECK(writeTextFile(root + "/data/terrain.json", "{ \"terrain\": [ {\"id\":\"plain\"} ] }"));
  CHECK(writeTextFile(root + "/data/weapons.json",
                      "{ \"weapons\": [{\"id\":\"sword\",\"group\":\"blade\"},{\"id\":\"club\"}] }"));
  CHECK(writeTextFile(root + "/data/units.json",
                      "{ \"units\": [{\"id\":\"grunt\",\"moveType\":\"foot\",\"stats\":{\"hp\":5,\"str\":2,\"mov\":3}" +
                          p.unitExtra + "}] }"));
  std::string ip = root + "/data/items.json", sp = root + "/data/structures.json";
  if (p.items.empty()) std::remove(ip.c_str());
  else CHECK(writeTextFile(ip, p.items));
  if (p.structures.empty()) std::remove(sp.c_str());
  else CHECK(writeTextFile(sp, p.structures));
  CHECK(writeTextFile(root + "/maps/m.json",
                      "{ \"grid\": {\"type\":\"square\",\"width\":3,\"height\":1}, \"legend\": {\".\":\"plain\"}, \"tiles\": [\"...\"],"
                      " \"sides\": [{\"id\":\"a\"" + p.mapSide0 + "},{\"id\":\"b\"}],"
                      " \"units\": [{\"type\":\"grunt\",\"side\":\"a\",\"x\":0,\"y\":0" + p.mapUnit0 + "},"
                      "            {\"type\":\"grunt\",\"side\":\"b\",\"x\":2,\"y\":0}] }"));
  return root;
}

// The error of loading the game and its map ("" = both load).
std::string problem(const char* name, const Parts& p) {
  Database db;
  MapDef map;
  std::string err;
  if (!loadGame(makeGame(name, p), db, err)) return err;
  if (!loadMap(db, "m", map, err)) return err;
  return "";
}

}  // namespace

TEST(item_loader_reads_everything) {
  Database db;
  MapDef map;
  std::string err;
  bool ok = loadGame(makeGame("ok", Parts()), db, err) && loadMap(db, "m", map, err);
  if (!ok) std::printf("  %s\n", err.c_str());
  CHECK(ok);
  if (!ok) return;
  CHECK(db.rules.items.carriedWeapons);
  CHECK_EQ(db.rules.items.inventorySize, 3);
  CHECK(db.rules.items.storage == ItemStorage::Leader);
  CHECK_EQ((int)db.items.size(), 4);
  const ItemDef& potion = db.items[0];
  CHECK(potion.kind == ItemKind::Usable && potion.uses == 3 && potion.heal == 10 && !potion.targetAlly);
  CHECK_STR(potion.description, "Restores 10 HP.");
  CHECK(potion.price.size() == 1 && potion.price[0].amount == 300);
  const ItemDef& elixir = db.items[1];
  CHECK(elixir.uses == -1 && elixir.heal == kHealFull && elixir.targetAlly && elixir.raise[1] == 2);
  CHECK(elixir.price.empty());
  CHECK(db.items[2].kind == ItemKind::Weapon && db.items[2].weapon == db.findWeapon("sword"));
  CHECK(db.items[3].kind == ItemKind::Accessory && db.items[3].bonus[2] == 1);
  CHECK(db.canWield(0, db.findWeapon("sword")));   // by group
  CHECK(!db.canWield(0, db.findWeapon("club")));
  const UnitTypeDef& grunt = db.unitTypes[0];
  CHECK(grunt.items.size() == 2 && grunt.items[0].item == 2 && grunt.items[0].uses == 40);
  CHECK(grunt.items[1].item == 0 && grunt.items[1].uses == 2);
  CHECK(db.structures[0].sells.size() == 2);
  CHECK(map.units[0].hasItems && map.units[0].items.size() == 1 && map.units[0].items[0].drop);
  CHECK(!map.units[1].hasItems);
  CHECK(map.sides[0].storage.size() == 2 && map.sides[0].storage[1].uses == 7);
}

TEST(item_loader_a_game_without_items_loads_as_before) {
  Parts p;
  p.rulesItems = p.items = p.unitExtra = p.structures = p.mapUnit0 = p.mapSide0 = "";
  Database db;
  std::string err;
  CHECK(loadGame(makeGame("none", p), db, err));
  CHECK(db.items.empty() && !db.rules.items.carriedWeapons && db.rules.items.storage == ItemStorage::Off);
  CHECK_EQ(db.rules.items.inventorySize, 5);
}

TEST(item_loader_says_what_is_wrong) {
  auto with = [](void (*change)(Parts&)) { Parts p; change(p); return p; };
  std::string e;
  e = problem("e1", with([](Parts& p) { p.rulesItems = "\"weapons\":\"fixed\""; p.mapSide0 = ""; }));
  CHECK(has(e, "item \"blade\"") && has(e, "carried"));
  e = problem("e2", with([](Parts& p) { p.rulesItems = "\"weapons\":\"both\""; }));
  CHECK(has(e, "rules.items") && has(e, "\"fixed\" or \"carried\""));
  e = problem("e3", with([](Parts& p) { p.rulesItems = "\"inventorySize\":0"; }));
  CHECK(has(e, "inventorySize"));
  e = problem("e4", with([](Parts& p) { p.rulesItems = "\"storage\":\"bank\""; }));
  CHECK(has(e, "\"storage\" must be"));
  e = problem("e5", with([](Parts& p) { p.items = "{ \"items\": [{\"id\":\"x\",\"kind\":\"usable\"}] }"; }));
  CHECK(has(e, "item \"x\"") && has(e, "needs \"heal\" and/or \"raise\""));
  e = problem("e6", with([](Parts& p) { p.items = "{ \"items\": [{\"id\":\"x\",\"kind\":\"hat\"}] }"; }));
  CHECK(has(e, "\"kind\" must be"));
  e = problem("e7", with([](Parts& p) { p.items = "{ \"items\": [{\"id\":\"x\",\"kind\":\"accessory\",\"bonus\":{\"luck\":1}}] }"; }));
  CHECK(has(e, "unknown stat \"luck\""));
  e = problem("e8", with([](Parts& p) { p.items = "{ \"items\": [{\"id\":\"x\",\"kind\":\"accessory\",\"heal\":3,\"bonus\":{\"str\":1}}] }"; }));
  CHECK(has(e, "\"heal\" belongs to items of kind \"usable\""));
  e = problem("e9", with([](Parts& p) { p.items = "{ \"items\": [{\"id\":\"x\",\"kind\":\"weapon\",\"weapon\":\"axe\"}] }"; }));
  CHECK(has(e, "unknown weapon \"axe\""));
  e = problem("e10", with([](Parts& p) { p.items = "{ \"items\": [{\"id\":\"x\",\"kind\":\"usable\",\"heal\":1,\"uses\":0}] }"; }));
  CHECK(has(e, "\"uses\" must be from 1 to 999"));
  e = problem("e11", with([](Parts& p) { p.structures = "{ \"structures\": [{\"id\":\"shop\",\"sells\":[\"ring\"]}] }"; }));
  CHECK(has(e, "structure \"shop\"") && has(e, "no \"price\""));
  e = problem("e12", with([](Parts& p) { p.unitExtra = ",\"items\":[\"potion\",\"potion\",\"potion\",\"potion\"]"; }));
  CHECK(has(e, "unit \"grunt\"") && has(e, "can carry 3"));
  e = problem("e13", with([](Parts& p) { p.mapUnit0 = ",\"items\":[\"sock\"]"; }));
  CHECK(has(e, "unknown item \"sock\""));
  e = problem("e14", with([](Parts& p) { p.mapUnit0 = ",\"items\":[{\"item\":\"potion\",\"uses\":9}]"; }));
  CHECK(has(e, "more than the 3"));
  e = problem("e15", with([](Parts& p) { p.mapUnit0 = ",\"items\":[{\"item\":\"ring\",\"uses\":2}]"; }));
  CHECK(has(e, "unlimited uses"));
  e = problem("e16", with([](Parts& p) { p.mapSide0 = ",\"storage\":[{\"item\":\"potion\",\"drop\":true}]"; }));
  CHECK(has(e, "\"drop\" only means something on a unit"));
  e = problem("e17", with([](Parts& p) { p.rulesItems = "\"weapons\":\"carried\""; }));
  CHECK(has(e, "side \"a\"") && has(e, "no shared storage"));
  e = problem("e18", with([](Parts& p) { p.items = ""; p.structures = ""; p.mapUnit0 = p.mapSide0 = ""; }));
  CHECK(has(e, "the game has no items"));
}
