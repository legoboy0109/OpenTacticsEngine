// The data hash of a save (docs/SAVES.md) and fog of war: the new rule data changes the hash only when it is present, so
// every game and map without fog keeps the hash (and the saves) it always had.
#include "save_helpers.h"

using namespace ote;

namespace {

// Writes a tiny game and returns its data hash ("" on a problem). The pieces are the parts a fog map can change.
std::string hashOf(const char* name, const std::string& terrainExtra, const std::string& unitExtra,
                   const std::string& structures, const std::string& mapExtra) {
  std::string dir = savetest::tempFolder(std::string("fog_hash_") + name);
  savetest::tempFolder(std::string("fog_hash_") + name + "/data");
  savetest::tempFolder(std::string("fog_hash_") + name + "/maps");
  CHECK(writeTextFile(dir + "/game.json",
                      R"({ "formatVersion": 1, "name": "G", "stats": [{"id":"hp"},{"id":"str"},{"id":"mov"}],
                           "roles": {"hp":"hp","move":"mov"}, "moveTypes": [{"id":"foot"}],
                           "resources": [{"id":"gold"}], "formulas": {"damage":"a.str"},
                           "maps": [{"id":"m","file":"maps/m.json"}] })"));
  CHECK(writeTextFile(dir + "/data/terrain.json", "{ \"terrain\": [ {\"id\":\"plain\"" + terrainExtra + "} ] }"));
  CHECK(writeTextFile(dir + "/data/weapons.json", R"({ "weapons": [ {"id":"sword"} ] })"));
  CHECK(writeTextFile(dir + "/data/units.json",
                      "{ \"units\": [{\"id\":\"grunt\",\"moveType\":\"foot\",\"stats\":{\"hp\":5,\"str\":2,\"mov\":3},\"weapons\":[\"sword\"]" +
                          unitExtra + "}] }"));
  if (!structures.empty()) CHECK(writeTextFile(dir + "/data/structures.json", structures));
  CHECK(writeTextFile(dir + "/maps/m.json",
                      "{ \"grid\": {\"type\":\"square\",\"width\":3,\"height\":1}, \"legend\": {\".\":\"plain\"}, \"tiles\": [\"...\"],"
                      " \"sides\": [{\"id\":\"a\"},{\"id\":\"b\"}],"
                      " \"units\": [{\"type\":\"grunt\",\"side\":\"a\",\"x\":0,\"y\":0},{\"type\":\"grunt\",\"side\":\"b\",\"x\":2,\"y\":0}]" +
                          mapExtra + " }"));
  Database db;
  std::string err, h;
  if (!loadGame(dir, db, err) || !computeDataHash(db, "m", h, err)) {
    std::printf("  %s\n", err.c_str());
    return "";
  }
  return h;
}

}  // namespace

TEST(fog_hash_changes_only_when_fog_data_is_present) {
  const std::string base = hashOf("base", "", "", "", "");
  CHECK_EQ((int)base.size(), 16);
  CHECK_STR(hashOf("again", "", "", "", ""), base);
  std::string fog = hashOf("fog", "", "", "", ",\"fog\":true");
  std::string vision = hashOf("vision", "", ",\"vision\":4", "", "");
  std::string bonus = hashOf("bonus", ",\"visionBonus\":2", "", "", "");
  std::string hides = hashOf("hides", ",\"hides\":true", "", "", "");
  std::string structure = hashOf("structure", "", "", R"({ "structures": [ { "id": "tower", "visionBonus": 3 } ] })", "");
  std::string all[] = {fog, vision, bonus, hides, structure};
  for (size_t i = 0; i < 5; ++i) {
    CHECK(all[i] != base);
    for (size_t k = i + 1; k < 5; ++k) CHECK(all[i] != all[k]);
  }
  CHECK(hashOf("vision5", "", ",\"vision\":5", "", "") != vision);  // a changed value counts too
}
