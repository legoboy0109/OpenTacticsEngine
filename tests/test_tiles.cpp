// The tile layer of a map (docs/GRAPHICS.md "Tile layer (hand-painted maps)"): the tilesets in graphics.json, a map's "tileLayer", tiles in
// map changes, the layer's state in a battle and its save. Game: tests/fixtures/tiles (10 x 7, tileset "test" of 8 x 4 tiles, tile 8
// animated; squares 4..6,2..4 are river with tile 8; change "dry" turns 4,3 into plain with tile 5, 5,3 into plain with no tile given,
// 6,3 into plain keeping its tile; the turn 2 event applies it and sets 4,2 to a bridge with tile 1).
#include "posix_compat.h"

#include <cstdio>
#include <cstring>
#include <functional>

#include "save_helpers.h"
#include "test.h"

using namespace ote;

namespace {

bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

// Only the header matters to the core, so this is not a decodable image.
void pngHeader(const std::string& path, unsigned w, unsigned h) {
  unsigned char b[33] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13, 'I', 'H', 'D', 'R'};
  b[18] = (unsigned char)(w >> 8);
  b[19] = (unsigned char)w;
  b[22] = (unsigned char)(h >> 8);
  b[23] = (unsigned char)h;
  b[24] = 8;
  b[25] = 6;
  FILE* f = std::fopen(path.c_str(), "wb");
  CHECK(f != nullptr);
  if (!f) return;
  std::fwrite(b, 1, sizeof b, f);
  std::fclose(f);
}

struct TCopy {
  std::string dir;
  explicit TCopy(const std::string& name) {
    dir = savetest::tempFolder(name);
    makeDir(dir + "/data");
    makeDir(dir + "/maps");
    makeDir(dir + "/art");
    static const char* const files[] = {"game.json", "data/terrain.json", "data/weapons.json", "data/items.json", "data/units.json",
                                        "data/graphics.json", "maps/tiled.json"};
    for (const char* f : files) {
      std::string text;
      CHECK(readTextFile(ote_test::sourceDir() + "/tests/fixtures/tiles/" + f, text));
      writeTextFile(dir + "/" + f, text);
    }
    pngHeader(dir + "/art/tiles.png", 128, 64);
  }
  std::string read(const std::string& rel) const {
    std::string text;
    readTextFile(dir + "/" + rel, text);
    return text;
  }
  void swap(const std::string& rel, const std::string& from, const std::string& to) {
    std::string text = read(rel);
    size_t at = text.find(from);
    if (at == std::string::npos) std::printf("  the test's text to replace is missing: %s\n", from.c_str());
    else writeTextFile(dir + "/" + rel, text.replace(at, from.size(), to));
  }
  // "" = the game and its map load; else the message.
  std::string loadError(const char* map = "tiled") const {
    Database db;
    MapDef m;
    std::string err;
    if (!loadGame(dir, db, err)) return err;
    if (!loadMap(db, map, m, err)) return err;
    return "";
  }
};

// The map with no layer: the layer is renamed away and the changes' tile keys removed.
void stripLayer(TCopy& c) {
  c.swap("maps/tiled.json", "\"tileLayer\"", "\"unusedLayer\"");
  c.swap("maps/tiled.json", ", \"tile\": 5", "");
  c.swap("maps/tiled.json", ", \"keepTile\": true", "");
  c.swap("maps/tiled.json", ", \"tile\": 1", "");
}

std::string bad(const char* name, const char* file, const char* from, const char* to) {
  TCopy c(name);
  c.swap(file, from, to);
  return c.loadError();
}

