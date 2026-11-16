// Standing terrain: the "stand" of a terrain look in a 3D style (docs/GRAPHICS.md "Standing terrain").
#include <sys/stat.h>

#include <cstdio>
#include <set>

#include "core/base/json.h"
#include "core/data/loader.h"
#include "test.h"

using namespace ote;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

// Only the header matters to the core, so this is not a decodable image.
void pngHeader(const std::string& path, unsigned w, unsigned h) {
  unsigned char b[33] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13, 'I', 'H', 'D', 'R'};
  b[18] = (unsigned char)(w >> 8); b[19] = (unsigned char)w;
  b[22] = (unsigned char)(h >> 8); b[23] = (unsigned char)h;
  b[24] = 8; b[25] = 6;
  FILE* f = std::fopen(path.c_str(), "wb");
  CHECK(f != nullptr);
  if (!f) return;
  std::fwrite(b, 1, sizeof b, f);
  std::fclose(f);
}

// A small game: terrains plain/forest, styles day (top-down), solid (3D isometric), full (3D), sheets "tiles"
// (8 x 6 cells of 16 px) and "trees" (4 x 2 cells of 16 x 24), two model files.
std::string make(const char* name, const std::string& body) {
  std::string root = ote_test::tempDir() + "/stand_" + name;
  mkdir(root.c_str(), 0755);
  mkdir((root + "/data").c_str(), 0755);
  mkdir((root + "/art").c_str(), 0755);
  mkdir((root + "/models").c_str(), 0755);
  CHECK(writeTextFile(root + "/game.json",
                      "{ \"formatVersion\": 1, \"name\": \"G\", \"stats\": [{\"id\":\"hp\"},{\"id\":\"mov\"}],"
                      " \"roles\": {\"hp\":\"hp\",\"move\":\"mov\"}, \"moveTypes\": [{\"id\":\"foot\"}],"
                      " \"formulas\": {\"damage\":\"1\"},"
                      " \"styles\": [{\"id\":\"day\"},{\"id\":\"solid\",\"view\":\"isometric-3d\"},{\"id\":\"full\",\"view\":\"3d\"}] }"));
  CHECK(writeTextFile(root + "/data/terrain.json", "{ \"terrain\": [{\"id\":\"plain\"},{\"id\":\"forest\"}] }"));
  CHECK(writeTextFile(root + "/data/weapons.json", "{ \"weapons\": [{\"id\":\"stick\"}] }"));
  CHECK(writeTextFile(root + "/data/units.json",
                      "{ \"units\": [{\"id\":\"grunt\",\"moveType\":\"foot\",\"stats\":{\"hp\":5,\"mov\":3}}] }"));
  pngHeader(root + "/art/tiles.png", 128, 96);
  pngHeader(root + "/art/trees.png", 64, 48);
  CHECK(writeTextFile(root + "/models/tree.glb", "not a model: the core only checks that it exists"));
  CHECK(writeTextFile(root + "/models/tree2.glb", "not a model either"));
  CHECK(writeTextFile(root + "/data/graphics.json", body));
  return root;
}

const char* kSheets =
    "\"sheets\": [ {\"id\":\"tiles\",\"file\":\"art/tiles.png\",\"cellWidth\":16,\"cellHeight\":16},"
    "              {\"id\":\"trees\",\"file\":\"art/trees.png\",\"cellWidth\":16,\"cellHeight\":24} ]";

// The forest's look in the style "solid", with `stand` as the body of its "stand".
std::string forest(const std::string& stand) {
  return std::string("{ ") + kSheets + ", \"styles\": { \"solid\": { \"terrain\": { \"forest\": "
         "{\"sheet\":\"tiles\",\"cell\":[1,0],\"stand\":" + stand + "} } } } }";
}

