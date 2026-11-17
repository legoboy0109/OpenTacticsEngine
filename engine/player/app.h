// The player App: owns the current scene and game session, switches scenes,
// applies the user's options (font, scale, style) and injects scripted input
// for automated tests.
#pragma once
#include <functional>
#include <memory>
#include <vector>

#include "campaign/story_scene.h"
#include "options/effective.h"
#include "platform/common/font_registry.h"
#include "scene.h"
#include "script_input.h"
#include "sound/audio_service.h"

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
  void onStop() override { audio_.shutdown(); }
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
  AudioService& audio() override { return audio_; }
  GameAudio& gameAudio() override { return gameAudio_; }
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
  void startCampaign(const std::string& gameId) override;
  void startFreeBattle(const std::string& gameId, const std::string& mapId) override;
  void campaignNext() override;
  void campaignSceneDone() override;
  void campaignBattleOver(ChapterResult result) override;
  void prepareDone() override;
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
  void doShowGame(const std::string& gameId);
  bool openGame(const std::string& gameId);  // session + look of a game, without a scene; false = error screen is up
  // app_campaign.cpp: the story flow (docs/EVENTS.md "Campaign")
  void doStartCampaign();
  void doCampaignNext();
  void saveAfterScene();
  SceneResolver runSceneFor(SceneKind kind, const Playthrough& before, std::vector<SceneLine>& lines);
  bool resumeCampaignSave(const SaveFile& file, std::string& err);

  // app_audio.cpp: choosing the sound device (docs/SOUND.md "Engine design")
  void startAudio();
  void openAudioDevice();
  void syncAudio();  // a new scene is on screen: load the game's sound data, pick its music

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
  AudioService audio_;
  GameAudio gameAudio_{audio_};
  GameAudio launcherAudio_{audio_};  // <assets>/launcher: the menu sounds while no game is open (never any music)
  bool launcherAudioTried_ = false;  // loaded once, on the first scene; a failure is one warning, then silence
  GameAudioCues cues_{gameAudio_, launcherAudio_, gameId_};  // installed as the sink of uiCue() in onStart
  unsigned audioScene_ = 0;         // Scene::audioId() of the scene the music was last told about
  std::string audioFailed_;         // a game whose audio.json could not be loaded for its page (warned once)
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
  std::string made3d_;  // the "3D graphics" choice r3d_ was made for
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
