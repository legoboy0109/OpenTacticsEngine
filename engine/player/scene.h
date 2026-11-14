// Scene interface and the services the App offers to scenes.
#pragma once
#include <string>
#include <vector>

#include "args.h"
#include "core/data/settings.h"
#include "game_library.h"
#include "game_session.h"
#include "platform/platform.h"
#include "battle/sprites.h"
#include "saves.h"

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
  virtual const Sprites* sprites() const = 0;  // art textures of the loaded game (null = none)
  virtual Renderer& renderer() = 0;     // for measuring text outside draw()
  virtual Settings& settings() = 0;     // options + remembered UI state (saved by the App)
  virtual const std::vector<GameEntry>& games() const = 0;
  virtual const std::string& notice() const = 0;  // one-line problem to show on the home screen, or ""
  virtual const std::string& currentGameId() const = 0;  // last game opened, or ""
  // Index of the visual style in the look last applied (0 = the game's default;
  // meaningful for the game being played or edited in the options).
  virtual int styleIndex() const = 0;

  // Save files of a game (docs/SAVES.md). Disabled (enabled() false) when saving is
  // off for this run: --no-saving, or a headless / --no-settings run without --saves,
  // or no user data folder. The game's own "saving" setting is checked by the scenes.
  virtual SaveStore saveStore(const std::string& gameId) = 0;
  // Loads save `slot` (0 = Continue) of currentGameId() and starts playing it. Checks
  // everything first; on failure returns false with a plain sentence and changes
  // nothing. On success the battle scene replaces the current scene after the
  // caller returns, so the caller must not use the host any further.
  virtual bool resumeSave(int slot, std::string& err) = 0;

  virtual void showHome() = 0;
  virtual void showGame(const std::string& gameId) = 0;
  // Options of one game, or the global options when gameId is empty.
  virtual void showOptions(const std::string& gameId) = 0;
  virtual void showMapSelect() = 0;
  virtual void showSlots(const std::string& gameId) = 0;  // the launcher's Load list
  virtual void loadGame(const std::string& gameId) = 0;   // then map select / battle
  virtual void startBattle(const std::string& mapId) = 0;
  // Re-applies font/size/scale after an option changed (immediately, so the
  // options screen shows the result). gameId as for showOptions.
  virtual void relook(const std::string& gameId) = 0;
  virtual void quit() = 0;
  // Error screen; Cancel/Confirm goes to `back`.
  enum Back { ToHome, ToGame, ToMapSelect };
  virtual void showError(const std::string& title, const std::string& message, Back back) = 0;
};

}  // namespace ote