void bad(const char* name, const std::string& body, const char* a, const char* b = "") {
  Database db;
  std::string err;
  bool ok = loadGame(make(name, body), db, err);
  CHECK(!ok);
  if (ok || !has(err, a) || !has(err, b)) std::printf("  [%s] message was: %s\n", name, err.c_str());
  CHECK(has(err, "graphics.json"));
  CHECK(has(err, a));
  CHECK(has(err, b));
}

bool loads(const char* name, const std::string& body, Database& db) {
  std::string err;
  bool ok = loadGame(make(name, body), db, err);
  if (!ok) std::printf("  [%s] %s\n", name, err.c_str());
  CHECK(ok);
  return ok;
}

}  // namespace

TEST(stand_picture_loads_with_variants_frames_and_width) {
  Database db;
  if (loads("pic_cell", forest("{\"sheet\":\"trees\",\"cell\":[1,0],\"width\":30,\"scatter\":true}"), db)) {
    const int forestIdx = db.findTerrain("forest");
    const StandLook& s = db.graphics.styles[1].stands[(size_t)forestIdx];
    CHECK(s.any() && !s.isModel());
    CHECK(s.picture.kind == Look::Still);
    CHECK(s.width == 30.0f && s.scatter);
    CHECK(db.graphics.styles[1].terrain[(size_t)forestIdx].kind == Look::Still);  // the ground picture stays
    CHECK(!db.graphics.styles[0].stands[(size_t)forestIdx].any());               // the top-down style has none
    CHECK(!db.graphics.styles[1].stands[(size_t)db.findTerrain("plain")].any());
    CHECK(db.graphics.styles[2].stands.size() == db.terrains.size());
  }
  Database v;
  if (loads("pic_var", forest("{\"sheet\":\"trees\",\"variants\":[[0,0],[1,0],[2,0]]}"), v)) {
    const StandLook& s = v.graphics.styles[1].stands[1];
    CHECK(s.picture.kind == Look::Variants && s.picture.cells.size() == 3);
    CHECK(s.width == 0.0f && !s.scatter);
  }
  Database f;
  if (loads("pic_frames", forest("{\"sheet\":\"trees\",\"frames\":[[0,1],[1,1]],\"frameMs\":250}"), f)) {
    const StandLook& s = f.graphics.styles[1].stands[1];
    CHECK(s.picture.kind == Look::Animated && s.picture.frameMs == 250);
    CHECK(pickStandCell(s, 3, 4, 0) == (SpriteCell{0, 1}));
    CHECK(pickStandCell(s, 3, 4, 260) == (SpriteCell{1, 1}));
  }
}

TEST(stand_model_loads_with_variants_height_and_turn) {
  Database db;
  if (loads("model_one", forest("{\"model\":\"models/tree.glb\",\"height\":1.4,\"turn\":90}"), db)) {
    const StandLook& s = db.graphics.styles[1].stands[1];
    CHECK(s.isModel() && s.models.size() == 1);
    CHECK_STR(s.models[0].file, "models/tree.glb");
    CHECK(s.models[0].height == 1.4f && s.models[0].turn == 90.0f);
    CHECK(s.models[0].sideMaterial.empty());
  }
  Database v;
  if (loads("model_var", forest("{\"variants\":[\"models/tree.glb\",\"models/tree2.glb\"],\"scatter\":true}"), v)) {
    const StandLook& s = v.graphics.styles[1].stands[1];
    CHECK(s.models.size() == 2 && s.scatter);
    CHECK_STR(s.models[1].file, "models/tree2.glb");
    CHECK(s.models[0].height == 1.0f);  // the default
  }
}

