// The camp of the summon tests (docs/SRPG.md "Summoning", step A1): a lord who can call a hound, on the miniature
// game of abilities_fixture.h.
#pragma once
#include "abilities_fixture.h"

namespace abtest {

// A hound type (hp6 atk3 def1 mov3, one item that would drop) next to the fixture's three types; returns its index.
inline int addHound(AbWorld& w, const char* id = "hound", const char* name = "Hound") {
  fixture::addItems(w);
  UnitTypeDef hound = w.db.unitTypes[0];
  hound.id = id;
  hound.name = name;
  hound.stats = {6, 3, 1, 3};
  hound.tags = {"soldier"};  // the sword's versus table knows this tag
  ItemGrant potion;
  potion.item = fixture::items::Potion;
  potion.uses = 2;
  potion.drop = true;
  hound.items = {potion};
  w.db.unitTypes.push_back(hound);
  return (int)w.db.unitTypes.size() - 1;
}

// Blue soldier 0 "lord" (level 3) at (2,2), red soldier 1 "foe" at (7,2), blue archer 2 "bard" at (0,0) so blue
// survives its lord. Both soldiers have "call", a summon of the hound with the settings the test gives. 9 x 5, open.
struct Camp {
  AbWorld w;
  int hound = -1, call = -1;
  explicit Camp(bool progression = false)
      : w(Square, false, {{"soldier", 0, 2, 2, false}, {"soldier", 1, 7, 2, false}, {"archer", 0, 0, 0, false}},
          fogtest::field(9, 5), {"lord", "foe", "bard"}) {
    w.map.units[0].level = 3;
    w.db.rules.progression = progression;
    if (progression) {
      w.db.rules.fXpHit = "30";
      w.db.rules.fXpKill = "60";
    }
    hound = addHound(w);
  }
  // Adds the ability (`edit` changes its settings first), gives it to both soldiers, begins the battle.
  template <class F>
  void setup(F edit) {
    AbilityDef d = AbWorld::summon("call", hound);
    edit(d);
    call = w.give("soldier", d);
    CHECK(w.begin());
  }
  void setup() {
    setup([](AbilityDef&) {});
  }
  // The id of the `skip`-th unit `summoner` made, -1 if none.
  int summonOf(int summoner, int skip = 0) const {
    for (const Unit& u : w.battle.units)
      if (u.summoner == summoner && skip-- == 0) return u.id;
    return -1;
  }
  // Blue ends its turn; red's soldier steps next to the lord and defeats it (events from here on only).
  void redKillsLord() {
    CHECK(w.act(Action::endTurn()));
    w.mut(0).hp = 1;
    w.battle.occupant[(size_t)w.cellOf(1)] = -1;
    w.mut(1).cell = w.at(1, 2);
    w.battle.occupant[(size_t)w.at(1, 2)] = 1;
    w.events.clear();
    CHECK(w.act(Action::attack(1, 0, 0)));
  }
};

}  // namespace abtest
