// Hexagon games, data side (docs/HEX.md "Data"): every loader refusal, structure rings, the rules hash, join pieces.
#include "core/battle/save.h"
#include "core/data/footprint.h"
#include "core/data/game_info.h"
#include "hex_fixture.h"

using namespace ote;
using namespace hexfix;

namespace {

const char* kCity = "{ \"structures\": [ {\"id\":\"city\"}, {\"id\":\"fort\",\"rings\":1} ] }";
const char* kUnit = "{\"type\":\"grunt\",\"side\":\"blue\",\"x\":0,\"y\":0}, {\"type\":\"grunt\",\"side\":\"red\",\"x\":4,\"y\":4}";

// Loads a hexagon game with the given pieces and returns the error text.
std::string refusal(const char* name, const std::string& grid, const std::string& structures, const std::string& map,
                    const std::string& styles = "") {
  Database db;
  MapDef m;
  std::string err = tryLoad(makeGame(name, grid, structures, map, styles), db, m);
  if (err.empty()) std::printf("  [%s] loaded, a refusal was expected\n", name);
  return err;
}

std::string okMap(const char* type = "hex") { return plainMap(type, 7, 7, "", kUnit); }

}  // namespace

TEST(hex_game_grid_loads_and_reaches_the_database_and_the_map) {
  for (int flat = 0; flat < 2; ++flat) {
    Database db;
    MapDef m;
    CHECK_STR(tryLoad(makeGame("ok", hexGrid(flat ? "flat" : "pointy"), kCity, okMap()), db, m), "");
    CHECK(db.gridType == GridType::Hex);
    CHECK(db.hexLayout == (flat ? HexLayout::Flat : HexLayout::Pointy));
    CHECK(m.gridType == GridType::Hex);
    CHECK(m.hexLayout == db.hexLayout);
    CHECK_EQ(db.structures[1].rings, 1);
    CHECK_EQ(db.structures[0].rings, 0);
  }
  Database db;
  MapDef m;
  CHECK_STR(tryLoad(makeGame("sq", "", "", okMap("square")), db, m), "");  // no "grid" = square
  CHECK(db.gridType == GridType::Square);
  CHECK_STR(tryLoad(makeGame("sq2", "\"grid\": {\"type\":\"square\"},", "", okMap("square")), db, m), "");
  CHECK(db.gridType == GridType::Square);
}

TEST(hex_game_info_exposes_the_grid) {
  GameInfo gi;
  std::string err;
  makeGame("info", hexGrid("flat"), "", okMap());
  CHECK(loadGameInfo(ote_test::tempDir() + "/hex_info", gi, err));
  CHECK(gi.gridType == GridType::Hex);
  CHECK(gi.hexLayout == HexLayout::Flat);
  CHECK(loadGameInfo(ote_test::sourceDir() + "/games/demo-srpg", gi, err));
  CHECK(gi.gridType == GridType::Square);
}

TEST(hex_loader_refuses_bad_game_grids) {
  std::string e = refusal("g1", "\"grid\": {\"type\":\"hex\"},", "", okMap());
  CHECK(has(e, "game.json: grid: a hexagon game needs a \"layout\": \"pointy\" or \"flat\""));
  e = refusal("g2", "\"grid\": {\"type\":\"square\",\"layout\":\"flat\"},", "", okMap("square"));
  CHECK(has(e, "game.json: grid: \"layout\" is only for hexagon games (\"type\": \"hex\")"));
  e = refusal("g3", "\"grid\": {\"type\":\"hex\",\"layout\":\"flat\",\"size\":3},", "", okMap());
  CHECK(has(e, "game.json: grid: unknown key \"size\""));
  e = refusal("g4", "\"grid\": {\"type\":\"hex\",\"layout\":\"round\"},", "", okMap());
  CHECK(has(e, "grid: \"layout\" must be \"pointy\" or \"flat\", not \"round\""));
  e = refusal("g5", "\"grid\": {\"type\":\"triangle\"},", "", okMap());
  CHECK(has(e, "grid: \"type\" must be \"square\" or \"hex\", not \"triangle\""));
  e = refusal("g6", "\"grid\": 5,", "", okMap());
  CHECK(has(e, "grid"));
}