TEST(stand_is_refused_outside_3d_styles) {
  // Every place a terrain look can be written that is not a 3D style.
  const std::string look = "{\"sheet\":\"tiles\",\"cell\":[1,0],\"stand\":{\"model\":\"models/tree.glb\"}}";
  bad("flat_root", std::string("{ ") + kSheets + ", \"terrain\": { \"forest\": " + look + " } }", "kind of view is 3D", "terrain \"forest\" stand");
  bad("flat_style", std::string("{ ") + kSheets + ", \"styles\": { \"day\": { \"terrain\": { \"forest\": " + look + " } } } }",
      "kind of view is 3D", "style \"day\"");
  // A 2D isometric style is not 3D either.
  const std::string picture = "{\"sheet\":\"trees\",\"cell\":[0,0]}";
  bad("flat_picture", std::string("{ ") + kSheets + ", \"styles\": { \"day\": { \"terrain\": { \"forest\": "
                      "{\"sheet\":\"tiles\",\"cell\":[1,0],\"stand\":" + picture + "} } } } }",
      "\"stand\" can only be used", "(\"isometric-3d\" or \"3d\")");
}

TEST(stand_refusals_are_exact) {
  bad("not_object", forest("5"), "stand: must be an object");
  bad("empty", forest("{}"), "needs a \"sheet\" (an upright picture) or a \"model\" (a 3D model)");
  bad("unknown_key", forest("{\"sheet\":\"trees\",\"cell\":[0,0],\"colour\":\"red\"}"), "has an unknown key \"colour\"");
  bad("unknown_key_model", forest("{\"model\":\"models/tree.glb\",\"shine\":1}"), "has an unknown key \"shine\"");
  bad("join_key", forest("{\"sheet\":\"trees\",\"join\":[0,0]}"), "has an unknown key \"join\"");
  bad("picture_turn", forest("{\"sheet\":\"trees\",\"cell\":[0,0],\"turn\":90}"), "is a picture, so it cannot have \"turn\"");
  bad("model_cell", forest("{\"model\":\"models/tree.glb\",\"cell\":[0,0]}"), "has a \"model\", so it cannot also have \"cell\"");
  bad("model_sheet", forest("{\"model\":\"models/tree.glb\",\"sheet\":\"trees\"}"), "has a \"model\", so it cannot also have \"sheet\"");
  bad("model_width", forest("{\"model\":\"models/tree.glb\",\"width\":20}"), "so it cannot also have \"width\"");
  bad("no_sheet_kind", forest("{\"cell\":[0,0]}"), "needs a \"sheet\"");
  bad("unknown_sheet", forest("{\"sheet\":\"nope\",\"cell\":[0,0]}"), "unknown sheet \"nope\"");
  bad("two_kinds", forest("{\"sheet\":\"trees\",\"cell\":[0,0],\"frames\":[[0,0],[1,0]]}"),
      "needs exactly one of \"cell\", \"variants\" or \"frames\"");
  bad("no_kind", forest("{\"sheet\":\"trees\"}"), "needs exactly one of \"cell\", \"variants\" or \"frames\"");
  bad("outside", forest("{\"sheet\":\"trees\",\"cell\":[9,9]}"), "cell [9, 9] is outside sheet \"trees\" (4 x 2 cells)");
  bad("width_small", forest("{\"sheet\":\"trees\",\"cell\":[0,0],\"width\":0}"), "\"width\" must be between 1 and 512");
  bad("width_big", forest("{\"sheet\":\"trees\",\"cell\":[0,0],\"width\":600}"), "\"width\" must be between 1 and 512");
  bad("width_text", forest("{\"sheet\":\"trees\",\"cell\":[0,0],\"width\":\"wide\"}"), "\"width\"");
  bad("scatter_text", forest("{\"sheet\":\"trees\",\"cell\":[0,0],\"scatter\":\"yes\"}"), "\"scatter\"");
  bad("model_missing", forest("{\"model\":\"models/nobody.glb\"}"), "model file \"models/nobody.glb\" does not exist");
  bad("variant_missing", forest("{\"variants\":[\"models/tree.glb\",\"models/nobody.glb\"]}"), "model file \"models/nobody.glb\" does not exist");
  bad("model_path", forest("{\"model\":\"../tree.glb\"}"), "a model must be a .glb or .gltf file inside the game folder");
  bad("model_type", forest("{\"model\":\"models/tree.png\"}"), "a model must be a .glb or .gltf file");
  bad("model_and_variants", forest("{\"model\":\"models/tree.glb\",\"variants\":[\"models/tree2.glb\"]}"),
      "needs exactly one of \"model\" (one file) or \"variants\" (a list of model files)");
  bad("variants_mixed", forest("{\"variants\":[\"models/tree.glb\",5]}"), "\"variants\" must list model files (text)");
  bad("height_big", forest("{\"model\":\"models/tree.glb\",\"height\":40}"), "\"height\" must be between 0.1 and 16");
  bad("height_small", forest("{\"model\":\"models/tree.glb\",\"height\":0}"), "\"height\" must be between 0.1 and 16");
  bad("turn_big", forest("{\"model\":\"models/tree.glb\",\"turn\":720}"), "\"turn\" must be between -360 and 360");
}

