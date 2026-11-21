// Item and weapon icons: data/graphics.json "icons" (docs/GRAPHICS.md "Icons"): loader, lookups, every refusal.
#include "posix_compat.h"

#include <cstdio>

#include "core/base/json.h"
#include "core/data/loader.h"
#include "test.h"

using namespace ote;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

// A game with the weapon "stick" (and the weapon item "stick_item" that wields it) and the usable item "potion"; one sheet "icons"
// of 4 x 2 cells of 8 x 8 pixels.
std::string makeGame(const char* name, const std::string& graphicsJson) {
  std::string root = ote_test::tempDir() + "/icons_" + name;
  mkdir(root.c_str(), 0755);
  mkdir((root + "/data").c_str(), 0755);
  mkdir((root + "/art").c_str(), 0755);
  CHECK(writeTextFile(root + "/game.json",
                      "{ \"formatVersion\": 1, \"name\": \"G\", \"stats\": [{\"id\":\"hp\"},{\"id\":\"mov\"}],"
                      " \"roles\": {\"hp\":\"hp\",\"move\":\"mov\"}, \"moveTypes\": [{\"id\":\"foot\"}],"
                      " \"formulas\": {\"damage\":\"1\"}, \"rules\": {\"items\": {\"weapons\": \"carried\"}} }"));
  CHECK(writeTextFile(root + "/data/terrain.json", "{ \"terrain\": [{\"id\":\"plain\"}] }"));
  CHECK(writeTextFile(root + "/data/weapons.json", "{ \"weapons\": [{\"id\":\"stick\"},{\"id\":\"club\"}] }"));
  CHECK(writeTextFile(root + "/data/items.json",
                      "{ \"items\": [ {\"id\":\"stick_item\",\"name\":\"Stick\",\"kind\":\"weapon\",\"weapon\":\"stick\",\"uses\":10},"
                      " {\"id\":\"potion\",\"name\":\"Potion\",\"kind\":\"usable\",\"uses\":2,\"heal\":5} ] }"));
  CHECK(writeTextFile(root + "/data/units.json",
                      "{ \"units\": [{\"id\":\"grunt\",\"moveType\":\"foot\",\"stats\":{\"hp\":5,\"mov\":3}}] }"));
  unsigned char b[33] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13, 'I', 'H', 'D', 'R', 0, 0, 0, 32, 0, 0, 0, 16, 8, 6};
  FILE* f = std::fopen((root + "/art/icons.png").c_str(), "wb");
  CHECK(f != nullptr);
  if (f) {
    std::fwrite(b, 1, sizeof b, f);
    std::fclose(f);
  }
  CHECK(writeTextFile(root + "/data/graphics.json", graphicsJson));
  return root;
}

const char* kSheet = "\"sheets\": [ {\"id\":\"icons\",\"file\":\"art/icons.png\",\"cellWidth\":8,\"cellHeight\":8} ]";

void expectError(const char* name, const std::string& body, const char* a, const char* b = "") {
  Database db;
  std::string err;
  bool ok = loadGame(makeGame(name, body), db, err);
  CHECK(!ok);
  if (ok || !has(err, a) || !has(err, b)) std::printf("  [%s] message was: %s\n", name, err.c_str());
  CHECK(has(err, "graphics.json") && has(err, a) && has(err, b));
}

}  // namespace

TEST(icons_load_for_weapons_and_items) {
  const std::string body = std::string("{ ") + kSheet +
      ", \"icons\": { \"club\": {\"sheet\":\"icons\",\"cell\":[1,0]}, \"potion\": {\"sheet\":\"icons\",\"cell\":[3,1]},"
      "             \"stick\": {\"sheet\":\"icons\",\"cell\":[2,1]}, \"stick_item\": null } }";
  Database db;
  std::string err;
  const bool ok = loadGame(makeGame("load", body), db, err);
  if (!ok) std::printf("  %s\n", err.c_str());
  CHECK(ok);
  if (!ok) return;
  const Look* club = db.graphics.weaponIcon(db.findWeapon("club"));
  CHECK(club != nullptr);
  if (club) {
    CHECK(club->kind == Look::Still);
    CHECK_EQ(club->sheet, 0);
    CHECK_EQ((int)club->cells[0].col, 1);
    CHECK_EQ((int)club->cells[0].row, 0);
  }
  const Look* potion = db.graphics.itemIcon(db.findItem("potion"));
  CHECK(potion != nullptr && potion->cells[0].col == 3 && potion->cells[0].row == 1);
  // "stick" is a weapon id only (the item is "stick_item"); null says "no icon".
  CHECK(db.graphics.weaponIcon(db.findWeapon("stick")) != nullptr);
  CHECK(db.graphics.itemIcon(db.findItem("stick_item")) == nullptr);
  CHECK(db.graphics.weaponIcon(-1) == nullptr && db.graphics.itemIcon(99) == nullptr);
}

TEST(icons_absent_means_none) {
  Database db;
  std::string err;
  CHECK(loadGame(makeGame("none", std::string("{ ") + kSheet + " }"), db, err));
  CHECK(db.graphics.weaponIcon(0) == nullptr && db.graphics.itemIcon(0) == nullptr);
  Database bare;  // no graphics.json at all
  CHECK(loadGame(makeGame("bare", "{ \"sheets\": [] }"), bare, err));
  CHECK(bare.graphics.weaponIcon(0) == nullptr);
}

TEST(icons_every_refusal) {
  const std::string head = std::string("{ ") + kSheet + ", \"icons\": ";
  expectError("sheet", head + "{ \"potion\": {\"sheet\":\"nope\",\"cell\":[0,0]} } }", "icon \"potion\"", "unknown sheet \"nope\"");
  expectError("outside", head + "{ \"potion\": {\"sheet\":\"icons\",\"cell\":[4,0]} } }", "icon \"potion\"", "outside sheet \"icons\" (4 x 2 cells)");
  expectError("outside2", head + "{ \"potion\": {\"sheet\":\"icons\",\"cell\":[0,2]} } }", "icon \"potion\"", "cell [0, 2] is outside");
  expectError("badcell", head + "{ \"potion\": {\"sheet\":\"icons\",\"cell\":[0]} } }", "icon \"potion\"", "[column, row]");
  expectError("noid", head + "{ \"sword\": {\"sheet\":\"icons\",\"cell\":[0,0]} } }", "icon \"sword\"", "no weapon or item with this id");
  expectError("emptyid", head + "{ \"\": {\"sheet\":\"icons\",\"cell\":[0,0]} } }", "icon \"\"");
  expectError("key", head + "{ \"potion\": {\"sheet\":\"icons\",\"cell\":[0,0],\"size\":3} } }", "icon \"potion\"", "size");
  expectError("frames", head + "{ \"potion\": {\"sheet\":\"icons\",\"frames\":[[0,0],[1,0]]} } }", "icon \"potion\"", "one \"cell\"");
  expectError("nocell", head + "{ \"potion\": {\"sheet\":\"icons\"} } }", "icon \"potion\"", "exactly one of");
  expectError("nosheet", head + "{ \"potion\": {\"cell\":[0,0]} } }", "icon \"potion\"", "sheet");
  expectError("notobj", head + "{ \"potion\": 5 } }", "icon \"potion\"", "must be an object");
  expectError("notmap", head + "[] }", "icons");
}
