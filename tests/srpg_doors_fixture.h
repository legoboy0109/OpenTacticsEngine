// Helpers of the doors, keys, chests and visiting tests (docs/SRPG.md section 10): a writable copy of tests/fixtures/srpg-doors
// and a loaded battle on one of its maps.
//
// "hall" (10 x 6, no fog): a wall column at x 5 with a cell door d1 (5,1; needs a brass or master key) and a latched door d2 (5,3;
// not locked); chest c1 (2,0; potion and 50 gold), strongbox c2 (2,5; heavy axe, only the strong key), hamlet v1 (3,3), camp v2
// (1,3; repeatable), hamlet v3 (8,0; red's) and hamlet v4 (8,4; blue's). Blue: hero (fighter, 4,1, sword and brass key), sneak
// (thief with "pick", 4,3), tink (tinker with "deft" and a lockpick, 2,1), sage (mystic with an unlock staff and a wild one, 1,5);
// red g1 at 7,2. Events: d1 opened -> switch opened; v1 visited -> switch visited and 30 gold; v4 destroyed -> razed; c1 opened by
// tink -> chest_done.
// "gatehouse" (10 x 5): a wall row at y 2 with the iron gate g1 (4..5, 2; 20 hit points, defense 3, blocks sight) in it. Blue hero
// (4,3; sword, master key), brute (5,3; heavy axe), sneak (7,3), sage (2,4); red r1 at 5,0. "gatehouse_fog" is the same with fog.
// "corridor" (9 x 3): wall column x 4 with cell door d1 at 4,1; blue lord at 7,1; red r1 at 1,1 with a brass key, goesFor doors.
// "market" (11 x 5): unlocked chests c1 (4,1; potion) and c2 (4,3; 40 gold), blue's hamlet v1 (8,4); blue lord at 10,0; red r1 (1,1)
// goesFor chests, r2 (1,3) unmarked, r3 (9,3) goesFor villages.
#pragma once
#include <functional>

#include "srpg_ranks_fixture.h"

namespace doorstest {

using namespace rankstest;

struct DCopy : Copy {
  explicit DCopy(const std::string& name) : Copy(name, "srpg-doors") {
    makeDir(dir + "/maps");
    static const char* const extra[] = {"data/structures.json", "data/abilities.json", "maps/hall.json", "maps/gatehouse.json",
                                        "maps/gatehouse_fog.json", "maps/corridor.json", "maps/market.json"};
    for (const char* f : extra) {
      std::string text;
      if (readTextFile(ote_test::sourceDir() + "/tests/fixtures/srpg-doors/" + f, text)) writeTextFile(dir + "/" + f, text);
    }
  }
};

// A battle that has started, on the fixture (or a changed copy of it).
struct Doors {
  DCopy copy;
  savetest::Rig rig;
  std::vector<Event> events;
  explicit Doors(const char* name, const char* map = "hall", const std::function<void(DCopy&)>& tweak = nullptr, bool start = true)
      : copy(name) {
    if (tweak) tweak(copy);
    CHECK(rig.load(copy.dir, map));
    if (start) rig.battle.start(events);
  }
  Battle& b() { return rig.battle; }
  const Database& db() { return rig.db; }
  int id(const char* story) { return b().unitByStoryId(story); }
  Unit& unit(const char* story) { return b().units[(size_t)id(story)]; }
  int item(const char* n) { return rig.db.findItem(n); }
  int place(const char* storyId) { return b().placeByStoryId(storyId); }
  StructureState& st(const char* storyId) { return b().structures[(size_t)place(storyId)]; }
  const std::string& kind(const char* storyId) { return rig.db.structures[(size_t)st(storyId).type].id; }
  Cell at(int x, int y) { return b().grid.at(x, y); }
  int slotOf(const char* story, const char* name) {
    const Unit& u = unit(story);
    for (size_t i = 0; i < u.items.size(); ++i)
      if (u.items[i].item == item(name)) return (int)i;
    return -1;
  }
  bool has(const char* story, const char* name) { return slotOf(story, name) >= 0; }
  bool sw(const char* name) { return b().storyState.switches[(size_t)rig.db.findSwitch(name)] != 0; }
  int gold(const char* side = "blue") {
    for (size_t i = 0; i < b().sides.size(); ++i)
      if (b().sides[i].id == side) return b().sides[i].resources[0];
    return -1;
  }
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
  std::string why(const Action& a) {
    std::string w;
    return b().validate(a, &w) ? "" : (w.empty() ? "(no reason)" : w);
  }
  // Open / visit by story id; `key` = the name of the key item (the unit's first of that kind), "" = none.
  Action open(const char* who, const char* placeId, const char* key = "") {
    return Action::openPlace(id(who), place(placeId), *key ? slotOf(who, key) : -1);
  }
  Action visit(const char* who, const char* placeId) { return Action::visitPlace(id(who), place(placeId)); }
  Action hit(const char* who, const char* placeId, const char* weapon) {
    return Action::breakPlace(id(who), place(placeId), rig.db.findWeapon(weapon));
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
  // The blue (team 0) endTurn then red's whole turn by the computer, the way the player runs it. Returns the events of red's turn.
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
  std::vector<PlaceOption> options(const char* who) {
    std::vector<PlaceOption> out;
    b().placeOptions(id(who), unit(who).cell, out);
    return out;
  }
};

}  // namespace doorstest
