#include "game_session.h"

#include "core/data/loader.h"

namespace ote {

bool GameSession::load(const std::string& gameDir, std::string& err) {
  dir = gameDir;
  db = Database();
  if (!loadGame(gameDir, db, err)) return false;
  // Formulas::init keeps a pointer to db, which is a member of this object.
  return formulas.init(db, err);
}

bool GameSession::startMap(const std::string& mapId, unsigned seed, std::string& err) {
  battle = Battle();
  startEvents.clear();
  if (!loadMap(db, mapId, map, err)) return false;
  if (!battle.init(db, map, formulas, seed, err)) return false;
  battle.start(startEvents);
  return true;
}

}  // namespace ote
