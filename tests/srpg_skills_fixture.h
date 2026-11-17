// A writable copy of tests/fixtures/srpg-skills (docs/SRPG.md step K1): the skills tests change a file or two, load
// the copy with the real loader and read back the message or play the battle. The source fixture is never written.
#pragma once
#include <string>

#include "core/data/loader.h"
#include "platform/platform.h"
#include "save_helpers.h"

namespace skilltest {

using namespace ote;

struct Copy {
  std::string dir;
  std::string fixture;  // the folder under tests/fixtures it was copied from

  // `from`: another fixture with the same file names (srpg-skillblocks); files it lacks are skipped.
  explicit Copy(const std::string& name, const char* from = "srpg-skills") : fixture(from) {
    dir = savetest::tempFolder(name);
    makeDir(dir + "/data");
    makeDir(dir + "/maps");
    static const char* const files[] = {"game.json",       "data/terrain.json", "data/structures.json", "data/weapons.json",
                                        "data/items.json", "data/units.json",   "data/characters.json", "data/skills.json", "data/ranks.json", "data/conditions.json", "data/abilities.json",
                                        "maps/field.json", "maps/woods.json",   "maps/story.json"};
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
  // data/skills.json with these skills (a JSON list body) added to the fixture's own: the fixture's units, weapons and
  // items name some of them.
  void skills(const std::string& list) {
    std::string base;
    readTextFile(ote_test::sourceDir() + "/tests/fixtures/" + fixture + "/data/skills.json", base);
    base = base.substr(base.find('[') + 1, base.rfind(']') - base.find('[') - 1);
    put("data/skills.json", "{ \"skills\": [" + base + (list.empty() ? "" : "," + list) + "] }");
  }
  // The game's load message ("" = it loaded).
  std::string gameError() const {
    Database db;
    std::string err;
    return loadGame(dir, db, err) ? "" : err;
  }
  // The map's load message ("" = it loaded).
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

// One skill "x" with this block (JSON text), as the body of a skills list.
inline std::string oneBlock(const std::string& block) {
  return "{ \"id\": \"x\", \"blocks\": [" + block + "] }";
}

// A loaded fixture game (or a copy of it) with a battle: started unless asked not to be.
struct World {
  savetest::Rig rig;
  std::vector<Event> events;  // everything since the last clear
  explicit World(const char* map = "field", const std::string& dir = "", bool start = true) {
    const bool ok = rig.load(dir.empty() ? ote_test::sourceDir() + "/tests/fixtures/srpg-skills" : dir, map);
    CHECK(ok);
    if (ok && start) rig.battle.start(events);
  }
  Battle& b() { return rig.battle; }
  const Database& db() { return rig.db; }
  int id(const char* storyId) { return b().unitByStoryId(storyId); }
  Unit& unit(const char* storyId) { return b().units[(size_t)id(storyId)]; }
  int stat(const Unit& u, const char* s) { return b().stat(u, rig.db.findStat(s)); }
  int skill(const char* id) { return rig.db.findSkill(id); }
  Cell at(int x, int y) { return b().grid.at(x, y); }
  // The unit's skills by id, in judging order, as one text: "a b c".
  std::string skillIds(const Unit& u, int weapon = -1) {
    std::vector<int> list;
    b().skillsOf(u, weapon, list);
    std::string out;
    for (int s : list) out += (out.empty() ? "" : " ") + rig.db.skills[(size_t)s].id;
    return out;
  }
  std::string learnedIds(const Unit& u) {
    std::string out;
    for (int s : u.learned) out += (out.empty() ? "" : " ") + rig.db.skills[(size_t)s].id;
    return out;
  }
  bool apply(const Action& a, std::string* why = nullptr) {
    events.clear();
    return b().apply(a, events, why);
  }
  std::string refusal(const Action& a) {
    std::string why;
    b().validate(a, &why);
    return why;
  }
  int count(EventType t) {
    int n = 0;
    for (const Event& e : events) n += e.type == t;
    return n;
  }
};

}  // namespace skilltest
