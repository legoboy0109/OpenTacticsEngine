#include "game_session.h"

#include "core/battle/save.h"
#include "core/base/log.h"
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
  battle.setAskChoices(askChoices);  // before init / start: a question may come with the first events
  startEvents.clear();
  campaign.reset();  // a free battle is not part of a playthrough
  if (!loadMap(db, mapId, map, err)) return false;
  if (!battle.init(db, map, formulas, seed, err)) return false;
  battle.start(startEvents);
  return true;
}

bool GameSession::resume(const std::string& mapId, const Json& state, std::string& err) {
  MapDef newMap;
  Battle fresh;
  fresh.setAskChoices(askChoices);
  if (!loadMap(db, mapId, newMap, err)) return false;
  if (!fresh.init(db, newMap, formulas, 1, err)) return false;
  std::string detail;
  if (!restoreBattleState(fresh, state, detail)) {
    OTE_WARN("save rejected: %s", detail.c_str());  // the exact field, for whoever fixes it
    err = "The save is damaged (" + detail + ")";
    return false;
  }
  map = newMap;
  battle = fresh;
  startEvents.clear();
  campaign.reset();  // a plain save is a free battle
  return true;
}

}  // namespace ote