struct World {
  TCopy copy;
  savetest::Rig rig;
  std::vector<Event> events;
  explicit World(const char* name, const std::function<void(TCopy&)>& tweak = nullptr, bool start = true) : copy(name) {
    if (tweak) tweak(copy);
    CHECK(rig.load(copy.dir, "tiled"));
    if (start) rig.battle.start(events);
  }
  Battle& b() { return rig.battle; }
  Cell at(int x, int y) { return b().grid.at(x, y); }
  int tile(int x, int y) { return b().tileAt(at(x, y)); }
  const std::string& terrain(int x, int y) { return rig.db.terrains[(size_t)b().cells[(size_t)at(x, y)].terrain].id; }
  int change(const char* n) {
    for (size_t i = 0; i < rig.map.changes.size(); ++i)
      if (rig.map.changes[i].id == n) return (int)i;
    return -1;
  }
  int terrainId(const char* n) { return rig.db.findTerrain(n); }
};

}  // namespace

// ---- loader: tilesets ----

TEST(tiles_the_fixture_loads_with_its_tileset_and_layer) {
  TCopy c("tl_load");
  CHECK_STR(c.loadError(), "");
  Database db;
  MapDef m;
  std::string err;
  CHECK(loadGame(c.dir, db, err) && loadMap(db, "tiled", m, err));
  CHECK_EQ((int)db.graphics.tilesets.size(), 1);
  const TilesetDef& ts = db.graphics.tilesets[0];
  CHECK_STR(ts.id, "test");
  CHECK_EQ(ts.cols, 8);
  CHECK_EQ(ts.rows, 4);
  CHECK_EQ(ts.count(), 32);
  CHECK_EQ((int)ts.anims.size(), 1);
  CHECK_EQ(ts.frame(8, 0), 8);
  CHECK_EQ(ts.frame(8, 200), 9);
  CHECK_EQ(ts.frame(8, 450), 10);
  CHECK_EQ(ts.frame(8, 600), 8);  // loops
  CHECK_EQ(ts.frame(13, 450), 13);  // an ordinary tile is itself
  CHECK_EQ(ts.cell(11).col, 3);
  CHECK_EQ(ts.cell(11).row, 1);
  CHECK_EQ(m.tileset, 0);
  CHECK_EQ((int)m.tileCells.size(), 70);
  CHECK_EQ(m.tileCells[0], 12);
  CHECK_EQ(m.tileCells[6 * 10 + 0], -1);
  CHECK_EQ(m.tileCells[2 * 10 + 4], 8);
}

TEST(tiles_a_map_without_a_layer_has_none) {
  TCopy c("tl_none");
  stripLayer(c);
  Database db;
  MapDef m;
  std::string err;
  CHECK(loadGame(c.dir, db, err) && loadMap(db, "tiled", m, err));
  CHECK_EQ(m.tileset, -1);
  CHECK(m.tileCells.empty());
}

TEST(tiles_loader_refuses_a_bad_tilesets_block) {
  const char* g = "data/graphics.json";
  CHECK(has(bad("tl_t1", g, "\"sheet\": \"tiles\", \"animations\"", "\"sheet\": \"nope\", \"animations\""), "tileset \"test\": unknown sheet \"nope\""));
  CHECK(has(bad("tl_t2", g, "\"sheet\": \"tiles\", \"animations\"", "\"sheet\": \"tiles\", \"oops\": 1, \"animations\""), "unknown key \"oops\""));
  CHECK(has(bad("tl_t3", g, "\"8\": {", "\"eight\": {"), "must be a tile number"));
  CHECK(has(bad("tl_t4", g, "\"8\": {", "\"32\": {"), "tile 32 is outside the sheet (32 tiles)"));
  CHECK(has(bad("tl_t5", g, "[8, 9, 10]", "[8, 40]"), "frame 2 must be a tile number from 0 to 31"));
  CHECK(has(bad("tl_t6", g, "[8, 9, 10]", "[8]"), "between 2 and 64 tiles"));
  CHECK(has(bad("tl_t7", g, "\"ms\": 200", "\"ms\": 5"), "\"ms\" must be between 20 and 60000"));
  CHECK(has(bad("tl_t8", g, "\"ms\": 200", "\"speed\": 5"), "unknown key \"speed\""));
  CHECK(has(bad("tl_t9", g, "\"test\": {", "\"te st\": {"), "letters, digits"));
  CHECK(has(bad("tl_t10", g, "\"tilesets\": { \"test\": { \"sheet\": \"tiles\", \"animations\": { \"8\": { \"frames\": [8, 9, 10], \"ms\": 200 } } } }",
                "\"tilesets\": { \"test\": 5 }"),
            "must be an object"));
}

