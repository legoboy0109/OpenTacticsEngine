// Loading structures: data/structures.json, map "structures", the refusals of the
// old terrain keys and the map "owners" list, and the art section for structures.
#include <sys/stat.h>

#include <cstdio>

#include "core/base/json.h"
#include "core/data/loader.h"
#include "test.h"

using namespace ote;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

void writePngHeader(const std::string& path, unsigned w, unsigned h) {
  unsigned char b[33] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13, 'I', 'H', 'D', 'R'};
  b[16] = (unsigned char)(w >> 24); b[17] = (unsigned char)(w >> 16); b[18] = (unsigned char)(w >> 8); b[19] = (unsigned char)w;
  b[20] = (unsigned char)(h >> 24); b[21] = (unsigned char)(h >> 16); b[22] = (unsigned char)(h >> 8); b[23] = (unsigned char)h;
  b[24] = 8; b[25] = 6;
  FILE* f = std::fopen(path.c_str(), "wb");
  CHECK(f != nullptr);
  if (!f) return;
  std::fwrite(b, 1, sizeof b, f);
  std::fclose(f);
}

const char* kTerrain =
    "{ \"terrain\": [ {\"id\":\"plain\"}, {\"id\":\"water\",\"moveCost\":{\"foot\":0,\"wheel\":0}} ] }";
const char* kStructures =
    "{ \"structures\": ["
    " {\"id\":\"city\",\"name\":\"City\",\"color\":\"#b7b7b7\",\"glyph\":\"c\",\"props\":{\"defense\":3},"
    "  \"capturePoints\":20,\"income\":{\"funds\":100}},"
    " {\"id\":\"castle\",\"width\":2,\"height\":2,\"produces\":[\"grunt\"],\"tags\":[\"hq\"],\"capturePoints\":30},"
    " {\"id\":\"pier\",\"width\":2,\"moveCost\":{\"foot\":1}},"
    " {\"id\":\"wall\",\"moveCost\":{\"foot\":0}} ] }";

// A small game in the scratch folder: move types foot/wheel, resource funds, unit grunt (foot), terrain
// plain/water, art sheets "tiles" (4 x 4 cells of 16 px) and "big" (2 cells of 32 x 40 px).
std::string makeGame(const char* name, const std::string& terrain, const std::string& structures,
                     const std::string& graphics = "") {
  std::string root = ote_test::tempDir() + "/str_" + name;
  mkdir(root.c_str(), 0755);
  mkdir((root + "/data").c_str(), 0755);
  mkdir((root + "/art").c_str(), 0755);
  mkdir((root + "/maps").c_str(), 0755);
  CHECK(writeTextFile(root + "/game.json",
                      "{ \"formatVersion\": 1, \"name\": \"G\", \"stats\": [{\"id\":\"hp\"},{\"id\":\"mov\"}],"
                      " \"roles\": {\"hp\":\"hp\",\"move\":\"mov\"}, \"moveTypes\": [{\"id\":\"foot\"},{\"id\":\"wheel\"}],"
                      " \"resources\": [{\"id\":\"funds\"}], \"formulas\": {\"damage\":\"1\"},"
                      " \"styles\": [{\"id\":\"day\"},{\"id\":\"night\"" +
                      std::string(structures.empty() ? "" : ",\"structureColors\":{\"city\":\"#102030\"}") + "}],"
                      " \"maps\": [{\"id\":\"m\",\"file\":\"maps/m.json\"}] }"));
  CHECK(writeTextFile(root + "/data/terrain.json", terrain));
  CHECK(writeTextFile(root + "/data/weapons.json", "{ \"weapons\": [{\"id\":\"stick\"}] }"));
  CHECK(writeTextFile(root + "/data/units.json",
                      "{ \"units\": [{\"id\":\"grunt\",\"moveType\":\"foot\",\"stats\":{\"hp\":5,\"mov\":3}}] }"));
  std::string sp = root + "/data/structures.json";
  if (structures.empty()) std::remove(sp.c_str());
  else CHECK(writeTextFile(sp, structures));
  writePngHeader(root + "/art/tiles.png", 64, 64);
  writePngHeader(root + "/art/big.png", 64, 40);
  std::string gp = root + "/data/graphics.json";
  if (graphics.empty()) std::remove(gp.c_str());
  else CHECK(writeTextFile(gp, graphics));
  return root;
}

