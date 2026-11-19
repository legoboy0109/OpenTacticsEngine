#include "game_library.h"

#include <algorithm>

#include "core/base/json.h"
#include "core/base/log.h"
#include "platform/platform.h"

namespace ote {

static GameEntry makeEntry(const std::string& gamesDir, const std::string& id) {
  GameEntry e;
  e.id = id;
  e.name = id;
  e.dir = gamesDir + "/" + id;
  if (loadGameInfo(gamesDir + "/" + id, e.info, e.error)) {
    if (!e.info.name.empty()) e.name = e.info.name;
  } else {
    e.readable = false;
    OTE_WARN("%s", e.error.c_str());
  }
  return e;
}

std::vector<GameEntry> discoverGames(const std::string& gamesDir, std::string& warning) {
  std::vector<GameEntry> out;
  std::vector<std::string> ids;
  auto known = [&](const std::string& id) { return std::find(ids.begin(), ids.end(), id) != ids.end(); };

  std::string text;
  if (readTextFile(gamesDir + "/index.json", text)) {
    Json index;
    std::string err;
    if (Json::parse(text, index, err)) {
      for (const Json& g : index["games"].items())
        if (!g.asString().empty() && !known(g.asString())) ids.push_back(g.asString());
    } else {
      warning = "games/index.json is unreadable (" + err + ")";
      OTE_WARN("%s: %s", (gamesDir + "/index.json").c_str(), err.c_str());
    }
  }

  std::vector<std::string> dirs;
  if (listSubdirs(gamesDir, dirs)) {
    std::sort(dirs.begin(), dirs.end());
    for (const std::string& d : dirs) {
      std::string probe;
      if (!known(d) && readTextFile(gamesDir + "/" + d + "/game.json", probe)) ids.push_back(d);
    }
  }
  for (const std::string& id : ids) out.push_back(makeEntry(gamesDir, id));
  return out;
}

std::vector<GameEntry> discoverGamesMerged(const std::string& gamesDir, const std::string& extraDir, std::string& warning) {
  std::vector<GameEntry> out = discoverGames(gamesDir, warning);
  if (extraDir.empty()) return out;
  std::string extraWarning;
  for (GameEntry& e : discoverGames(extraDir, extraWarning)) {
    bool replaced = false;
    for (GameEntry& have : out)
      if (have.id == e.id) { have = e; replaced = true; break; }
    if (!replaced) out.push_back(e);
  }
  if (warning.empty()) warning = extraWarning;
  return out;
}

}  // namespace ote
