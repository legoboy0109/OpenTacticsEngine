// Map art for prerendered isometric games: the map's "backdrop" (docs/VIEWS.md "Backdrop"), character looks, poses and facing,
// a sheet's "unitOffset" and "portraitSize" (docs/GRAPHICS.md). Loading and refusals; the drawing is tests/iso_map_art_player_test.py.
#include "posix_compat.h"

#include <cstdio>

#include "core/data/loader.h"
#include "test.h"

using namespace ote;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

void png(const std::string& path, int w, int h) {
  unsigned char b[33] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13, 'I', 'H', 'D', 'R', 0, 0, (unsigned char)(w >> 8),
                         (unsigned char)w, 0, 0, (unsigned char)(h >> 8), (unsigned char)h, 8, 6};
  FILE* f = std::fopen(path.c_str(), "wb");
  CHECK(f != nullptr);
  if (!f) return;
  std::fwrite(b, 1, sizeof b, f);
  std::fclose(f);
}

const char* kSheets =
    "\"sheets\": [{\"id\":\"men\",\"file\":\"art/men.png\",\"cellWidth\":16,\"cellHeight\":16},"
    "{\"id\":\"ground\",\"file\":\"art/ground.png\",\"cellWidth\":160,\"cellHeight\":120},"
    "{\"id\":\"props\",\"file\":\"art/props.png\",\"cellWidth\":160,\"cellHeight\":120},"
    "{\"id\":\"faces\",\"file\":\"art/faces.png\",\"cellWidth\":64,\"cellHeight\":64}]";

// A game of 4 x 3 squares, styles "iso" (2D isometric) and "flat" (unless `styles` says otherwise), character "eirik" (a knight).
// `gfx`: more keys of data/graphics.json after the sheets; `mapExtra`: more keys of the map file.
std::string makeGame(const char* name, const std::string& gfx, const std::string& mapExtra = "",
                     const std::string& styles = "[{\"id\":\"iso\",\"view\":\"isometric\"},{\"id\":\"flat\"}]") {
  std::string root = ote_test::tempDir() + "/isoart_" + name;
  mkdir(root.c_str(), 0755);
  mkdir((root + "/data").c_str(), 0755);
  mkdir((root + "/art").c_str(), 0755);
  mkdir((root + "/maps").c_str(), 0755);
  CHECK(writeTextFile(root + "/game.json",
                      "{ \"formatVersion\": 1, \"name\": \"G\", \"stats\": [{\"id\":\"hp\"},{\"id\":\"mov\"}],"
                      " \"roles\": {\"hp\":\"hp\",\"move\":\"mov\"}, \"moveTypes\": [{\"id\":\"foot\"}], \"formulas\": {\"damage\":\"1\"},"
                      " \"styles\": " + styles + ", \"maps\": [{\"id\":\"m\",\"name\":\"M\",\"file\":\"maps/m.json\"}] }"));
  CHECK(writeTextFile(root + "/data/terrain.json", "{ \"terrain\": [{\"id\":\"plain\"}] }"));
  CHECK(writeTextFile(root + "/data/weapons.json", "{ \"weapons\": [{\"id\":\"sword\",\"group\":\"blade\"}] }"));
  CHECK(writeTextFile(root + "/data/units.json",
                      "{ \"units\": [{\"id\":\"knight\",\"moveType\":\"foot\",\"stats\":{\"hp\":5,\"mov\":3},\"weapons\":[\"sword\"]}] }"));
  CHECK(writeTextFile(root + "/data/characters.json", "{ \"characters\": [{\"id\":\"eirik\",\"name\":\"Eirik\",\"type\":\"knight\"}] }"));
  CHECK(writeTextFile(root + "/maps/m.json",
                      "{ \"name\": \"M\", \"grid\": {\"type\":\"square\",\"width\":4,\"height\":3}, \"legend\": {\".\":\"plain\"},"
                      " \"tiles\": [\"....\",\"....\",\"....\"], \"heights\": [[1,1,1,1],[1,2,1,1],[1,1,1,1]],"
                      " \"sides\": [{\"id\":\"a\",\"controller\":\"human\"},{\"id\":\"b\",\"controller\":\"ai\"}],"
                      " \"units\": [{\"character\":\"eirik\",\"side\":\"a\",\"x\":0,\"y\":0},{\"type\":\"knight\",\"side\":\"b\",\"x\":3,\"y\":2}]" +
                          mapExtra + " }"));
  png(root + "/art/men.png", 128, 32);
  png(root + "/art/ground.png", 160, 120);
  png(root + "/art/props.png", 160, 120);
  png(root + "/art/faces.png", 128, 64);
  CHECK(writeTextFile(root + "/data/graphics.json", std::string("{ ") + kSheets + (gfx.empty() ? "" : ", " + gfx) + " }"));
  return root;
}