std::string mapJson(const std::string& structures, const std::string& units = "[]", const std::string& extra = "") {
  return "{ \"grid\": {\"type\":\"square\",\"width\":6,\"height\":4}, \"legend\": {\".\":\"plain\",\"~\":\"water\"},"
         " \"tiles\": [\"......\",\"..~...\",\"......\",\"......\"],"
         " \"sides\": [{\"id\":\"blue\"},{\"id\":\"red\",\"controller\":\"ai\"}],"
         " \"structures\": " + structures + ", \"units\": " + units + extra + " }";
}

// Loads the game written by makeGame and its map "m" with the given map text.
bool loadWithMap(const char* name, const std::string& map, Database& db, MapDef& m, std::string& err) {
  std::string root = makeGame(name, kTerrain, kStructures);
  CHECK(writeTextFile(root + "/maps/m.json", map));
  return loadGame(root, db, err) && loadMap(db, "m", m, err);
}

void expectGameError(const char* name, const std::string& terrain, const std::string& structures, const char* file,
                     const char* a, const char* b = "") {
  Database db;
  std::string err;
  bool ok = loadGame(makeGame(name, terrain, structures), db, err);
  CHECK(!ok);
  if (ok || !has(err, file) || !has(err, a) || !has(err, b)) std::printf("  [%s] message was: %s\n", name, err.c_str());
  CHECK(has(err, file));
  CHECK(has(err, a));
  CHECK(has(err, b));
}

void expectMapError(const char* name, const std::string& map, const char* a, const char* b = "") {
  Database db;
  MapDef m;
  std::string err;
  bool ok = loadWithMap(name, map, db, m, err);
  CHECK(!ok);
  if (ok || !has(err, "m.json") || !has(err, a) || !has(err, b)) std::printf("  [%s] message was: %s\n", name, err.c_str());
  CHECK(has(err, "m.json"));
  CHECK(has(err, a));
  CHECK(has(err, b));
}

}  // namespace

TEST(structures_load_all_fields) {
  Database db;
  std::string err;
  bool ok = loadGame(makeGame("ok", kTerrain, kStructures), db, err);
  if (!ok) std::printf("  %s\n", err.c_str());
  CHECK(ok);
  if (!ok) return;
  CHECK_EQ(db.structures.size(), size_t(4));
  const StructureDef& city = db.structures[(size_t)db.findStructure("city")];
  CHECK_STR(city.name, "City");
  CHECK_EQ((int)city.color, 0xb7b7b7);
  CHECK_EQ((int)city.glyph, (int)'c');
  CHECK_EQ(city.width, 1);   // defaults
  CHECK_EQ(city.height, 1);
  CHECK_EQ(city.capturePoints, 20);
  CHECK_EQ(city.income.size(), size_t(1));
  CHECK_EQ(city.income[0].amount, 100);
  double v = 0;
  CHECK(propFind(city.props, "defense", v) && v == 3);
  CHECK_EQ(city.moveCost.size(), size_t(2));
  CHECK_EQ(city.moveCost[0], -1);  // not set
  CHECK_EQ(city.moveCost[1], -1);
  const StructureDef& castle = db.structures[(size_t)db.findStructure("castle")];
  CHECK_STR(castle.name, "castle");  // name defaults to the id
  CHECK_EQ(castle.width, 2);
  CHECK_EQ(castle.height, 2);
  CHECK_EQ(castle.produces.size(), size_t(1));
  CHECK_EQ(castle.produces[0], db.findUnitType("grunt"));
  CHECK_STR(castle.tags[0], "hq");
  const StructureDef& pier = db.structures[(size_t)db.findStructure("pier")];
  CHECK_EQ(pier.width, 2);
  CHECK_EQ(pier.height, 1);
  CHECK_EQ(pier.moveCost[0], 1);   // set
  CHECK_EQ(pier.moveCost[1], -1);  // wheel not set
  CHECK_EQ(db.structures[(size_t)db.findStructure("wall")].moveCost[0], 0);  // 0 = cannot enter
  CHECK_EQ(db.findStructure("nope"), -1);
}

