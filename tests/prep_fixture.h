// The preparation screen's tests (docs/EVENTS.md "Preparation") play tests/fixtures/srpg-prep, copied into the build
// directory so a test can change single files (a map's "prep" block, the terrain) and load them with the real loader.
#pragma once
#include <map>
#include <string>

#include "core/campaign/campaign_save.h"
#include "core/campaign/playthrough.h"
#include "core/campaign/prep.h"
#include "core/data/loader.h"
#include "core/script/formulas.h"
#include "platform/platform.h"
#include "save_helpers.h"
#include "test.h"

namespace preptest {

using namespace ote;

inline const char* const kFiles[] = {"game.json", "data/terrain.json", "data/weapons.json", "data/units.json",
                                     "data/items.json", "data/campaign.json", "maps/meadow.json", "maps/orchard.json",
                                     "maps/hilltop.json"};

inline std::string replaceOnce(std::string text, const std::string& from, const std::string& to) {
  size_t at = text.find(from);
  if (at == std::string::npos) std::printf("  replaceOnce: \"%s\" not found\n", from.c_str());
  else text.replace(at, from.size(), to);
  return text;
}

struct Game {
  Database db;
  Formulas formulas;
  std::string dir;
  std::map<std::string, std::string> files;  // relative path -> whole text, replacing the fixture's

  static std::string fixture(const std::string& rel) {
    std::string text;
    if (!readTextFile(ote_test::sourceDir() + "/tests/fixtures/srpg-prep/" + rel, text)) std::printf("  cannot read %s\n", rel.c_str());
    return text;
  }
  // The fixture's text of one file with one piece replaced (kept for load()).
  void edit(const std::string& rel, const std::string& from, const std::string& to) {
    files[rel] = replaceOnce(files.count(rel) ? files[rel] : fixture(rel), from, to);
  }
  bool load(const std::string& folder, std::string* errOut = nullptr) {
    dir = savetest::tempFolder(folder);
    makeDir(dir + "/data");
    makeDir(dir + "/maps");
    for (const char* rel : kFiles)
      if (!writeTextFile(dir + "/" + rel, files.count(rel) ? files[rel] : fixture(rel))) return false;
    std::string err;
    bool ok = loadGame(dir, db, err) && formulas.init(db, err);
    if (!ok && errOut) *errOut = err;
    else if (!ok) std::printf("  load: %s\n", err.c_str());
    return ok;
  }
  // The loader's message for a map ("" = it loads).
  std::string mapError(const std::string& mapId) {
    MapDef m;
    std::string err;
    return loadMap(db, mapId, m, err) ? "" : err;
  }
  MapDef map(const std::string& id) {
    MapDef m;
    std::string err;
    if (!loadMap(db, id, m, err)) std::printf("  loadMap(%s): %s\n", id.c_str(), err.c_str());
    return m;
  }
};

struct Run {
  Game g;
  Playthrough pt;
  Battle b;
  std::vector<Event> ev;
  std::vector<SceneLine> lines;
  MapDef mapDef;  // the current chapter's map

  bool begin(const std::string& folder) { return g.load(folder) && pt.begin(g.db); }
  const ChapterDef& chapter() const { return g.db.campaign->chapters[(size_t)pt.chapter]; }
  void loadChapterMap() { mapDef = g.map(g.db.maps[(size_t)chapter().map].id); }
  bool before() {
    lines.clear();
    return runScene(g.db, pt, SceneKind::Before, lines);
  }
  bool start(uint32_t seed = 1) {
    std::string err;
    ev.clear();
    if (!startChapterBattle(g.db, pt, g.formulas, seed, b, ev, err)) {
      std::printf("  startChapterBattle: %s\n", err.c_str());
      return false;
    }
    return true;
  }
  int unit(const char* id) const { return b.unitByStoryId(id); }
  void decide(bool humanWins) {
    b.over = true;
    b.winnerTeam = humanWins ? b.humanTeam() : 1 - b.humanTeam();
  }
  void kill(const char* id) {
    Unit& u = b.units[(size_t)unit(id)];
    b.occupant[(size_t)u.cell] = -1;
    u.alive = false;
    u.hp = 0;
    u.cell = kNoCell;
  }
  // Chapter 1 played and won (no preparation there), its after scene run: the playthrough stands at chapter 2's before scene.
  bool toChapter2() {
    if (!before() || !start()) return false;
    decide(true);
    if (finishChapterBattle(g.db, pt, b) != ChapterResult::Won) return false;
    lines.clear();
    return runScene(g.db, pt, SceneKind::After, lines);
  }
  // Chapter 2's before scene, then the screen opens.
  bool openChapter2() {
    if (!before()) return false;
    loadChapterMap();
    prepOpen(g.db, pt, mapDef, true);
    return pt.prep.open;
  }
  // The deployed ids in slot order, "-" for an empty slot: "sera tomas wren -".
  std::string slots() const {
    std::string out;
    for (size_t s = 0; s < mapDef.prep->slots.size(); ++s) {
      std::string who = "-";
      for (const PrepDeploy& d : pt.prep.deployed)
        if (d.slot == (int)s) who = d.id;
      out += (s ? " " : "") + who;
    }
    return out;
  }
  std::string why;
  bool deploy(const char* id, int slot = -1) { return prepDeploy(g.db, pt, mapDef, id, slot, why); }
  bool bench(const char* id) { return prepBench(g.db, pt, mapDef, id, why); }
  bool move(const char* id, int slot) { return prepMove(g.db, pt, mapDef, id, slot, why); }
  std::string state() const {
    Json j;
    savePlaythrough(g.db, pt, j);
    return j.dump(0);
  }
  const RosterUnit* roster(const char* id) const { return pt.findUnit(id); }
};

}  // namespace preptest
