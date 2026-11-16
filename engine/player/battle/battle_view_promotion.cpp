// View hooks for UnitPromoted (docs/SRPG.md "Player UI > Promotion", step U2).
//
// The rules already show the unit as its new type when the event plays, so push() marks the unit with its OLD type
// (UnitView::type, which every drawer reads through shownType) and this entry changes it back in the middle:
//   0 .. kFlashDur   the unit flashes white (the wash of drawStory); at the brightest moment the picture changes to
//                    the new type (sprite, glyph, 3D model alike)
//   then             the banner "Kell is now a Captain" for kBannerDur (it covers the middle of the screen, which is
//                    why the flash comes first, in the open)
//   then             the stat changes in the level-up box until Confirm (an AI side and autoplay: a short while).
// An automatic promotion is the same entry, right after its LevelUp one, so its box follows the level-up box.
#include <algorithm>
#include <cmath>

#include "battle/battle_view.h"
#include "battle/promotion_text.h"
#include "core/base/log.h"

namespace ote {
namespace {

constexpr float kFlashDur = 0.5f;     // the flash
constexpr float kSwapAt = 0.25f;      // when the picture changes
constexpr float kBannerDur = 0.9f;    // the banner after it
constexpr float kBoxAutoSecs = 1.2f;  // how long the box stays up when nobody confirms it

}  // namespace

void BattleView::pushPromotion(const Event& e) {
  if (e.type != EventType::UnitPromoted || e.a < 0 || (size_t)e.a >= units.size()) return;
  if (units[(size_t)e.a].type < 0) units[(size_t)e.a].type = e.b;
}

bool BattleView::beginPromotion(const Battle& b) {
  const Event& e = cur_.ev;
  if (e.type != EventType::UnitPromoted) return false;
  if (e.a < 0 || (size_t)e.a >= units.size() || (size_t)e.a >= b.units.size() || e.c < 0 || (size_t)e.c >= b.db->unitTypes.size()) {
    cur_.dur = 0;  // nothing sensible to show
    return true;
  }
  const Unit& u = b.units[(size_t)e.a];
  units[(size_t)e.a].type = e.b;  // (push() did this already unless the view was re-synced in between)
  OTE_INFO("promotion plays: %s, %s to %s, level %d", u.name.c_str(), b.db->unitTypes[(size_t)e.b].name.c_str(),
           b.db->unitTypes[(size_t)e.c].name.c_str(), e.d);
  cur_.dur = 1e9f;  // ends in progressPromotion: after the box was confirmed
  return true;
}

bool BattleView::progressPromotion(const Battle& b) {
  const Event& e = cur_.ev;
  if (e.type != EventType::UnitPromoted || e.a < 0 || (size_t)e.a >= units.size() || (size_t)e.a >= b.units.size()) return false;
  UnitView& v = units[(size_t)e.a];
  const Unit& u = b.units[(size_t)e.a];
  hasFocus = true;
  focusX = v.px + tile_ / 2.0f;
  focusY = v.py + tile_ / 2.0f;

  if (cur_.t < kFlashDur) {
    const float k = cur_.t / kSwapAt;  // 0..1 up to the swap, then back down
    v.flash = cur_.t < kSwapAt ? k : std::max(0.0f, 1.0f - (cur_.t - kSwapAt) / (kFlashDur - kSwapAt));
    if (cur_.t >= kSwapAt && v.type >= 0) {
      v.type = -1;  // the new picture
      v.hp = u.hp;  // a better HP stat may have healed it
    }
    return true;
  }
  if (cur_.t < kFlashDur + kBannerDur) {
    if (banner.empty()) {
      v.flash = 0;
      banner = promotionBannerText(u.name, b.db->unitTypes[(size_t)e.c].name);
      bannerSub.clear();
      bannerColor = Color(255, 200, 70);
    }
    return true;
  }
  if (!cur_.fired) {  // the first frame after the banner: banner away, stat changes up
    cur_.fired = true;
    banner.clear();
    levelUp.active = true;
    levelUp.timer = 0;
    levelUp.autoContinue = autoplay_ || b.sides[u.side].ai;
    levelUp.title = "Stat changes";
    levelUp.lines.clear();
    for (size_t s = 0; s < e.data.size() && s < b.db->stats.size(); ++s)
      if (e.data[s] != 0) levelUp.lines.push_back((e.data[s] > 0 ? "+" : "") + std::to_string(e.data[s]) + " " + b.db->stats[s].name);
    if (levelUp.lines.empty()) levelUp.lines.push_back("No stat changes");
  }
  levelUp.timer = cur_.t - kFlashDur - kBannerDur;
  if (levelUp.autoContinue && levelUp.timer >= kBoxAutoSecs) cur_.dur = 0;  // ends this frame
  return true;
}

bool BattleView::finishPromotion(const Battle& b) {
  const Event& e = cur_.ev;
  if (e.type != EventType::UnitPromoted) return false;
  if (e.a >= 0 && (size_t)e.a < units.size()) {
    UnitView& v = units[(size_t)e.a];
    v.type = -1;
    v.flash = 0;
    if ((size_t)e.a < b.units.size()) v.hp = b.units[(size_t)e.a].hp;
  }
  banner.clear();
  levelUp.active = false;
  return true;
}

}  // namespace ote