// Loads the game and its map; "" when both load, else the message.
std::string loadError(const std::string& root, Database& db, MapDef& m) {
  std::string err;
  if (!loadGame(root, db, err)) return err;
  if (!loadMap(db, "m", m, err)) return err;
  return "";
}

void expectRefused(const char* name, const std::string& gfx, const std::string& mapExtra, const char* a, const char* b = "",
                   const std::string& styles = "[{\"id\":\"iso\",\"view\":\"isometric\"},{\"id\":\"flat\"}]") {
  Database db;
  MapDef m;
  const std::string err = loadError(makeGame(name, gfx, mapExtra, styles), db, m);
  CHECK(!err.empty());
  if (!has(err, a) || !has(err, b)) std::printf("  [%s] message was: %s\n", name, err.c_str());
  CHECK(has(err, a) && has(err, b));
}

const char* kBackdrop =
    ", \"backdrop\": { \"sheet\":\"ground\", \"origin\":[64, 20], \"over\":\"props\","
    " \"pieces\": [ {\"rect\":[10,20,30,40],\"square\":[1,1]}, {\"rect\":[0,0,160,120],\"square\":[3,2]} ] }";

}  // namespace

TEST(backdrop_loads_and_games_without_one_have_none) {
  Database db;
  MapDef m;
  std::string err = loadError(makeGame("bd_ok", "", kBackdrop), db, m);
  if (!err.empty()) std::printf("  %s\n", err.c_str());
  CHECK(err.empty());
  const MapBackdrop& b = m.backdrop;
  CHECK(b.any());
  CHECK_EQ(b.sheet, db.graphics.findSheet("ground"));
  CHECK_EQ(b.over, db.graphics.findSheet("props"));
  CHECK_EQ(b.originX, 64);
  CHECK_EQ(b.originY, 20);
  CHECK_EQ((int)b.pieces.size(), 2);
  CHECK(b.pieces[0].x == 10 && b.pieces[0].y == 20 && b.pieces[0].w == 30 && b.pieces[0].h == 40 && b.pieces[0].gx == 1 && b.pieces[0].gy == 1);
  CHECK(b.styles.empty() && b.inStyle(0) && b.inStyle(1));  // every 2D isometric style (the drawer checks the kind)
  // "styles" narrows it; a map without the key has none.
  err = loadError(makeGame("bd_styles", "", ", \"backdrop\": { \"sheet\":\"ground\", \"styles\":[\"iso\"] }"), db, m);
  CHECK(err.empty());
  CHECK(m.backdrop.inStyle(0) && !m.backdrop.inStyle(1) && m.backdrop.over == -1 && m.backdrop.pieces.empty());
  err = loadError(makeGame("bd_none", ""), db, m);
  CHECK(err.empty() && !m.backdrop.any());
}

