// Helpers of the map change and fog option tests (docs/EVENTS.md "Changing the map", docs/ECONOMY.md "Fog of war"): a writable copy of
// tests/fixtures/mapchange and a loaded battle on one of its maps.
//
// "bridge" (9 x 3, no fog): a river column at x 4 (foot cannot cross, fly can). Blue hero (fighter, 1,1) and wing (bird, 1,0); red r1
// (raider, 7,1). Changes: bridge (4,1 -> bridge), sink (2,1 and 2,2 -> river), ford (4,0 -> bridge, 4,1 -> rubble). Events: turn 2
// applyChange bridge; turn 4 setTerrain.
// "cracked" (9 x 6, fog): a wall row at y 2 with a 3-wide gap (3..5) filled by the cracked wall w1 ( 12 hit points, break only) in it. Blue hero (4,3;
// sword, torch), sneak (thief, 2,3); red r1 (4,0). Change rubble (3..5,2 -> rubble); the place trigger "w1 opened" applies it.
// "dark" (13 x 5, fog): a thicket column at x 5 (blocks sight) with the portcullis gate (5,2; blocks sight) in it, a brazier (11,0;
// light 2). Blue hero (3,2; sword, torch, stub), sneak (thief, 1,4), seer (keen eye, 1,0); red r1 (8,2), r2 (12,4). Changes: gap (5,1 ->
// plain, gate -> raised portcullis), night (5,0 -> thicket). Events: turn 2 fog lifts; turn 3 fog back and vision -1.
#pragma once
#include <functional>

#include "save_helpers.h"
#include "test.h"

namespace mctest {

using namespace ote;

struct MCopy {
  std::string dir;
  explicit MCopy(const std::string& name) {
    dir = savetest::tempFolder(name);
    makeDir(dir + "/data");
    makeDir(dir + "/maps");
    static const char* const files[] = {"game.json", "data/terrain.json", "data/weapons.json", "data/items.json", "data/units.json",
                                        "data/skills.json", "data/structures.json", "maps/bridge.json", "maps/cracked.json", "maps/dark.json"};
    for (const char* f : files) {
      std::string text;
      if (readTextFile(ote_test::sourceDir() + "/tests/fixtures/mapchange/" + f, text)) writeTextFile(dir + "/" + f, text);
    }
  }
  void put(const std::string& rel, const std::string& text) { writeTextFile(dir + "/" + rel, text); }
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
  // The load message of a map ("" = it loaded).
  std::string loadError(const char* map) const {
    Database db;
    MapDef m;
    std::string err;
    if (!loadGame(dir, db, err)) return err;
    if (!loadMap(db, map, m, err)) return err;
    return "";
  }
};

struct World {
  MCopy copy;
  savetest::Rig rig;
  std::vector<Event> events;
  explicit World(const char* name, const char* map = "bridge", const std::function<void(MCopy&)>& tweak = nullptr, bool start = true)
      : copy(name) {
    if (tweak) tweak(copy);
    CHECK(rig.load(copy.dir, map));
    if (start) rig.battle.start(events);
  }
  Battle& b() { return rig.battle; }
  const Database& db() { return rig.db; }
  int id(const char* story) { return b().unitByStoryId(story); }
  Unit& unit(const char* story) { return b().units[(size_t)id(story)]; }
  Cell at(int x, int y) { return b().grid.at(x, y); }
  int terrainId(const char* name) { return rig.db.findTerrain(name); }
  const std::string& terrainAtXY(int x, int y) { return rig.db.terrains[(size_t)b().cells[(size_t)at(x, y)].terrain].id; }
  int change(const char* name) {
    const std::vector<MapChangeDef>& list = rig.map.changes;
    for (size_t i = 0; i < list.size(); ++i)
      if (list[i].id == name) return (int)i;
    return -1;
  }
  int place(const char* storyId) { return b().placeByStoryId(storyId); }
  const std::string& kind(const char* storyId) { return rig.db.structures[(size_t)b().structures[(size_t)place(storyId)].type].id; }
  int item(const char* n) { return rig.db.findItem(n); }
  int slotOf(const char* who, const char* name) {
    const Unit& u = unit(who);
    for (size_t i = 0; i < u.items.size(); ++i)
      if (u.items[i].item == item(name)) return (int)i;
    return -1;
  }
  bool sw(const char* name) { return b().storyState.switches[(size_t)rig.db.findSwitch(name)] != 0; }
  // Puts a unit on a square by hand (a test arranging the board).
  void put(const char* who, int x, int y) {
    Unit& u = unit(who);
    if (u.cell != kNoCell) b().occupant[(size_t)u.cell] = -1;
    u.cell = at(x, y);
    b().occupant[(size_t)u.cell] = u.id;
    b().refreshSight();
  }
  bool run(const Action& a, bool quiet = false) {
    events.clear();
    std::string why;
    const bool ok = b().apply(a, events, &why);
    if (!ok && !quiet) std::printf("  refused: %s\n", why.c_str());
    return ok;
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
  // The blue endTurn, then red's whole turn by the computer, the way the player runs it. Returns the events of red's turn.
  std::vector<Event> redTurn() {
    std::vector<Event> all;
    run(Action::endTurn());
    all.insert(all.end(), events.begin(), events.end());
    for (int i = 0; i < 40 && !b().over && b().activeSide == 1; ++i) {
      events.clear();
      aiTakeStep(b(), events);
      all.insert(all.end(), events.begin(), events.end());
    }
    return all;
  }
  // The moves cost of the unit's type to enter the square (<= 0 = cannot).
  int cost(const char* who, int x, int y) { return b().enterCost(at(x, y), b().typeOf(unit(who)).moveType); }
  bool canReach(const char* who, int x, int y) {
    Reach r;
    b().reachable(id(who), r);
    return r.canStop(at(x, y));
  }
  bool sees(int team, int x, int y) { return b().visibleTo(team, at(x, y)); }
};

}  // namespace mctest
