// Unit tests of editor/core/map_grid.* and map_resize.cpp: reading and writing a map's tile grid, legend symbols, flood
// regions on square and hexagon grids, and resizing with anchors (written without Qt; the native painter tests check the same
// logic through the undo stack).
#include <algorithm>
#include <string>

#include "map_grid.h"
#include "test.h"

using namespace ote_editor;
using ote::Json;

namespace {
Json J(const char* text) {
  Json j;
  std::string e;
  Json::parse(text, j, e);
  return j;
}

const char* kSquare = R"({
  "grid": {"type": "square", "width": 4, "height": 3},
  "legend": {".": "plain", "f": "forest", "~": "water"},
  "tiles": ["..f~", "ff.~", "...."],
  "heights": [[0,1,2,0],[0,0,0,0],[3,0,0,0]],
  "tileLayer": {"tileset": "world", "cells": ["000100020003....", "................", "0010............"]},
  "units": [{"type": "knight", "side": "blue", "x": 3, "y": 2, "id": "gilda"}, {"type": "archer", "side": "blue", "x": 0, "y": 0}],
  "structures": [{"type": "fort", "x": 2, "y": 0}, {"type": "castle", "x": 1, "y": 1}],
  "prep": {"slots": [{"x": 0, "y": 1}, {"x": 3, "y": 1}]},
  "changes": {"bridge": {"cells": [{"x": 3, "y": 0, "terrain": "plain"}]}}
})";

MapGrid read(const Json& j, bool flat = false) {
  MapGrid g;
  std::string err;
  CHECK(readMapGrid(j, flat, g, err));
  CHECK_STR(err, "");
  return g;
}

TEST(map_grid_reads_and_writes_every_part) {
  const MapGrid g = read(J(kSquare));
  CHECK_EQ(g.width, 4);
  CHECK_EQ(g.height, 3);
  CHECK(g.hasHeights && g.hasLayer);
  CHECK_STR(g.terrainAt(2, 0), "forest");
  CHECK_EQ(g.heightAt(2, 0), 2);
  CHECK_EQ(g.layerAt(1, 0), 2);
  CHECK_EQ(g.layerAt(3, 0), -1);
  CHECK_EQ(g.layerAt(0, 2), 16);
  CHECK_STR(tilesJson(g).dump(0), J(kSquare)["tiles"].dump(0));
  CHECK_STR(heightsJson(g).dump(0), J(kSquare)["heights"].dump(0));
  CHECK_STR(layerCellsJson(g).dump(0), J(kSquare)["tileLayer"]["cells"].dump(0));
}

TEST(map_grid_refuses_what_the_loader_would_refuse) {
  MapGrid g;
  std::string err;
  Json bad = J(kSquare);
  bad.set("tiles", J(R"(["..f~", "ff.~"])"));
  CHECK(!readMapGrid(bad, false, g, err));
  CHECK(!err.empty());
  bad = J(kSquare);
  bad.set("heights", J("[[0,1,2],[0,0,0,0],[3,0,0,0]]"));
  CHECK(!readMapGrid(bad, false, g, err));
  bad = J(kSquare);
  bad.set("tileLayer", J(R"({"tileset": "w", "cells": ["00zz0000000000", "", ""]})"));
  CHECK(!readMapGrid(bad, false, g, err));
  bad = J(kSquare);
  bad.set("legend", J(R"({"ab": "plain"})"));
  CHECK(!readMapGrid(bad, false, g, err));
}

TEST(map_grid_legend_symbols_follow_the_maps_style) {
  bool isNew = false;
  MapGrid g = read(J(kSquare));
  CHECK_EQ(legendSymbolFor(g, "forest", 0, isNew), 'f');  // already in the legend
  CHECK(!isNew);
  CHECK_EQ(legendSymbolFor(g, "sand", 's', isNew), 's');  // the terrain's own glyph, free
  CHECK(isNew);
  CHECK_EQ(legendSymbolFor(g, "frost", 'f', isNew), 'r');  // glyph taken: letters of the id, in the map's (lower case) style
  CHECK_EQ(legendSymbolFor(g, "sand", 0, isNew), 's');
  // A legend of capitals: new symbols are capitals.
  Json up = J(kSquare);
  up.set("legend", J(R"({".": "plain", "A": "forest", "B": "water"})"));
  up.set("tiles", J(R"(["....", "....", "...."])"));
  g = read(up);
  CHECK_EQ(legendSymbolFor(g, "sand", 0, isNew), 'S');
  CHECK_EQ(legendSymbolFor(g, "xyz", 0, isNew), 'X');
  // Digits.
  up.set("legend", J(R"({".": "plain", "1": "forest", "2": "water", "3": "road"})"));
  g = read(up);
  CHECK_EQ(legendSymbolFor(g, "sand", 0, isNew), '4');
  // Never a quote or backslash, and nothing left gives 0.
  Json full = J(kSquare);
  Json legend = Json::makeObject();
  for (char c = '!'; c < 127; ++c)
    if (c != '"' && c != '\\') legend.set(std::string(1, c), Json::makeString("t"));
  full.set("legend", legend);
  full.set("tiles", J(R"(["!!!!", "!!!!", "!!!!"])"));
  g = read(full);
  CHECK_EQ(legendSymbolFor(g, "new", 0, isNew), 0);
}

