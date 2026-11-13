// The player App: owns the current scene and game session, switches scenes,
// and injects scripted input for automated tests.
#pragma once
#include <functional>
#include <memory>
#include <vector>

#include "scene.h"
#include "scene_select.h"
#include "script_input.h"

namespace ote {

class PlayerApp : public App, public SceneHost {
 public:
  explicit PlayerApp(const Args& args);
  // Picks the first scene. Returns false (message in err) on a bad --input script.
  bool start(std::string& err);

  // App
  void onInput(const InputEvent& e) override;
  void update(double dt) override;
  void draw(Renderer& r) override;
  bool wantsQuit() const override { return quit_; }

  // SceneHost
  const Args& args() const override { return args_; }
  GameSession* session() override { return session_.get(); }
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

  Args args_;
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
