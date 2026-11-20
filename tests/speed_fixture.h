// Shared by the speed-turn-order tests (docs/TURN_ORDER.md): a scene of tests/fixtures/speed-order with a map written for the test.
//
// Numbers of the fixture: threshold 100, preview 8, a round every 10 ticks, a turn costs 100 / 80 / 60 (both / one / neither). Speeds: scout 20,
// knight / twin / dummy / frail / mage / dancer 10, brute 5, stiff 0. Damage = str + might - def (sword 4, fire 6, bolt 7; woods +2 def); hp: dummy 40,
// knight / scout / twin / stiff 30, mage / dancer 20, frail 5, brute 40. "fire" (range 2-5, radius 1) charges at 25 per tick = 4 ticks, "bolt"
// (range 2-4, one unit) at 20 = 5 ticks, "mend" (heal 10, ally, range 1-3) at 20, "hush" (sleep, aimed at a square) at 25.
#pragma once
#include <functional>
#include <memory>
#include <string>
#include <vector>

#include "area_fixture.h"

namespace speedtest {

using namespace ote;
using areatest::plain;
using areatest::U;
using skilltest::Copy;
using skilltest::World;

struct Scene {
  Copy c;
  std::unique_ptr<World> w;
  Scene(const std::string& name, const std::vector<std::string>& rows, const std::vector<U>& units, bool bothAi = false,
        const std::function<void(Copy&)>& tweak = nullptr, const char* gridType = "square")
      : c(name, "speed-order") {
    if (std::string(gridType) == "hex") c.swap("game.json", "\"formatVersion\": 1,", "\"formatVersion\": 1, \"grid\": { \"type\": \"hex\", \"layout\": \"pointy\" },");
    c.put("maps/field.json", areatest::mapJson((int)rows[0].size(), (int)rows.size(), rows, units, bothAi, gridType));
    if (tweak) tweak(c);
    w.reset(new World("field", c.dir));
  }
  // A whole map written by the test (structures, resources, two human sides...).
  Scene(const std::string& name, const std::string& mapText, const std::function<void(Copy&)>& tweak = nullptr) : c(name, "speed-order") {
    c.put("maps/field.json", mapText);
    if (tweak) tweak(c);
    w.reset(new World("field", c.dir));
  }
  Battle& b() { return w->b(); }
  const Database& db() { return w->db(); }
  Cell at(int x, int y) { return b().grid.at(x, y); }
  int weapon(const char* id) { return db().findWeapon(id); }
  int ability(const char* id) { return db().findAbility(id); }
  int id(const char* story) { return w->id(story); }
  Unit& unit(const char* story) { return w->unit(story); }
  // The story id of the unit that has the turn ("" = nobody).
  std::string active() { return b().activeUnit < 0 ? "" : b().units[(size_t)b().activeUnit].storyId; }
  bool go(const char* who, int x, int y, std::string* why = nullptr) { return w->apply(Action::move(id(who), at(x, y)), why); }
  bool hit(const char* who, const char* target, const char* weaponId, std::string* why = nullptr) {
    return w->apply(Action::attack(id(who), id(target), weapon(weaponId)), why);
  }
  // An attack aimed at a square (an area weapon), with the unit standing there as its target.
  bool shoot(const char* who, const char* weaponId, int x, int y, std::string* why = nullptr) {
    Action a = Action::attack(id(who), -1, weapon(weaponId));
    a.cell = at(x, y);
    a.target = b().unitAt(a.cell);
    return w->apply(a, why);
  }
  bool cast(const char* who, const char* abilityId, const char* target, std::string* why = nullptr) {
    return w->apply(Action::useAbility(id(who), ability(abilityId), id(target)), why);
  }
  bool wait(const char* who) { return w->apply(Action::wait(id(who))); }
  // The active unit waits (its turn ends); false when nobody has the turn.
  bool pass() { return b().activeUnit >= 0 && w->apply(Action::wait(b().activeUnit)); }
  // `n` turns in a row each ended by Wait, as the story ids in the order the units had them (the first is the one that has the turn now).
  std::string turns(int n) {
    std::string out;
    for (int i = 0; i < n && !b().over && b().activeUnit >= 0; ++i) {
      out += (out.empty() ? "" : " ") + active();
      pass();
    }
    return out;
  }
  // Plays until `who` has the turn (everybody else just waits); false when that did not happen within 200 turns.
  bool until(const char* who) {
    for (int i = 0; i < 200 && !b().over; ++i) {
      if (active() == who) return true;
      pass();
    }
    return false;
  }
  std::string refused(const Action& a) {
    std::string why;
    return b().validate(a, &why) ? std::string("(allowed)") : why;
  }
  // The preview as story ids; a charged action is "<caster>*".
  std::string preview(int n = 8) {
    std::vector<TurnEntry> order;
    b().turnPreview(n, order);
    std::string out;
    for (const TurnEntry& e : order) out += (out.empty() ? "" : " ") + b().units[(size_t)e.unit].storyId + (e.charged ? "*" : "");
    return out;
  }
  int count(EventType t) { return w->count(t); }
  // The first event of a type in the last action's events (nullptr = none).
  const Event* find(EventType t) {
    for (const Event& e : w->events)
      if (e.type == t) return &e;
    return nullptr;
  }
};

}  // namespace speedtest
