// Small generated hexagon games for the tests/test_hex_*.cpp files: a tiny rules set (foot units, a melee weapon and
// an artillery weapon with range 2..3), written into the build's scratch folder with the grid, styles, structures
// and map the test asks for.
#pragma once
#include "posix_compat.h"

#include <string>

#include "core/data/loader.h"
#include "test.h"

namespace hexfix {

using namespace ote;

inline bool has(const std::string& hay, const std::string& needle) { return hay.find(needle) != std::string::npos; }

// A map of `w` x `h` plain cells with the given tiles override (empty = all plain), two sides and `extra` entries
// (a full "structures": [...] and/or "units": [...] text, each followed by a comma-free tail handled by the caller).
inline std::string plainMap(const char* gridType, int w, int h, const std::string& structuresJson,
                            const std::string& unitsJson) {
  std::string tiles;
  for (int y = 0; y < h; ++y) tiles += std::string(y ? "," : "") + "\"" + std::string((size_t)w, '.') + "\"";
  return "{ \"name\": \"M\", \"grid\": {\"type\":\"" + std::string(gridType) + "\",\"width\":" + std::to_string(w) +
         ",\"height\":" + std::to_string(h) + "}, \"legend\": {\".\":\"plain\"}, \"tiles\": [" + tiles +
         "], \"sides\": [{\"id\":\"blue\",\"controller\":\"human\"},{\"id\":\"red\",\"controller\":\"ai\"}],"
         " \"structures\": [" + structuresJson + "], \"units\": [" + unitsJson + "] }";
}

// Writes a game under the scratch folder and returns its path. `gridJson` is the text of game.json's "grid" entry
// including its key and a trailing comma (empty = none); `stylesJson` likewise for "styles".
inline std::string makeGame(const char* name, const std::string& gridJson, const std::string& structures,
                            const std::string& map, const std::string& stylesJson = "") {
  std::string root = ote_test::tempDir() + "/hex_" + name;
  mkdir(root.c_str(), 0755);
  mkdir((root + "/data").c_str(), 0755);
  mkdir((root + "/maps").c_str(), 0755);
  CHECK(writeTextFile(root + "/game.json",
                      "{ \"formatVersion\": 1, \"name\": \"G\", " + gridJson + stylesJson +
                          " \"stats\": [{\"id\":\"hp\"},{\"id\":\"mov\"}], \"roles\": {\"hp\":\"hp\",\"move\":\"mov\"},"
                          " \"moveTypes\": [{\"id\":\"foot\"}], \"resources\": [{\"id\":\"funds\"}],"
                          " \"formulas\": {\"damage\":\"1\"}, \"maps\": [{\"id\":\"m\",\"file\":\"maps/m.json\"}] }"));
  CHECK(writeTextFile(root + "/data/terrain.json", "{ \"terrain\": [ {\"id\":\"plain\"} ] }"));
  CHECK(writeTextFile(root + "/data/weapons.json",
                      "{ \"weapons\": [ {\"id\":\"stick\"}, {\"id\":\"cannon\",\"range\":[2,3],\"counter\":false} ] }"));
  CHECK(writeTextFile(root + "/data/units.json",
                      "{ \"units\": [ {\"id\":\"grunt\",\"moveType\":\"foot\",\"stats\":{\"hp\":5,\"mov\":3},\"weapons\":[\"stick\"]},"
                      " {\"id\":\"gun\",\"moveType\":\"foot\",\"stats\":{\"hp\":5,\"mov\":3},\"weapons\":[\"cannon\"]} ] }"));
  std::string sp = root + "/data/structures.json";
  if (structures.empty()) std::remove(sp.c_str());
  else CHECK(writeTextFile(sp, structures));
  CHECK(writeTextFile(root + "/maps/m.json", map));
  return root;
}

inline std::string hexGrid(const char* layout) { return std::string("\"grid\": {\"type\":\"hex\",\"layout\":\"") + layout + "\"},"; }

// loadGame + loadMap; returns the error text ("" = success).
inline std::string tryLoad(const std::string& root, Database& db, MapDef& map) {
  std::string err;
  if (!loadGame(root, db, err)) return err;
  if (!loadMap(db, "m", map, err)) return err;
  return "";
}

}  // namespace hexfix
