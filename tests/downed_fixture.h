// The game of the downed-unit tests (docs/SRPG.md "Downed units and revive"): tests/ability_conditions_fixture.h with rules.downed, revive
// abilities, a revive item and an "autoRevive" condition.
//
// Units (ids in this order): blue hero (mage, 30 HP) 1,1; blue pal (knight, 30 HP) 0,1; red bandit (brigand, 20 HP) 2,1; red thug (brigand) 6,1.
// Hero's abilities: raise (ally, range 1, revives with 50%), arise (unit, range 0-3, area radius 1, revives with 5 HP), hex (an inflict on an
// enemy); it carries a sword and a Phoenix (usable on an adjacent ally: revives with 7 HP). Damage = atk - def, hit 100: hero hits for 5.
#pragma once
#include "ability_conditions_fixture.h"

namespace downedtest {

using namespace ote;
using namespace acf;

inline Opts downedOpts(const std::string& downedRules = "{}") {
  Opts o;
  o.rules = "\"downed\":" + downedRules + ",";
  o.conditions +=
      ",{\"id\":\"reraise\",\"name\":\"Reraise\",\"kind\":\"helpful\",\"until\":\"cured\",\"autoRevive\":{\"percent\":20}}";
  o.abilities +=
      ",{\"id\":\"raise\",\"name\":\"Raise\",\"target\":\"ally\",\"range\":[1,1],\"effect\":{\"revive\":{\"percent\":50}}},"
      "{\"id\":\"arise\",\"name\":\"Arise\",\"target\":\"unit\",\"range\":[0,3],\"effect\":{\"revive\":5},\"area\":{\"shape\":\"radius\",\"radius\":1}}";
  o.mageAbilities = "[\"raise\",\"arise\",\"hex\"]";
  o.moreItems = ",{\"id\":\"phoenix\",\"name\":\"Phoenix\",\"kind\":\"usable\",\"uses\":1,\"target\":\"ally\",\"revive\":7}";
  o.mageItems = "[\"iron_sword\",\"phoenix\"]";
  o.units = "{\"type\":\"mage\",\"id\":\"hero\",\"side\":\"blue\",\"x\":1,\"y\":1},"
            "{\"type\":\"knight\",\"id\":\"pal\",\"side\":\"blue\",\"x\":0,\"y\":1},"
            "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":2,\"y\":1},"
            "{\"type\":\"brigand\",\"id\":\"thug\",\"side\":\"red\",\"x\":6,\"y\":1}";
  return o;
}

// A unit lying downed on its square, as a fall leaves it (and as a save restores it).
inline void lay(World& w, const char* who) {
  Unit& u = w.unit(who);
  u.alive = false;
  u.downed = true;
  u.hp = 0;
  u.conditions.clear();
}

// Hero (sword, 5 damage) strikes the bandit next to it, left with `hp`.
inline bool heroStrikes(World& w, int hp) {
  w.unit("bandit").hp = hp;
  return w.run(Action::attack(w.id("hero"), w.id("bandit"), 0));
}

inline const Event* withFlag(const World& w, EventType t, int flag) {
  for (const Event& e : w.events)
    if (e.type == t && (e.flags & flag)) return &e;
  return nullptr;
}

inline int slotOf(World& w, const char* who, const char* item) {
  const Unit& u = w.unit(who);
  for (size_t i = 0; i < u.items.size(); ++i)
    if (u.items[i].item == w.rig.db.findItem(item)) return (int)i;
  return -1;
}

}  // namespace downedtest
