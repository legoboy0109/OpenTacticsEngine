#include "core/data/settings.h"

#include <cstdio>

#include "core/base/fileio.h"

namespace ote {

bool Settings::load(const std::string& path, std::string& err) {
  path_ = path;
  global_ = Json::makeObject();
  games_ = Json::makeObject();
  state_ = Json::makeObject();
  dirty_ = false;
  corrupt_ = false;

  std::string text;
  if (!readTextFile(path, text)) return true;  // first run
  Json root;
  std::string parseErr;
  if (!Json::parse(text, root, parseErr) || !root.isObject()) {
    err = path + ": " + (parseErr.empty() ? "settings must be a JSON object" : parseErr);
    corrupt_ = true;
    return false;
  }
  if (root["global"].isObject()) global_ = root["global"];
  if (root["games"].isObject()) {
    for (const Json::Member& m : root["games"].members())
      if (m.second.isObject()) games_.set(m.first, m.second);
  }
  if (root["state"].isObject()) state_ = root["state"];
  return true;
}

bool Settings::save(std::string& err) {
  if (path_.empty()) return true;
  if (corrupt_) {
    // Keep the unreadable file for the user instead of destroying it.
#if defined(OTE_NXDK)
    nxReplaceFile(path_, path_ + ".bad");
#else
    std::rename(path_.c_str(), (path_ + ".bad").c_str());
#endif
    corrupt_ = false;
  }
  Json root = Json::makeObject();
  root.set("version", Json::makeNumber(1));
  root.set("global", global_);
  root.set("games", games_);
  root.set("state", state_);
  std::string tmp = path_ + ".tmp";
  if (!writeTextFile(tmp, root.dump(2) + "\n")) {
    err = tmp + ": cannot write settings";
    return false;
  }
#if defined(OTE_NXDK)
  const bool replaced = nxReplaceFile(tmp, path_);  // one kernel call that replaces (the Xbox kernel wants backslashes)
#else
  std::remove(path_.c_str());  // rename() does not replace on Windows
  const bool replaced = std::rename(tmp.c_str(), path_.c_str()) == 0;
#endif
  if (!replaced) {
    err = path_ + ": cannot replace settings file";
    return false;
  }
  dirty_ = false;
  return true;
}

const Json& Settings::get(const std::string& gameId, const std::string& key) const {
  if (!gameId.empty()) {
    const Json& over = games_[gameId][key];
    if (!over.isNull()) return over;
  }
  return global_[key];
}

std::string Settings::getString(const std::string& gameId, const std::string& key,
                                const std::string& def) const {
  const Json& v = get(gameId, key);
  return v.isString() ? v.asString() : def;
}

int Settings::getInt(const std::string& gameId, const std::string& key, int def) const {
  const Json& v = get(gameId, key);
  return v.isNumber() ? v.asInt() : def;
}

const Json& Settings::global(const std::string& key) const { return global_[key]; }

bool Settings::hasOverride(const std::string& gameId, const std::string& key) const {
  return !gameId.empty() && !games_[gameId][key].isNull();
}

void Settings::setGlobal(const std::string& key, const Json& value) {
  if (value.isNull()) global_.erase(key);
  else global_.set(key, value);
  dirty_ = true;
}

void Settings::setOverride(const std::string& gameId, const std::string& key, const Json& value) {
  if (gameId.empty()) return;
  if (value.isNull()) { clearOverride(gameId, key); return; }
  Json game = games_[gameId].isObject() ? games_[gameId] : Json::makeObject();
  game.set(key, value);
  games_.set(gameId, game);
  dirty_ = true;
}

void Settings::clearOverride(const std::string& gameId, const std::string& key) {
  if (!games_[gameId].isObject()) return;
  Json game = games_[gameId];
  if (!game.erase(key)) return;
  if (game.size() == 0) games_.erase(gameId);
  else games_.set(gameId, game);
  dirty_ = true;
}

std::string Settings::state(const std::string& key, const std::string& def) const {
  return state_[key].isString() ? state_[key].asString() : def;
}

void Settings::setState(const std::string& key, const std::string& value) {
  if (state_[key].isString() && state_[key].asString() == value) return;
  state_.set(key, Json::makeString(value));
  dirty_ = true;
}

}  // namespace ote