TEST(backdrop_refusals) {
  expectRefused("bd_shape", "", ", \"backdrop\": 3", "maps/m.json", "backdrop: must be an object");
  expectRefused("bd_key", "", ", \"backdrop\": {\"sheet\":\"ground\",\"offset\":[0,0]}", "backdrop", "unknown key \"offset\"");
  expectRefused("bd_nosheet", "", ", \"backdrop\": {\"origin\":[0,0]}", "backdrop", "\"sheet\"");
  expectRefused("bd_sheet", "", ", \"backdrop\": {\"sheet\":\"sky\"}", "backdrop", "\"sheet\" names unknown sheet \"sky\"");
  expectRefused("bd_over", "", ", \"backdrop\": {\"sheet\":\"ground\",\"over\":\"tree\"}", "\"over\" names unknown sheet \"tree\"");
  expectRefused("bd_origin", "", ", \"backdrop\": {\"sheet\":\"ground\",\"origin\":[1]}", "\"origin\" must be [x, y]");
  expectRefused("bd_origin_big", "", ", \"backdrop\": {\"sheet\":\"ground\",\"origin\":[0, 9000]}", "\"origin\" must be [x, y]");
  expectRefused("bd_style", "", ", \"backdrop\": {\"sheet\":\"ground\",\"styles\":[\"flat\"]}", "\"flat\", which is not of the 2D isometric kind");
  expectRefused("bd_style_unknown", "", ", \"backdrop\": {\"sheet\":\"ground\",\"styles\":[\"x\"]}", "unknown visual style \"x\"");
  expectRefused("bd_noiso", "", ", \"backdrop\": {\"sheet\":\"ground\"}", "game.json has no style with \"view\": \"isometric\"", "",
                "[{\"id\":\"flat\"}]");
  expectRefused("bd_pieces_noover", "", ", \"backdrop\": {\"sheet\":\"ground\",\"pieces\":[{\"rect\":[0,0,1,1],\"square\":[0,0]}]}",
                "they need an \"over\" sheet");
  expectRefused("bd_rect", "", ", \"backdrop\": {\"sheet\":\"ground\",\"over\":\"props\",\"pieces\":[{\"rect\":[0,0,0,1],\"square\":[0,0]}]}",
                "pieces[0]", "\"rect\" must be [x, y, width, height]");
  expectRefused("bd_rect_out", "", ", \"backdrop\": {\"sheet\":\"ground\",\"over\":\"props\",\"pieces\":[{\"rect\":[150,0,20,1],\"square\":[0,0]}]}",
                "reaches outside the over picture \"props\" (160 x 120 pixels)");
  expectRefused("bd_square_out", "", ", \"backdrop\": {\"sheet\":\"ground\",\"over\":\"props\",\"pieces\":[{\"rect\":[0,0,1,1],\"square\":[4,0]}]}",
                "the square 4,0 is outside the map (4 x 3)");
  expectRefused("bd_piece_key", "", ", \"backdrop\": {\"sheet\":\"ground\",\"over\":\"props\",\"pieces\":[{\"rect\":[0,0,1,1],\"square\":[0,0],\"z\":1}]}",
                "unknown key \"z\"");
  expectRefused("bd_pieces_empty", "", ", \"backdrop\": {\"sheet\":\"ground\",\"over\":\"props\",\"pieces\":[]}", "between 1 and 4096 pieces");
}