TEST(hex_loader_allows_every_kind_of_view_and_refuses_a_tile_height) {
  // All four kinds of view work on hexagons (the 3D ones since docs/HEX.md "Implementation notes (3D)").
  const char* kinds[] = {"top-down", "isometric", "isometric-3d", "3d"};
  const StyleView views[] = {StyleView::TopDown, StyleView::Isometric, StyleView::Isometric3D, StyleView::Full3D};
  for (int i = 0; i < 4; ++i) {
    CHECK(hexGameAllowsView(views[i]));
    for (const char* layout : {"pointy", "flat"}) {
      Database db3;
      MapDef m3;
      CHECK_STR(tryLoad(makeGame("st", hexGrid(layout), "", okMap(), std::string("\"styles\": [{\"id\":\"v\",\"view\":\"") + kinds[i] + "\"}],"), db3, m3), "");
      CHECK(db3.styles[0].view == views[i]);
    }
  }
  // The isometric measures of the 3D isometric kind follow the 2D rules: no tileHeight on a hexagon game.
  std::string e3 = refusal("st6", hexGrid("pointy"), "", okMap(),
                           "\"styles\": [{\"id\":\"iso\",\"view\":\"isometric-3d\",\"isometric\":{\"tileWidth\":48,\"tileHeight\":24}}],");
  CHECK(has(e3, "\"tileHeight\" is not used on hexagon games"));
  // 2D isometric works on hexagons; its squash is fixed, so tileHeight is refused (tileWidth, heightStep, unitWidth stay).
  std::string e = refusal("st3", hexGrid("flat"), "", okMap(),
                          "\"styles\": [{\"id\":\"iso\",\"view\":\"isometric\",\"isometric\":{\"tileWidth\":48,\"tileHeight\":24}}],");
  CHECK(has(e, "game.json: style \"iso\" \"isometric\": \"tileHeight\" is not used on hexagon games"));
  Database dbi;
  MapDef mi;
  CHECK_STR(tryLoad(makeGame("st4", hexGrid("pointy"), "", okMap(),
                             "\"styles\": [{\"id\":\"iso\",\"view\":\"isometric\",\"isometric\":{\"tileWidth\":40,\"heightStep\":6,\"unitWidth\":20}}],"),
                    dbi, mi), "");
  CHECK(dbi.styles[0].view == StyleView::Isometric && dbi.styles[0].iso.tileWidth == 40 && dbi.styles[0].iso.tileHeight == 0);
  // The same block on a square game keeps tileHeight.
  Database dbs;
  MapDef ms;
  CHECK_STR(tryLoad(makeGame("st5", "", "", okMap("square"),
                             "\"styles\": [{\"id\":\"iso\",\"view\":\"isometric\",\"isometric\":{\"tileHeight\":24}}],"),
                    dbs, ms), "");
  Database db;
  MapDef m;
  CHECK_STR(tryLoad(makeGame("st2", hexGrid("pointy"), "", okMap(), "\"styles\": [{\"id\":\"a\"},{\"id\":\"b\",\"view\":\"top-down\"}],"), db, m), "");
}

TEST(hex_loader_checks_the_map_grid_against_the_game) {
  std::string e = refusal("m1", hexGrid("pointy"), "", okMap("square"));
  CHECK(has(e, "maps/m.json: grid: this is a hexagon game (game.json \"grid\"), but the map's type is \"square\""));
  e = refusal("m2", "", "", okMap("hex"));
  CHECK(has(e, "maps/m.json: grid: this is a square game (game.json \"grid\"), but the map's type is \"hex\""));
  e = refusal("m3", hexGrid("flat"), "", okMap("cube"));
  CHECK(has(e, "grid: type \"cube\" is not supported"));
}

