// Where the standing portraits of the scene stage go (docs/EVENTS.md "Standing portraits"). Pure maths, no drawing, so the
// "never overlap badly" rules are unit-tested (tests/test_scene_stage_layout.cpp).
//
// Seven slots across the stage, 0 (farLeft) .. 6 (farRight); see stageSlot() in core/data/scene_fx.h. Two layouts:
//  - classic: only left / center / right are used (every game before the extra slots). left and right sit against the screen
//    edge and center in the middle, exactly as they always did, at the largest whole-number scale that fits.
//  - wide: any of farLeft / midLeft / midRight / farRight is on the stage. The seven slots are evenly spread from the left
//    edge to the right edge (farLeft against the left edge, farRight against the right edge), and the portraits shrink (whole
//    scales first, then fractions) until neighbours on the stage overlap by at most 40% of their width.
// A phone-shaped screen (taller than wide) shows at most three portraits: the speaker and its neighbours on the stage (the
// previous and the next one in slot order); with nobody speaking, the three in the middle of the stage. The three are spread
// to the left edge, the middle and the right edge (two: left and right edge; one: where its slot is).
#pragma once
#include <vector>

namespace ote {

struct StageItem {
  float slot = 0;      // 0..6, fractional while the portrait slides
  int artW = 0, artH = 0;
  bool lit = false;    // the speaker
};

struct StagePlace {
  bool shown = false;  // false: left out because the screen is too narrow for it (phone rule)
  int x = 0, w = 0, h = 0;  // left edge, size; the bottom sits on the caller's base line
  float scale = 1;
};

// `baseY` = the line the portraits stand on (the top of the dialogue box). `wide` = see above (any of the four added slots in
// use). Result has one entry per item, same order.
std::vector<StagePlace> layoutStage(int screenW, int screenH, int baseY, bool wide, const std::vector<StageItem>& items);

// Items drawn at all on a phone-shaped screen: indexes into `items`, at most three. Public for the tests.
std::vector<size_t> phoneVisible(const std::vector<StageItem>& items);

constexpr int kMaxPhonePortraits = 3;

}  // namespace ote