TEST(stand_null_and_overrides) {
  // "stand": null means nothing; a terrain set to null in a later place drops its stand with the look.
  Database db;
  if (loads("null", forest("null"), db)) CHECK(!db.graphics.styles[1].stands[1].any());
  // Without the key nothing is added and the pictures load as they always did.
  Database plain;
  std::string body = std::string("{ ") + kSheets + ", \"styles\": { \"solid\": { \"terrain\": { \"forest\": {\"sheet\":\"tiles\",\"cell\":[1,0]} } } } }";
  if (loads("none", body, plain)) {
    for (const GraphicsSet& s : plain.graphics.styles)
      for (const StandLook& st : s.stands) CHECK(!st.any());
  }
}

TEST(stand_choice_is_stable_and_mixes_variants) {
  Database db;
  if (!loads("choice", forest("{\"sheet\":\"trees\",\"variants\":[[0,0],[1,0],[2,0],[3,0]],\"scatter\":true}"), db)) return;
  const StandLook& s = db.graphics.styles[1].stands[1];
  std::set<int> seen;
  for (int y = 0; y < 10; ++y)
    for (int x = 0; x < 10; ++x) {
      SpriteCell a = pickStandCell(s, x, y, 0), b = pickStandCell(s, x, y, 123456);
      CHECK(a == b);  // the same square always shows the same one, whatever the time
      CHECK(a == pickStandCell(s, x, y, 0));
      seen.insert(a.col);
    }
  CHECK_EQ((int)seen.size(), 4);  // a forest is not a row of identical trees
  // The same choice terrain variants make for the same square.
  Look terrain;
  terrain.kind = Look::Variants;
  terrain.cells = s.picture.cells;
  for (int y = 0; y < 6; ++y)
    for (int x = 0; x < 6; ++x) CHECK(pickCell(terrain, x, y, 0, 0) == pickStandCell(s, x, y, 0));
  // Scatter: stable, small, and not the same everywhere.
  std::set<int> turns;
  for (int y = 0; y < 8; ++y)
    for (int x = 0; x < 8; ++x) {
      float dx, dz, t, dx2, dz2, t2;
      standScatter(x, y, dx, dz, t);
      standScatter(x, y, dx2, dz2, t2);
      CHECK(dx == dx2 && dz == dz2 && t == t2);
      CHECK(dx >= -0.19f && dx <= 0.19f && dz >= -0.19f && dz <= 0.19f && t >= 0 && t < 360);
      turns.insert((int)t / 30);
    }
  CHECK(turns.size() > 6);
  // Models pick by the same hash.
  Database m;
  if (loads("choice_m", forest("{\"variants\":[\"models/tree.glb\",\"models/tree2.glb\"]}"), m)) {
    const StandLook& ms = m.graphics.styles[1].stands[1];
    std::set<int> picked;
    for (int y = 0; y < 8; ++y)
      for (int x = 0; x < 8; ++x) {
        int i = pickStandModel(ms, x, y);
        CHECK(i == pickStandModel(ms, x, y) && i >= 0 && i < 2);
        picked.insert(i);
      }
    CHECK_EQ((int)picked.size(), 2);
  }
}
