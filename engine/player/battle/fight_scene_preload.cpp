// Asks the sheet cache for everything a starting fight draws (called at the end of FightScene::start), so the PNGs decode
// during the transition into the scene and not on the first frame an animation shows them (docs/GRAPHICS.md "Art memory").
// A sheet missed here is simply loaded when first drawn.
#include "battle/fight_scene.h"

namespace ote {

void FightScene::preloadArt() const {
  if (!env_.sprites) return;
  const Sprites& sp = *env_.sprites;
  const Database& db = *env_.battle->db;
  const BattleArt& art = db.graphics.battle;
  for (int s = 0; s < 2; ++s) {
    const Fighter& f = fighter_[s];
    if (f.set >= 0) {
      const BattleSet& set = art.sets[(size_t)f.set];
      const bool mirror = set.mirror && ((s == 1) != set.facesLeft);  // as drawn: the right-hand fighter faces left
      for (int a = 0; a < kBattleAnims; ++a) {
        const BattleAnim& an = set.anims[a];
        if (!an.any()) continue;
        const int own = an.sheetFor(f.sideId);
        sp.request(own, mirror);
        if (own != an.sheet && !sp.texture(own)) sp.request(an.sheet, mirror);  // (a side sheet that failed: the set's own is drawn)
      }
    }
    if (f.look) sp.request(f.look->sheet);
    if (platform_[s] >= 0) sp.request(art.platforms[(size_t)platform_[s]].sheet, art.stage.on && s == 1);  // the stage turns the left picture round
  }
  if (background_ >= 0) sp.request(art.backgrounds[(size_t)background_].sheet);
  for (const std::vector<int>& strike : effectsOfStrike_)
    for (int e : strike) {
      const BattleAnim& an = art.effects[(size_t)e].anim;
      for (size_t fr = 0; fr < an.frames.size(); ++fr) sp.request(an.sheetOfFrame((int)fr), true);  // an effect turns with the side it flies from
    }
}

}  // namespace ote
