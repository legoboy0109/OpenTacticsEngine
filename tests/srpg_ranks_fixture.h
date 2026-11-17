// Helpers of the weapon rank and stat cap tests (docs/SRPG.md section 7): a writable copy of tests/fixtures/srpg-ranks
// (a test changes one file, loads the copy and reads back the message) and a loaded battle on its "duel" map.
//
// The duel map: kell (blue, level 2, a fighter; sword experience 4 = rank D; carries bronze, steel and war sword, a
// might draught) at 1,1; mara (blue archer, bow D; short and long bow) at 1,0; g1 (red grunt, 60 HP, hits for 1) at
// 2,1 next to kell; g2 (red grunt) far away. Ladder e 0, d 3, c 7, b 12; sword steel needs D, war sword B, long bow C.
#pragma once
#include <functional>
#include <string>

#include "core/data/loader.h"
#include "platform/platform.h"
#include "save_helpers.h"

namespace rankstest {

using namespace ote;

struct Copy {
  std::string dir;

  // `fixture`: another folder under tests/fixtures with the same file names (srpg-locks; files it lacks are skipped).
  explicit Copy(const std::string& name, const char* fixture = "srpg-ranks") {
    dir = savetest::tempFolder(name);
    makeDir(dir + "/data");
    makeDir(dir + "/maps");
    static const char* const files[] = {"game.json",         "data/terrain.json", "data/weapons.json", "data/items.json",
                                        "data/units.json",   "data/characters.json", "data/ranks.json", "data/caps.json", "data/skills.json", "data/conditions.json",
                                        "maps/field.json",   "maps/duel.json", "maps/clinic.json", "maps/skirmish.json", "maps/yard.json", "maps/mist.json", "maps/gate.json"};
    for (const char* f : files) {
      std::string text;
      if (readTextFile(ote_test::sourceDir() + "/tests/fixtures/" + fixture + "/" + f, text)) writeTextFile(dir + "/" + f, text);
    }
  }
  void put(const std::string& rel, const std::string& text) { writeTextFile(dir + "/" + rel, text); }
  void drop(const std::string& rel) { removeFile(dir + "/" + rel); }
  std::string read(const std::string& rel) const {
    std::string text;
    readTextFile(dir + "/" + rel, text);
    return text;
  }
  // `rel` with the first `from` replaced by `to`.
  void swap(const std::string& rel, const std::string& from, const std::string& to) {
    std::string text = read(rel);
    size_t at = text.find(from);
    if (at == std::string::npos) std::printf("  the test's text to replace is missing: %s\n", from.c_str());
    else put(rel, text.replace(at, from.size(), to));
  }
  // The game's load message ("" = it loaded).
  std::string gameError() const {
    Database db;
    std::string err;
    return loadGame(dir, db, err) ? "" : err;
  }
  std::string mapError(const char* map) const {
    Database db;
    MapDef m;
    std::string err;
    if (!loadGame(dir, db, err)) return err;
    return loadMap(db, map, m, err) ? "" : err;
  }
};

inline bool has(const std::string& hay, const std::string& needle) { return hay.find(needle) != std::string::npos; }
// The message must contain `needle` (prints both when not).
inline bool saysIn(const std::string& err, const std::string& needle) {
  if (!has(err, needle)) std::printf("  got: %s\n  wanted: %s\n", err.c_str(), needle.c_str());
  return has(err, needle);
}

// The fixture (or a changed copy) loaded into a battle that has started.
struct World {
  Copy copy;
  savetest::Rig rig;
  std::vector<Event> events;
  explicit World(const char* name, const char* map = "duel", const std::function<void(Copy&)>& tweak = nullptr,
                 const char* fixture = "srpg-ranks")
      : copy(name, fixture) {
    if (tweak) tweak(copy);
    CHECK(rig.load(copy.dir, map));
    rig.battle.start(events);
  }
  Battle& b() { return rig.battle; }
  int id(const char* story) { return b().unitByStoryId(story); }
  Unit& unit(const char* story) { return b().units[(size_t)id(story)]; }
  int stat(const char* s) const { return rig.db.findStat(s); }
  int group(const char* g) const { return rig.db.ranks.findGroup(g); }
  int item(const char* n) const { return rig.db.findItem(n); }
  int weapon(const char* n) const { return rig.db.findWeapon(n); }
  int slotOf(const char* story, const char* name) {
    const Unit& u = unit(story);
    for (size_t i = 0; i < u.items.size(); ++i)
      if (u.items[i].item == item(name)) return (int)i;
    return -1;
  }
  bool run(const Action& a) {
    events.clear();
    std::string why;
    const bool ok = b().apply(a, events, &why);
    if (!ok) std::printf("  refused: %s\n", why.c_str());
    return ok;
  }
  std::string why(const Action& a) {
    std::string w;
    return b().validate(a, &w) ? "" : (w.empty() ? "(no reason)" : w);
  }
  int count(EventType t) const {
    int n = 0;
    for (const Event& e : events) n += e.type == t;
    return n;
  }
  const Event* first(EventType t) const {
    for (const Event& e : events)
      if (e.type == t) return &e;
    return nullptr;
  }
};

}  // namespace rankstest