TEST(map_grid_flood_region_square_and_hex) {
  Json m = J(R"({"grid": {"type": "square", "width": 4, "height": 3}, "legend": {".": "plain", "#": "wall"},
                 "tiles": ["..#.", "..#.", "###."]})");
  MapGrid g = read(m);
  CHECK_EQ(floodRegion(g, 0, 0).size(), (size_t)4);  // the top-left room
  CHECK_EQ(floodRegion(g, 3, 0).size(), (size_t)3);  // the right column
  CHECK_EQ(floodRegion(g, 2, 0).size(), (size_t)5);  // the wall, joined across the bottom row
  CHECK_EQ(floodRegion(g, 9, 9).size(), (size_t)0);
  // Hexagons: with odd rows shoved right, (0,0) touches (0,1)?? Pointy even row y=0: lower right = (x, y+1), lower left = (x-1, y+1).
  Json h = J(R"({"grid": {"type": "hex", "width": 3, "height": 3}, "legend": {".": "plain", "#": "wall"},
                 "tiles": ["#.#", ".#.", "#.#"]})");
  h.set("grid", J(R"({"type": "hex", "width": 3, "height": 3})"));
  g = read(h);
  // (0,0) is a wall; its lower-right neighbour in an even row is (0,1) = '.', its right is (1,0) = '.', so no wall touches it.
  CHECK_EQ(floodRegion(g, 0, 0).size(), (size_t)1);
  // The middle wall (1,1) (odd row): upper right (2,0)=wall, upper left (1,0)='.', lower right (2,2)=wall, lower left (1,2)='.'.
  const std::vector<int> mid = floodRegion(g, 1, 1);
  CHECK_EQ(mid.size(), (size_t)3);
  CHECK(std::find(mid.begin(), mid.end(), 0 * 3 + 2) != mid.end());
  CHECK(std::find(mid.begin(), mid.end(), 2 * 3 + 2) != mid.end());
}

TEST(map_grid_resize_offset_on_hexagons_keeps_row_parity) {
  MapGrid g = read(J(kSquare));
  ResizeSpec s;
  s.width = 8;
  s.height = 7;
  s.anchorX = s.anchorY = 1;
  int dx, dy;
  resizeOffset(g, s, dx, dy);
  CHECK_EQ(dx, 2);
  CHECK_EQ(dy, 2);
  s.height = 6;  // (6 - 3) / 2 = 1 on a square map: fine
  resizeOffset(g, s, dx, dy);
  CHECK_EQ(dy, 1);
  g.hex = true;  // pointy: rows must move in pairs
  resizeOffset(g, s, dx, dy);
  CHECK_EQ(dy, 0);
  CHECK_EQ(dx, 2);
  g.flatTop = true;  // flat: columns in pairs
  s.width = 7;
  resizeOffset(g, s, dx, dy);
  CHECK_EQ(dx, 0);
  CHECK_EQ(dy, 1);
}

