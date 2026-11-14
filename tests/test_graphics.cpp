// data/graphics.json loading and cell picking.
#include <sys/stat.h>

#include <cstdio>

#include "core/base/json.h"
#include "core/data/loader.h"
#include "test.h"

using namespace ote;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

// Only the header matters to the core, so this is not a decodable image.
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

// A small game in the scratch folder: terrains plain/water/road, unit grunt,
// styles day/night, sheets "tiles" (8 x 6 cells of 16 px) and "men" (4 x 2).
std::string makeGame(const char* name, const std::string& graphicsJson) {
  std::string root = ote_test::tempDir() + "/gfx_" + name;
  mkdir(root.c_str(), 0755);
  mkdir((root + "/data").c_str(), 0755);
  mkdir((root + "/art").c_str(), 0755);
  CHECK(writeTextFile(root + "/game.json",
                      "{ \"formatVersion\": 1, \"name\": \"G\", \"stats\": [{\"id\":\"hp\"},{\"id\":\"mov\"}],"
                      " \"roles\": {\"hp\":\"hp\",\"move\":\"mov\"}, \"moveTypes\": [{\"id\":\"foot\"}],"
                      " \"formulas\": {\"damage\":\"1\"},"
                      " \"styles\": [{\"id\":\"day\"},{\"id\":\"night\"}] }"));
  CHECK(writeTextFile(root + "/data/terrain.json",
                      "{ \"terrain\": [{\"id\":\"plain\"},{\"id\":\"water\"},{\"id\":\"road\"}] }"));
  CHECK(writeTextFile(root + "/data/weapons.json", "{ \"weapons\": [{\"id\":\"stick\"}] }"));
  CHECK(writeTextFile(root + "/data/units.json",
                      "{ \"units\": [{\"id\":\"grunt\",\"moveType\":\"foot\",\"stats\":{\"hp\":5,\"mov\":3}}] }"));
  writePngHeader(root + "/art/tiles.png", 128, 96);
  writePngHeader(root + "/art/men.png", 64, 32);
  writePngHeader(root + "/art/huge.png", 4096, 16);
  CHECK(writeTextFile(root + "/art/notpng.png", "hello, I am text"));
  if (graphicsJson.empty()) std::remove((root + "/data/graphics.json").c_str());
  else CHECK(writeTextFile(root + "/data/graphics.json", graphicsJson));
  return root;
}

const char* kSheets =
    "\"sheets\": [ {\"id\":\"tiles\",\"file\":\"art/tiles.png\",\"cellWidth\":16,\"cellHeight\":16},"
    "              {\"id\":\"men\",\"file\":\"art/men.png\",\"cellWidth\":16,\"cellHeight\":16} ]";

void expectError(const char* name, const std::string& body, const char* a, const char* b = "") {
  Database db;
  std::string err;
  bool ok = loadGame(makeGame(name, body), db, err);
  CHECK(!ok);
  if (ok || !has(err, a) || !has(err, b)) std::printf("  [%s] message was: %s\n", name, err.c_str());
  CHECK(has(err, "graphics.json"));
  CHECK(has(err, a));
  CHECK(has(err, b));
}

}  // namespace

TEST(graphics_absent_file_means_no_art) {
  Database db;
  std::string err;
  CHECK(loadGame(makeGame("none", ""), db, err));
  CHECK(!db.graphics.any());
  CHECK(db.graphics.set(0) == nullptr);
}

