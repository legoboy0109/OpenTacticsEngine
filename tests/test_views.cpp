// Kinds of view of a visual style, and the isometric projection (docs/VIEWS.md).
#include <cmath>

#include "core/data/game_info.h"
#include "core/data/loader.h"
#include "core/view/iso.h"
#include "save_helpers.h"
#include "test.h"

using namespace ote;
using savetest::copyFile;
using savetest::tempFolder;

namespace {
IsoGeometry geo(int w, int h, int levels) {
  IsoGeometry g;
  g.gridW = w;
  g.gridH = h;
  g.levels = levels;
  return g;
}

// A copy of demo-srpg whose game.json "styles" list is replaced.
std::string gameWithStyles(const char* name, const std::string& styles) {
  std::string src = ote_test::sourceDir() + "/games/demo-srpg", dir = tempFolder(name);
  tempFolder(std::string(name) + "/data");
  tempFolder(std::string(name) + "/maps");
  tempFolder(std::string(name) + "/scripts");
  const char* files[] = {"game.json", "data/weapons.json", "data/units.json", "data/terrain.json",
                         "data/structures.json", "maps/bridge.json", "scripts/formulas.lua"};
  for (const char* f : files) CHECK(copyFile(src + "/" + f, dir + "/" + f));
  std::string text;
  CHECK(readTextFile(dir + "/game.json", text));
  size_t a = text.find("\"styles\": ["), b = text.find("\"view\":");
  CHECK(a != std::string::npos && b != std::string::npos && a < b);
  size_t end = text.rfind("],", b);
  CHECK(end != std::string::npos && end > a);
  text.replace(a, end + 1 - a, "\"styles\": [" + styles + "]");
  CHECK(writeTextFile(dir + "/game.json", text));
  return dir;
}
bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }
}  // namespace

TEST(views_iso_projection_round_trips) {
  IsoGeometry g = geo(5, 3, 2);
  float wx, wy, gx, gy;
  g.project(0, 0, 0, wx, wy);  // top corner of square (0,0): gridH half-diamonds from the left
  CHECK_EQ((int)wx, 3 * 24);
  CHECK_EQ((int)wy, 2 * 12);   // two levels of room above ground level
  g.project(1, 0, 0, wx, wy);  // x grows to the lower right
  CHECK_EQ((int)wx, 4 * 24);
  CHECK_EQ((int)wy, 2 * 12 + 12);
  g.project(0, 1, 0, wx, wy);  // y grows to the lower left
  CHECK_EQ((int)wx, 2 * 24);
  g.project(0, 0, 2, wx, wy);  // elevation raises
  CHECK_EQ((int)wy, 0);
  for (float e : {0.0f, 1.0f, 2.0f}) {
    g.project(3.25f, 1.5f, e, wx, wy);
    g.unproject(wx, wy, e, gx, gy);
    CHECK(std::fabs(gx - 3.25f) < 0.001f && std::fabs(gy - 1.5f) < 0.001f);
  }
  CHECK_EQ(g.worldW(), 8 * 24);
  CHECK_EQ(g.worldH(), 8 * 12 + 2 * 12 + 12);
}

TEST(views_iso_pick_prefers_the_square_nearest_the_viewer) {
  IsoGeometry g = geo(4, 4, 3);
  std::vector<int> flat(16, 0);
  float wx, wy;
  for (int y = 0; y < 4; ++y)
    for (int x = 0; x < 4; ++x) {
      g.project(x + 0.5f, y + 0.5f, 0, wx, wy);
      CHECK_EQ(g.pick(wx, wy, flat), y * 4 + x);
    }
  g.project(-0.5f, 0.5f, 0, wx, wy);
  CHECK_EQ(g.pick(wx, wy, flat), -1);  // off the map

  // Square (2,2) stands 3 levels high (36 px): its top hides the centre of (1,1)
  // (one diamond = 24 px further back), and its side faces hide (2,2)'s ground spot.
  std::vector<int> tower = flat;
  tower[2 * 4 + 2] = 3;
  g.project(2.5f, 2.5f, 3, wx, wy);
  CHECK_EQ(g.pick(wx, wy, tower), 2 * 4 + 2);  // its top face
  g.project(1.5f, 1.5f, 0, wx, wy);            // where (1,1)'s centre would show
  CHECK_EQ(g.pick(wx, wy, tower), 2 * 4 + 2);
  g.project(2.5f, 2.5f, 0, wx, wy);            // on the tower's side faces
  CHECK_EQ(g.pick(wx, wy, tower), 2 * 4 + 2);
  g.project(3.5f, 3.5f, 0, wx, wy);            // in front of it: untouched
  CHECK_EQ(g.pick(wx, wy, tower), 3 * 4 + 3);

  CHECK(g.hidden(1, 1, tower));
  CHECK(!g.hidden(2, 2, tower));
  CHECK(!g.hidden(3, 3, tower));
  CHECK(!g.hidden(0, 3, tower));
  CHECK(!g.hidden(1, 1, flat));
  tower[2 * 4 + 2] = 1;  // one level (12 px) does not reach a diamond further back
  CHECK(!g.hidden(1, 1, tower));
  CHECK(!g.hidden(1, 2, tower));  // a neighbour beside it only reaches the middle with its corner
}

