// Map weapons: what the loader reads and refuses (docs/SRPG.md "Map weapons"). Game: tests/fixtures/srpg-mapweapon.
#include "srpg_mapweapon_fixture.h"

using namespace ote;
using namespace mwtest;

namespace {

bool saysIn(const std::string& text, const char* part) {
  const bool ok = text.find(part) != std::string::npos;
  if (!ok) std::printf("  \"%s\" is not in: %s\n", part, text.c_str());
  return ok;
}

// The game's load message after one text of structures.json is swapped.
std::string withKinds(const char* name, const char* from, const char* to) {
  MCopy c(name);
  c.swap("data/structures.json", from, to);
  return c.gameError();
}

std::string withMap(const char* name, const char* from, const char* to) {
  MCopy c(name);
  c.swap("maps/field.json", from, to);
  return c.mapError("field");
}

}  // namespace

TEST(mapweapon_the_fixture_loads_and_the_kinds_are_read) {
  MCopy c("mapweapon_load");
  CHECK_STR(c.gameError(), "");
  CHECK_STR(c.mapError("field"), "");
  CHECK_STR(c.mapError("siege"), "");
  MapWorld w("mapweapon_kinds");
  const Database& db = w.db();
  const MapWeaponDef& ballista = db.structures[(size_t)db.findStructure("ballista")].mapWeapon;
  CHECK(ballista.has());
  CHECK_EQ(ballista.weapon, w.bolt());
  CHECK_EQ(ballista.uses, 3);
  CHECK(ballista.restricted);
  CHECK_EQ((int)ballista.byTypes.size(), 1);
  CHECK_EQ(ballista.byTypes[0], db.findUnitType("archer"));
  CHECK_EQ(ballista.becomes, db.findStructure("ballista_wreck"));
  CHECK(!ballista.ignoreRank);
  const MapWeaponDef& catapult = db.structures[(size_t)db.findStructure("catapult")].mapWeapon;
  CHECK(catapult.has() && catapult.uses == -1 && !catapult.restricted && catapult.becomes == -1);
  CHECK(!db.structures[(size_t)db.findStructure("tower")].mapWeapon.has());
  // A placed structure's own uses.
  CHECK_EQ(w.st("b1").weaponUses, 3);
  CHECK_EQ(w.st("b2").weaponUses, 1);
  CHECK_EQ(w.st("c1").weaponUses, -1);
}

TEST(mapweapon_the_loader_refuses_a_weapon_that_is_not_one) {
  CHECK(saysIn(withKinds("mw_r1", "\"weapon\": \"ballista_bolt\", \"uses\": 3", "\"weapon\": \"nope\", \"uses\": 3"), "unknown item \"nope\""));
  CHECK(saysIn(withKinds("mw_r2", "\"weapon\": \"ballista_bolt\", \"uses\": 3", "\"weapon\": \"potion\", \"uses\": 3"), "\"potion\", which is not a weapon"));
  CHECK(saysIn(withKinds("mw_r3", "\"uses\": 3", "\"uses\": 0"), "\"uses\" must be a whole number from 1 to 9999"));
  CHECK(saysIn(withKinds("mw_r4", "\"uses\": 3", "\"uses\": 10000"), "\"uses\" must be a whole number from 1 to 9999"));
  CHECK(saysIn(withKinds("mw_r5", "\"uses\": 3", "\"uses\": 2.5"), "\"uses\" must be a whole number from 1 to 9999"));
  CHECK(saysIn(withKinds("mw_r6", "\"glyph\": \"T\"", "\"glyph\": \"T\", \"uses\": 2"), "belongs with a \"weapon\""));
  CHECK(saysIn(withKinds("mw_r7", "\"glyph\": \"T\"", "\"glyph\": \"T\", \"becomes\": \"catapult\""), "belongs with a \"weapon\""));
  CHECK(saysIn(withKinds("mw_r8", "\"glyph\": \"T\"", "\"glyph\": \"T\", \"ignoreRank\": true"), "belongs with a \"weapon\""));
}