TEST(structures_file_is_optional) {
  Database db;
  std::string err;
  CHECK(loadGame(makeGame("none", kTerrain, ""), db, err));
  CHECK(db.structures.empty());
  // A map of a game without structures simply has none.
  MapDef m;
  CHECK(writeTextFile(db.rootPath + "/maps/m.json", mapJson("[]")));
  CHECK(loadMap(db, "m", m, err));
  CHECK(m.structures.empty());
}

TEST(structures_loader_refuses_terrain_building_keys) {
  // Each old terrain setting is refused with a message that says where it went.
  const char* keys[] = {"capture", "capturePoints", "income", "produces"};
  const char* values[] = {"20", "20", "{\"funds\":1}", "[\"grunt\"]"};
  for (int i = 0; i < 4; ++i) {
    std::string t = std::string("{ \"terrain\": [ {\"id\":\"plain\"}, {\"id\":\"city\",\"") + keys[i] + "\":" + values[i] + "} ] }";
    Database db;
    std::string err;
    CHECK(!loadGame(makeGame("oldkey", t, kStructures), db, err));
    std::string want = std::string("terrain \"city\": \"") + keys[i] + "\" now belongs to structures (data/structures.json), see docs/STRUCTURES.md";
    if (!has(err, want.c_str())) std::printf("  [%s] message was: %s\n", keys[i], err.c_str());
    CHECK(has(err, want.c_str()));
    CHECK(has(err, "terrain.json"));
  }
  expectGameError("oldhq", "{ \"terrain\": [ {\"id\":\"plain\"}, {\"id\":\"hq\",\"tags\":[\"hq\"]} ] }", kStructures, "terrain.json",
                  "terrain \"hq\": the tag \"hq\" now belongs to structures", "docs/STRUCTURES.md");
  // Other tags on terrain are still fine.
  Database db;
  std::string err;
  CHECK(loadGame(makeGame("oktag", "{ \"terrain\": [ {\"id\":\"plain\",\"tags\":[\"open\"]} ] }", kStructures), db, err));
}

TEST(structures_loader_errors_in_structures_json) {
  auto s = [](const char* body) { return std::string("{ \"structures\": [ ") + body + " ] }"; };
  expectGameError("s_dup", kTerrain, s("{\"id\":\"a\"},{\"id\":\"a\"}"), "structures.json", "structure \"a\"", "duplicate id");
  expectGameError("s_noid", kTerrain, s("{\"name\":\"x\"}"), "structures.json", "structure[0]", "id");
  expectGameError("s_w0", kTerrain, s("{\"id\":\"a\",\"width\":0}"), "structures.json", "structure \"a\"", "between 1 and 8");
  expectGameError("s_h9", kTerrain, s("{\"id\":\"a\",\"height\":9}"), "structures.json", "structure \"a\"", "between 1 and 8");
  expectGameError("s_wfrac", kTerrain, s("{\"id\":\"a\",\"width\":1.5}"), "structures.json", "\"width\"", "whole number");
  expectGameError("s_color", kTerrain, s("{\"id\":\"a\",\"color\":\"red\"}"), "structures.json", "\"color\"", "#rrggbb");
  expectGameError("s_mt", kTerrain, s("{\"id\":\"a\",\"moveCost\":{\"boat\":1}}"), "structures.json", "unknown move type \"boat\"");
  expectGameError("s_neg", kTerrain, s("{\"id\":\"a\",\"moveCost\":{\"foot\":-1}}"), "structures.json", "\"moveCost\".\"foot\"", "0 or more");
  expectGameError("s_cap", kTerrain, s("{\"id\":\"a\",\"capturePoints\":-5}"), "structures.json", "\"capturePoints\"", "negative");
  expectGameError("s_capkey", kTerrain, s("{\"id\":\"a\",\"capture\":20}"), "structures.json", "\"capture\"", "capturePoints");
  expectGameError("s_prod", kTerrain, s("{\"id\":\"a\",\"produces\":[\"dragon\"]}"), "structures.json", "unknown unit type \"dragon\"");
  expectGameError("s_inc", kTerrain, s("{\"id\":\"a\",\"income\":{\"gold\":5}}"), "structures.json", "unknown resource \"gold\"");
  expectGameError("s_notlist", kTerrain, "{ \"structures\": 5 }", "structures.json", "\"structures\"", "a list");
  expectGameError("s_notobj", kTerrain, s("7"), "structures.json", "structure[0]", "object");
}

