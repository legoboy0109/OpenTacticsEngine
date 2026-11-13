// Scene interface and the services the App offers to scenes.
#pragma once
#include <string>

#include "args.h"
#include "game_session.h"
#include "platform/platform.h"

namespace ote {

class Scene {
 public:
  virtual ~Scene() {}
  virtual void onInput(const InputEvent& e) = 0;
  virtual void update(double dt) {}
  virtual void draw(Renderer& r) = 0;
};

// Scene switches are deferred by the App (a scene must not be destroyed while
// its own method is running), so scenes just call these and return.
class SceneHost {
 public:
  virtual ~SceneHost() {}
  virtual const Args& args() const = 0;
  virtual GameSession* session() = 0;  // null until a game is loaded
  virtual Renderer& renderer() = 0;     // for measuring text outside draw()
  virtual void showGameSelect() = 0;
  virtual void showMapSelect() = 0;
  virtual void loadGame(const std::string& gameId) = 0;   // then map select / battle
  virtual void startBattle(const std::string& mapId) = 0;
  // Error screen; Cancel/Confirm goes to `back`.
  enum Back { ToGameSelect, ToMapSelect };
  virtual void showError(const std::string& title, const std::string& message, Back back) = 0;
};

}  // namespace ote
