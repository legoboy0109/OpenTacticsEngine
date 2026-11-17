#include "core/battle/save_difficulty.h"

#include "core/base/log.h"
#include "core/campaign/playthrough.h"

namespace ote {
namespace savejson {

namespace {

// The list position of a saved id; an id the game does not have any more means its default (with a note for whoever debugs).
int indexOrDefault(const Database& db, const std::string& id, const char* where) {
  const int i = db.difficulties.find(id);
  if (i >= 0) return i;
  OTE_WARN("save: %s: difficulty \"%s\" no longer exists in the game; using the default \"%s\"", where, id.c_str(),
           db.difficulties.defaultId.c_str());
  return db.difficulties.defaultIndex;
}

}  // namespace

void writeBattleDifficulty(const Battle& b, Json& out) {
  if (!b.db || !b.db->difficulties.any() || b.difficulty < 0 || b.difficulty >= (int)b.db->difficulties.list.size()) return;
  out.set("difficulty", Json::makeString(b.db->difficulties.list[(size_t)b.difficulty].id));
}

bool readBattleDifficulty(Reader& rd, const Json& saved, Battle& b) {
  if (!saved.has("difficulty")) return true;  // a save from before the game had difficulties: what init chose (the default)
  if (!saved["difficulty"].isString()) return rd.fail("battle.difficulty", "not text");
  if (!b.db->difficulties.any()) {
    OTE_WARN("save: battle.difficulty \"%s\" ignored: the game has no difficulties any more", saved["difficulty"].asString().c_str());
    return true;
  }
  b.setDifficulty(indexOrDefault(*b.db, saved["difficulty"].asString(), "battle"));
  return true;
}

void writePlaythroughDifficulty(const Database& db, const Playthrough& pt, Json& out) {
  if (!db.difficulties.any() || pt.difficulty.empty()) return;
  out.set("difficulty", Json::makeString(pt.difficulty));
}

bool readPlaythroughDifficulty(Reader& rd, const Database& db, const Json& saved, Playthrough& pt) {
  if (!saved.has("difficulty")) {
    if (db.difficulties.any()) pt.difficulty = db.difficulties.defaultId;
    return true;
  }
  if (!saved["difficulty"].isString()) return rd.fail("playthrough.difficulty", "not text");
  if (!db.difficulties.any()) {
    OTE_WARN("save: playthrough.difficulty \"%s\" ignored: the game has no difficulties any more", saved["difficulty"].asString().c_str());
    return true;
  }
  pt.difficulty = db.difficulties.list[(size_t)indexOrDefault(db, saved["difficulty"].asString(), "playthrough")].id;
  return true;
}

}  // namespace savejson
}  // namespace ote