TEST(hex_loader_refuses_structure_sizes_that_do_not_fit_the_grid) {
  std::string e = refusal("s1", hexGrid("pointy"), "{ \"structures\": [ {\"id\":\"fort\",\"width\":2} ] }", okMap());
  CHECK(has(e, "structure \"fort\": \"width\" is for square games; a hexagon game uses \"rings\""));
  e = refusal("s2", hexGrid("flat"), "{ \"structures\": [ {\"id\":\"fort\",\"height\":3} ] }", okMap());
  CHECK(has(e, "structure \"fort\": \"height\" is for square games; a hexagon game uses \"rings\""));
  e = refusal("s3", hexGrid("pointy"), "{ \"structures\": [ {\"id\":\"fort\",\"rings\":2} ] }", okMap());
  CHECK(has(e, "structure \"fort\": \"rings\" must be 0 or 1"));
  e = refusal("s4", "", "{ \"structures\": [ {\"id\":\"fort\",\"rings\":1} ] }", okMap("square"));
  CHECK(has(e, "structure \"fort\": \"rings\" is for hexagon games"));
  Database db;
  MapDef m;  // width and height of 1 are what a hexagon structure is anyway
  CHECK_STR(tryLoad(makeGame("s5", hexGrid("pointy"), "{ \"structures\": [ {\"id\":\"fort\",\"width\":1,\"height\":1} ] }", okMap()), db, m), "");
}

TEST(hex_upgrades_keep_the_rings) {
  std::string e = refusal("u1", hexGrid("pointy"),
                          "{ \"structures\": [ {\"id\":\"a\",\"upgrades\":[{\"to\":\"b\"}]}, {\"id\":\"b\",\"rings\":1} ] }", okMap());
  CHECK(has(e, "an upgrade keeps the footprint"));
  Database db;
  MapDef m;
  CHECK_STR(tryLoad(makeGame("u2", hexGrid("pointy"),
                             "{ \"structures\": [ {\"id\":\"a\",\"rings\":1,\"upgrades\":[{\"to\":\"b\"}]}, {\"id\":\"b\",\"rings\":1} ] }",
                             okMap()), db, m), "");
}

TEST(hex_ring_structures_cover_seven_hexagons_and_must_fit_and_not_overlap) {
  Database db;
  MapDef m;
  // x, y is the middle hexagon.
  std::string fort = "{\"type\":\"fort\",\"x\":3,\"y\":3}";
  for (int flat = 0; flat < 2; ++flat) {
    std::string g = hexGrid(flat ? "flat" : "pointy");
    CHECK_STR(tryLoad(makeGame("r1", g, kCity, plainMap("hex", 7, 7, fort, kUnit)), db, m), "");
    Grid grid(GridType::Hex, 7, 7, db.hexLayout);
    std::vector<Cell> cells;
    CHECK(structureFootprint(grid, db.structures[1], 3, 3, cells));
    CHECK_EQ((int)cells.size(), 7);
    CHECK_EQ(cells[0], grid.at(3, 3));
    // Out of the map: a ring on the edge, and a one-hexagon structure outside.
    std::string e = refusal("r2", g, kCity, plainMap("hex", 7, 7, "{\"type\":\"fort\",\"x\":0,\"y\":3}", kUnit));
    CHECK(has(e, "structures[0] (fort at 0,3): covers its hexagon and the six around it (rings 1) but they do not all fit inside the map (7 x 7)"));
    e = refusal("r3", g, kCity, plainMap("hex", 7, 7, "{\"type\":\"city\",\"x\":7,\"y\":3}", kUnit));
    CHECK(has(e, "structures[0] (city at 7,3): is outside the map (7 x 7)"));
    // Overlap: a city inside the fort's ring (its neighbour in direction 1), and a second fort touching the first.
    Cell right = grid.neighbor(grid.at(3, 3), 1);
    std::string city = "{\"type\":\"city\",\"x\":" + std::to_string(grid.x(right)) + ",\"y\":" + std::to_string(grid.y(right)) + "}";
    e = refusal("r4", g, kCity, plainMap("hex", 7, 7, fort + "," + city, kUnit));
    CHECK(has(e, "structures[1] (city at " + std::to_string(grid.x(right)) + "," + std::to_string(grid.y(right)) + "): overlaps structures[0] (fort at 3,3) on hexagon " +
                     std::to_string(grid.x(right)) + "," + std::to_string(grid.y(right))));
    e = refusal("r5", g, kCity, plainMap("hex", 7, 7, fort + ",{\"type\":\"fort\",\"x\":3,\"y\":4}", kUnit));
    CHECK(has(e, "overlaps structures[0]"));
    // A fort two hexagons away does not touch it.
    Cell far = grid.at(6, 6);
    CHECK_STR(tryLoad(makeGame("r6", g, kCity, plainMap("hex", 9, 9, "{\"type\":\"fort\",\"x\":2,\"y\":2},{\"type\":\"fort\",\"x\":6,\"y\":6}", kUnit)), db, m), "");
    (void)far;
  }
}