TEST(graphics_loads_all_kinds) {
  std::string body = std::string("{ ") + kSheets +
      ", \"terrain\": {"
      "   \"plain\": {\"sheet\":\"tiles\",\"variants\":[[0,0],[1,0],[2,0]]},"
      "   \"water\": {\"sheet\":\"tiles\",\"frames\":[[0,1],[1,1]],\"frameMs\":250},"
      "   \"road\":  {\"sheet\":\"tiles\",\"join\":[4,2],\"joinWith\":[\"water\"],\"joinEdge\":false} },"
      " \"units\": { \"grunt\": {\"sheet\":\"men\",\"frames\":[[0,0],[1,0]],"
      "                         \"sides\": {\"red\": {\"sheet\":\"men\",\"cell\":[2,1]}}} },"
      " \"styles\": { \"night\": { \"terrain\": { \"plain\": {\"sheet\":\"tiles\",\"cell\":[7,5]}, \"water\": null } } } }";
  Database db;
  std::string err;
  bool ok = loadGame(makeGame("all", body), db, err);
  if (!ok) std::printf("  %s\n", err.c_str());
  CHECK(ok);
  if (!ok) return;
  CHECK_EQ((int)db.graphics.sheets.size(), 2);
  CHECK_EQ(db.graphics.sheets[0].cols, 8);
  CHECK_EQ(db.graphics.sheets[0].rows, 6);
  CHECK_EQ((int)db.graphics.styles.size(), 2);
  CHECK(db.graphics.unitMarker);

  const GraphicsSet& day = *db.graphics.set(0);
  CHECK(day.terrain[0].kind == Look::Variants);
  CHECK_EQ((int)day.terrain[0].cells.size(), 3);
  CHECK(day.terrain[1].kind == Look::Animated);
  CHECK_EQ(day.terrain[1].frameMs, 250);
  const Look& road = day.terrain[2];
  CHECK(road.kind == Look::Join);
  CHECK(!road.joinEdge);
  CHECK_EQ((int)road.joinWith[0], 0);
  CHECK_EQ((int)road.joinWith[1], 1);  // water, from joinWith
  CHECK_EQ((int)road.joinWith[2], 1);  // itself
  CHECK(day.units[0].base.kind == Look::Animated);
  CHECK(day.units[0].forSide("red").kind == Look::Still);
  CHECK(day.units[0].forSide("blue").kind == Look::Animated);

  // The style overrides plain, removes water's art, keeps everything else.
  const GraphicsSet& night = *db.graphics.set(1);
  CHECK(night.terrain[0].kind == Look::Still);
  CHECK_EQ((int)night.terrain[0].cells[0].col, 7);
  CHECK(night.terrain[1].kind == Look::None);
  CHECK(night.terrain[2].kind == Look::Join);
  CHECK(night.units[0].base.kind == Look::Animated);
  // Out-of-range style index falls back to the first style.
  CHECK(db.graphics.set(99) == db.graphics.set(0));
}

TEST(graphics_pick_cell) {
  Look v;
  v.kind = Look::Variants;
  for (int i = 0; i < 4; ++i) {
    SpriteCell c;
    c.col = (int16_t)i;
    v.cells.push_back(c);
  }
  // Stable per square, independent of time, and all variants get used.
  int seen[4] = {0, 0, 0, 0};
  for (int y = 0; y < 16; ++y)
    for (int x = 0; x < 16; ++x) {
      SpriteCell a = pickCell(v, x, y, 0, 0), b = pickCell(v, x, y, 15, 99999);
      CHECK(a == b);
      seen[a.col]++;
    }
  for (int i = 0; i < 4; ++i) CHECK(seen[i] > 30);

  Look a = v;
  a.kind = Look::Animated;
  a.frameMs = 100;
  CHECK_EQ((int)pickCell(a, 5, 5, 0, 0).col, 0);
  CHECK_EQ((int)pickCell(a, 5, 5, 0, 199).col, 1);
  CHECK_EQ((int)pickCell(a, 5, 5, 0, 400).col, 0);  // loops

  Look j;
  j.kind = Look::Join;
  SpriteCell at;
  at.col = 4;
  at.row = 2;
  j.cells.push_back(at);
  SpriteCell none = pickCell(j, 0, 0, 0, 0);
  CHECK_EQ((int)none.col, 4);
  CHECK_EQ((int)none.row, 2);
  SpriteCell ns = pickCell(j, 0, 0, JoinN | JoinS, 0);  // 5 -> column 1, row 1
  CHECK_EQ((int)ns.col, 5);
  CHECK_EQ((int)ns.row, 3);
  SpriteCell all = pickCell(j, 0, 0, 15, 0);
  CHECK_EQ((int)all.col, 7);
  CHECK_EQ((int)all.row, 5);
}

