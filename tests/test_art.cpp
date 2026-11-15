// Art: join-piece neighbour test, PNG decoding, and that every shipped game's
// art covers all of its terrain and unit types.
#include <cstdio>

#include "core/data/loader.h"
#include "platform/common/image.h"
#include "test.h"

using namespace ote;

namespace {

// A grid of terrain indices given as text rows ('0'..'9'), for the join test.
struct TextGrid {
  std::vector<std::string> rows;
  int w() const { return (int)rows[0].size(); }
  int h() const { return (int)rows.size(); }
  int at(int x, int y) const { return rows[(size_t)y][(size_t)x] - '0'; }
};

Look joinLook(std::vector<uint8_t> with, bool edge) {
  Look l;
  l.kind = Look::Join;
  l.joinWith = with;
  l.joinEdge = edge;
  l.cells.push_back(SpriteCell());
  return l;
}

int maskAt(const Look& l, const TextGrid& g, int x, int y) {
  return joinMaskAt(l, x, y, g.w(), g.h(), [&](int nx, int ny) { return g.at(nx, ny); });
}

}  // namespace

TEST(join_mask_counts_same_terrain_neighbours) {
  // 1 = road. Road counts as itself only (index 1 flagged); 2 = bridge counts too.
  TextGrid g = {{"00000",
                 "01110",
                 "00100",
                 "00100",
                 "01110"}};
  Look road = joinLook({0, 1, 0}, false);
  CHECK_EQ(maskAt(road, g, 1, 1), JoinE);                 // west end of the bar: only E
  CHECK_EQ(maskAt(road, g, 2, 1), JoinE | JoinS | JoinW);  // T junction, nothing to the north
  CHECK_EQ(maskAt(road, g, 3, 1), JoinW);
  CHECK_EQ(maskAt(road, g, 2, 2), JoinN | JoinS);
  CHECK_EQ(maskAt(road, g, 2, 4), JoinN | JoinE | JoinW);
  CHECK_EQ(maskAt(road, g, 0, 0), 0);  // not road, no neighbours that count
  // pieces: E+S+W = 14, N+S = 5
  CHECK_EQ(pickCell(road, 2, 1, maskAt(road, g, 2, 1), 0).col, 0 + 14 % 4);
  CHECK_EQ(pickCell(road, 2, 1, maskAt(road, g, 2, 1), 0).row, 14 / 4);
  // Other terrain listed in joinWith counts as the same.
  TextGrid h = {{"122"}};  // road, bridge, bridge
  Look both = joinLook({0, 1, 1}, false);
  CHECK_EQ(maskAt(both, h, 1, 0), JoinE | JoinW);
  CHECK_EQ(maskAt(road, h, 1, 0), JoinW);
}

TEST(join_mask_map_edge_counts_only_when_asked) {
  TextGrid g = {{"11",
                 "11"}};
  Look edge = joinLook({0, 1}, true), noEdge = joinLook({0, 1}, false);
  // Top-left square: real neighbours E and S, plus the edge to the N and W.
  CHECK_EQ(maskAt(edge, g, 0, 0), 15);
  CHECK_EQ(maskAt(noEdge, g, 0, 0), JoinE | JoinS);
  // Lone square in a 1x1 map.
  TextGrid one = {{"1"}};
  CHECK_EQ(maskAt(edge, one, 0, 0), 15);
  CHECK_EQ(maskAt(noEdge, one, 0, 0), 0);
}

TEST(png_decodes_rgba_and_rejects_garbage) {
  // 2x2: red, half-transparent green / blue, fully transparent.
  static const uint8_t png[] = {
      0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
      0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02, 0x08, 0x06, 0x00, 0x00, 0x00, 0x72, 0xb6, 0x0d,
      0x24, 0x00, 0x00, 0x00, 0x17, 0x49, 0x44, 0x41, 0x54, 0x78, 0xda, 0x05, 0xc1, 0x01, 0x01, 0x00,
      0x00, 0x00, 0x82, 0x20, 0xa6, 0xf7, 0xdc, 0x40, 0x24, 0x43, 0xc1, 0x01, 0x3a, 0xdc, 0x05, 0x7c,
      0x4a, 0xf5, 0x3d, 0x1c, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82};
  Image img;
  std::string err;
  CHECK(decodePng(png, sizeof png, img, err));
  CHECK_EQ(img.width, 2);
  CHECK_EQ(img.height, 2);
  CHECK_EQ((int)img.rgba.size(), 16);
  if (img.rgba.size() == 16) {
    CHECK_EQ((int)img.rgba[0], 255);  // red, opaque
    CHECK_EQ((int)img.rgba[3], 255);
    CHECK_EQ((int)img.rgba[5], 255);  // green, alpha 128
    CHECK_EQ((int)img.rgba[7], 128);
    CHECK_EQ((int)img.rgba[11], 255);  // blue
    CHECK_EQ((int)img.rgba[15], 0);    // transparent
  }
  // Not a PNG at all, and a PNG cut off in the middle: errors, no crash.
  static const uint8_t junk[] = {'h', 'e', 'l', 'l', 'o', ' ', 'w', 'o', 'r', 'l', 'd'};
  CHECK(!decodePng(junk, sizeof junk, img, err));
  CHECK(err.find("not a PNG") != std::string::npos);
  CHECK(!decodePng(png, 40, img, err));
  CHECK(!err.empty());
  CHECK(!loadPngFile(ote_test::tempDir() + "/no-such-file.png", img, err));
}

