// Save files of one game (docs/SAVES.md): where they live, how a save file
// is built, and reading / writing / deleting them. Slot 0 is the automatic
// Continue save; slots 1..kSlots are the manual ones. The battle state inside
// a file comes from core/battle/save.*; this module never looks inside it.
#pragma once
#include <string>

#include "core/base/json.h"
#include "core/battle/battle.h"

namespace ote {

constexpr int kSaveFileVersion = 1;

// What a list needs to know about one save, without restoring it.
struct SaveInfo {
  enum State { Empty, Ok, Unusable };
  State state = Empty;
  std::string reason;  // Unusable: a plain sentence for the player
  std::string mapId, mapName;
  int round = 0;          // the battle's turn; 0 = no battle in the save (a campaign save between battles)
  long long savedAt = 0;  // seconds since 1970 (UTC)
  // Campaign saves (docs/SAVES.md "Campaign saves"): a whole playthrough.
  bool campaign = false;
  bool inBattle = false;     // campaign: the save holds a battle that is under way
  std::string chapterName;   // campaign: the chapter the playthrough is at
  std::string phase;         // campaign: "before" | "battle" | "after" | "finished" | "over"
  // Campaign saves of a game with difficulties (docs/DIFFICULTY.md): the playthrough's difficulty id and, while a battle is under
  // way, the one that battle was built with. Empty in every other save. `difficultyName` = the name when the game is known.
  std::string difficulty, battleDifficulty, difficultyName;
};

// A save read and checked completely, ready for restoreBattleState.
struct SaveFile {
  std::string mapId;
  Json battle;    // a plain save's battle state
  Json root;      // the whole file (a campaign save is restored from this)
  SaveInfo info;  // info.campaign tells which kind it is
};

class SaveStore {
 public:
  static constexpr int kSlots = 5;

  SaveStore() {}  // disabled: nothing is read or written
  // `gameFolder` is <saves folder>/<game folder name>.
  explicit SaveStore(const std::string& gameFolder) : dir_(gameFolder) {}
  bool enabled() const { return !dir_.empty(); }
  const std::string& folder() const { return dir_; }
  // 0 = continue.json, 1..kSlots = slotN.json.
  std::string path(int slot) const;

  // Summary of one save. With `db` (the loaded game) the data hash and map are
  // checked too; without it only the file itself is. Never changes anything.
  SaveInfo inspect(int slot, const Database* db, const std::string& gameId) const;
  // Reads, checks and returns the save; false with `err` (a plain sentence)
  // when it cannot be loaded. The file is left alone either way.
  bool read(int slot, const Database& db, const std::string& gameId, SaveFile& out, std::string& err) const;
  // Atomic write (temp file + rename), then commitUserData().
  bool write(int slot, const Json& file, std::string& err) const;
  // Deletes the save; true also when there was none.
  bool remove(int slot, std::string& err) const;
  bool exists(int slot) const;

 private:
  std::string dir_;
};

// Builds the JSON of a save file for the battle in play. `gameId` is the game's
// folder name, `savedAt` seconds since 1970.
bool buildSaveFile(const Battle& battle, const Database& db, const std::string& gameId,
                   const std::string& mapId, long long savedAt, Json& out, std::string& err);

long long nowSeconds();
// "2026-11-14 14:30" in the viewer's local time.
std::string formatSavedAt(long long seconds);

}  // namespace ote
