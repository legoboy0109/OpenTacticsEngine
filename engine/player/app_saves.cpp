// Save files in the player App: where they live, loading one, the launcher's
// slot list (docs/SAVES.md).
#include "app.h"
#include "battle/battle_scene.h"
#include "core/base/log.h"
#include "platform/platform.h"
#include "scene_home.h"
#include "scene_slots.h"

namespace ote {

// Saving is off for a run when asked (--no-saving) or when it is a test run:
// headless and --no-settings runs (the editor's preview) must never touch the
// player's real saves, so they save only into a folder given with --saves.
void PlayerApp::findSavesFolder() {
  savesRoot_.clear();
  if (args_.noSaving) return;
  if (!args_.saves.empty()) {
    savesRoot_ = args_.saves;
    while (savesRoot_.size() > 1 && (savesRoot_.back() == '/' || savesRoot_.back() == '\\')) savesRoot_.pop_back();
    return;
  }
  if (args_.headless || args_.noSettings) return;
  std::string dir = userDataDir();
  if (dir.empty()) {
    OTE_WARN("no user data folder: battles cannot be saved");
    return;
  }
  savesRoot_ = dir + "/saves";
}

SaveStore PlayerApp::saveStore(const std::string& gameId) {
  if (savesRoot_.empty() || gameId.empty()) return SaveStore();
  return SaveStore(savesRoot_ + "/" + gameId);
}

bool PlayerApp::resumeSave(int slot, std::string& err) {
  if (gameId_.empty()) {
    err = "No game is open";
    return false;
  }
  SaveStore store = saveStore(gameId_);
  if (!store.enabled()) {
    err = "Saving is not available";
    return false;
  }
  const bool hadSession = session_ != nullptr;
  if (!hadSession && !loadSession(gameId_, err)) return false;
  auto fail = [&]() {
    if (!hadSession) dropSession();  // leave things as the launcher had them
    return false;
  };
  const Database& db = session_->db;
  if (db.saving == SavingMode::Off) {
    err = "This game does not use saves";
    return fail();
  }
  if (slot > 0 && db.saving != SavingMode::Any) {
    err = "This game only keeps its automatic Continue save";
    return fail();
  }
  SaveFile file;
  if (!store.read(slot, db, gameId_, file, err)) return fail();
  const std::string which = slot == 0 ? "the Continue save" : "slot " + std::to_string(slot);
  if (file.info.campaign) {
    if (!resumeCampaignSave(file, err)) return fail();
    OTE_INFO("resumed %s of '%s' (campaign, chapter %s, %s)", which.c_str(), gameId_.c_str(),
             file.info.chapterName.c_str(), file.info.phase.c_str());
    later([this] {
      scene_.reset();
      applyGameLook();
      doCampaignNext();  // by phase: a scene, the battle (resumed or started), or an end screen
    });
    return true;
  }
  if (!session_->resume(file.mapId, file.battle, err)) return fail();
  OTE_INFO("resumed %s of '%s' (map %s, turn %d)", which.c_str(), gameId_.c_str(), file.mapId.c_str(), file.info.round);
  later([this] {
    scene_.reset();  // the old battle (if any) goes first; the new scene rebuilds its view from the state
    applyGameLook();
    scene_.reset(new BattleScene(*this));
  });
  return true;
}

void PlayerApp::showSlots(const std::string& gameId) {
  later([this, gameId] {
    const GameEntry* entry = nullptr;
    for (const GameEntry& g : games_)
      if (g.id == gameId) entry = &g;
    scene_.reset();
    dropSession();
    applyLauncherLook();
    if (!entry) { gameId_.clear(); scene_.reset(new HomeScene(*this)); return; }
    gameId_ = gameId;
    scene_.reset(new SlotScene(*this, *entry));
  });
}

// --game X --continue / --load-slot N: straight into the saved battle.
void PlayerApp::startFromSave() {
  gameId_ = args_.game;
  int slot = args_.continueSave ? 0 : args_.loadSlot;
  std::string err;
  if (!resumeSave(slot, err)) {
    applyLauncherLook();
    showError(slot == 0 ? "Cannot continue" : "Cannot load slot " + std::to_string(slot), err, ToGame);
  }
  runPending();
}

}  // namespace ote
