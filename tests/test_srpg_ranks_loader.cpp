// Weapon ranks and stat caps (docs/SRPG.md section 7): what the loaders read and every refusal with its message.
#include "srpg_ranks_fixture.h"

using namespace ote;
using namespace rankstest;

TEST(ranks_loader_reads_the_fixture) {
  Copy c("ranks_loader_ok");
  Database db;
  std::string err;
  CHECK(loadGame(c.dir, db, err));
  CHECK_EQ((int)db.ranks.ladder.size(), 4);
  CHECK(db.ranks.growByUse);
  CHECK_EQ((int)db.ranks.groups.size(), 2);  // sword, bow: in weapons.json order, "stick" has none
  CHECK_STR(db.ranks.groups[0], "sword");
  CHECK_EQ(db.weapons[(size_t)db.findWeapon("steel_sword")].rankNeed, 1);
  CHECK_EQ(db.weapons[(size_t)db.findWeapon("steel_sword")].rankXp, 2);
  CHECK_EQ(db.weapons[(size_t)db.findWeapon("bronze_sword")].rankNeed, -1);
  CHECK_EQ(db.weapons[(size_t)db.findWeapon("bronze_sword")].rankXp, 1);
  CHECK_EQ(db.weapons[(size_t)db.findWeapon("stick")].rankGroup, -1);
  CHECK_EQ(db.unitTypes[(size_t)db.findUnitType("knight")].rankStart[0], 7);  // "c"
  CHECK_EQ(db.characters[(size_t)db.findCharacter("kell")].rankStart[0], 4);  // a number is experience
  CHECK_EQ(db.statCaps[(size_t)db.findStat("str")], 10);
  CHECK_EQ(db.statCaps[(size_t)db.findStat("def")], -1);
  CHECK_EQ(db.unitTypes[(size_t)db.findUnitType("knight")].caps[(size_t)db.findStat("str")], 14);
  CHECK_EQ(db.characters[(size_t)db.findCharacter("mara")].capAdjust[(size_t)db.findStat("str")], 2);
}

TEST(ranks_loader_ladder_refusals) {
  struct Case { const char* text; const char* says; };
  const Case cases[] = {
      {R"({"ranks":[{"id":"e","xp":1}]})", "the first rank needs \"xp\": 0"},
      {R"({"ranks":[{"id":"e","xp":0},{"id":"d","xp":0}]})", "\"xp\" must be higher than the one of \"e\""},
      {R"({"ranks":[{"id":"e","xp":0},{"id":"E","xp":4}]})", "lowercase letters"},
      {R"({"ranks":[{"id":"e","xp":0},{"id":"e","xp":4}]})", "duplicate id"},
      {R"({"ranks":[{"id":"e","xp":0,"bonus":1}]})", "unknown key \"bonus\""},
      {R"({"ranks":[{"id":"e"}]})", "missing required \"xp\""},
      {R"({"ranks":[{"id":"e","xp":10000}]})", "\"xp\" must be from 0 to 9999"},
      {R"({"growth":"often","ranks":[{"id":"e","xp":0}]})", "\"growth\" must be \"fixed\" or \"use\""},
      {R"({"ranks":[{"id":"e","xp":0}],"mode":1})", "unknown key \"mode\""},
  };
  for (const Case& k : cases) {
    Copy c("ranks_loader_ladder");
    c.put("data/ranks.json", k.text);
    CHECK(saysIn(c.gameError(), k.says));
  }
  Copy many("ranks_loader_many");
  std::string list = R"({"ranks":[)";
  for (int i = 0; i < 13; ++i) list += std::string(i ? "," : "") + "{\"id\":\"r" + std::to_string(i) + "\",\"xp\":" + std::to_string(i) + "}";
  many.put("data/ranks.json", list + "]}");
  CHECK(saysIn(many.gameError(), "too many ranks (at most 12)"));
}

TEST(ranks_loader_need_carried_weapons) {
  Copy c("ranks_loader_fixed");
  c.swap("game.json", "\"weapons\": \"carried\"", "\"weapons\": \"fixed\"");
  CHECK(saysIn(c.gameError(), "weapon ranks need carried weapons"));
}

TEST(ranks_loader_weapon_keys) {
  {
    Copy c("ranks_loader_w1");
    c.swap("data/weapons.json", "\"rank\": \"d\", \"rankXp\": 2", "\"rank\": \"z\"");
    CHECK(saysIn(c.gameError(), "weapon \"steel_sword\": \"rank\" names unknown rank \"z\" (data/ranks.json)"));
  }
  {
    Copy c("ranks_loader_w2");
    c.swap("data/weapons.json", "\"id\": \"stick\",        \"name\": \"Stick\",", "\"id\": \"stick\", \"name\": \"Stick\", \"rank\": \"d\",");
    CHECK(saysIn(c.gameError(), "weapon \"stick\": \"rank\" needs a \"group\""));
  }
  {
    Copy c("ranks_loader_w3");
    c.swap("data/weapons.json", "\"rankXp\": 2", "\"rankXp\": 100");
    CHECK(saysIn(c.gameError(), "\"rankXp\" must be from 1 to 99"));
  }
  {
    Copy c("ranks_loader_w4");
    c.drop("data/ranks.json");
    CHECK(saysIn(c.gameError(), "\"rank\" is given but this game has no weapon ranks (data/ranks.json)"));
  }
}

