// Difficulties in the player App (docs/DIFFICULTY.md): the choice screen before a new campaign or a free battle, and the
// --difficulty command-line option.
#include "app.h"
#include "core/base/log.h"
#include "scene_difficulty.h"

namespace ote {

// True when the choice screen is due: the game has more than one difficulty and nothing answers for the person (a
// --difficulty on the command line; --autoplay, which has nobody to ask and takes the default).
bool PlayerApp::difficultyNeeded() const {
  return session_ && session_->db.difficulties.list.size() > 1 && args_.difficulty.empty() && !args_.autoplay;
}

// Shows the choice. The pick (and Cancel, which goes back to the game's page) arrive through later(), so nothing runs
// while the screen is still on the stack.
void PlayerApp::askDifficulty(const std::string& title, std::function<void(const std::string&)> done) {
  scene_.reset();
  scene_.reset(new DifficultyScene(
      *this, title, [this, done](const std::string& id) { later([done, id] { done(id); }); },
      [this] { later([this] { doShowGame(gameId_); }); }));
}

// --difficulty must name one of the game's difficulties. A game without any refuses it too, so a typo or a wrong game
// in a test run does not pass silently.
bool PlayerApp::checkDifficultyArg(std::string& err) const {
  if (args_.difficulty.empty() || session_->db.difficulties.find(args_.difficulty) >= 0) return true;
  const DifficultyRules& rules = session_->db.difficulties;
  if (!rules.any()) {
    err = "--difficulty " + args_.difficulty + ": this game has no difficulties (game.json \"difficulties\")";
  } else {
    std::string ids;
    for (const DifficultyDef& d : rules.list) ids += (ids.empty() ? "" : ", ") + d.id;
    err = "--difficulty " + args_.difficulty + ": no such difficulty (this game has: " + ids + ")";
  }
  OTE_ERROR("%s", err.c_str());
  return false;
}

}  // namespace ote