TEST(views_style_view_kinds_load) {
  Database db;
  std::string err;
  std::string dir = gameWithStyles("views_ok",
      "{\"id\":\"a\"}, {\"id\":\"b\",\"view\":\"top-down\"},"
      "{\"id\":\"c\",\"view\":\"isometric\",\"isometric\":{\"tileWidth\":64,\"tileHeight\":32,\"heightStep\":8,\"unitWidth\":40}},"
      "{\"id\":\"d\",\"view\":\"isometric-3d\"}, {\"id\":\"e\",\"view\":\"3d\"}, {\"id\":\"f\",\"view\":\"isometric\"}");
  CHECK(loadGame(dir, db, err));
  if (!err.empty()) std::printf("  %s\n", err.c_str());
  CHECK_EQ((int)db.styles.size(), 6);
  CHECK(db.styles[0].view == StyleView::TopDown && db.styles[1].view == StyleView::TopDown);
  CHECK(db.styles[2].view == StyleView::Isometric);
  CHECK_EQ(db.styles[2].iso.tileWidth, 64);
  CHECK_EQ(db.styles[2].iso.tileHeight, 32);
  CHECK_EQ(db.styles[2].iso.heightStep, 8);
  CHECK_EQ(db.styles[2].iso.unitWidth, 40);
  CHECK(db.styles[3].view == StyleView::Isometric3D && db.styles[4].view == StyleView::Full3D);
  CHECK_EQ(db.styles[5].iso.tileWidth, 0);  // sizes left to the defaults
  CHECK_EQ(db.styles[5].iso.heightStep, -1);

  GameInfo info;
  CHECK(loadGameInfo(dir, info, err));
  CHECK_EQ((int)info.styles.size(), 6);
  CHECK(info.styles[2].view == StyleView::Isometric && info.styles[4].view == StyleView::Full3D);
  CHECK(info.styles[0].view == StyleView::TopDown);
}

TEST(views_style_view_errors) {
  struct Bad { const char* styles; const char* a; const char* b; };
  const Bad bad[] = {
      {"{\"id\":\"a\",\"view\":\"sideways\"}", "\"view\"", "isometric"},
      {"{\"id\":\"a\",\"view\":3}", "\"view\"", "style \"a\""},
      {"{\"id\":\"a\",\"view\":\"isometric\",\"isometric\":{\"tileWidth\":47}}", "tileWidth", "even"},
      {"{\"id\":\"a\",\"view\":\"isometric\",\"isometric\":{\"tileHeight\":2}}", "tileHeight", "4 to 256"},
      {"{\"id\":\"a\",\"view\":\"isometric\",\"isometric\":{\"heightStep\":-1}}", "heightStep", "0 to 128"},
      {"{\"id\":\"a\",\"view\":\"isometric\",\"isometric\":{\"unitWidth\":2}}", "unitWidth", "4 to 512"},
      {"{\"id\":\"a\",\"isometric\":[]}", "isometric", "style \"a\""},
  };
  for (const Bad& b : bad) {
    Database db;
    std::string err;
    CHECK(!loadGame(gameWithStyles("views_bad", b.styles), db, err));
    if (!(has(err, b.a) && has(err, b.b))) std::printf("  unexpected message: %s\n", err.c_str());
    CHECK(has(err, b.a) && has(err, b.b));
  }
}