// ---- loader: the map's layer ----

TEST(tiles_loader_refuses_a_bad_layer) {
  const char* m = "maps/tiled.json";
  CHECK(has(bad("tl_l1", m, "\"tileset\": \"test\"", "\"tileset\": \"other\""), "unknown tileset \"other\""));
  CHECK(has(bad("tl_l2", m, "\"tileset\": \"test\", ", ""), "\"tileset\""));
  CHECK(has(bad("tl_l3", m, "\"tileLayer\": {", "\"tileLayer\": { \"extra\": 1,"), "unknown key \"extra\""));
  CHECK(has(bad("tl_l4", m, "\"tileLayer\": { \"tileset\": \"test\", \"cells\": [", "\"tileLayer\": { \"tileset\": \"test\", \"cells\": [ \"\","),
            "\"cells\" has 8 rows but the grid height is 7"));
  CHECK(has(bad("tl_l5", m, "\"000c000f001200150018001b001e000d00100013\"", "\"000c000f\""), "cells row 0 has 8 characters but 40 are needed"));
  CHECK(has(bad("tl_l6", m, "\"000c000f001200150018001b001e000d00100013\"", "\"000c000f001200150018001b001e000d0010001z\""),
            "cells row 0, column 9: \"001z\" is not four hex digits"));
  CHECK(has(bad("tl_l7", m, "\"000c000f001200150018001b001e000d00100013\"", "\"000c000f001200150018001b001e000d00100020\""),
            "cells row 0, column 9: tile 32 is outside tileset \"test\" (32 tiles)"));
  CHECK(has(bad("tl_l8", m, "\"000c000f001200150018001b001e000d00100013\"", "5"), "cells row 0 must be a string"));
  CHECK(has(bad("tl_l9", m, "\"tileLayer\": {", "\"tileLayer\": 4, \"was\": {"), "tileLayer: must be an object"));
  // The row as a list of numbers (-1 or null = none) is read too.
  TCopy ok("tl_l10");
  ok.swap("maps/tiled.json", "\"000c000f001200150018001b001e000d00100013\"", "[12, null, -1, 3, 4, 5, 6, 7, 8, 9]");
  CHECK_STR(ok.loadError(), "");
  CHECK(has(bad("tl_l11", m, "\"000c000f001200150018001b001e000d00100013\"", "[12, 99, -1, 3, 4, 5, 6, 7, 8, 9]"), "tile 99 is outside"));
  CHECK(has(bad("tl_l12", m, "\"000c000f001200150018001b001e000d00100013\"", "[12, 2.5, -1, 3, 4, 5, 6, 7, 8, 9]"), "must be a whole number"));
  CHECK(has(bad("tl_l13", m, "\"000c000f001200150018001b001e000d00100013\"", "[12, 1]"), "has 2 squares but the grid width is 10"));
}

TEST(tiles_a_layer_without_tilesets_names_the_missing_file_entry) {
  TCopy c("tl_l14");
  writeTextFile(c.dir + "/data/graphics.json", "{ \"sheets\": [ { \"id\": \"tiles\", \"file\": \"art/tiles.png\", \"cellWidth\": 16, \"cellHeight\": 16 } ] }");
  CHECK(has(c.loadError(), "unknown tileset \"test\" (data/graphics.json has no \"tilesets\")"));
}

TEST(tiles_a_hexagon_game_refuses_a_layer_with_a_plain_message) {
  TCopy c("tl_hex");
  c.swap("maps/tiled.json", "\"type\": \"square\"", "\"type\": \"hex\"");
  Database db;
  MapDef m;
  std::string err;
  CHECK(loadGame(c.dir, db, err));
  db.gridType = GridType::Hex;  // pretend: a hexagon game with this map
  CHECK(!loadMap(db, "tiled", m, err));
  CHECK(has(err, "tile layers are not supported on hexagon games"));
}