TEST(character_looks_replace_the_unit_type_look) {
  const std::string gfx =
      "\"units\": { \"knight\": {\"sheet\":\"men\",\"cell\":[0,0]} },"
      " \"characters\": { \"eirik\": {\"sheet\":\"men\",\"cell\":[1,0],\"sides\":{\"b\":{\"sheet\":\"men\",\"cell\":[2,0]}}} },"
      " \"styles\": { \"iso\": { \"characters\": { \"eirik\": {\"sheet\":\"men\",\"cell\":[3,0]} } } }";
  Database db;
  MapDef m;
  const std::string err = loadError(makeGame("chars", gfx), db, m);
  if (!err.empty()) std::printf("  %s\n", err.c_str());
  CHECK(err.empty());
  const int knight = db.findUnitType("knight"), eirik = db.findCharacter("eirik");
  const GraphicsSet* iso = db.graphics.set(0);
  const GraphicsSet* flat = db.graphics.set(1);
  CHECK(iso && flat);
  if (!iso || !flat) return;
  CHECK_EQ(flat->unitLook(knight, eirik)->base.cells[0].col, 1);         // the character's own look
  CHECK_EQ(flat->unitLook(knight, eirik)->forSide("b").cells[0].col, 2);  // ... with its own per-side looks
  CHECK_EQ(flat->unitLook(knight, -1)->base.cells[0].col, 0);            // a unit that is no character: its type's
  CHECK_EQ(iso->unitLook(knight, eirik)->base.cells[0].col, 3);           // a 2D isometric style starts with no art: its own entry
  CHECK(!iso->unitLook(knight, -1)->base.any());
  expectRefused("chars_unknown", "\"characters\": { \"nobody\": {\"sheet\":\"men\",\"cell\":[0,0]} }", "", "character \"nobody\"",
                "there is no character with this id");
  expectRefused("chars_join", "\"characters\": { \"eirik\": {\"sheet\":\"men\",\"join\":[0,0]} }", "", "character \"eirik\"", "can only use");
}

TEST(poses_and_facing_load_with_diagonals_and_fallbacks) {
  const std::string gfx =
      "\"units\": { \"knight\": {\"sheet\":\"men\",\"cell\":[0,0],"
      " \"facing\": {\"downLeft\":[[0,1],[1,1]],\"upLeft\":[[2,1]],\"frameMs\":300},"
      " \"attack\": {\"sheet\":\"men\",\"downLeft\":[[3,1],[4,1],[5,1]],\"frameMs\":100},"
      " \"hurt\": {\"down\":[[6,1]]},"
      " \"walk\": {\"downRight\":[[0,0]],\"upRight\":[[1,0]]} } }";
  Database db;
  MapDef m;
  const std::string err = loadError(makeGame("poses", gfx), db, m);
  if (!err.empty()) std::printf("  %s\n", err.c_str());
  CHECK(err.empty());
  if (!err.empty()) return;
  const Look& k = db.graphics.set(1)->units[(size_t)db.findUnitType("knight")].base;
  CHECK(k.poses != nullptr);
  if (!k.poses) return;
  const UnitPoses& p = *k.poses;
  // A missing diagonal is its partner turned round.
  CHECK(!p.facing.mirrored[WalkDownLeft] && p.facing.mirrored[WalkDownRight] && p.facing.frames[WalkDownRight][1].col == 1);
  CHECK(p.facing.mirrored[WalkUpRight] && p.facing.frames[WalkUpRight][0].col == 2);
  CHECK(p.facing.frames[WalkDown].empty() && p.facing.frameMs == 300);
  // Iso steps on the screen (2:1): along x down-right, along y down-left, -x up-left, -y up-right.
  CHECK_EQ(p.facing.pick(2, 1), (int)WalkDownRight);
  CHECK_EQ(p.facing.pick(-2, 1), (int)WalkDownLeft);
  CHECK_EQ(p.facing.pick(-2, -1), (int)WalkUpLeft);
  CHECK_EQ(p.facing.pick(2, -1), (int)WalkUpRight);
  CHECK_EQ(p.facing.pick(0, -1), (int)WalkUpLeft);  // straight up (top-down): a tie, the first in the order
  // Played once: the last frame is held.
  CHECK_EQ(p.attack.cellOnce(WalkDownLeft, 0).col, 3);
  CHECK_EQ(p.attack.cellOnce(WalkDownLeft, 150).col, 4);
  CHECK_EQ(p.attack.cellOnce(WalkDownLeft, 5000).col, 5);
  // Fallbacks: KO without frames uses hurt; a pose without frames has none (the standing picture).
  CHECK(p.of(PoseAttack) == &p.attack && p.of(PoseHurt) == &p.hurt && p.of(PoseKo) == &p.hurt && p.of(PoseNone) == nullptr);
  UnitPoses none;
  CHECK(none.of(PoseAttack) == nullptr && none.of(PoseKo) == nullptr);
  // Walk with diagonals only: a down-left step is the down-right frames turned round.
  CHECK(k.walk.mirrored[WalkDownLeft] && k.walk.frames[WalkDownLeft][0].col == 0 && k.walk.pick(-2, 1) == (int)WalkDownLeft);
  // A look without poses keeps none (every existing game).
  Database db2;
  CHECK(loadError(makeGame("poses_none", "\"units\": { \"knight\": {\"sheet\":\"men\",\"cell\":[0,0]} }"), db2, m).empty());
  CHECK(db2.graphics.set(1)->units[0].base.poses == nullptr);
}

