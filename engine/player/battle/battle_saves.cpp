// Saving inside the battle scene (contract: docs/SAVES.md).
//   * Continue save: written at the start of every turn and after every
//     completed action of a human player, once the view has finished playing
//     it; deleted when the battle ends.
//   * Manual slots: Save / Load in the End Turn menu (game setting "any").
// A failed write never interrupts play: one notice says saving is not working.
#include "battle/battle_scene.h"
#include "campaign/campaign_logic.h"
#include "core/base/log.h"
#include "sound/cue_sink.h"

namespace ote {

bool BattleScene::savingOn() const {
  return store_.enabled() && session_.db.saving != SavingMode::Off && !autoplay_;
}

// The Continue file is the playthrough's. A free battle of a campaign game never writes or deletes it
// (docs/SAVES.md): it could not tell whose position it holds.
bool BattleScene::continueOn() const {
  return continueIsWritten(savingOn(), session_.db.campaign != nullptr, chapterBattle());
}

bool BattleScene::buildSave(Json& file, std::string& err) const {
  if (chapterBattle())
    return buildCampaignSave(session_.db, host_.currentGameId(), session_.campaign->pt, prep_ ? nullptr : &battle_, nowSeconds(), file, err);  // a preparation has no battle in the file yet
  return buildSaveFile(battle_, session_.db, host_.currentGameId(), session_.map.id, nowSeconds(), file, err);
}

bool BattleScene::manualSaves() const { return savingOn() && session_.db.saving == SavingMode::Any; }

void BattleScene::say(const std::string& text) {
  notice_ = text;
  noticeTime_ = 6.0f;
}

void BattleScene::saveFailed(const std::string& err) {
  OTE_WARN("saving failed: %s", err.c_str());
  if (saveFailWarned_) return;
  saveFailWarned_ = true;
  say("Saving is not working - progress may be lost");
}

void BattleScene::writeContinue() {
  Json file;
  std::string err;
  if (!buildSave(file, err) || !store_.write(0, file, err)) saveFailed(err);
}

// The app is going to the background and may be killed there: the same write as "Leave battle" (the very latest position,
// including a unit that has only moved). No new file kind; a decided battle writes nothing (its Continue file is gone or the
// story flow owns it), exactly as updateSaves() does.
void BattleScene::onSuspend() {
  if (!continueOn() || mode_ == Mode::Result || battle_.over) return;
  writeContinue();
  OTE_INFO("lifecycle: Continue save written");
}

void BattleScene::clearContinue() {
  if (continueCleared_ || !continueOn()) return;
  continueCleared_ = true;
  std::string err;
  if (!store_.remove(0, err)) saveFailed(err);
}

// Called every frame the view is idle.
void BattleScene::updateSaves() {
  if (!continueOn() || mode_ == Mode::Result || prep_) return;
  if (battle_.over) {  // a chapter battle's result is handled by chapterDecided() (the story goes on)
    if (!chapterBattle()) clearContinue();
    return;
  }
  long long turn = (long long)battle_.round * 1024 + battle_.activeSide;
  if (turn != savedTurn_) {  // a new turn has begun (either side)
    savedTurn_ = turn;
    dirty_ = true;
  }
  if (!dirty_) return;
  dirty_ = false;
  writeContinue();
}

void BattleScene::openSlotList(SlotPanel::Mode mode) {
  std::vector<SaveInfo> infos;
  for (int s = 1; s <= SaveStore::kSlots; ++s) infos.push_back(store_.inspect(s, &session_.db, host_.currentGameId()));
  slots_.open(mode, infos, "You will leave the current battle.");
  mode_ = Mode::SlotList;
  menu_.close();
}

void BattleScene::slotInput(const InputEvent& e) {
  Renderer& r = host_.renderer();
  slots_.fitScreen(r);
  SlotPanel::Result res = slots_.onInput(e, r);
  if (res == SlotPanel::Back) {
    slots_.close();
    if (prep_) prepMainMenu();
    else mode_ = Mode::Idle;
  } else if (res == SlotPanel::Picked && slots_.mode() == SlotPanel::Save) {
    Json file;
    std::string err;
    int slot = slots_.slot();
    if (buildSave(file, err) && store_.write(slot, file, err)) {
      slots_.close();
      if (prep_) prepMainMenu();
      else mode_ = Mode::Idle;
      uiCue(Cue::Save);  // the game's "save" sound
      say("Saved to slot " + std::to_string(slot));
    } else {
      OTE_WARN("saving to slot %d failed: %s", slot, err.c_str());
      slots_.showMessage("Could not save to slot " + std::to_string(slot) + ": " + err);
    }
  } else if (res == SlotPanel::Picked) {
    std::string err;
    // On success the whole scene is replaced after this returns; touch nothing more.
    if (!host_.resumeSave(slots_.slot(), err)) slots_.showMessage("Cannot load this save: " + err);
  }
}

}  // namespace ote
