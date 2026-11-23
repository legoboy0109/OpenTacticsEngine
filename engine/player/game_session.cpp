#include "game_session.h"

#include "core/battle/save.h"
#include "core/base/log.h"
#include "core/campaign/free_supply.h"
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

int GameSession::nextFactionPick(const MapDef& m) const {
  for (size_t i = 0; i < m.sides.size(); ++i)
    if (m.sides[i].factionChoose && !m.sides[i].ai && (i >= factionPicks.size() || factionPicks[i] < 0)) return (int)i;
  return -1;
}

void GameSession::applyFactionPicks(MapDef& m) const {
  for (size_t i = 0; i < m.sides.size() && i < factionPicks.size(); ++i) {
    if (!m.sides[i].factionChoose || m.sides[i].ai || factionPicks[i] < 0 || (size_t)factionPicks[i] >= db.factions.size()) continue;
    m.sides[i].faction = factionPicks[i];
    m.sides[i].factionChoose = false;
    OTE_INFO("faction: side \"%s\" plays %s", m.sides[i].id.c_str(), db.factions[(size_t)factionPicks[i]].id.c_str());
  }
}

bool GameSession::startMap(const std::string& mapId, unsigned seed, std::string& err) {
  battle = Battle();
  battle.setAskChoices(askChoices);  // before init / start: a question may come with the first events
  battle.setDifficulty(db.difficulties.find(difficulty));  // docs/DIFFICULTY.md
  battle.setTestLevel(testLevel);
  startEvents.clear();
  campaign.reset();  // a free battle is not part of a playthrough
  if (!loadMap(db, mapId, map, err)) return false;
  applyFactionPicks(map);  // the person's choices of the faction screen
  applyCommanderPicks(map);  // ... and of the commander screen (docs/COMMANDERS.md)
  applyFreeSupply(db, map);  // rules.items.freeBattleStorage: a free battle's stocked supply (never a campaign's)
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
  battle.setTestLevel(testLevel);
  return battle.init(db, picked, formulas, seed, err);
}

bool GameSession::startMapPrep(const std::string& mapId, unsigned seed, std::string& err) {
  battle = Battle();
  startEvents.clear();
  campaign.reset();
  if (!loadMap(db, mapId, map, err)) return false;
  applyFactionPicks(map);
  applyCommanderPicks(map);
  applyFreeSupply(db, map);  // before the screen folds the prep side's allowance into what it shows
  freePrep.reset(new Playthrough());
  prepOpenFree(db, *freePrep, map);
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
  battle.setBeforePrepSeen(true);  // the screen was shown after the `beforePrep` events: they run again, silently, on the final deployment
  battle.start(startEvents);
  std::string log;  // what the preparation handed over: each human named unit's pack, the storage and the funds
  for (const Unit& u : battle.units) {
    if (u.storyId.empty() || battle.sides[(size_t)u.side].ai) continue;
    log += " " + u.storyId + "[";
    for (size_t i = 0; i < u.items.size(); ++i) log += (i ? "," : "") + db.items[(size_t)u.items[i].item].id;
    log += "]";
  }
  const SideState& mine = battle.sides[(size_t)map.prep->side];
  OTE_INFO("free battle starts:%s storage=%d funds=%d", log.c_str(), (int)mine.storage.size(), mine.resources.empty() ? 0 : mine.resources[0]);
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