TEST(graphics_sheet_smooth_flag) {
  // "smooth" is optional (default: hard pixels) and must be a boolean.
  Database db;
  std::string err;
  std::string body =
      "{ \"sheets\": [ {\"id\":\"tiles\",\"file\":\"art/tiles.png\",\"cellWidth\":16,\"cellHeight\":16,\"smooth\":true},"
      "              {\"id\":\"men\",\"file\":\"art/men.png\",\"cellWidth\":16,\"cellHeight\":16} ] }";
  bool ok = loadGame(makeGame("smooth", body), db, err);
  if (!ok) std::printf("  %s\n", err.c_str());
  CHECK(ok);
  if (!ok) return;
  CHECK(db.graphics.sheets[0].smooth);
  CHECK(!db.graphics.sheets[1].smooth);
  expectError("e_smooth", "{ \"sheets\": [{\"id\":\"a\",\"file\":\"art/tiles.png\",\"cellWidth\":16,\"cellHeight\":16,\"smooth\":1}] }",
              "smooth");
}

TEST(graphics_animated_join) {
  // tiles.png is 8 x 6 cells: two 4 x 4 blocks side by side fit in rows 0..3.
  std::string body = std::string("{ ") + kSheets +
      ", \"terrain\": { \"water\": {\"sheet\":\"tiles\",\"join\":[[0,0],[4,0]],\"frameMs\":200} } }";
  Database db;
  std::string err;
  bool ok = loadGame(makeGame("animjoin", body), db, err);
  if (!ok) std::printf("  %s\n", err.c_str());
  CHECK(ok);
  if (!ok) return;
  const Look& l = db.graphics.set(0)->terrain[1];
  CHECK(l.kind == Look::Join);
  CHECK_EQ((int)l.cells.size(), 2);
  CHECK_EQ(l.frameMs, 200);
  // Piece 6 (E+S) is column 2, row 1 inside whichever block the time selects.
  SpriteCell a = pickCell(l, 3, 3, JoinE | JoinS, 0), b = pickCell(l, 3, 3, JoinE | JoinS, 200),
             c = pickCell(l, 3, 3, JoinE | JoinS, 400);
  CHECK_EQ((int)a.col, 2);
  CHECK_EQ((int)a.row, 1);
  CHECK_EQ((int)b.col, 6);
  CHECK_EQ((int)b.row, 1);
  CHECK(a == c);  // loops

  // The second block must fit too.
  expectError("e_animjoin", std::string("{ ") + kSheets +
                                ", \"terrain\": {\"water\": {\"sheet\":\"tiles\",\"join\":[[0,0],[5,0]]}} }",
              "4 x 4 block at [5, 0]", "outside");
}

TEST(graphics_join_except) {
  // Terrains: plain(0), water(1), road(2). The road joins everything but water.
  std::string s = std::string("{ ") + kSheets + ", ";
  Database db;
  std::string err;
  bool ok = loadGame(makeGame("joinexcept", s + "\"terrain\": {\"road\": {\"sheet\":\"tiles\",\"join\":[0,0],"
                                                "\"joinExcept\":[\"water\"]}} }"), db, err);
  if (!ok) std::printf("  %s\n", err.c_str());
  CHECK(ok);
  if (!ok) return;
  const Look& l = db.graphics.set(0)->terrain[2];
  CHECK_EQ((int)l.joinWith[0], 1);
  CHECK_EQ((int)l.joinWith[1], 0);
  CHECK_EQ((int)l.joinWith[2], 1);
  expectError("e_except_both", s + "\"terrain\": {\"road\": {\"sheet\":\"tiles\",\"join\":[0,0],"
                                   "\"joinWith\":[\"plain\"],\"joinExcept\":[\"water\"]}} }", "not both");
  expectError("e_except_self", s + "\"terrain\": {\"road\": {\"sheet\":\"tiles\",\"join\":[0,0],"
                                   "\"joinExcept\":[\"road\"]}} }", "cannot list the terrain itself");
  expectError("e_except_unknown", s + "\"terrain\": {\"road\": {\"sheet\":\"tiles\",\"join\":[0,0],"
                                      "\"joinExcept\":[\"lava\"]}} }", "joinExcept", "lava");
}