// ---- loader: tiles in changes and setTerrain ----

TEST(tiles_the_changes_read_tiles_and_keepTile) {
  World w("tl_chg", nullptr, false);
  const MapChangeDef& d = w.rig.map.changes[(size_t)w.change("dry")];
  CHECK_EQ((int)d.cells.size(), 3);
  CHECK_EQ(d.cells[0].tile, 5);
  CHECK_EQ(d.cells[1].tile, kTileUnset);
  CHECK(!d.cells[1].keepTile);
  CHECK(d.cells[2].keepTile);
}

TEST(tiles_loader_refuses_bad_tiles_in_changes_and_commands) {
  const char* m = "maps/tiled.json";
  CHECK(has(bad("tl_c1", m, "\"terrain\": \"plain\", \"tile\": 5", "\"terrain\": \"plain\", \"tile\": 77"), "\"tile\" must be -1 (no tile) or a tile number from 0 to 31"));
  CHECK(has(bad("tl_c2", m, "\"terrain\": \"plain\", \"tile\": 5", "\"terrain\": \"plain\", \"tile\": \"five\""), "\"tile\" must be -1"));
  CHECK(has(bad("tl_c3", m, "\"terrain\": \"plain\", \"keepTile\": true", "\"terrain\": \"plain\", \"keepTile\": 1"), "keepTile"));
  CHECK(has(bad("tl_c4", m, "\"tile\": 1 }", "\"tile\": 99 }"), "\"tile\" must be -1 (no tile) or a tile number"));
  CHECK(has(bad("tl_c5", m, "\"tile\": 1 }", "\"tile\": 1, \"keepTile\": \"yes\" }"), "keepTile"));
  // On a map with no layer a tile is refused with the reason.
  TCopy c("tl_c6");
  c.swap("maps/tiled.json", "\"tileLayer\"", "\"unusedLayer\"");
  CHECK(has(c.loadError(), "needs a tile layer: the map has no \"tileLayer\""));
  // ... and so is a setTerrain tile.
  TCopy d("tl_c7");
  d.swap("maps/tiled.json", "\"tileLayer\"", "\"unusedLayer\"");
  d.swap("maps/tiled.json", "\"terrain\": \"plain\", \"tile\": 5", "\"terrain\": \"plain\"");
  CHECK(has(d.loadError(), "\"tile\" needs a tile layer"));
}

// ---- the layer in a battle ----

TEST(tiles_a_battle_starts_with_the_layer_as_the_map_gave_it) {
  World w("tl_init");
  CHECK(w.b().hasTiles());
  CHECK_EQ(w.b().tileset, 0);
  CHECK_EQ(w.tile(0, 0), 12);
  CHECK_EQ(w.tile(5, 3), 8);
  CHECK_EQ(w.tile(0, 6), -1);
  CHECK_EQ(w.tile(9, 6), 12 + (9 * 3 + 6 * 7) % 20);
}

TEST(tiles_apply_change_sets_clears_or_keeps_each_squares_tile_and_revert_restores_them) {
  World w("tl_apply");
  const int dry = w.change("dry");
  w.events.clear();
  w.b().storyApplyChange(dry, w.events);
  CHECK_STR(w.terrain(4, 3), "plain");
  CHECK_EQ(w.tile(4, 3), 5);   // its own tile
  CHECK_EQ(w.tile(5, 3), -1);  // the terrain changed and no tile was given: cleared
  CHECK_EQ(w.tile(6, 3), 8);   // keepTile
  CHECK_EQ(w.tile(4, 4), 8);   // not in the change
  CHECK_EQ((int)w.events.size() >= 1, 1);
  CHECK(w.events[0].type == EventType::TerrainChanged);
  CHECK_EQ(w.events[0].a, 3);
  w.events.clear();
  w.b().storyRevertChange(dry, w.events);
  CHECK_STR(w.terrain(4, 3), "river");
  CHECK_EQ(w.tile(4, 3), 8);
  CHECK_EQ(w.tile(5, 3), 8);
  CHECK_EQ(w.tile(6, 3), 8);
}