TEST(mapweapon_the_loader_refuses_bad_who_lists_and_bad_becomes) {
  CHECK(saysIn(withKinds("mw_b1", "\"tags\": [\"archer\"]", "\"types\": [\"dragon\"]"), "unknown unit type \"dragon\""));
  CHECK(saysIn(withKinds("mw_b2", "\"tags\": [\"archer\"]", "\"tags\": [\"flying\"]"), "tag \"flying\" is not used by any unit type"));
  CHECK(saysIn(withKinds("mw_b3", "{ \"tags\": [\"archer\"] }", "{ }"), "\"by\" is empty"));
  CHECK(saysIn(withKinds("mw_b4", "{ \"tags\": [\"archer\"] }", "{ \"who\": [\"archer\"] }"), "unknown key \"who\""));
  CHECK(saysIn(withKinds("mw_b5", "{ \"tags\": [\"archer\"] }", "[\"archer\"]"), "\"by\" must be"));
  CHECK(saysIn(withKinds("mw_b6", "\"becomes\": \"ballista_wreck\"", "\"becomes\": \"nowhere\""), "unknown structure \"nowhere\""));
  CHECK(saysIn(withKinds("mw_b7", "\"becomes\": \"ballista_wreck\"", "\"becomes\": \"ballista\""), "cannot name the structure itself"));
  CHECK(saysIn(withKinds("mw_b8", "\"id\": \"ballista_wreck\", \"name\": \"Broken Ballista\"", "\"id\": \"ballista_wreck\", \"width\": 2, \"name\": \"Broken Ballista\""),
               "not the same size"));
  // A kind nothing can stand on cannot have a weapon to use.
  CHECK(saysIn(withKinds("mw_b9", "\"glyph\": \"C\", \"weapon\"", "\"glyph\": \"C\", \"moveCost\": { \"foot\": 0 }, \"weapon\""), "no unit can stand on it"));
  CHECK(saysIn(withKinds("mw_b10", "\"glyph\": \"B\",", "\"glyph\": \"B\", \"moveCost\": { \"foot\": 0 },"), "none of the unit types in \"by\" can stand"));
}

TEST(mapweapon_the_loader_refuses_a_bad_placement_number) {
  CHECK(saysIn(withMap("mw_p1", "\"id\": \"b2\", \"uses\": 1", "\"id\": \"b2\", \"uses\": 0"), "\"uses\" must be a whole number from 1 to 9999"));
  CHECK(saysIn(withMap("mw_p2", "\"id\": \"b2\", \"uses\": 1", "\"id\": \"b2\", \"uses\": \"many\""), "\"uses\" must be a whole number"));
  CHECK(saysIn(withMap("mw_p3", "\"type\": \"tower\", \"x\": 6, \"y\": 0", "\"type\": \"tower\", \"x\": 6, \"y\": 0, \"uses\": 2"), "only belongs to a structure whose kind has a \"weapon\""));
  // A kind without a limit takes a placement's limit.
  CHECK_STR(withMap("mw_p4", "\"id\": \"c1\"", "\"id\": \"c1\", \"uses\": 4"), "");
}

TEST(mapweapon_goes_for_weapons_is_read_and_a_human_side_cannot_have_it) {
  MCopy c("mapweapon_goes");
  c.swap("maps/siege.json", "\"id\": \"g1\", \"side\": \"red\"", "\"id\": \"g1\", \"side\": \"red\", \"goesFor\": [\"weapons\"]");
  CHECK_STR(c.mapError("siege"), "");
  Database db;
  MapDef m;
  std::string err;
  CHECK(loadGame(c.dir, db, err) && loadMap(db, "siege", m, err));
  int found = 0;
  for (const MapUnitDef& u : m.units) found += (u.aiGoals & kAiWeapons) != 0;
  CHECK_EQ(found, 1);
  MCopy h("mapweapon_goes_human");
  h.swap("maps/siege.json", "\"id\": \"kn\", \"side\": \"blue\"", "\"id\": \"kn\", \"side\": \"blue\", \"goesFor\": [\"weapons\"]");
  CHECK(saysIn(h.mapError("siege"), "this unit is on a side a person plays"));
}