TEST(structures_map_loads_and_places_structures) {
  Database db;
  MapDef m;
  std::string err;
  std::string map = mapJson("[ {\"type\":\"city\",\"x\":0,\"y\":0,\"side\":\"blue\"}, {\"type\":\"castle\",\"x\":3,\"y\":2}, "
                            " {\"type\":\"pier\",\"x\":1,\"y\":1} ]",
                            "[ {\"type\":\"grunt\",\"side\":\"blue\",\"x\":2,\"y\":1} ]");
  bool ok = loadWithMap("map_ok", map, db, m, err);
  if (!ok) std::printf("  %s\n", err.c_str());
  CHECK(ok);
  if (!ok) return;
  CHECK_EQ(m.structures.size(), size_t(3));
  CHECK_EQ(m.structures[0].type, db.findStructure("city"));
  CHECK_EQ(m.structures[0].side, 0);
  CHECK_EQ(m.structures[1].side, -1);  // side optional = neutral
  CHECK_EQ(m.structures[1].x, 3);
  CHECK_EQ(m.structures[1].y, 2);
  CHECK_EQ(m.structures[2].type, db.findStructure("pier"));
  // The pier (2 x 1) at (1,1) covers the water square (2,1) and makes it enterable for foot units:
  // a unit may start there ("judged with the structure's costs applied").
  CHECK_EQ(m.terrain[1 * 6 + 2], db.findTerrain("water"));
}

// Older editor versions wrote "owners": [] into every map; that assigns nothing and must still load.
TEST(structures_map_accepts_an_empty_owners_list) {
  Database db;
  MapDef m;
  std::string err;
  bool ok = loadWithMap("map_empty_owners", mapJson("[]", "[]", ", \"owners\": []"), db, m, err);
  if (!ok) std::printf("  %s\n", err.c_str());
  CHECK(ok);
}

TEST(structures_map_loader_errors) {
  expectMapError("m_owners", mapJson("[]", "[]", ", \"owners\": [ {\"x\":0,\"y\":0,\"side\":\"blue\"} ]"), "\"owners\" is no longer used",
                 "ownership now belongs to structures");
  expectMapError("m_type", mapJson("[ {\"type\":\"temple\",\"x\":0,\"y\":0} ]"), "structures[0]", "unknown structure type \"temple\"");
  expectMapError("m_side", mapJson("[ {\"type\":\"city\",\"x\":0,\"y\":0,\"side\":\"green\"} ]"), "unknown side \"green\"");
  expectMapError("m_out", mapJson("[ {\"type\":\"castle\",\"x\":5,\"y\":0} ]"), "castle at 5,0", "does not fit inside the map");
  expectMapError("m_out_neg", mapJson("[ {\"type\":\"city\",\"x\":-1,\"y\":0} ]"), "does not fit inside the map");
  expectMapError("m_out_bottom", mapJson("[ {\"type\":\"castle\",\"x\":0,\"y\":3} ]"), "2 x 2", "(6 x 4)");
  expectMapError("m_overlap", mapJson("[ {\"type\":\"castle\",\"x\":2,\"y\":0}, {\"type\":\"city\",\"x\":3,\"y\":1} ]"),
                 "overlaps structures[0]", "square 3,1");
  expectMapError("m_missing", mapJson("[ {\"x\":0,\"y\":0} ]"), "structures[0]", "\"type\"");
  expectMapError("m_xy", mapJson("[ {\"type\":\"city\",\"x\":\"a\",\"y\":0} ]"), "\"x\"", "whole number");
  // A unit may not start on a square it cannot enter: judged with the structure's cost (wall = 0)...
  expectMapError("m_unit_wall", mapJson("[ {\"type\":\"wall\",\"x\":4,\"y\":3} ]", "[ {\"type\":\"grunt\",\"side\":\"blue\",\"x\":4,\"y\":3} ]"),
                 "structure \"wall\"", "impassable for move type \"foot\"");
  // ...and water without a pier is still water.
  expectMapError("m_unit_water", mapJson("[]", "[ {\"type\":\"grunt\",\"side\":\"blue\",\"x\":2,\"y\":1} ]"), "terrain \"water\"", "impassable");
}