TEST(graphics_errors_are_readable) {
  std::string s = std::string("{ ") + kSheets + ", ";
  expectError("e_sheet", s + "\"terrain\": {\"plain\": {\"sheet\":\"nope\",\"cell\":[0,0]}} }", "terrain \"plain\"",
              "unknown sheet \"nope\"");
  expectError("e_outside", s + "\"terrain\": {\"plain\": {\"sheet\":\"tiles\",\"cell\":[8,0]}} }", "cell [8, 0]",
              "8 x 6 cells");
  expectError("e_join", s + "\"terrain\": {\"road\": {\"sheet\":\"tiles\",\"join\":[5,3]}} }", "4 x 4 block", "outside");
  expectError("e_kinds", s + "\"terrain\": {\"plain\": {\"sheet\":\"tiles\",\"cell\":[0,0],\"join\":[0,0]}} }",
              "exactly one of");
  expectError("e_nokind", s + "\"terrain\": {\"plain\": {\"sheet\":\"tiles\"}} }", "exactly one of");
  expectError("e_terrain", s + "\"terrain\": {\"lava\": {\"sheet\":\"tiles\",\"cell\":[0,0]}} }", "terrain \"lava\"",
              "no terrain");
  expectError("e_unit", s + "\"units\": {\"ghost\": {\"sheet\":\"men\",\"cell\":[0,0]}} }", "unit \"ghost\"");
  expectError("e_unitjoin", s + "\"units\": {\"grunt\": {\"sheet\":\"men\",\"join\":[0,0]}} }", "units can only use");
  expectError("e_style", s + "\"styles\": {\"dusk\": {}} }", "style \"dusk\"", "no visual style");
  expectError("e_joinwith",
              s + "\"terrain\": {\"road\": {\"sheet\":\"tiles\",\"join\":[0,0],\"joinWith\":[\"lava\"]}} }",
              "joinWith", "lava");
  expectError("e_cellshape", s + "\"terrain\": {\"plain\": {\"sheet\":\"tiles\",\"cell\":[0]}} }", "[column, row]");
  expectError("e_side", s + "\"units\": {\"grunt\": {\"sheet\":\"men\",\"cell\":[0,0],"
                            "\"sides\":{\"red\":{\"sheet\":\"men\",\"cell\":[9,9]}}}} }",
              "side \"red\"", "outside");

  expectError("e_missing", "{ \"sheets\": [{\"id\":\"a\",\"file\":\"art/gone.png\",\"cellWidth\":16,\"cellHeight\":16}] }",
              "art/gone.png", "missing");
  expectError("e_notpng", "{ \"sheets\": [{\"id\":\"a\",\"file\":\"art/notpng.png\",\"cellWidth\":16,\"cellHeight\":16}] }",
              "not a PNG");
  expectError("e_huge", "{ \"sheets\": [{\"id\":\"a\",\"file\":\"art/huge.png\",\"cellWidth\":16,\"cellHeight\":16}] }",
              "4096 x 16", "2048");
  expectError("e_path", "{ \"sheets\": [{\"id\":\"a\",\"file\":\"../x.png\",\"cellWidth\":16,\"cellHeight\":16}] }",
              "inside the game folder");
  expectError("e_bigcell", "{ \"sheets\": [{\"id\":\"a\",\"file\":\"art/men.png\",\"cellWidth\":100,\"cellHeight\":16}] }",
              "larger than image");
  expectError("e_dup", "{ \"sheets\": [{\"id\":\"a\",\"file\":\"art/men.png\",\"cellWidth\":16,\"cellHeight\":16},"
                       "{\"id\":\"a\",\"file\":\"art/men.png\",\"cellWidth\":16,\"cellHeight\":16}] }",
              "duplicate id");
}