TEST(hex_square_footprints_are_unchanged) {
  Database db;
  MapDef m;
  CHECK_STR(tryLoad(makeGame("sqf", "", "{ \"structures\": [ {\"id\":\"castle\",\"width\":2,\"height\":3} ] }",
                             plainMap("square", 6, 6, "{\"type\":\"castle\",\"x\":1,\"y\":2}", kUnit)), db, m), "");
  Grid grid(GridType::Square, 6, 6);
  std::vector<Cell> cells;
  CHECK(structureFootprint(grid, db.structures[0], 1, 2, cells));
  CHECK_EQ((int)cells.size(), 6);
  CHECK_EQ(cells[0], grid.at(1, 2));
  CHECK_EQ(cells[1], grid.at(2, 2));  // row by row from the top-left corner
  CHECK_EQ(cells[5], grid.at(2, 4));
  CHECK(!structureFootprint(grid, db.structures[0], 5, 2, cells));
  CHECK(cells.empty());
}

TEST(hex_rules_hash_covers_the_grid_and_leaves_square_games_alone) {
  std::string h[6];
  const std::string sqMap = okMap("square"), hexMap = okMap("hex");
  struct Case { const char* name; std::string grid, map; } cases[] = {
      {"h0", "", sqMap}, {"h1", "\"grid\": {\"type\":\"square\"},", sqMap}, {"h2", hexGrid("pointy"), hexMap},
      {"h3", hexGrid("flat"), hexMap}, {"h4", "", sqMap}};
  for (int i = 0; i < 5; ++i) {
    Database db;
    MapDef m;
    CHECK_STR(tryLoad(makeGame(cases[i].name, cases[i].grid, "", cases[i].map), db, m), "");
    std::string err;
    CHECK(computeDataHash(db, "m", h[i], err));
  }
  CHECK_STR(h[0], h[1]);  // an explicit square grid is the same game as none
  CHECK_STR(h[0], h[4]);
  CHECK(h[2] != h[3]);    // layout counts
  CHECK(h[2] != h[0] && h[3] != h[0]);
}

TEST(hex_join_mask_has_one_bit_per_neighbour_in_contract_order) {
  Look look;
  look.kind = Look::Join;
  look.joinWith.assign(3, 0);
  look.joinEdge = false;
  look.joinWith[1] = 1;
  for (int flat = 0; flat < 2; ++flat) {
    Grid g(GridType::Hex, 6, 6, flat ? HexLayout::Flat : HexLayout::Pointy);
    std::vector<int> terrain((size_t)g.cellCount(), 0);
    auto at = [&](Cell c) { return terrain[(size_t)c]; };
    for (int y = 0; y < 6; ++y)
      for (int x = 0; x < 6; ++x) {
        Cell c = g.at(x, y);
        // Each direction on its own, then all of them.
        for (int i = 0; i < 6; ++i) {
          std::fill(terrain.begin(), terrain.end(), 0);
          Cell n = g.neighbor(c, i);
          if (n != kNoCell) terrain[(size_t)n] = 1;
          CHECK_EQ(joinMaskHex(look, g, c, at), n != kNoCell ? 1 << i : 0);
        }
        std::fill(terrain.begin(), terrain.end(), 1);
        int all = 0;
        for (int i = 0; i < 6; ++i) all |= g.neighbor(c, i) != kNoCell ? 1 << i : 0;
        CHECK_EQ(joinMaskHex(look, g, c, at), all);
      }
    // Spelled out from the contract: pointy (2,2) has upper right (2,1), lower left (1,3), upper left (1,1).
    std::fill(terrain.begin(), terrain.end(), 0);
    if (!flat) {
      terrain[(size_t)g.at(2, 1)] = terrain[(size_t)g.at(1, 3)] = terrain[(size_t)g.at(1, 1)] = 1;
      CHECK_EQ(joinMaskHex(look, g, g.at(2, 2), at), 1 | 8 | 32);
    } else {  // flat (2,2): up (2,1), lower right (3,2) is direction 2, upper left (1,1) is 5
      terrain[(size_t)g.at(2, 1)] = terrain[(size_t)g.at(3, 2)] = terrain[(size_t)g.at(1, 1)] = 1;
      CHECK_EQ(joinMaskHex(look, g, g.at(2, 2), at), 1 | 4 | 32);
    }
    // joinEdge: off-map neighbours count as the same.
    std::fill(terrain.begin(), terrain.end(), 0);
    look.joinEdge = true;
    int edge = joinMaskHex(look, g, g.at(0, 0), at);
    int offMap = 0;
    for (int i = 0; i < 6; ++i) offMap |= g.neighbor(g.at(0, 0), i) == kNoCell ? 1 << i : 0;
    CHECK_EQ(edge, offMap);
    CHECK_EQ(joinMaskHex(look, g, g.at(3, 3), at), 0);
    look.joinEdge = false;
  }
}

