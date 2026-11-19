// Portraits of the dialogue box: data/graphics.json "portraits" (loader, per-style merge, errors).
#include "posix_compat.h"

#include <cstdio>

#include "core/base/json.h"
#include "core/data/loader.h"
#include "test.h"

using namespace ote;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }

// A game with styles day (top-down), night (top-down) and solid (3D isometric); one sheet "faces" of 4 x 2 cells.
std::string makeGame(const char* name, const std::string& graphicsJson) {
  std::string root = ote_test::tempDir() + "/portrait_" + name;
  mkdir(root.c_str(), 0755);
  mkdir((root + "/data").c_str(), 0755);
  mkdir((root + "/art").c_str(), 0755);
  CHECK(writeTextFile(root + "/game.json",
                      "{ \"formatVersion\": 1, \"name\": \"G\", \"stats\": [{\"id\":\"hp\"},{\"id\":\"mov\"}],"
                      " \"roles\": {\"hp\":\"hp\",\"move\":\"mov\"}, \"moveTypes\": [{\"id\":\"foot\"}],"
                      " \"formulas\": {\"damage\":\"1\"},"
                      " \"styles\": [{\"id\":\"day\"},{\"id\":\"night\"},{\"id\":\"solid\",\"view\":\"isometric-3d\"}] }"));
  CHECK(writeTextFile(root + "/data/terrain.json", "{ \"terrain\": [{\"id\":\"plain\"}] }"));
  CHECK(writeTextFile(root + "/data/weapons.json", "{ \"weapons\": [{\"id\":\"stick\"}] }"));
  CHECK(writeTextFile(root + "/data/units.json",
                      "{ \"units\": [{\"id\":\"grunt\",\"moveType\":\"foot\",\"stats\":{\"hp\":5,\"mov\":3}}] }"));
  // Only the PNG header matters to the core (64 x 32 pixels).
  unsigned char b[33] = {0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A, 0, 0, 0, 13, 'I', 'H', 'D', 'R', 0, 0, 0, 64, 0, 0, 0, 32, 8, 6};
  FILE* f = std::fopen((root + "/art/faces.png").c_str(), "wb");
  CHECK(f != nullptr);
  if (f) {
    std::fwrite(b, 1, sizeof b, f);
    std::fclose(f);
  }
  CHECK(writeTextFile(root + "/data/graphics.json", graphicsJson));
  return root;
}

const char* kFaces = "\"sheets\": [ {\"id\":\"faces\",\"file\":\"art/faces.png\",\"cellWidth\":16,\"cellHeight\":16} ]";

void expectError(const char* name, const std::string& body, const char* a, const char* b = "") {
  Database db;
  std::string err;
  bool ok = loadGame(makeGame(name, body), db, err);
  CHECK(!ok);
  if (ok || !has(err, a) || !has(err, b)) std::printf("  [%s] message was: %s\n", name, err.c_str());
  CHECK(has(err, "graphics.json") && has(err, a) && has(err, b));
}

}  // namespace

TEST(portraits_load_and_merge_per_style) {
  std::string body = std::string("{ ") + kFaces +
      ", \"portraits\": { \"eirik\": {\"sheet\":\"faces\",\"cell\":[1,0]}, \"lyra\": {\"sheet\":\"faces\",\"cell\":[2,1]},"
      "                 \"old\": {\"sheet\":\"faces\",\"cell\":[0,0]} },"
      " \"styles\": { \"night\": { \"portraits\": { \"eirik\": {\"sheet\":\"faces\",\"cell\":[3,1]}, \"lyra\": null } } } }";
  Database db;
  std::string err;
  bool ok = loadGame(makeGame("merge", body), db, err);
  if (!ok) std::printf("  %s\n", err.c_str());
  CHECK(ok);
  if (!ok) return;
  const GraphicsSet& day = *db.graphics.set(0);
  const Look* eirik = day.portrait("eirik");
  CHECK(eirik != nullptr);
  if (eirik) {
    CHECK(eirik->kind == Look::Still);
    CHECK_EQ((int)eirik->cells[0].col, 1);
    CHECK_EQ(eirik->sheet, 0);
  }
  CHECK(day.portrait("lyra") != nullptr);
  CHECK(day.portrait("nobody") == nullptr);

  // A style replaces per id and null removes; the rest is inherited.
  const GraphicsSet& night = *db.graphics.set(1);
  CHECK(night.portrait("eirik") != nullptr && night.portrait("eirik")->cells[0].col == 3);
  CHECK(night.portrait("eirik") != nullptr && night.portrait("eirik")->cells[0].row == 1);
  CHECK(night.portrait("lyra") == nullptr);
  CHECK(night.portrait("old") != nullptr);

  // A portrait is not tied to the kind of view: the 3D style has the general ones too.
  const GraphicsSet& solid = *db.graphics.set(2);
  CHECK(solid.portrait("eirik") != nullptr && solid.portrait("eirik")->cells[0].col == 1);
  CHECK(solid.portrait("lyra") != nullptr);
}

TEST(portraits_only_in_a_style_work_and_are_optional) {
  Database db;
  std::string err;
  CHECK(loadGame(makeGame("styleonly", std::string("{ ") + kFaces +
                                           ", \"styles\": { \"solid\": { \"portraits\": { \"x\": {\"sheet\":\"faces\",\"cell\":[0,1]} } } } }"),
                 db, err));
  CHECK(db.graphics.set(2)->portrait("x") != nullptr);
  CHECK(db.graphics.set(0)->portrait("x") == nullptr);
  Database plain;
  CHECK(loadGame(makeGame("noportraits", std::string("{ ") + kFaces + " }"), plain, err));
  CHECK(plain.graphics.set(0)->portraits.empty());
}

TEST(portraits_errors_are_readable) {
  std::string s = std::string("{ ") + kFaces + ", ";
  expectError("p_sheet", s + "\"portraits\": {\"eirik\": {\"sheet\":\"nope\",\"cell\":[0,0]}} }", "portrait \"eirik\"",
              "unknown sheet \"nope\"");
  expectError("p_cell", s + "\"portraits\": {\"eirik\": {\"sheet\":\"faces\",\"cell\":[4,0]}} }", "portrait \"eirik\"",
              "outside");
  expectError("p_shape", s + "\"portraits\": {\"eirik\": {\"sheet\":\"faces\",\"cell\":[0]}} }", "[column, row]");
  expectError("p_frames", s + "\"portraits\": {\"eirik\": {\"sheet\":\"faces\",\"frames\":[[0,0],[1,0]]}} }", "one \"cell\"");
  expectError("p_none", s + "\"portraits\": {\"eirik\": {\"sheet\":\"faces\"}} }", "exactly one of");
  expectError("p_style", s + "\"styles\": {\"night\": {\"portraits\": {\"lyra\": {\"sheet\":\"faces\",\"cell\":[0,9]}}}} }",
              "style \"night\"", "portrait \"lyra\"");
  expectError("p_type", s + "\"portraits\": [] }", "portraits");
}
