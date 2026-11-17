#include "core/campaign/difficulty.h"

namespace ote {

int playthroughDifficulty(const Database& db, const Playthrough& pt) {
  if (!db.difficulties.any()) return -1;
  const int i = db.difficulties.find(pt.difficulty);
  return i >= 0 ? i : db.difficulties.defaultIndex;
}

bool setPlaythroughDifficulty(const Database& db, Playthrough& pt, const std::string& id, bool choosing, std::string& why) {
  if (!db.difficulties.any()) {
    why = "this game has no difficulties";
    return false;
  }
  const int to = db.difficulties.find(id);
  if (to < 0) {
    why = "there is no difficulty \"" + id + "\"";
    return false;
  }
  const int from = playthroughDifficulty(db, pt);
  if (!choosing && to != from && !db.difficulties.mayChange(from, to)) {
    why = db.difficulties.change == DifficultyChange::Never ? "this game does not allow changing the difficulty"
                                                            : "this game only allows changing to an easier difficulty";
    return false;
  }
  pt.difficulty = id;
  return true;
}

}  // namespace ote
