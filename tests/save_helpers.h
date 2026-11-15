// Shared helpers of the save tests: a loaded demo game with a battle that can
// be copied, played with the AI and compared, plus small file utilities.
#pragma once
#include <cstdio>
#include <set>
#include <string>
#include <vector>

#include "core/ai/ai.h"
#include "core/base/json.h"
#include "core/battle/battle.h"
#include "core/battle/save.h"
#include "core/data/loader.h"
#include "core/script/formulas.h"
#include "platform/platform.h"
#include "test.h"

namespace savetest {

using namespace ote;

// A loaded game and a freshly initialised battle (NOT started: restoring
// needs exactly that; call start() for a new battle).
struct Rig {
  Database db;
  MapDef map;
  Formulas formulas;
  Battle battle;
  std::string gameDir, mapId;

  bool load(const std::string& dir, const std::string& map_, unsigned seed = 7) {
    gameDir = dir;
    mapId = map_;
    std::string err;
    bool ok = loadGame(dir, db, err) && loadMap(db, map_, map, err) && formulas.init(db, err) &&
              battle.init(db, map, formulas, seed, err);
    if (!ok) std::printf("  %s\n", err.c_str());
    return ok;
  }
  bool loadDemo(const char* game, const char* map_, unsigned seed = 7) {
    return load(ote_test::sourceDir() + "/games/" + game, map_, seed);
  }
  void start() {
    std::vector<Event> ev;
    battle.start(ev);
  }
};

inline std::string stateText(const Battle& b) {
  Json j;
  saveBattleState(b, j);
  return j.dump(0);
}

inline bool sameEvents(const std::vector<Event>& a, const std::vector<Event>& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    const Event &x = a[i], &y = b[i];
    if (x.type != y.type || x.a != y.a || x.b != y.b || x.c != y.c || x.d != y.d || x.flags != y.flags ||
        x.data != y.data || x.seen != y.seen || x.seenSteps != y.seenSteps)
      return false;
  }
  return true;
}

// Plays the AI for up to `steps` decisions (stops when the battle is over).
inline void playAi(Battle& b, int steps, std::vector<Event>& events) {
  for (int i = 0; i < steps && !b.over; ++i) aiTakeStep(b, events);
}

// A folder inside the build directory for files written by tests.
inline std::string tempFolder(const std::string& name) {
  std::string dir = ote_test::tempDir() + "/" + name;
  makeDir(dir);
  return dir;
}

inline bool copyFile(const std::string& from, const std::string& to) {
  std::string text;
  return readTextFile(from, text) && writeTextFile(to, text);
}

}  // namespace savetest