TEST(tiles_set_terrain_with_a_tile_without_one_and_with_keepTile) {
  World w("tl_set");
  const int river = w.terrainId("river"), bridge = w.terrainId("bridge"), plain = w.terrainId("plain");
  w.events.clear();
  w.b().storySetTerrain({w.at(4, 2)}, bridge, w.events, 1, false);
  CHECK_EQ(w.tile(4, 2), 1);
  w.b().storySetTerrain({w.at(5, 2)}, bridge, w.events);  // no tile given: the tile goes
  CHECK_EQ(w.tile(5, 2), -1);
  w.b().storySetTerrain({w.at(6, 2)}, bridge, w.events, kTileUnset, true);  // keepTile
  CHECK_EQ(w.tile(6, 2), 8);
  // The same terrain again with no tile given changes nothing (and keeps the tile).
  w.events.clear();
  w.b().storySetTerrain({w.at(4, 3)}, river, w.events);
  CHECK_EQ(w.tile(4, 3), 8);
  CHECK_EQ((int)w.events.size(), 0);
  // A tile alone (the terrain is what it is) is still a change the view is told about.
  w.b().storySetTerrain({w.at(4, 3)}, river, w.events, 20, false);
  CHECK_EQ(w.tile(4, 3), 20);
  CHECK_EQ((int)w.events.size(), 1);
  CHECK(w.events[0].type == EventType::TerrainChanged);
  CHECK_EQ(w.events[0].data.size(), (size_t)1);
  // -1 explicitly clears; a square with no tile can get one.
  w.b().storySetTerrain({w.at(4, 3), w.at(0, 6)}, plain, w.events, -1, false);
  CHECK_EQ(w.tile(4, 3), -1);
  w.b().storySetTerrain({w.at(0, 6)}, plain, w.events, 7, false);
  CHECK_EQ(w.tile(0, 6), 7);
}

TEST(tiles_a_change_applied_by_the_turn_2_event_in_play) {
  World w("tl_event");
  std::vector<Event> ev;
  CHECK(w.b().apply(Action::endTurn(), ev, nullptr));
  for (int i = 0; i < 40 && !w.b().over && w.b().activeSide == 1; ++i) {
    ev.clear();
    aiTakeStep(w.b(), ev);
  }
  CHECK_EQ(w.b().round, 2);
  CHECK_EQ(w.tile(4, 3), 5);
  CHECK_EQ(w.tile(5, 3), -1);
  CHECK_EQ(w.tile(4, 2), 1);
  CHECK_STR(w.terrain(4, 2), "bridge");
}

TEST(tiles_do_not_change_the_rules_a_map_without_the_layer_plays_the_same) {
  World a("tl_same_a");
  World b("tl_same_b", [](TCopy& c) { stripLayer(c); });
  CHECK(!b.b().hasTiles());
  CHECK_STR(savetest::stateText(a.b()), savetest::stateText(b.b()));
  std::vector<Event> ea, eb;
  a.b().storyApplyChange(a.change("dry"), ea);
  b.b().storyApplyChange(b.change("dry"), eb);
  for (int y = 0; y < 7; ++y)
    for (int x = 0; x < 10; ++x) CHECK_EQ(a.b().cells[(size_t)a.at(x, y)].terrain, b.b().cells[(size_t)b.at(x, y)].terrain);
  CHECK_EQ(ea.size(), eb.size());
}

// ---- saves ----

TEST(tiles_a_battle_with_no_tile_change_saves_exactly_as_one_with_no_layer) {
  World a("tl_save_a");
  World b("tl_save_b", [](TCopy& c) { stripLayer(c); });
  const std::string text = savetest::stateText(a.b());
  CHECK(!has(text, "mapState"));
  CHECK(!has(text, "tiles"));
  CHECK_STR(text, savetest::stateText(b.b()));
  // A terrain-only change on the map without a layer writes no "tiles" anywhere.
  std::vector<Event> ev;
  b.b().storyApplyChange(b.change("dry"), ev);
  const std::string after = savetest::stateText(b.b());
  CHECK(has(after, "mapState"));
  CHECK(!has(after, "\"tiles\""));
}

