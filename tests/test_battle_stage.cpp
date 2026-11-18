// The fixed battle stage's geometry (core/data/battle_stage.h): scale, place, crop and the screen points, as a pure function.
#include "core/data/battle_stage.h"
#include "test.h"

using namespace ote;

namespace {

// A 240 x 160 stage: feet close (88,104) (152,104), far (56,104) (184,104); platforms 64 x 32 at (8,88) / (128,88), far at (0,88) / (136,88).
BattleStage handheld(BattleStage::Align align = BattleStage::AlignCenter, bool contain = false) {
  BattleStage s;
  s.on = true;
  s.width = 240;
  s.height = 160;
  s.feet[0][0] = 88; s.feet[0][1] = 104; s.feet[1][0] = 152; s.feet[1][1] = 104;
  s.hasFeetFar = true;
  s.feetFar[0][0] = 56; s.feetFar[0][1] = 104; s.feetFar[1][0] = 184; s.feetFar[1][1] = 104;
  s.hasPlatform = true;
  s.platform[0][0] = 8; s.platform[0][1] = 88; s.platform[1][0] = 128; s.platform[1][1] = 88;
  s.hasPlatformFar = true;
  s.platformFar[0][0] = 0; s.platformFar[0][1] = 88; s.platformFar[1][0] = 136; s.platformFar[1][1] = 88;
  s.align = align;
  s.contain = contain;
  return s;
}

const int kPw[2] = {64, 64}, kPh[2] = {32, 32};
// The boxes end at row 46 and the hint line starts at row 257 on the 480 x 270 screen (what the player's font gives; the rule only needs the numbers).
StageGeo at(int W, int H, const BattleStage& s, bool far = false, int hud = 46, int hint = -1) {
  return stageGeometry(W, H, hud, hint < 0 ? H - 13 : hint, s, far, kPw, kPh);
}

}  // namespace

TEST(stage_geometry_480x270_cover_aligns) {
  // k = 2 (480 / 240); the stage is 320 rows tall, the screen 270: 50 rows are cropped.
  StageGeo b = at(480, 270, handheld(BattleStage::AlignBottom));
  CHECK(b.valid);
  CHECK_EQ(b.scale, 2);
  CHECK_EQ(b.x, 0);
  CHECK_EQ(b.y, -50);  // the lowest 135 rows show
  CHECK_EQ(b.cropTop, 25);
  CHECK_EQ(b.cropBottom, 0);
  CHECK(b.cropped);
  CHECK_EQ(b.feet[0][0], 176);
  CHECK_EQ(b.feet[0][1], 158);
  CHECK_EQ(b.feet[1][0], 304);
  CHECK_EQ(b.platform[0][0], 16);  // (8, 88) -> (16, 176 - 50)
  CHECK_EQ(b.platform[0][1], 126);
  CHECK_EQ(b.platformW[0], 128);
  CHECK_EQ(b.platformH[0], 64);
  StageGeo t = at(480, 270, handheld(BattleStage::AlignTop));
  CHECK_EQ(t.y, 0);
  CHECK_EQ(t.cropTop, 0);
  CHECK_EQ(t.cropBottom, 25);
  CHECK_EQ(t.feet[0][1], 208);
  CHECK(t.platform[0][1] + t.platformH[0] <= 257);  // the platform's bottom (88 + 32 = 120 -> 240) is above the hint line
  StageGeo c = at(480, 270, handheld(BattleStage::AlignCenter));
  CHECK_EQ(c.y, -25);
  CHECK_EQ(c.cropTop, 12);
  CHECK_EQ(c.cropBottom, 12);
  CHECK_EQ(c.feet[0][1], 183);
}

TEST(stage_geometry_far_points) {
  StageGeo g = at(480, 270, handheld(BattleStage::AlignBottom), true);
  CHECK_EQ(g.feet[0][0], 112);  // far feet 56 -> 112
  CHECK_EQ(g.feet[1][0], 368);
  CHECK_EQ(g.platform[0][0], 0);
  CHECK_EQ(g.platform[1][0], 272);
  // No far points given: the close ones are used at range.
  BattleStage s = handheld();
  s.hasFeetFar = s.hasPlatformFar = false;
  StageGeo n = at(480, 270, s, true);
  CHECK_EQ(n.feet[0][0], at(480, 270, s, false).feet[0][0]);
}

