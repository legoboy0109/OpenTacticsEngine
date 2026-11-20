// The scene's side of the enemy-range display (battle/enemy_ranges.h, docs/SRPG.md "Enemy ranges"): marking an enemy, the "show all"
// toggle, keeping the cached areas current and handing them to the map drawing. Display only: nothing here touches the Battle.
#include "battle/battle_scene.h"
#include "core/base/log.h"
#include "sound/cue_sink.h"

namespace ote {

int BattleScene::viewTeam() const { return battle_.sides[(size_t)battle_.activeSide].team; }

bool BattleScene::rangesOn() const { return host_.enemyRanges() > 0 && !prep_; }

// Called every frame the player is in charge and the view is at rest; does the work only when the board, the marks or the
// settings changed (EnemyRanges::update).
void BattleScene::refreshRanges() {
  ranges_.setStyle(host_.enemyRanges());
  ranges_.update(battle_, viewTeam());
}

// Confirm on a visible enemy: mark it, or unmark it.
bool BattleScene::markEnemy(int unit) {
  bool now = false;
  if (!rangesOn() || !ranges_.toggleMark(battle_, viewTeam(), unit, now)) return false;
  uiCue(Cue::Confirm);
  const std::string& name = battle_.units[(size_t)unit].name;
  OTE_INFO("enemy mark: %s %s", name.c_str(), now ? "on" : "off");
  say(now ? "Showing the reach of " + name : "Reach of " + name + " hidden");
  refreshRanges();  // (so the log line and the picture are for this very frame)
  return true;
}

// The Ranges button / the map menu entry: every enemy that can be seen, or none of the "all" part.
void BattleScene::toggleAllRanges() {
  if (!rangesOn()) return;
  ranges_.toggleAll();
  uiCue(Cue::Confirm);
  OTE_INFO("enemy ranges all: %s", ranges_.all() ? "on" : "off");
  say(ranges_.all() ? "Showing the reach of every enemy in sight" : "Enemy ranges hidden");
  refreshRanges();
}

// Cancel on the plain map: hide whatever the display shows (marks and "all"). False when there was nothing to clear.
bool BattleScene::clearRanges() {
  if (!ranges_.all() && ranges_.unitCount() == 0) return false;
  ranges_.clearMarks();
  OTE_INFO("enemy ranges cleared");
  refreshRanges();
  return true;
}

// The areas go to the map drawing next to the player's own highlight; the picture shows them only while the player can act on the
// board it draws (the view at rest, the player's turn, no menu or panel over the map).
void BattleScene::addRangeTint(Overlay& ov) const {
  if (!humanTurn() || view_.busy() || prep_) return;
  if (mode_ != Mode::Idle && mode_ != Mode::Selected && mode_ != Mode::ActionMenu && mode_ != Mode::Targeting) return;
  ov.foeMove = ranges_.moveMask();
  ov.foeAttack = ranges_.attackMask();
}

}  // namespace ote