TEST(poses_refusals) {
  auto unit = [](const std::string& extra) { return "\"units\": { \"knight\": {\"sheet\":\"men\",\"cell\":[0,0]," + extra + "} }"; };
  expectRefused("p_empty", unit("\"attack\": {\"frameMs\":100}"), "", "attack", "needs at least one of");
  expectRefused("p_key", unit("\"hurt\": {\"down\":[[0,0]],\"back\":[[0,0]]}"), "", "hurt", "unknown key \"back\"");
  expectRefused("p_out", unit("\"ko\": {\"down\":[[9,0]]}"), "", "ko", "outside sheet \"men\"");
  expectRefused("p_sheet", unit("\"facing\": {\"sheet\":\"zip\",\"downLeft\":[[0,0]]}"), "", "facing", "unknown sheet \"zip\"");
  expectRefused("p_terrain", "\"terrain\": { \"plain\": {\"sheet\":\"men\",\"cell\":[0,0],\"attack\":{\"down\":[[0,0]]}} }", "",
                "only a unit picture can have \"attack\" frames");
  expectRefused("p_model", "\"styles\": {\"flat\": {\"units\": {\"knight\": {\"model\":\"m.glb\",\"hurt\":{\"down\":[[0,0]]}}}}}", "", "\"model\"");
}

TEST(unit_offset_and_portrait_size) {
  const std::string root = makeGame("offset", "\"portraitSize\": [64, 80]");
  std::string text;
  CHECK(readTextFile(root + "/data/graphics.json", text));
  text.replace(text.find("\"cellHeight\":16}"), 16, "\"cellHeight\":16,\"unitOffset\":[0,4]}");
  CHECK(writeTextFile(root + "/data/graphics.json", text));
  Database db;
  MapDef m;
  const std::string err = loadError(root, db, m);
  if (!err.empty()) std::printf("  %s\n", err.c_str());
  CHECK(err.empty());
  const SheetDef& men = db.graphics.sheets[(size_t)db.graphics.findSheet("men")];
  CHECK(men.unitOffX == 0 && men.unitOffY == 4);
  CHECK(db.graphics.portraitW == 64 && db.graphics.portraitH == 80);
  Database db2;
  CHECK(loadError(makeGame("offset_none", "\"portraitSize\": 64"), db2, m).empty());
  CHECK(db2.graphics.portraitW == 64 && db2.graphics.portraitH == 64 && db2.graphics.sheets[0].unitOffY == 0);
  Database db3;
  CHECK(loadError(makeGame("offset_default", ""), db3, m).empty());
  CHECK(db3.graphics.portraitW == 48 && db3.graphics.portraitH == 48);  // as always
  expectRefused("ps_small", "\"portraitSize\": 20", "", "\"portraitSize\" must be a whole number or [width, height]");
  expectRefused("ps_shape", "\"portraitSize\": [64]", "", "\"portraitSize\" must be");
  const std::string bad = makeGame("offset_bad", "");
  CHECK(readTextFile(bad + "/data/graphics.json", text));
  text.replace(text.find("\"cellHeight\":16}"), 16, "\"cellHeight\":16,\"unitOffset\":[0,300]}");
  CHECK(writeTextFile(bad + "/data/graphics.json", text));
  Database db4;
  const std::string e4 = loadError(bad, db4, m);
  CHECK(has(e4, "sheet \"men\"") && has(e4, "\"unitOffset\" must be [x, y]"));
}