TEST(hex_join_pieces_live_in_an_8_by_8_block) {
  Look look;
  look.kind = Look::Join;
  SpriteCell block;
  block.col = 8;
  block.row = 16;
  look.cells.push_back(block);
  SpriteCell c = pickCell(look, 0, 0, 37, 0, 8);  // 37 = 4*8 + 5
  CHECK_EQ((int)c.col, 8 + 5);
  CHECK_EQ((int)c.row, 16 + 4);
  c = pickCell(look, 0, 0, 63, 0, 8);
  CHECK_EQ((int)c.col, 8 + 7);
  CHECK_EQ((int)c.row, 16 + 7);
  c = pickCell(look, 0, 0, 13, 0);  // squares: unchanged 4 x 4 block
  CHECK_EQ((int)c.col, 8 + 1);
  CHECK_EQ((int)c.row, 16 + 3);
  CHECK_EQ(joinBlockSide(Grid(GridType::Hex, 3, 3)), 8);
  CHECK_EQ(joinBlockSide(Grid(GridType::Square, 3, 3)), 4);
}

namespace {

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

// A game with a 128 x 128 sheet (8 x 8 cells of 16 px) and a join look for the terrain at `join`.
std::string joinError(const char* name, const std::string& grid, const char* mapType, const char* join, Database& db) {
  std::string root = makeGame(name, grid, "", okMap(mapType));
  mkdir((root + "/art").c_str(), 0755);
  writePngHeader(root + "/art/tiles.png", 128, 128);
  CHECK(writeTextFile(root + "/data/graphics.json",
                      std::string("{ \"sheets\": [ {\"id\":\"tiles\",\"file\":\"art/tiles.png\",\"cellWidth\":16,\"cellHeight\":16} ],"
                                  " \"terrain\": { \"plain\": {\"sheet\":\"tiles\",\"join\":") + join + "} } }"));
  MapDef m;
  return tryLoad(root, db, m);
}

}  // namespace

TEST(hex_join_blocks_are_8_by_8_on_hexagon_games_and_4_by_4_on_squares) {
  Database db;
  CHECK_STR(joinError("j1", hexGrid("pointy"), "hex", "[0,0]", db), "");
  CHECK(db.graphics.styles[0].terrain[0].kind == Look::Join);
  CHECK_STR(joinError("j2", hexGrid("flat"), "hex", "[[0,0],[0,0]]", db), "");  // animated: several blocks
  std::string e = joinError("j3", hexGrid("pointy"), "hex", "[1,0]", db);
  CHECK(has(e, "the 8 x 8 block at [1, 0] is outside sheet \"tiles\" (8 x 8 cells)"));
  CHECK_STR(joinError("j4", "", "square", "[4,4]", db), "");  // squares: 4 x 4, as before
  e = joinError("j5", "", "square", "[5,4]", db);
  CHECK(has(e, "the 4 x 4 block at [5, 4] is outside sheet"));
}
