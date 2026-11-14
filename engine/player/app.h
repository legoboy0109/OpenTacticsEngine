// The player App: owns the current scene and game session, switches scenes,
// applies the user's options (font, scale, style) and injects scripted input
// for automated tests.
#pragma once
#include <functional>
#include <memory>
#include <vector>

#include "options/effective.h"
#include "platform/common/font_registry.h"
#include "scene.h"
#include "script_input.h"

namespace ote {

class PlayerApp : public App, public SceneHost {
 public:
  explicit PlayerApp(const Args& args);
  ~PlayerApp() override;
  // Validates the command line (bad --input script -> false, message in err).
  // The first scene is chosen later, in onStart, once a Renderer exists.
  bool start(std::string& err);

  // App
  void onInput(const InputEvent& e) override;
  void update(double dt) override;
  void draw(Renderer& r) override;
  void onStart(Renderer& r) override;
  bool wantsQuit() const override { return quit_; }

  // SceneHost
  const Args& args() const override { return args_; }
  GameSession* session() override { return session_.get(); }
  const Sprites* sprites() const override { return session_ && sprites_.any() ? &sprites_ : nullptr; }
  IsoTiles* isoTiles() override { return &isoTiles_; }
  Renderer3D& renderer3d() override;
  Show3D& show3d() override { return show3d_; }
  Renderer& renderer() override { return *renderer_; }
  Settings& settings() override { return settings_; }
  const std::vector<GameEntry>& games() const override { return games_; }
  const std::string& notice() const override { return notice_; }
  const std::string& currentGameId() const override { return gameId_; }
  int styleIndex() const override { return look_.styleIndex; }
  SaveStore saveStore(const std::string& gameId) override;
  bool resumeSave(int slot, std::string& err) override;
  void showSlots(const std::string& gameId) override;
  void showHome() override;
  void showGame(const std::string& gameId) override;
  void showOptions(const std::string& gameId) override;
  void showMapSelect() override;
  void loadGame(const std::string& gameId) override;
  void startBattle(const std::string& mapId) override;
  void relook(const std::string& gameId) override;
  void quit() override { quit_ = true; flushSettings(); }
  void showError(const std::string& title, const std::string& message, Back back) override;

 private:
  void later(std::function<void()> fn) { pending_ = std::move(fn); }
  void runPending();
  void dropSession();
  void doLoadGame(const std::string& gameId);
  bool loadSession(const std::string& gameId, std::string& err);  // session_ + art; no scene, no look
  void applyGameLook();
  // app_saves.cpp
  void findSavesFolder();
  void startFromSave();  // --continue / --load-slot  // logical size, font and scale of the loaded game
  void doStartBattle(const std::string& mapId);

  // app_look.cpp: settings file and the look derived from the options.
  void loadSettings();
  void flushSettings();  // writes the settings file if anything changed
  // Resolves options + command line + the game's defaults and applies the
  // result (logical size, font, scale). gameId empty = launcher screens.
  void applyLook(const std::string& gameId, const GameLook& game, int logicalW, int logicalH);
  void applyLauncherLook() { applyLook("", GameLook(), 480, 270); }

  Args args_;
  Renderer* renderer_ = nullptr;
  Settings settings_;
  std::string settingsPath_;  // empty = in-memory only
  bool saveWarned_ = false;
  std::string savesRoot_;  // folder holding <game>/continue.json and slotN.json; empty = no saving this run
  std::string notice_;
  FontRequest appliedFont_;
  bool fontApplied_ = false;
  EffectiveLook look_;
  std::unique_ptr<GameSession> session_;
  IsoTiles isoTiles_;  // released with sprites_
  Sprites sprites_;  // textures of session_'s art sheets; released whenever session_ goes
  std::unique_ptr<Renderer3D> r3d_;  // before scene_: scenes free their 3D things when they end
  Show3D show3d_;                    // released with sprites_
  std::unique_ptr<Scene> scene_;
  std::vector<GameEntry> games_;
  std::string gameId_;
  std::function<void()> pending_;
  ScriptInput script_;
  bool quit_ = false;
  bool firstLoad_ = true;  // --map applies only to the first game load
};

}  // namespace ote
