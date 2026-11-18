#include "saves.h"

#include <cerrno>
#include <cstdio>
#include <ctime>

#include "core/battle/save.h"
#include "core/campaign/campaign_save.h"
#include "platform/platform.h"

namespace ote {

namespace {
const char* const kDifferentVersion = "Made with a different version of this game";

// Creates every folder of `path` that is missing (the platform call makes one level).
// Failures of the upper levels do not matter (a drive root cannot be "created");
// only the folder itself has to exist at the end.
bool makeDirs(const std::string& path) {
  for (size_t i = 1; i < path.size(); ++i)
    if (path[i] == '/') makeDir(path.substr(0, i));
  return makeDir(path);
}

enum class Probe { Missing, Unreadable, Readable };

Probe probe(const std::string& path) {
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (f) { std::fclose(f); return Probe::Readable; }
  return (errno == ENOENT || errno == ENOTDIR) ? Probe::Missing : Probe::Unreadable;
}

SaveInfo unusable(const std::string& why) {
  SaveInfo i;
  i.state = SaveInfo::Unusable;
  i.reason = why;
  return i;
}

// Everything about a file that does not need the loaded game.
bool parseFile(const std::string& path, const std::string& gameId, Json& root, SaveInfo& info,
               std::string& hash) {
  std::string text;
  if (!readTextFile(path, text)) { info = unusable("The file cannot be read"); return false; }
  std::string perr;
  if (!Json::parse(text, root, perr) || !root.isObject()) {
    info = unusable("The file is damaged");
    return false;
  }
  if (!root["saveVersion"].isNumber() || root["saveVersion"].asNumber() != kSaveFileVersion) {
    info = unusable(kDifferentVersion);
    return false;
  }
  if (!root["game"].isString() || root["game"].asString() != gameId) {
    info = unusable("This save belongs to another game");
    return false;
  }
  const bool campaign = isCampaignSave(root);
  // A campaign save has a round and a battle only while a battle is under way.
  const bool inBattle = campaign && root.has("battle");
  if (!root["map"].isString() || !root["dataHash"].isString() ||
      (!campaign && (!root["round"].isNumber() || !root["battle"].isObject())) ||
      (campaign && (!root["playthrough"]["phase"].isString() || (inBattle && !root["round"].isNumber())))) {
    info = unusable("The file is damaged");
    return false;
  }
  info = SaveInfo();
  info.state = SaveInfo::Ok;
  info.mapId = root["map"].asString();
  info.mapName = root["mapName"].asString(info.mapId);
  info.round = inBattle || !campaign ? root["round"].asInt() : 0;
  info.campaign = campaign;
  if (campaign) {
    info.inBattle = inBattle;
    info.chapterName = root["chapterName"].asString("");
    info.phase = root["playthrough"]["phase"].asString();
    info.difficulty = root["playthrough"]["difficulty"].asString("");
    if (inBattle) info.battleDifficulty = root["battle"]["difficulty"].asString("");
  }
  info.savedAt = (long long)root["savedAt"].asNumber(0);
  hash = root["dataHash"].asString();
  return true;
}
}  // namespace

std::string SaveStore::path(int slot) const {
  return dir_ + (slot <= 0 ? std::string("/continue.json") : "/slot" + std::to_string(slot) + ".json");
}

bool SaveStore::exists(int slot) const { return enabled() && probe(path(slot)) != Probe::Missing; }

SaveInfo SaveStore::inspect(int slot, const Database* db, const std::string& gameId) const {
  if (!enabled()) return SaveInfo();
  Probe p = probe(path(slot));
  if (p == Probe::Missing) return SaveInfo();
  if (p == Probe::Unreadable) return unusable("The file cannot be read");
  Json root;
  SaveInfo info;
  std::string hash;
  if (!parseFile(path(slot), gameId, root, info, hash)) return info;
  if (db && info.campaign) {  // judged by the campaign's own rules (version, game, hash over every chapter)
    const int d = db->difficulties.find(info.difficulty);
    if (d >= 0) info.difficultyName = db->difficulties.list[(size_t)d].name;
    std::string reason;
    if (!campaignSaveUsable(*db, gameId, root, reason)) {
      info.state = SaveInfo::Unusable;
      info.reason = reason;
    }
  } else if (db) {
    if (db->findMap(info.mapId) < 0) {
      info.state = SaveInfo::Unusable;
      info.reason = "Its map is no longer in this game";
      return info;
    }
    std::string now, err;
    if (!computeDataHash(*db, info.mapId, now, err) || now != hash) {
      info.state = SaveInfo::Unusable;
      info.reason = kDifferentVersion;
    }
  }
  return info;
}

bool SaveStore::read(int slot, const Database& db, const std::string& gameId, SaveFile& out,
                     std::string& err) const {
  SaveInfo info = inspect(slot, &db, gameId);
  if (info.state == SaveInfo::Empty) { err = "There is no save here"; return false; }
  if (info.state == SaveInfo::Unusable) { err = info.reason; return false; }
  Json root;
  std::string hash;
  parseFile(path(slot), gameId, root, info, hash);  // cannot fail: inspect just did the same
  out.info = info;
  out.mapId = info.mapId;
  out.battle = root["battle"];
  out.root = root;
  return true;
}

bool SaveStore::write(int slot, const Json& file, std::string& err) const { return writeAt(path(slot), file, err); }

std::string SaveStore::progressPath() const { return dir_ + "/progress.json"; }

bool SaveStore::writeAt(const std::string& target, const Json& file, std::string& err) const {
  if (!enabled()) { err = "saving is switched off"; return false; }
  if (!makeDirs(dir_)) { err = dir_ + ": cannot create the saves folder"; return false; }
  std::string tmp = target + ".tmp";
  if (!writeTextFile(tmp, file.dump(0) + "\n")) {
    removeFile(tmp);
    err = tmp + ": cannot write the save";
    return false;
  }
  std::remove(target.c_str());  // rename() does not replace on Windows
  if (std::rename(tmp.c_str(), target.c_str()) != 0) {
    err = target + ": cannot replace the save file";
    return false;
  }
  commitUserData();
  return true;
}

bool SaveStore::remove(int slot, std::string& err) const {
  if (!enabled()) return true;
  if (!removeFile(path(slot))) {
    err = path(slot) + ": cannot delete the save";
    return false;
  }
  commitUserData();
  return true;
}

bool buildSaveFile(const Battle& battle, const Database& db, const std::string& gameId,
                   const std::string& mapId, long long savedAt, Json& out, std::string& err) {
  if (battle.choiceWaiting()) {  // the state would be half way through an action (docs/EVENTS.md "Dialogue choices")
    err = "a question is waiting for its answer";
    return false;
  }
  std::string hash;
  if (!computeDataHash(db, mapId, hash, err)) return false;
  int mi = db.findMap(mapId);
  out = Json::makeObject();
  out.set("saveVersion", Json::makeNumber(kSaveFileVersion));
  out.set("game", Json::makeString(gameId));
  out.set("gameName", Json::makeString(db.name));
  out.set("map", Json::makeString(mapId));
  out.set("mapName", Json::makeString(db.maps[(size_t)mi].name));
  out.set("dataHash", Json::makeString(hash));
  out.set("savedAt", Json::makeNumber((double)savedAt));
  out.set("round", Json::makeNumber(battle.round));
  out.set("activeSide", Json::makeNumber(battle.activeSide));
  Json state;
  saveBattleState(battle, state);
  out.set("battle", state);
  return true;
}

long long nowSeconds() { return (long long)std::time(nullptr); }

std::string formatSavedAt(long long seconds) {
  if (seconds <= 0) return "";
  std::time_t t = (std::time_t)seconds;
  std::tm local;
#if defined(_WIN32)
  if (localtime_s(&local, &t) != 0) return "";
#else
  if (!localtime_r(&t, &local)) return "";
#endif
  char buf[32];
  if (std::strftime(buf, sizeof buf, "%Y-%m-%d %H:%M", &local) == 0) return "";
  return buf;
}

}  // namespace ote
