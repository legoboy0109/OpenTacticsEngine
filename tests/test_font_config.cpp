#include "core/base/json.h"
#include "core/data/loader.h"
#include "test.h"

using namespace ote;

TEST(font_config_defaults_to_builtin_pixel) {
  Database db;
  std::string err;
  CHECK(loadGame(ote_test::sourceDir() + "/games/demo-economy", db, err));
  CHECK_STR(db.font.name, "pixel");
  CHECK(db.font.file.empty());
  CHECK_EQ(db.font.size, 0);
  CHECK_EQ(db.font.smooth, -1);
}

TEST(font_index_lists_shipped_fonts_that_exist) {
  std::string dir = ote_test::sourceDir() + "/assets/fonts/";
  Json index;
  std::string err;
  CHECK(loadJsonFile(dir + "fonts.json", index, err));
  CHECK(index["fonts"].size() >= 4);
  for (const Json& f : index["fonts"].items()) {
    std::string bytes;
    CHECK(!f["id"].asString().empty());
    CHECK(readTextFile(dir + f["file"].asString(), bytes) && bytes.size() > 10000);
    CHECK(f["size"].asInt() > 0);
  }
}
