#include "core/data/game_info.h"
#include "test.h"

using namespace ote;

TEST(game_info_reads_demo_launcher_fields) {
  GameInfo gi;
  std::string err;
  CHECK(loadGameInfo(ote_test::sourceDir() + "/games/demo-srpg", gi, err));
  CHECK_STR(gi.name, "Emblem Demo");
  CHECK(!gi.description.empty());
  CHECK(gi.mapCount >= 1);
  CHECK_EQ((int)gi.styles.size(), 4);
  CHECK(gi.styles[3].view == StyleView::Isometric3D);
  CHECK(gi.styles[2].view == StyleView::Isometric);
  CHECK_STR(gi.styles[0].id, "classic");
  CHECK_STR(gi.styles[1].name, "Parchment");
  CHECK_STR(gi.font.name, "pixel");
}

TEST(game_info_defaults_style_and_reports_missing_file) {
  GameInfo gi;
  std::string err;
  CHECK(loadGameInfo(ote_test::sourceDir() + "/tests/fixtures/base", gi, err));
  CHECK_EQ((int)gi.styles.size(), 1);
  CHECK_STR(gi.styles[0].id, "default");
  CHECK(!loadGameInfo(ote_test::sourceDir() + "/tests/fixtures/nope", gi, err));
  CHECK(!err.empty());
}
