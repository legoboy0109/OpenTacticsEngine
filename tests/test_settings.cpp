#include <cstdio>

#include "core/data/loader.h"
#include "core/data/settings.h"
#include "test.h"

using namespace ote;

static std::string tempPath(const char* name) {
  return ote_test::sourceDir() + "/build/" + name;  // build/ is gitignored
}

TEST(settings_global_applies_everywhere_until_overridden_per_game) {
  Settings s;
  CHECK(s.get("any", "font").isNull());                 // unset
  CHECK_STR(s.getString("any", "font", "pixel"), "pixel");
  s.setGlobal("font", Json::makeString("sans"));
  s.setGlobal("uiScale", Json::makeNumber(150));
  CHECK_STR(s.getString("demo-srpg", "font", ""), "sans");
  CHECK_STR(s.getString("", "font", ""), "sans");

  s.setOverride("demo-srpg", "font", Json::makeString("serif"));
  CHECK_STR(s.getString("demo-srpg", "font", ""), "serif");   // this game only
  CHECK_STR(s.getString("demo-economy", "font", ""), "sans"); // others follow global
  CHECK_EQ(s.getInt("demo-srpg", "uiScale", 100), 150);       // other options still global
  CHECK(s.hasOverride("demo-srpg", "font"));
  CHECK(!s.hasOverride("demo-srpg", "uiScale"));

  s.setGlobal("font", Json::makeString("mono"));
  CHECK_STR(s.getString("demo-srpg", "font", ""), "serif");   // override survives
  s.clearOverride("demo-srpg", "font");
  CHECK_STR(s.getString("demo-srpg", "font", ""), "mono");
  s.setGlobal("font", Json());                                // unset again
  CHECK(s.get("demo-srpg", "font").isNull());
}

TEST(settings_round_trip_and_corrupt_file_is_preserved) {
  std::string path = tempPath("test_settings.json");
  std::remove(path.c_str());
  std::remove((path + ".bad").c_str());
  std::string err;
  {
    Settings s;
    CHECK(s.load(path, err));  // missing file is fine
    CHECK(!s.dirty());
    s.setGlobal("font", Json::makeString("sans"));
    s.setOverride("g", "style", Json::makeString("night"));
    s.setState("lastGame", "g");
    CHECK(s.dirty());
    CHECK(s.save(err));
    CHECK(!s.dirty());
  }
  {
    Settings s;
    CHECK(s.load(path, err));
    CHECK_STR(s.getString("g", "font", ""), "sans");
    CHECK_STR(s.getString("g", "style", ""), "night");
    CHECK_STR(s.getString("other", "style", "none"), "none");
    CHECK_STR(s.state("lastGame", ""), "g");
  }
  CHECK(writeTextFile(path, "{ not json"));
  {
    Settings s;
    CHECK(!s.load(path, err));
    CHECK(err.find("test_settings.json") != std::string::npos);
    s.setGlobal("font", Json::makeString("mono"));
    CHECK(s.save(err));
    std::string bad;
    CHECK(readTextFile(path + ".bad", bad) && bad == "{ not json");
    Settings again;
    CHECK(again.load(path, err));
    CHECK_STR(again.getString("", "font", ""), "mono");
  }
  std::remove(path.c_str());
  std::remove((path + ".bad").c_str());
}

TEST(game_styles_cover_every_terrain) {
  Database db;
  std::string err;
  CHECK(loadGame(ote_test::sourceDir() + "/games/demo-economy", db, err));
  CHECK(db.styles.size() >= 1);
  CHECK_EQ((int)db.styles[0].terrainColors.size(), (int)db.terrains.size());
}

TEST(demo_styles_load_with_terrain_recolors) {
  Database db;
  std::string err;
  bool ok = loadGame(ote_test::sourceDir() + "/games/demo-economy", db, err);
  if (!ok) std::printf("  %s\n", err.c_str());
  CHECK(ok);
  CHECK_EQ((int)db.styles.size(), 2);
  CHECK_STR(db.styles[0].id, "classic");
  CHECK_EQ(db.styles[0].terrainColors[(size_t)db.findTerrain("sea")], -1);
  int night = db.findStyle("night");
  CHECK_EQ(night, 1);
  CHECK(db.styles[1].hasBackground);
  CHECK_EQ(db.styles[1].terrainColors[(size_t)db.findTerrain("sea")], 0x101a40);
  CHECK(!db.description.empty());
  Database srpg;
  ok = loadGame(ote_test::sourceDir() + "/games/demo-srpg", srpg, err);
  if (!ok) std::printf("  %s\n", err.c_str());
  CHECK(ok);
  CHECK_EQ(srpg.styles[1].showGlyphs, 0);
}