TEST(structures_graphics_section) {
  const char* sheets =
      "\"sheets\": [ {\"id\":\"tiles\",\"file\":\"art/tiles.png\",\"cellWidth\":16,\"cellHeight\":16},"
      "             {\"id\":\"big\",\"file\":\"art/big.png\",\"cellWidth\":32,\"cellHeight\":40} ]";
  std::string body = std::string("{ ") + sheets +
      ", \"structures\": {"
      "   \"city\": {\"sheet\":\"tiles\",\"cell\":[0,0],\"sides\":{\"red\":{\"sheet\":\"tiles\",\"cell\":[1,0]}}},"
      "   \"castle\": {\"sheet\":\"big\",\"frames\":[[0,0],[1,0]],\"frameMs\":300} },"
      " \"styles\": { \"night\": { \"structures\": { \"city\": {\"sheet\":\"tiles\",\"cell\":[2,0]}, \"castle\": null } } } }";
  Database db;
  std::string err;
  bool ok = loadGame(makeGame("g_ok", kTerrain, kStructures, body), db, err);
  if (!ok) std::printf("  %s\n", err.c_str());
  CHECK(ok);
  if (!ok) return;
  const GraphicsSet* day = db.graphics.set(db.findStyle("day"));
  const GraphicsSet* night = db.graphics.set(db.findStyle("night"));
  CHECK(day && night);
  if (!day || !night) return;
  CHECK_EQ(day->structures.size(), db.structures.size());
  int city = db.findStructure("city"), castle = db.findStructure("castle"), pier = db.findStructure("pier");
  CHECK(day->structures[(size_t)city].base.kind == Look::Still);
  CHECK_EQ((int)day->structures[(size_t)city].forSide("red").cells[0].col, 1);   // the owner's own picture
  CHECK_EQ((int)day->structures[(size_t)city].forSide("blue").cells[0].col, 0);  // others use the base
  CHECK(day->structures[(size_t)castle].base.kind == Look::Animated);
  CHECK_EQ(day->structures[(size_t)castle].base.frameMs, 300);
  CHECK(!day->structures[(size_t)pier].base.any());  // no art: placeholder
  CHECK_EQ((int)night->structures[(size_t)city].base.cells[0].col, 2);  // style override
  CHECK(!night->structures[(size_t)castle].base.any());               // null removes the art for that style
  CHECK(day->structures[(size_t)castle].base.any());                  // the other style is untouched

  auto bad = [&](const char* name, const std::string& structuresPart, const char* a, const char* b = "") {
    Database d2;
    std::string e2;
    bool ok2 = loadGame(makeGame(name, kTerrain, kStructures, std::string("{ ") + sheets + ", " + structuresPart + " }"), d2, e2);
    CHECK(!ok2);
    if (ok2 || !has(e2, "graphics.json") || !has(e2, a) || !has(e2, b)) std::printf("  [%s] message was: %s\n", name, e2.c_str());
    CHECK(has(e2, "graphics.json"));
    CHECK(has(e2, a));
    CHECK(has(e2, b));
  };
  bad("g_unknown", "\"structures\": {\"temple\": {\"sheet\":\"tiles\",\"cell\":[0,0]}}", "structure \"temple\"", "there is no structure with this id");
  bad("g_join", "\"structures\": {\"city\": {\"sheet\":\"tiles\",\"join\":[0,0]}}", "structure \"city\"", "structures can only use");
  bad("g_cell", "\"structures\": {\"castle\": {\"sheet\":\"big\",\"cell\":[2,0]}}", "structure \"castle\"", "outside sheet \"big\"");
  bad("g_sheet", "\"structures\": {\"city\": {\"sheet\":\"nope\",\"cell\":[0,0]}}", "structure \"city\"", "unknown sheet");
  bad("g_style", "\"styles\": {\"night\": {\"structures\": {\"temple\": null}}}", "style \"night\"", "structure \"temple\"");
  bad("g_side", "\"structures\": {\"city\": {\"sheet\":\"tiles\",\"cell\":[0,0],\"sides\":{\"\":{\"sheet\":\"tiles\",\"cell\":[0,0]}}}}",
      "structure \"city\"", "empty side id");
}

