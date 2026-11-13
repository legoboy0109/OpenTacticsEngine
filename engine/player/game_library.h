// Which games the player offers: discovery in the games folder.
#pragma once
#include <string>
#include <vector>

#include "core/data/game_info.h"

namespace ote {

struct GameEntry {
  std::string id;    // folder name under the games dir
  std::string name;  // display name from game.json (falls back to id)
  bool readable = true;
  std::string error;  // why game.json could not be read (readable == false)
  GameInfo info;      // launcher preview data (empty when unreadable)
};

// Order: the entries of <dir>/index.json (if present), then every other
// sub-folder that contains a game.json, alphabetically. Folders are only
// found where the platform can list them (listSubdirs); elsewhere the index
// is the whole list. A broken game stays in the list with its error.
// `warning` receives a message if index.json exists but is unusable.
std::vector<GameEntry> discoverGames(const std::string& gamesDir, std::string& warning);

}  // namespace ote