TEST(map_grid_resize_grows_and_moves_everything_with_the_anchor) {
  const Json m = J(kSquare);
  const MapGrid g = read(m);
  ResizeSpec s;
  s.width = 6;
  s.height = 4;
  s.anchorX = 2;  // old map stays at the right and the bottom
  s.anchorY = 2;
  s.fill = '~';
  ResizeResult r;
  std::string err;
  CHECK(resizeMap(m, g, s, {}, r, err));
  CHECK_EQ(r.dx, 2);
  CHECK_EQ(r.dy, 1);
  CHECK(r.lost.empty());
  Json out = m;
  for (const auto& kv : r.set) out.set(kv.first, kv.second);
  CHECK_STR(out["grid"].dump(0), R"({"type":"square","width":6,"height":4})");
  CHECK_STR(out["tiles"].dump(0), R"(["~~~~~~","~~..f~","~~ff.~","~~...."])");
  CHECK_STR(out["heights"].dump(0), "[[0,0,0,0,0,0],[0,0,0,1,2,0],[0,0,0,0,0,0],[0,0,3,0,0,0]]");
  CHECK_STR(out["tileLayer"]["cells"][1].asString(), "........000100020003....");
  CHECK_STR(out["tileLayer"]["cells"][0].asString(), std::string(24, '.'));
  CHECK_STR(out["units"][0]["x"].dump(0) + "," + out["units"][0]["y"].dump(0), "5,3");
  CHECK_STR(out["structures"][1]["x"].dump(0) + "," + out["structures"][1]["y"].dump(0), "3,2");
  CHECK_STR(out["prep"]["slots"][1]["x"].dump(0), "5");
  CHECK_STR(out["changes"]["bridge"]["cells"][0]["x"].dump(0), "5");
  // Keys that did not change are not in the result, and the order of the file is kept.
  CHECK(std::none_of(r.set.begin(), r.set.end(), [](const auto& kv) { return kv.first == "legend"; }));
}

TEST(map_grid_resize_removes_what_no_longer_fits_and_says_so) {
  const Json m = J(kSquare);
  const MapGrid g = read(m);
  ResizeSpec s;
  s.width = 3;
  s.height = 2;
  s.fill = '.';
  std::map<std::string, Footprint> fp;
  fp["castle"].w = 2;
  fp["castle"].h = 2;
  ResizeResult r;
  std::string err;
  CHECK(resizeMap(m, g, s, fp, r, err));
  std::vector<std::string> what;
  for (const LostThing& t : r.lost) what.push_back(t.kind + ":" + t.label);
  // knight (3,2) is outside; the 2x2 castle at (1,1) sticks out of a 3x2 map; slot (3,1) and the change square (3,0) too.
  CHECK(std::find(what.begin(), what.end(), "unit:knight (gilda)") != what.end());
  CHECK(std::find(what.begin(), what.end(), "structure:castle") != what.end());
  CHECK(std::find(what.begin(), what.end(), "preparation slot:") != what.end());
  CHECK(std::find(what.begin(), what.end(), "map change square:") != what.end());
  CHECK_EQ(what.size(), (size_t)4);
  Json out = m;
  for (const auto& kv : r.set) out.set(kv.first, kv.second);
  CHECK_EQ(out["units"].size(), (size_t)1);
  CHECK_EQ(out["structures"].size(), (size_t)1);
  CHECK_STR(out["tiles"].dump(0), R"(["..f","ff."])");
  CHECK_STR(out["heights"].dump(0), "[[0,1,2],[0,0,0]]");
}

TEST(map_grid_resize_hex_ring_structures_must_fit_whole) {
  Json m = J(R"({"grid": {"type": "hex", "width": 5, "height": 5}, "legend": {".": "plain"},
                 "tiles": [".....", ".....", ".....", ".....", "....."],
                 "structures": [{"type": "fortress", "x": 2, "y": 2}]})");
  const MapGrid g = read(m);
  std::map<std::string, Footprint> fp;
  fp["fortress"].rings = 1;
  ResizeSpec s;
  s.width = 3;
  s.height = 5;
  s.fill = '.';
  ResizeResult r;
  std::string err;
  CHECK(resizeMap(m, g, s, fp, r, err));
  CHECK_EQ(r.lost.size(), (size_t)1);  // the middle (2,2) is still on a 3-wide map but its right neighbours are not
  s.width = 5;
  s.height = 7;
  CHECK(resizeMap(m, g, s, fp, r, err));
  CHECK(r.lost.empty());
  CHECK_EQ(r.set.size(), (size_t)2);  // grid and tiles; the fortress stays where it is, so structures is not in the result
}

TEST(map_grid_resize_refuses_bad_sizes) {
  const Json m = J(kSquare);
  const MapGrid g = read(m);
  ResizeSpec s;
  s.width = 0;
  s.height = 3;
  ResizeResult r;
  std::string err;
  CHECK(!resizeMap(m, g, s, {}, r, err));
  s.width = 300;
  CHECK(!resizeMap(m, g, s, {}, r, err));
  s.width = 4;
  s.fill = 'q';  // not in the legend
  s.height = 5;
  CHECK(!resizeMap(m, g, s, {}, r, err));
  s.fill = '.';
  s.height = 3;  // same size: nothing to do, and no error
  CHECK(resizeMap(m, g, s, {}, r, err));
  CHECK(r.set.empty());
}

}  // namespace
