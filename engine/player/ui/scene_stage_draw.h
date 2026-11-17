// Drawing a SceneStage (scene scene_stage.h): the background picture, the standing portraits, the weather and the fade.
// Each layer is a separate call because the battle screen and the campaign's scene screen stack them differently with their own
// things (docs/EVENTS.md "Scene presentation").
#pragma once
#include "battle/sprites.h"
#include "core/data/database.h"
#include "ui/scene_stage.h"

namespace ote {

struct StageDrawCtx {
  const Database& db;
  const Sprites* sprites;  // may be null (no art loaded): pictures are then simply not drawn
  int styleIdx;
};

// Where a picture of `sw` x `sh` pixels is drawn so that it covers a `W` x `H` screen: the scale is a whole number when the
// picture has to be made bigger (the pixel-art rule: no blurry fractions), a fraction only when it has to shrink; the
// excess is cropped evenly all round. `srcX/srcY/srcW/srcH` are the part of the picture shown (relative to its top-left),
// `dstX/dstY/dstW/dstH` where it lands. Pure, for tests.
struct CoverFit {
  int srcX, srcY, srcW, srcH;
  int dstX, dstY, dstW, dstH;
  float scale;
};
CoverFit fitCover(int sw, int sh, int W, int H);

// The scene's background over the whole screen (the previous one cross-fades out). Draws black first when something covers
// the screen but its picture is missing. `shakeX` / `shakeY` move the picture.
void drawStageBackground(Renderer& r, const StageDrawCtx& ctx, const SceneStage& stage);
// The standing portraits, bottoms on `baseY` (the top of the dialogue box). The speaker is bright, the others dimmed.
void drawStagePortraits(Renderer& r, const StageDrawCtx& ctx, const SceneStage& stage, int baseY);
void drawStageWeather(Renderer& r, const SceneStage& stage);
void drawStageFade(Renderer& r, const SceneStage& stage);

}  // namespace ote
