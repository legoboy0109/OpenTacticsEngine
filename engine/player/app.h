// The player App: owns the current scene and game session, switches scenes,
// and injects scripted input for automated tests.
#pragma once
#include <functional>
#include <memory>
#include <vector>

#include "platform/common/font_registry.h"
#include "scene.h"
#include "scene_select.h"
#include "script_input.h"

namespace ote {

class PlayerApp : public App, public SceneHost {
 public:
  explicit PlayerApp(const Args& args);
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
  Renderer& renderer() override { return *renderer_; }
  void showGameSelect() override;
  void showMapSelect() override;
  void loadGame(const std::string& gameId) override;
  void startBattle(const std::string& mapId) override;
  void showError(const std::string& title, const std::string& message, Back back) override;

 private:
  void later(std::function<void()> fn) { pending_ = std::move(fn); }
  void runPending();
  void readIndex();
  void doLoadGame(const std::string& gameId);
  void doStartBattle(const std::string& mapId);
  // Font and logical size: the game's own while one is loaded, defaults on the lists.
  void applyScreen(const Database* game);

  Args args_;
  Renderer* renderer_ = nullptr;
  FontRequest appliedFont_;
  bool fontApplied_ = false;
  std::unique_ptr<GameSession> session_;
  std::unique_ptr<Scene> scene_;
  std::vector<GameEntry> games_;
  std::string indexError_;
  std::function<void()> pending_;
  ScriptInput script_;
  bool quit_ = false;
  bool firstLoad_ = true;  // --map applies only to the first game load
};

}  // namespace ote
