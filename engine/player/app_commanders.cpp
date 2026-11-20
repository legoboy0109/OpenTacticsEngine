// Commanders in the player App (docs/COMMANDERS.md "Player"): the choice screen a free battle shows for each person-controlled side whose map says
// "commander": "choose", right after the faction screens (the faction's "commanders" list limits what the side may pick). Same shape as app_factions.cpp.
#include "app.h"
#include "core/base/log.h"
#include "core/data/loader.h"
#include "scene_commander.h"

namespace ote {

// Shows the choice for the first side still to pick, or returns false when none is left (or nobody is there to ask: --autoplay takes the engine's
// default pick). The pick (and Cancel) arrive through later(), so nothing runs while the screen is still on the stack; the pick goes into the session
// and the battle start is tried again.
bool PlayerApp::askCommanderIfNeeded(const std::string& mapId) {
  if (!session_ || args_.autoplay || session_->db.commanders.empty()) return false;
  std::string err;
  MapDef peek;
  if (!loadMap(session_->db, mapId, peek, err)) return false;  // the start reports that
  const int side = session_->nextCommanderPick(peek);
  if (side < 0) return false;
  const std::string name = peek.sides[(size_t)side].name.empty() ? peek.sides[(size_t)side].id : peek.sides[(size_t)side].name;
  scene_.reset();
  scene_.reset(new CommanderScene(
      *this, name, session_->commanderChoices(peek, side),
      [this, mapId, side](int commander) {
        later([this, mapId, side, commander] {
          if (!session_) return;
          if ((int)session_->commanderPicks.size() <= side) session_->commanderPicks.resize((size_t)side + 1, -1);
          session_->commanderPicks[(size_t)side] = commander;
          doStartBattle(mapId);  // the next side's pick, or the battle
        });
      },
      [this] { later([this] { doShowGame(gameId_); }); }));
  return true;
}

}  // namespace ote
