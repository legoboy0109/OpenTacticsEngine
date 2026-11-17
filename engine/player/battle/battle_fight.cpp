// BattleScene's side of the side-view battle scene (docs/BATTLE_SCENE.md): when the view holds the run of events of a fight,
// open the scene on it; when the scene is done, give the run back to the view. The scene itself is fight_scene*.cpp.
#include "battle/battle_scene.h"
#include "core/base/log.h"

namespace ote {

void BattleScene::startFight() {
  FightScene::Env env;
  env.battle = &battle_;
  env.view = &view_;
  env.sprites = host_.sprites();
  env.styleIndex = styleIdx_;
  env.mapBackground = session_.map.battleBackground;
  env.audio = &host_.gameAudio();
  env.levelsGained = view_.fight.levelsGained;
  env.hpGained = view_.fight.hpGained;
  fightSerial_ = view_.fight.serial;
  if (!fightScene_.start(env, view_.fight.events)) {  // nothing to show after all: the map plays it
    OTE_WARN("battle scene: the fight has no strike to show; played on the map");
    view_.fightFinished(battle_);
  }
}

void BattleScene::updateFight(double dt) {
  if (view_.fight.active && (!fightScene_.active() || fightSerial_ != view_.fight.serial)) startFight();
  if (!fightScene_.active()) return;
  if (!view_.fight.active) {  // the view dropped the fight (a resync): close the scene without a result to hand back
    fightScene_.close();
    return;
  }
  fightScene_.update(dt);
  if (fightScene_.finished()) {
    fightScene_.close();
    view_.fightFinished(battle_);
  }
}

}  // namespace ote