TEST(ranks_loader_class_character_and_map_keys) {
  {
    Copy c("ranks_loader_u1");
    c.swap("data/units.json", "\"ranks\": { \"sword\": \"e\" }", "\"ranks\": { \"axe\": \"e\" }");
    CHECK(saysIn(c.gameError(), "unit \"fighter\": \"ranks\" names unknown weapon group \"axe\""));
  }
  {
    Copy c("ranks_loader_u2");
    c.swap("data/units.json", "\"ranks\": { \"sword\": \"e\" }", "\"ranks\": { \"sword\": \"q\" }");
    CHECK(saysIn(c.gameError(), "\"ranks\".\"sword\" names unknown rank \"q\""));
  }
  {
    Copy c("ranks_loader_u3");
    c.swap("data/units.json", "\"ranks\": { \"sword\": \"e\" }", "\"ranks\": { \"sword\": 13 }");
    CHECK(saysIn(c.gameError(), "must be a rank id or weapon experience from 0 to 12"));
  }
  {
    Copy c("ranks_loader_u4");
    c.swap("data/units.json", "\"caps\": { \"str\": 14 }", "\"caps\": { \"luck\": 14 }");
    CHECK(saysIn(c.gameError(), "\"caps\" names unknown stat \"luck\""));
  }
  {
    Copy c("ranks_loader_u5");
    c.swap("data/units.json", "\"caps\": { \"str\": 14 }", "\"caps\": { \"str\": 0 }");
    CHECK(saysIn(c.gameError(), "\"caps\".\"str\" must be a whole number from 1 to 99999"));
  }
  {
    Copy c("ranks_loader_c1");
    c.swap("data/characters.json", "\"ranks\": { \"sword\": 4 }", "\"ranks\": { \"sword\": \"zz\" }");
    CHECK(saysIn(c.gameError(), "character \"kell\": \"ranks\".\"sword\" names unknown rank \"zz\""));
  }
  {
    Copy c("ranks_loader_c2");
    c.swap("data/characters.json", "\"capAdjust\": { \"str\": 2 }", "\"capAdjust\": { \"luck\": 2 }");
    CHECK(c.gameError().find("luck") != std::string::npos);
  }
  {
    Copy c("ranks_loader_m1");
    c.swap("maps/field.json", "\"ranks\": { \"sword\": \"e\" }", "\"ranks\": { \"sword\": \"x\" }");
    CHECK(saysIn(c.mapError("field"), "\"ranks\".\"sword\" names unknown rank \"x\""));
  }
  {
    Copy c("ranks_loader_n1");  // ranks on a unit of a game that has none
    c.drop("data/ranks.json");
    c.swap("data/weapons.json", "\"rank\": \"d\", \"rankXp\": 2, ", "");
    c.swap("data/weapons.json", "\"rank\": \"b\", \"rankXp\": 3, ", "");
    c.swap("data/weapons.json", "\"rank\": \"c\", ", "");
    CHECK(saysIn(c.gameError(), "\"ranks\" is given but this game has no weapon ranks"));
  }
}

TEST(caps_loader_refusals) {
  struct Case { const char* text; const char* says; };
  const Case cases[] = {
      {R"({"caps":{"luck":9}})", "names unknown stat \"luck\""},
      {R"({"caps":{"str":0}})", "\"str\" must be a whole number from 1 to 99999"},
      {R"({"caps":{"str":1.5}})", "\"str\" must be a whole number"},
      {R"({"caps":{"str":9},"other":1})", "unknown key \"other\""},
      {R"({"cap":{"str":9}})", "unknown key \"cap\""},
      {R"({"caps":[]})", "\"caps\" must be an object"},
  };
  for (const Case& k : cases) {
    Copy c("caps_loader");
    c.put("data/caps.json", k.text);
    CHECK(saysIn(c.gameError(), k.says));
  }
}

TEST(ranks_and_caps_files_that_hold_nothing_are_no_feature) {
  Copy c("ranks_loader_empty");
  c.put("data/ranks.json", R"({ "growth": "use", "ranks": [] })");
  c.put("data/caps.json", R"({ "caps": {} })");
  // The weapons still name ranks, which now do not exist.
  CHECK(saysIn(c.gameError(), "this game has no weapon ranks"));
  Copy plain("ranks_loader_empty2");
  plain.put("data/ranks.json", R"({ "growth": "use", "ranks": [] })");
  plain.put("data/caps.json", R"({ "caps": {} })");
  plain.swap("data/weapons.json", "\"rank\": \"d\", \"rankXp\": 2, ", "");
  plain.swap("data/weapons.json", "\"rank\": \"b\", \"rankXp\": 3, ", "");
  plain.swap("data/weapons.json", "\"rank\": \"c\", ", "");
  plain.put("data/units.json", R"({ "units": [ { "id": "grunt", "moveType": "foot", "stats": {"hp":10,"str":3,"def":1,"mov":3}, "wields": ["sword"] } ] })");
  plain.put("data/characters.json", R"({ "characters": [] })");
  plain.put("maps/duel.json", R"({ "name": "D", "grid": { "type": "square", "width": 3, "height": 1 }, "legend": { ".": "plain" },
    "tiles": ["..."], "sides": [ { "id": "blue", "controller": "human" }, { "id": "red", "controller": "ai" } ],
    "units": [ { "type": "grunt", "side": "blue", "x": 0, "y": 0 }, { "type": "grunt", "side": "red", "x": 2, "y": 0 } ] })");
  Database db;
  std::string err;
  CHECK(loadGame(plain.dir, db, err));
  CHECK(!db.ranks.on());
  CHECK(db.statCaps.empty());
}
