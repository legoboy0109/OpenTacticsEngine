// The battle scene's screen layout (shared by fight_scene_draw.cpp and fight_scene_stage.cpp). Internal to the scene.
#pragma once
#include "battle/fight_scene.h"
#include "core/data/battle_stage.h"

namespace ote {

struct FightScene::Geo {
  int W = 0, H = 0;
  bool portrait = false;
  int lh = 8, rowH = 10;
  int boxW = 0, boxH = 0, boxY = 4, boxX[2] = {4, 4};
  int stageTop = 0, stageBottom = 0;  // the area between the boxes and the hint line
  int groundY = 0;                    // the ground line (with a fixed stage: the mean of the two foot points' rows)
  int footX[2] = {0, 0}, footY[2] = {0, 0};  // each fighter's foot point (the same row for both without a fixed stage)
  int scale[2] = {1, 1};  // each fighter's whole-number enlargement (a small map picture is enlarged more than a big animation frame)
  int frameW[2] = {24, 24}, frameH[2] = {24, 24};  // each fighter's picture in design pixels
  // A fixed stage (docs/BATTLE_SCENE.md "Fixed stage"): the stage's place and the one scale of effects and platforms.
  int shakeX = 0, shakeY = 0;  // the stage is moved by this much by an effect's shake (already added to the points above)
  bool stage = false;
  bool panning = false;  // a range band that shows one fighter at a time: the points are moved by the camera (docs/BATTLE_SCENE.md "Range bands")
  double viewX = 0;           // a panning band: the world x of the picture's left edge (stage pixels)
  bool scrollBackdrop = false;  // pan "backdrop": "scroll": the backdrop moves with viewX and wraps
  StageGeo sg;
};

}  // namespace ote
