// The whole save file of a campaign game: header fields, hash, playthrough and battle.
#include "core/battle/save.h"
#include "core/battle/save_hash.h"
#include "core/campaign/campaign_save.h"
#include "core/campaign/prep.h"
#include "core/data/loader.h"
#include "core/script/formulas.h"

namespace ote {

bool computeCampaignHash(const Database& db, std::string& out, std::string& err) {
  if (!db.campaign) {
    err = "the game has no campaign";
    return false;
  }
  savehash::Fnv f;
  if (!savehash::mixRules(f, db, err) || !savehash::mixCampaignFile(f, db, err)) return false;
  // Every chapter's map, in chapter order (a map used twice is mixed twice; harmless).
  for (const ChapterDef& ch : db.campaign->chapters)
    if (!savehash::mixMapFile(f, db, db.maps[(size_t)ch.map].file, err)) return false;
  out = f.hex();
  return true;
}

bool buildCampaignSave(const Database& db, const std::string& gameId, const Playthrough& pt, const Battle* battle,
                       long long savedAt, Json& out, std::string& err) {
  if (!db.campaign || pt.chapter < 0 || pt.chapter >= (int)db.campaign->chapters.size()) {
    err = "the playthrough does not belong to this game";
    return false;
  }
  if (battle && battle->choiceWaiting()) {  // half way through an action (docs/EVENTS.md "Dialogue choices")
    err = "a question is waiting for its answer";
    return false;
  }
  if (battle && pt.phase != Phase::Battle) {
    err = "a battle can only be saved in the playthrough's battle phase";
    return false;
  }
  std::string hash;
  if (!computeCampaignHash(db, hash, err)) return false;
  const ChapterDef& ch = db.campaign->chapters[(size_t)pt.chapter];
  const MapRef& map = db.maps[(size_t)ch.map];
  out = Json::makeObject();
  out.set("saveVersion", Json::makeNumber(kCampaignSaveVersion));
  out.set("game", Json::makeString(gameId));
  out.set("gameName", Json::makeString(db.name));
  out.set("map", Json::makeString(map.id));
  out.set("mapName", Json::makeString(map.name));
  out.set("chapter", Json::makeString(ch.id));
  out.set("chapterName", Json::makeString(ch.name));
  out.set("dataHash", Json::makeString(hash));
  out.set("savedAt", Json::makeNumber((double)savedAt));
  if (battle) {
    out.set("round", Json::makeNumber(battle->round));
    out.set("activeSide", Json::makeNumber(battle->activeSide));
  }
  Json play;
  savePlaythrough(db, pt, play);
  out.set("playthrough", play);
  if (battle) {
    Json state;
    saveBattleState(*battle, state);
    out.set("battle", state);
  }
  return true;
}

bool isCampaignSave(const Json& root) { return root.isObject() && root.has("playthrough"); }

bool campaignSaveUsable(const Database& db, const std::string& gameId, const Json& root, std::string& reason) {
  if (!root.isObject() || !root["playthrough"].isObject() || !root["dataHash"].isString()) {
    reason = "The file is damaged";
    return false;
  }
  if (!root["saveVersion"].isNumber() || root["saveVersion"].asNumber() != kCampaignSaveVersion) {
    reason = "Made with a different version of this game";
    return false;
  }
  if (!root["game"].isString() || root["game"].asString() != gameId) {
    reason = "This save belongs to another game";
    return false;
  }
  if (!db.campaign) {
    reason = "This game no longer has a campaign";
    return false;
  }
  std::string hash, err;
  if (!computeCampaignHash(db, hash, err) || hash != root["dataHash"].asString()) {
    reason = "Made with a different version of this game";
    return false;
  }
  return true;
}

bool restoreCampaignSave(const Database& db, Formulas& formulas, const std::string& gameId, const Json& root,
                         Playthrough& pt, Battle& battle, bool& hasBattle, std::string& err) {
  std::string reason;
  if (!campaignSaveUsable(db, gameId, root, reason)) {
    err = reason;
    return false;
  }
  Playthrough restored;
  if (!restorePlaythrough(db, root["playthrough"], restored, err)) return false;
  if (restored.prep.any()) {  // the deployment must fit the chapter's map
    MapDef map;
    const std::string mapId = db.maps[(size_t)db.campaign->chapters[(size_t)restored.chapter].map].id;
    if (!loadMap(db, mapId, map, err)) return false;
    if (!prepValidate(db, restored, map, err)) {
      err = "playthrough.prep: " + err;
      return false;
    }
  }
  Battle fresh;
  bool withBattle = root.has("battle");
  if (withBattle) {
    if (restored.phase != Phase::Battle) {
      err = "battle: the file has a battle but the playthrough is not in its battle phase";
      return false;
    }
    // The seed does not matter: the saved random generator state replaces it.
    if (!prepareChapterBattle(db, restored, formulas, 1, fresh, err)) return false;
    if (!restoreBattleState(fresh, root["battle"], err)) return false;
  }
  pt = restored;
  if (withBattle) battle = fresh;
  hasBattle = withBattle;
  return true;
}

}  // namespace ote
