// Helpers of the foe's weapon tests (docs/SRPG.md "Foe's weapon"): tests/fixtures/srpg-triangle loaded into a
// battle, optionally from a writable copy a test changes first.
//
// Map "duel" (6x3), blue: sw (swordsman, iron sword) 1,1; sc (scout, speed 12, iron sword) 5,0.
// Red: ax (iron axe) 2,1 next to sw; la (iron lance) 1,2; mk (monk, no weapon) 1,0; br (brute, only a great axe,
// which needs rank B: it cannot wield it) 0,1. Map "choice": red rs (sword) 3,1 between blue bx (axe) 3,0 and bl
// (lance) 3,2.
// Formulas (game.json): damage = str + might - def + 3 * tri; hit = 70 + 2 * (a.spd - b.spd) + 15 * tri + 2 * fw.weight;
// strikes = 2 when a.spd - w.weight >= b.spd - fw.weight + 5. tri = +1 / -1 / 0, scripts/triangle.lua.
#pragma once
#include <functional>
#include <string>

#include "core/ai/ai.h"
#include "platform/platform.h"
#include "save_helpers.h"

namespace tritest {

using namespace ote;

// The fixture copied into a temp folder; `tweak` may rewrite files before it loads.
inline std::string copyFixture(const char* name, const std::function<void(const std::string& dir)>& tweak) {
  const std::string dir = savetest::tempFolder(name);
  makeDir(dir + "/data");
  makeDir(dir + "/maps");
  makeDir(dir + "/scripts");
  static const char* const files[] = {"game.json", "scripts/triangle.lua", "data/terrain.json", "data/ranks.json",
                                      "data/weapons.json", "data/items.json", "data/units.json", "maps/duel.json",
                                      "maps/choice.json"};
  for (const char* f : files) {
    std::string text;
    if (readTextFile(ote_test::sourceDir() + "/tests/fixtures/srpg-triangle/" + f, text)) writeTextFile(dir + "/" + f, text);
  }
  if (tweak) tweak(dir);
  return dir;
}

inline void swapText(const std::string& path, const std::string& from, const std::string& to) {
  std::string text;
  readTextFile(path, text);
  const size_t at = text.find(from);
  if (at == std::string::npos) std::printf("  the test's text to replace is missing: %s\n", from.c_str());
  else writeTextFile(path, text.replace(at, from.size(), to));
}

struct World {
  savetest::Rig rig;
  std::vector<Event> events;
  explicit World(const char* map = "duel", const std::function<void(const std::string& dir)>& tweak = nullptr) {
    if (tweak) CHECK(rig.load(copyFixture("triangle", tweak), map));
    else CHECK(rig.load(ote_test::sourceDir() + "/tests/fixtures/srpg-triangle", map));
    rig.battle.start(events);
  }
  Battle& b() { return rig.battle; }
  int id(const char* story) { return b().unitByStoryId(story); }
  Unit& unit(const char* story) { return b().units[(size_t)id(story)]; }
  int weapon(const char* n) const { return rig.db.findWeapon(n); }
  // The forecast of `attacker` striking `target` from the attacker's own square (or `from`).
  Forecast forecast(const char* attacker, const char* target, const char* weapon, Cell from = kNoCell) {
    Forecast f;
    const Cell at = from == kNoCell ? unit(attacker).cell : from;
    CHECK(b().forecast(id(attacker), id(target), this->weapon(weapon), at, f));
    return f;
  }
};

}  // namespace tritest
