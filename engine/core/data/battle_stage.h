// The layout of a fixed battle stage (docs/BATTLE_SCENE.md "Fixed stage"). Pure: screen size and stage data in, scale, position and
// screen points out. No drawing, no player code, so the unit tests and the player use the very same numbers.
#pragma once
#include "core/data/battle_art.h"

namespace ote {

struct StageGeo {
  bool valid = false;
  int scale = 1;                    // the one whole-number scale of the whole stage
  int x = 0, y = 0, w = 0, h = 0;   // the stage on the screen (unclipped: x / y may be negative, y + h may pass the screen's bottom)
  int cropTop = 0, cropBottom = 0;  // whole stage pixel rows that fall off the screen at the top / bottom
  bool cropped = false;             // some of the stage is off the screen (rows, or columns at the sides)
  int feet[2][2] = {{0, 0}, {0, 0}};      // each fighter's foot point, screen pixels
  int platform[2][2] = {{0, 0}, {0, 0}};  // top-left of each platform picture, screen pixels
  int platformW[2] = {0, 0}, platformH[2] = {0, 0};  // its size on the screen (0 = no platform picture)
};

// `W` x `H`: the screen in design pixels. `hudBottom`: the first row below the hit-point boxes; `hintTop`: the first row of the hint line.
// The area the scene has for the fight is rows hudBottom..hintTop; a stage that does not fit it may run under the boxes and the hint.
// `far`: the fight is at range 2+ (feetFar / platformFar). `picW` / `picH`: each platform picture's size in stage pixels (0 = none; may be null).
// Scale: the largest whole number k with width * k <= W (1 when the stage is wider than the screen). "contain": k is then reduced
// until height * k fits the area (never below 1). Vertical place: a stage that fits the area is centred in it; a taller one covers
// the screen height (placed or cropped by `align`), then is moved the least that keeps every foot point and platform inside the
// area. Horizontally the stage is centred.
StageGeo stageGeometry(int W, int H, int hudBottom, int hintTop, const BattleStage& st, bool far, const int picW[2], const int picH[2]);

}  // namespace ote