TEST(stage_geometry_contain_and_portrait) {
  // contain: the area between the boxes and the hint line is 211 rows; k = 2 needs 320, so k drops to 1 and the stage is centred.
  StageGeo c = at(480, 270, handheld(BattleStage::AlignBottom, true));
  CHECK_EQ(c.scale, 1);
  CHECK_EQ(c.x, 120);
  CHECK_EQ(c.h, 160);
  CHECK_EQ(c.y, 46 + (211 - 160) / 2);
  CHECK(!c.cropped);
  CHECK_EQ(c.feet[0][0], 120 + 88);
  CHECK_EQ(c.feet[0][1], c.y + 104);
  // Portrait 270 x 480: k = 1 (270 / 240), the whole stage fits between the boxes and the hint: centred, nothing hidden.
  StageGeo p = at(270, 480, handheld(BattleStage::AlignBottom));
  CHECK_EQ(p.scale, 1);
  CHECK_EQ(p.x, 15);
  CHECK(!p.cropped);
  CHECK(p.y >= 46 && p.y + p.h <= 467);
  // cover and contain agree when the stage fits.
  CHECK_EQ(at(270, 480, handheld(BattleStage::AlignTop, true)).y, p.y);
}

TEST(stage_geometry_960x540_and_native_size) {
  // 960 x 540: k = 4 (640 rows) under cover; the area is 481 rows: cropped, bottom aligned.
  StageGeo c = at(960, 540, handheld(BattleStage::AlignBottom), false, 46, 527);
  CHECK_EQ(c.scale, 4);
  CHECK_EQ(c.x, 0);
  CHECK_EQ(c.y, 540 - 640);
  CHECK_EQ(c.cropTop, 25);
  CHECK_EQ(c.feet[1][1], -100 + 416);
  // contain: k = 3 (480 rows) fits the 481 rows area: centred in it (one row spare).
  StageGeo n = at(960, 540, handheld(BattleStage::AlignTop, true), false, 46, 527);
  CHECK_EQ(n.scale, 3);
  CHECK_EQ(n.x, 120);
  CHECK_EQ(n.y, 46);
  CHECK(!n.cropped);
  // A screen exactly the stage's size (240 x 160): k = 1, the stage is the screen; cropped by the boxes only in that it runs under them.
  StageGeo e = at(240, 160, handheld(BattleStage::AlignBottom), false, 46, 150);
  CHECK_EQ(e.scale, 1);
  CHECK_EQ(e.x, 0);
  CHECK_EQ(e.y, 0);
  CHECK(!e.cropped);
  CHECK_EQ(e.feet[0][1], 104);
  CHECK_EQ(e.platform[1][0], 128);
}

TEST(stage_geometry_keeps_feet_visible) {
  // Feet low in a tall stage with align top: the lowest point is pulled up above the hint line (the stage then leaves the top edge).
  BattleStage s = handheld(BattleStage::AlignTop);
  s.width = 240;
  s.height = 320;
  s.feet[0][1] = s.feet[1][1] = 300;
  s.hasPlatform = false;
  StageGeo g = stageGeometry(480, 270, 46, 257, s, false, nullptr, nullptr);
  CHECK_EQ(g.scale, 2);
  CHECK(g.feet[0][1] <= 257 && g.feet[0][1] >= 46);
  // High feet with align bottom: pushed down under the boxes' bottom edge.
  BattleStage h = handheld(BattleStage::AlignBottom);
  h.height = 320;
  h.feet[0][1] = h.feet[1][1] = 40;
  h.hasPlatform = false;
  StageGeo gh = stageGeometry(480, 270, 46, 257, h, false, nullptr, nullptr);
  CHECK(gh.feet[0][1] >= 46);
}

TEST(stage_geometry_unpositioned_platform_and_wide_stage) {
  BattleStage s = handheld();
  s.hasPlatform = s.hasPlatformFar = false;
  StageGeo g = at(480, 270, s);
  CHECK_EQ(g.platform[0][0], g.feet[0][0] - 64);  // centred on the foot point (128 wide on screen)
  CHECK_EQ(g.platform[0][1], g.feet[0][1] - 32);
  // A stage wider than the screen: k = 1, centred, both sides cropped.
  BattleStage w = handheld();
  w.width = 600;
  StageGeo wg = at(480, 270, w);
  CHECK_EQ(wg.scale, 1);
  CHECK_EQ(wg.x, -60);
  CHECK(wg.cropped);
  // Off: nothing.
  BattleStage off;
  CHECK(!at(480, 270, off).valid);
}