TEST(graphics_models_load_only_in_3d_styles) {
  // The same small game with a third style of the 3D isometric kind and a model file.
  auto make = [](const char* name, const std::string& body) {
    std::string root = makeGame(name, "");
    mkdir((root + "/models").c_str(), 0755);
    CHECK(writeTextFile(root + "/models/grunt.glb", "not really a model: the core only checks that it is there"));
    CHECK(writeTextFile(root + "/game.json",
                        "{ \"formatVersion\": 1, \"name\": \"G\", \"stats\": [{\"id\":\"hp\"},{\"id\":\"mov\"}],"
                        " \"roles\": {\"hp\":\"hp\",\"move\":\"mov\"}, \"moveTypes\": [{\"id\":\"foot\"}],"
                        " \"formulas\": {\"damage\":\"1\"},"
                        " \"styles\": [{\"id\":\"day\"},{\"id\":\"night\"},{\"id\":\"solid\",\"view\":\"isometric-3d\"}] }"));
    CHECK(writeTextFile(root + "/data/graphics.json", body));
    return root;
  };
  Database db;
  std::string err;
  bool ok = loadGame(make("model_ok", "{ \"styles\": { \"solid\": { \"units\": { \"grunt\": { \"model\": \"models/grunt.glb\","
                                      " \"height\": 1.5, \"turn\": 90, \"sideMaterial\": \"cloth\","
                                      " \"animations\": { \"move\": \"Run\", \"die\": \"Fall\" } } } } } }"),
                     db, err);
  if (!ok) std::printf("  %s\n", err.c_str());
  CHECK(ok);
  if (ok) {
    // No sheets at all, yet the style has its set.
    CHECK_EQ((int)db.graphics.styles.size(), 3);
    const ModelLook& m = db.graphics.styles[2].units[0].model;
    CHECK(m.any());
    CHECK_STR(m.file, "models/grunt.glb");
    CHECK(m.height == 1.5f && m.turn == 90.0f);
    CHECK_STR(m.sideMaterial, "cloth");
    CHECK_STR(m.clips[AnimIdle], "Idle");
    CHECK_STR(m.clips[AnimMove], "Run");
    CHECK_STR(m.clips[AnimAttack], "Attack");
    CHECK_STR(m.clips[AnimDie], "Fall");
    CHECK(!db.graphics.styles[0].units[0].model.any());
  }
  auto bad = [&](const char* name, const std::string& body, const char* a, const char* b = "") {
    Database d;
    std::string e;
    bool loaded = loadGame(make(name, body), d, e);
    CHECK(!loaded);
    if (loaded || !has(e, a) || !has(e, b)) std::printf("  [%s] message was: %s\n", name, e.c_str());
    CHECK(has(e, a) && has(e, b));
  };
  bad("model_flat", "{ \"units\": { \"grunt\": { \"model\": \"models/grunt.glb\" } } }", "kind of view is 3D", "unit \"grunt\"");
  bad("model_flat_style", "{ \"styles\": { \"night\": { \"units\": { \"grunt\": { \"model\": \"models/grunt.glb\" } } } } }",
      "kind of view is 3D", "style \"night\"");
  bad("model_missing", "{ \"styles\": { \"solid\": { \"units\": { \"grunt\": { \"model\": \"models/nobody.glb\" } } } } }",
      "does not exist", "models/nobody.glb");
  bad("model_path", "{ \"styles\": { \"solid\": { \"units\": { \"grunt\": { \"model\": \"../grunt.glb\" } } } } }", ".glb or .gltf");
  bad("model_type", "{ \"styles\": { \"solid\": { \"units\": { \"grunt\": { \"model\": \"models/grunt.png\" } } } } }", ".glb or .gltf");
  bad("model_both", "{ \"styles\": { \"solid\": { \"units\": { \"grunt\": { \"model\": \"models/grunt.glb\", \"frameMs\": 100 } } } } }",
      "cannot also have", "frameMs");
  bad("model_height", "{ \"styles\": { \"solid\": { \"units\": { \"grunt\": { \"model\": \"models/grunt.glb\", \"height\": 40 } } } } }",
      "\"height\" must be between");
  bad("model_anim", "{ \"styles\": { \"solid\": { \"units\": { \"grunt\": { \"model\": \"models/grunt.glb\","
                    " \"animations\": { \"dance\": \"Jig\" } } } } } }", "not \"dance\"");
}
