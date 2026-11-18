// Sounds that follow the clock while a box is on screen (docs/SOUND.md "Level-up ticks" and "Promotion sounds"):
//   - the level-up box asks for one `levelUpTick` per stat line that rose, kTickGap seconds apart, so each is heard (the audio service
//     plays one identical sound per frame at most);
//   - a promotion starts the game's `promotionSounds` list: each sound plays `atMs` after the promotion began, next to the promotion jingle.
// Both run from BattleView::update, whatever the entry on screen is, and only count entries the viewer saw (the entry has to begin).
#include "battle/battle_view.h"
#include "core/base/log.h"
#include "sound/game_audio.h"

namespace ote {
namespace {
constexpr float kTickGap = 0.12f;  // seconds between two level-up ticks
}

// The number of stats that rose in a level-up entry (a stat at its cap that did not rise is not counted).
int BattleView::risenStats(const Event& e) const {
  int n = 0;
  for (int v : e.data) n += v > 0 ? 1 : 0;
  return n;
}

void BattleView::startPromotionSounds() {
  if (!audio_ || audio_->data().promotionSounds.empty()) return;
  promoSoundClock_ = 0;
  promoSoundNext_ = 0;
}

void BattleView::tickSounds(float dt) {
  if (!audio_) return;
  if (levelUp.ticks > 0 && levelUp.active) {
    levelUp.tickClock -= dt;
    if (levelUp.tickClock <= 0) {
      audio_->cue(Cue::LevelUpTick);
      OTE_INFO("level-up tick %d", levelUp.ticksSent + 1);
      ++levelUp.ticksSent;
      --levelUp.ticks;
      levelUp.tickClock = kTickGap;
    }
  }
  if (promoSoundClock_ >= 0) {
    const std::vector<std::pair<int, int>>& plan = audio_->data().promotionSounds;
    promoSoundClock_ += dt;
    while (promoSoundNext_ < plan.size() && (float)plan[promoSoundNext_].first <= promoSoundClock_ * 1000.0f + 0.5f) {
      audio_->soundIndex(plan[promoSoundNext_].second);
      OTE_INFO("promotion sound at %d ms", plan[promoSoundNext_].first);
      ++promoSoundNext_;
    }
    if (promoSoundNext_ >= plan.size()) promoSoundClock_ = -1;
  }
}

}  // namespace ote