TEST(tiles_a_save_mid_battle_with_changed_tiles_comes_back_the_same) {
  World w("tl_round");
  std::vector<Event> ev;
  w.b().storyApplyChange(w.change("dry"), ev);
  w.b().storySetTerrain({w.at(4, 2)}, w.terrainId("bridge"), ev, 1, false);
  w.b().storySetTerrain({w.at(0, 6), w.at(9, 0)}, w.terrainId("plain"), ev, 31, false);
  w.b().storySetTerrain({w.at(1, 6)}, w.terrainId("plain"), ev, -1, false);  // nothing: no tile there already
  const std::string saved = savetest::stateText(w.b());
  CHECK(has(saved, "\"mapState\""));
  CHECK(has(saved, "\"tiles\""));
  // Only what differs from the map file is stored: 4,3 5,3 4,2 0,6 9,0 (6,3 kept its tile, 1,6 had none).
  Json j;
  std::string err;
  CHECK(Json::parse(saved, j, err));
  CHECK_EQ((int)j["mapState"]["tiles"].size(), 5);
  World g("tl_round_b", nullptr, false);
  CHECK(restoreBattleState(g.b(), j, err));
  CHECK_STR(err, "");
  for (int y = 0; y < 7; ++y)
    for (int x = 0; x < 10; ++x) CHECK_EQ(g.tile(x, y), w.tile(x, y));
  CHECK_STR(savetest::stateText(g.b()), saved);
  // The restored battle still knows how to take the change back: the tiles it replaced come back.
  std::vector<Event> ev2;
  g.b().storyRevertChange(g.change("dry"), ev2);
  CHECK_EQ(g.tile(4, 3), 8);
  CHECK_EQ(g.tile(5, 3), 8);
  CHECK_EQ(g.tile(6, 3), 8);
  CHECK_STR(g.terrain(5, 3), "river");
}

TEST(tiles_a_damaged_tile_save_is_refused_with_a_path) {
  World w("tl_bad");
  std::vector<Event> ev;
  w.b().storySetTerrain({w.at(4, 2)}, w.terrainId("bridge"), ev, 1, false);
  const std::string saved = savetest::stateText(w.b());
  auto refuse = [&](const char* from, const char* to, const char* msg) {
    std::string text = saved;
    size_t at = text.find(from);
    CHECK(at != std::string::npos);
    if (at == std::string::npos) return;
    text.replace(at, strlen(from), to);
    Json j;
    std::string err;
    CHECK(Json::parse(text, j, err));
    World g("tl_bad_b", nullptr, false);
    const bool ok = restoreBattleState(g.b(), j, err);
    CHECK(!ok);
    if (ok || !has(err, msg)) std::printf("  message was: %s\n", err.c_str());
    CHECK(has(err, msg));
  };
  refuse("\"tiles\":[[24,1]]", "\"tiles\":[[24,99]]", "battle.mapState.tiles[0]");
  refuse("\"tiles\":[[24,1]]", "\"tiles\":[[70,1]]", "not a square of the map");
  refuse("\"tiles\":[[24,1]]", "\"tiles\":[[24,1],[24,2]]", "listed twice");
  refuse("\"tiles\":[[24,1]]", "\"tiles\":[[24]]", "not a pair");
  // A save with tiles for a map that has no layer.
  TCopy c("tl_bad_c");
  stripLayer(c);
  savetest::Rig rig;
  CHECK(rig.load(c.dir, "tiled"));
  Json j;
  std::string err;
  CHECK(Json::parse(saved, j, err));
  CHECK(!restoreBattleState(rig.battle, j, err));
  CHECK(has(err, "this map has no tile layer"));
}
