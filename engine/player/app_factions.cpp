// Factions in the player App (docs/ECONOMY.md "Factions"): the choice screen a free battle shows for each person-controlled side whose
// map says "faction": "choose". It is a minimal pick step of its own: the player has no battle setup screen except the optional
// preparation one, which only some maps have.
#include "app.h"
#include "core/base/log.h"
#include "core/data/loader.h"
#include "scene_faction.h"

namespace ote {

// Shows the choice for the first side still to pick, or returns false when none is left (or nobody is there to ask:
// --autoplay takes the engine's default pick). The pick (and Cancel) arrive through later(), so nothing runs while the screen is
// still on the stack; the pick goes into the session and the battle start is tried again.
bool PlayerApp::askFactionIfNeeded(const std::string& mapId) {
  if (!session_ || args_.autoplay || session_->db.factions.empty()) return false;
  std::string err;
  MapDef peek;
  if (!loadMap(session_->db, mapId, peek, err)) return false;  // the start reports that
  const int side = session_->nextFactionPick(peek);
  if (side < 0) return false;
  const std::string name = peek.sides[(size_t)side].name.empty() ? peek.sides[(size_t)side].id : peek.sides[(size_t)side].name;
  scene_.reset();
  scene_.reset(new FactionScene(
      *this, name,
      [this, mapId, side](int faction) {
        later([this, mapId, side, faction] {
          if (!session_) return;
          if ((int)session_->factionPicks.size() <= side) session_->factionPicks.resize((size_t)side + 1, -1);
          session_->factionPicks[(size_t)side] = faction;
          doStartBattle(mapId);  // the next side's pick, or the battle
        });
      },
      [this] { later([this] { doShowGame(gameId_); }); }));
  return true;
}

}  // namespace ote