// Every game that ships with the engine has art for all its terrain and unit
// types (in the default set and in every style), and the PNGs decode to the
// size the loader read from their headers.
TEST(shipped_games_have_complete_art) {
  const char* games[] = {"games/demo-srpg", "games/demo-economy", "editor/templates/strategy-rpg",
                         "editor/templates/economy-tactics", "editor/templates/blank",
                         "games/demo-hex", "editor/templates/hex-tactics-pointy", "editor/templates/hex-tactics-flat"};
  for (const char* g : games) {
    Database db;
    std::string err;
    std::string dir = ote_test::sourceDir() + "/" + g;
    bool ok = loadGame(dir, db, err);
    if (!ok) std::printf("  %s: %s\n", g, err.c_str());
    CHECK(ok);
    if (!ok) continue;
    CHECK(db.graphics.any());
    CHECK_EQ(db.graphics.styles.size(), db.styles.size());
    for (size_t s = 0; s < db.graphics.styles.size(); ++s) {
      const GraphicsSet& set = db.graphics.styles[s];
      for (size_t t = 0; t < db.terrains.size(); ++t)
        if (!set.terrain[t].any()) std::printf("  %s: terrain \"%s\" has no look in style %zu\n", g, db.terrains[t].id.c_str(), s);
      for (size_t u = 0; u < db.unitTypes.size(); ++u)
        if (!set.units[u].base.any() && !set.units[u].model.any()) std::printf("  %s: unit \"%s\" has no look in style %zu\n", g, db.unitTypes[u].id.c_str(), s);
      for (size_t t = 0; t < db.terrains.size(); ++t) CHECK(set.terrain[t].any());
      for (size_t u = 0; u < db.unitTypes.size(); ++u) CHECK(set.units[u].base.any() || set.units[u].model.any());  // a picture or a 3D model
    }
    for (const SheetDef& sh : db.graphics.sheets) {
      Image img;
      CHECK(loadPngFile(dir + "/" + sh.file, img, err));
      CHECK_EQ(img.width, sh.width);
      CHECK_EQ(img.height, sh.height);
    }
  }
}

// The starter water is an animated join (shoreline banks, shimmering water), in every style.
// A bridge joins everything but the water, and the water counts the bridge as water.
TEST(shipped_water_is_an_animated_join) {
  const char* games[] = {"games/demo-srpg", "games/demo-economy", "editor/templates/strategy-rpg",
                         "editor/templates/economy-tactics"};
  int waters = 0;
  for (const char* g : games) {
    Database db;
    std::string err;
    CHECK(loadGame(ote_test::sourceDir() + "/" + g, db, err));
    int bridge = -1;
    for (size_t t = 0; t < db.terrains.size(); ++t)
      if (db.terrains[t].id == "bridge") bridge = (int)t;
    for (size_t si = 0; si < db.graphics.styles.size(); ++si) {
      const GraphicsSet& set = db.graphics.styles[si];
      if (db.styles[si].view != StyleView::TopDown) continue;  // joining pieces are the top-down art's
      for (size_t t = 0; t < db.terrains.size(); ++t) {
        const std::string& id = db.terrains[t].id;
        if (id != "sea" && id != "water") continue;
        const Look& l = set.terrain[t];
        if (si == 0) ++waters;
        CHECK(l.kind == Look::Join);
        CHECK(l.cells.size() >= 2);
        CHECK(l.joinEdge);
        if (bridge >= 0) {
          CHECK_EQ((int)l.joinWith[bridge], 1);
          const Look& b = set.terrain[bridge];
          CHECK(b.kind == Look::Join);
          CHECK(b.cells.size() >= 2);
          CHECK_EQ((int)b.joinWith[t], 0);
        }
      }
    }
  }
  CHECK_EQ(waters, 3);
}
