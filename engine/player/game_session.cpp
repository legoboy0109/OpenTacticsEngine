#include "game_session.h"

#include "core/battle/save.h"
#include "core/base/log.h"
#include "core/campaign/prep.h"
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
  battle.setDifficulty(db.difficulties.find(difficulty));  // docs/DIFFICULTY.md
  startEvents.clear();
  campaign.reset();  // a free battle is not part of a playthrough
  if (!loadMap(db, mapId, map, err)) return false;
  if (map.prep) {  // no screen on this path (autoplay, the simulator's rules): the default pick, docs/EVENTS.md "Preparation"
    Playthrough scratch;
    MapDef picked;
    prepApplyToMap(db, scratch, map, picked);
    map = picked;
  }
  if (!battle.init(db, map, formulas, seed, err)) return false;
  battle.start(startEvents);
  return true;
}

bool GameSession::rebuildFreePrep(unsigned seed, std::string& err) {
  MapDef picked;
  prepApplyToMap(db, *freePrep, map, picked);
  battle = Battle();
  battle.setAskChoices(askChoices);
  battle.setDifficulty(db.difficulties.find(difficulty));
  return battle.init(db, picked, formulas, seed, err);
}

bool GameSession::startMapPrep(const std::string& mapId, unsigned seed, std::string& err) {
  battle = Battle();
  startEvents.clear();
  campaign.reset();
  if (!loadMap(db, mapId, map, err)) return false;
  freePrep.reset(new Playthrough());
  prepOpen(db, *freePrep, map, false);
  return rebuildFreePrep(seed, err);
}

bool GameSession::startPreparedMap(unsigned seed, std::string& err) {
  std::string why;
  if (!prepBegin(db, *freePrep, map, why)) {
    err = why;
    return false;
  }
  if (!rebuildFreePrep(seed, err)) return false;
  startEvents.clear();
  battle.start(startEvents);
  freePrep.reset();
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
