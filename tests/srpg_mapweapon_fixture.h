// Helpers of the map weapon tests (docs/SRPG.md "Map weapons"): a writable copy of tests/fixtures/srpg-mapweapon and a battle on it.
//
// "field" (12 x 3, square): blue archer ar (3,1, leader) and knight kn (3,0); red grunts g1 (9,1) and g2 (11,2), 40 hit points each.
// Structures: catapult c1 (4,0; bolt, unlimited, anyone), ballista b1 (4,1; bolt, 3 uses, archers only, becomes a wreck),
// ballista b2 (4,2; the same with its own 1 use), a tower (6,0). Bolt: range 2..6, might 10; damage = str + might - def, hit 100.
// "siege" (14 x 3): catapult c1 (5,1); blue knight kn (0,1, 60 hit points); red grunt g1 (8,1, leader).
// "hexfield" (9 x 5 hexagons; the file is not in game.json: the hexagon test makes the game a hexagon game and lists it): ballista b1 (3,2); blue
// archer ar (2,2); red grunt g1 (7,2).
#pragma once
#include <functional>

#include "srpg_ranks_fixture.h"

namespace mwtest {

using namespace rankstest;

struct MCopy : Copy {
  explicit MCopy(const std::string& name) : Copy(name, "srpg-mapweapon") {
    static const char* const extra[] = {"data/structures.json", "maps/siege.json", "maps/hexfield.json"};
    for (const char* f : extra) {
      std::string text;
      if (readTextFile(ote_test::sourceDir() + "/tests/fixtures/srpg-mapweapon/" + f, text)) writeTextFile(dir + "/" + f, text);
    }
  }
};

struct MapWorld {
  MCopy copy;
  savetest::Rig rig;
  std::vector<Event> events;
  explicit MapWorld(const char* name, const char* map = "field", const std::function<void(MCopy&)>& tweak = nullptr) : copy(name) {
    if (tweak) tweak(copy);
    CHECK(rig.load(copy.dir, map));
    rig.battle.start(events);
  }
  Battle& b() { return rig.battle; }
  const Database& db() { return rig.db; }
  int id(const char* story) { return b().unitByStoryId(story); }
  Unit& unit(const char* story) { return b().units[(size_t)id(story)]; }
  Cell at(int x, int y) { return b().grid.at(x, y); }
  int bolt() { return rig.db.findWeapon("ballista_bolt"); }
  int sword() { return rig.db.findWeapon("sword"); }
  int place(const char* storyId) { return b().placeByStoryId(storyId); }
  StructureState& st(const char* storyId) { return b().structures[(size_t)place(storyId)]; }
  const std::string& kind(const char* storyId) { return rig.db.structures[(size_t)st(storyId).type].id; }
  // Puts a unit on a square by hand (arranging the board) and gives it its turn back.
  void put(const char* who, int x, int y) {
    Unit& u = unit(who);
    if (u.cell != kNoCell) b().occupant[(size_t)u.cell] = -1;
    u.cell = at(x, y);
    b().occupant[(size_t)u.cell] = u.id;
    u.moved = u.done = false;
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
  void fresh(const char* who) { unit(who).moved = unit(who).done = false; }
  // The attack options of `who` from where it stands, as (target story id, weapon) pairs: how many are with the map weapon / any.
  int options(const char* who, int* mapOnes = nullptr) {
    std::vector<AttackOption> o;
    b().attackOptions(id(who), unit(who).cell, o);
    if (mapOnes) {
      *mapOnes = 0;
      for (const AttackOption& x : o) *mapOnes += x.mapWeapon;
    }
    return (int)o.size();
  }
  const Event* attackEvent() const {
    for (const Event& e : events)
      if (e.type == EventType::Attack) return &e;
    return nullptr;
  }
  int count(EventType t) const {
    int n = 0;
    for (const Event& e : events) n += e.type == t;
    return n;
  }
  // Red's whole turn by the computer after blue ends its own. Returns the events of red's turn.
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
};

}  // namespace mwtest