TEST(structures_style_colors) {
  Database db;
  std::string err;
  CHECK(loadGame(makeGame("style", kTerrain, kStructures), db, err));
  int city = db.findStructure("city");
  CHECK_EQ(db.styles[0].structureColors.size(), db.structures.size());
  CHECK_EQ(db.styles[0].structureColors[(size_t)city], -1);
  CHECK_EQ(db.styles[1].structureColors[(size_t)city], 0x102030);
  Database bad;
  std::string root = makeGame("style_bad", kTerrain, kStructures);
  CHECK(writeTextFile(root + "/game.json",
                      "{ \"formatVersion\": 1, \"name\": \"G\", \"stats\": [{\"id\":\"hp\"},{\"id\":\"mov\"}],"
                      " \"roles\": {\"hp\":\"hp\",\"move\":\"mov\"}, \"moveTypes\": [{\"id\":\"foot\"},{\"id\":\"wheel\"}],"
                      " \"resources\": [{\"id\":\"funds\"}], \"formulas\": {\"damage\":\"1\"}, \"styles\": [{\"id\":\"n\",\"structureColors\":{\"temple\":\"#000000\"}}] }"));
  CHECK(!loadGame(root, bad, err));
  CHECK(has(err, "structureColors") && has(err, "temple"));
}

// Every shipped game with structures: every structure has a picture in every visual style, and
// every sheet it uses exists and is big enough for the cell.
TEST(structures_shipped_games_have_pictures_for_every_structure) {
  const char* dirs[] = {"games/demo-srpg", "games/demo-economy", "editor/templates/strategy-rpg",
                        "editor/templates/economy-tactics", "editor/templates/blank"};
  int withStructures = 0;
  for (const char* d : dirs) {
    Database db;
    std::string err;
    bool ok = loadGame(ote_test::sourceDir() + "/" + d, db, err);
    if (!ok) std::printf("  %s: %s\n", d, err.c_str());
    CHECK(ok);
    if (!ok) continue;
    if (db.structures.empty()) continue;
    ++withStructures;
    CHECK(db.graphics.any());
    for (size_t s = 0; s < db.styles.size(); ++s) {
      const GraphicsSet* set = db.graphics.set((int)s);
      CHECK(set != nullptr);
      if (!set) continue;
      CHECK_EQ(set->structures.size(), db.structures.size());
      for (size_t i = 0; i < db.structures.size(); ++i) {
        const Look& look = set->structures[i].base;
        if (!look.any()) std::printf("  %s: structure \"%s\" has no picture in style \"%s\"\n", d, db.structures[i].id.c_str(), db.styles[s].id.c_str());
        CHECK(look.any());
        if (!look.any()) continue;
        // The cell is as wide as the footprint is (a picture is stretched across it, nothing is squeezed).
        const SheetDef& sh = db.graphics.sheets[(size_t)look.sheet];
        CHECK_EQ(sh.cellW % 24, 0);
        if (db.styles[s].view == StyleView::Isometric) {
          // Isometric: the footprint is (width + height) half-diamonds wide and half as high.
          int span = db.structures[i].width + db.structures[i].height;
          CHECK_EQ(sh.cellW / 24, span);
          CHECK(sh.cellH >= 12 * span);
          continue;
        }
        CHECK_EQ(sh.cellW / 24, db.structures[i].width);
        CHECK(sh.cellH >= 24 * db.structures[i].height);  // at least the footprint; taller ones stick out
      }
    }
    // The maps place their structures inside the map and none of them on top of another.
    for (const MapRef& ref : db.maps) {
      MapDef m;
      CHECK(loadMap(db, ref.id, m, err));
    }
  }
  CHECK_EQ(withStructures, 3);  // demo-srpg, demo-economy, economy-tactics
}

TEST(structures_shipped_maps_use_a_structure_larger_than_one_square) {
  Database db;
  std::string err;
  CHECK(loadGame(ote_test::sourceDir() + "/games/demo-economy", db, err));
  const StructureDef& hq = db.structures[(size_t)db.findStructure("hq")];
  CHECK(hq.width == 2 && hq.height == 2);
  // Taller than its footprint: the picture sticks out above.
  const GraphicsSet* set = db.graphics.set(0);
  CHECK(set != nullptr);
  if (set) {
    const SheetDef& sh = db.graphics.sheets[(size_t)set->structures[(size_t)db.findStructure("hq")].base.sheet];
    CHECK(sh.cellH > 24 * hq.height);
  }
}
