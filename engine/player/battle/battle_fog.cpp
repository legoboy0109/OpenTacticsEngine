// Fog of war in the battle scene (docs/ECONOMY.md "Fog of war > Player"): who the screen is drawn for, the AI's
// unseen steps, and the hot-seat cover. How things are drawn is in the draw files; how events are played for the
// viewer is battle_view_fog.cpp.
//
// The viewer (a team) is: the team of the human side whose turn it is; during the AI's turns the team of the human
// side that played last. With no fog, with --show-all, with no human side (autoplay / all AI) there is no viewer at
// all and every picture is exactly what it was before fog existed.
#include <set>

#include "battle/battle_scene.h"
#include "core/ai/ai.h"
#include "core/base/log.h"

namespace ote {

void BattleScene::initFog() {
  if (!battle_.fogOn()) return;
  int firstHuman = -1;
  std::set<int> humanTeams;
  for (size_t s = 0; s < battle_.sides.size(); ++s)
    if (!battle_.sides[s].ai) {
      if (firstHuman < 0) firstHuman = (int)s;
      humanTeams.insert(battle_.sides[s].team);
    }
  if (host_.args().showAll) {
    OTE_INFO("fog: on, drawn without fog (--show-all)");
    return;
  }
  if (autoplay_ || firstHuman < 0) {
    OTE_INFO("fog: on, no human side: everyone sees everything");
    return;
  }
  fogActive_ = true;
  hotSeat_ = humanTeams.size() >= 2;
  const int side = humanTurn() ? battle_.activeSide : firstHuman;
  setViewerSide(side);
  if (hotSeat_ && humanTurn() && !prep_) raiseCover(side);  // a game that starts or resumes on a human turn starts with the cover
}

void BattleScene::setViewerSide(int side) {
  viewerSide_ = side;
  const int team = battle_.sides[(size_t)side].team;
  view_.setViewer(battle_, side, team);
  OTE_INFO("fog: viewer %s (team %d)%s", battle_.sides[(size_t)side].name.c_str(), team, hotSeat_ ? ", hot-seat" : "");
}

void BattleScene::raiseCover(int side) {
  covered_ = true;
  cover_.open(battle_.sides[(size_t)side].name, Color::rgb(battle_.sides[(size_t)side].color));
  OTE_INFO("fog: cover up: pass the device to %s", battle_.sides[(size_t)side].name.c_str());
}

// Called when a human turn is about to begin (the view is idle). True = the turn must wait (the cover went up).
bool BattleScene::fogTurnGate() {
  if (!fogActive_) return false;
  const int side = battle_.activeSide, team = battle_.sides[(size_t)side].team;
  if (hotSeat_ && team != coverTeam_) {
    raiseCover(side);
    return true;
  }
  if (side != viewerSide_) setViewerSide(side);  // another side of the same team: the same eyes
  return false;
}

// The next player confirmed: show the map for them.
void BattleScene::coverConfirmed() {
  covered_ = false;
  cover_.close();
  coverTeam_ = battle_.sides[(size_t)battle_.activeSide].team;
  if (battle_.activeSide != viewerSide_) setViewerSide(battle_.activeSide);
  OTE_INFO("fog: cover down: %s plays", battle_.sides[(size_t)battle_.activeSide].name.c_str());
}

// One step (or, under fog, a run of steps) of the AI. A step whose events the viewer cannot see costs no time at
// all, so the next one follows in the same frame: a long unseen turn must not take longer than a short one.
void BattleScene::aiStep() {
  if (aiTurnKey_ != battle_.round * 1000 + battle_.activeSide) {
    aiTurnKey_ = battle_.round * 1000 + battle_.activeSide;
    OTE_INFO("ai: %s acts, round %d t=%.2f", battle_.sides[(size_t)battle_.activeSide].name.c_str(), battle_.round, time_);
  }
  int budget = fogActive_ ? 256 : 1;
  do {
    events_.clear();
    aiTakeStep(battle_, events_);
    view_.push(events_);
    if (fogActive_) view_.flushUnseen(battle_);
  } while (--budget > 0 && !view_.busy() && !battle_.over && !humanTurn());
}

}  // namespace ote
