// Doors and chests: where the unlock ABILITY (the trait) may be used from follows the kind's "from" (docs/SRPG.md section 10 "Reach").
// "on" = from one of the structure's own squares, whatever the ability's range; "next" = beside it; "either" = the range decides.
// Game: tests/fixtures/srpg-doors (chest c1 at 2,0 on the hall map; tink has deft [range 1-2, lockpick], sneak has pick [range 1]).
#include <algorithm>

#include "core/ai/ai_internal.h"
#include "srpg_doors_fixture.h"

using namespace ote;
using namespace doorstest;

namespace {

void chestFrom(DCopy& c, const char* from) {
  c.swap("data/structures.json", "\"chest\": { \"opens\": \"chest_open\" }", std::string("\"chest\": { \"opens\": \"chest_open\", \"from\": \"") + from + "\" }");
}

void arrange(Doors& w, const char* who, int x, int y) {
  w.put(who, x, y);
  w.unit(who).moved = w.unit(who).done = false;
}

bool inTargets(Doors& w, const char* who, const char* ability, Cell from, Cell cell) {
  std::vector<int> us;
  std::vector<Cell> cs;
  w.b().abilityTargets(w.id(who), w.db().findAbility(ability), from, us, cs);
  return std::find(cs.begin(), cs.end(), cell) != cs.end();
}

Action use(Doors& w, const char* who, const char* ability, int x, int y) {
  return Action::useAbility(w.id(who), w.db().findAbility(ability), -1, w.at(x, y));
}

}  // namespace

TEST(doors_from_on_chest_the_trait_works_from_the_chests_own_square_only) {
  Doors w("doors_from_on", "hall", [](DCopy& c) { chestFrom(c, "on"); });
  CHECK(w.db().structures[(size_t)w.db().findStructure("chest")].place.fromOn);
  // Next to it (2,1): refused with a plain reason, in every range, and the menu entry says why.
  arrange(w, "tink", 2, 1);
  CHECK(w.why(use(w, "tink", "deft", 2, 0)).find("stand on the chest") != std::string::npos);
  CHECK(!inTargets(w, "tink", "deft", w.at(2, 1), w.at(2, 0)));
  std::vector<AbilityOption> menu;
  w.b().abilityOptions(w.id("tink"), w.at(2, 1), menu);
  CHECK(menu.size() == 1 && !menu[0].ok && menu[0].why.find("stand on the chest") != std::string::npos);
  // Two squares away (inside deft's range 1-2): still refused.
  arrange(w, "tink", 2, 2);
  CHECK(w.why(use(w, "tink", "deft", 2, 0)).find("stand on the chest") != std::string::npos);
  // On it: offered, opens, the lockpick wears, the contents are given.
  arrange(w, "tink", 2, 0);
  CHECK(inTargets(w, "tink", "deft", w.at(2, 0), w.at(2, 0)));
  w.b().abilityOptions(w.id("tink"), w.at(2, 0), menu);
  CHECK(menu.size() == 1 && menu[0].ok);
  const int slot = w.slotOf("tink", "lockpick");
  CHECK(w.run(use(w, "tink", "deft", 2, 0)));
  CHECK_STR(w.kind("c1"), "chest_open");
  CHECK_EQ(w.unit("tink").items[(size_t)w.slotOf("tink", "lockpick")].uses, w.db().items[(size_t)w.item("lockpick")].uses - 1);
  CHECK(slot >= 0 && w.has("tink", "potion") && w.gold() == 50);
  CHECK(w.unit("tink").done);
}

TEST(doors_from_on_chest_without_the_lockpick_the_square_alone_is_not_enough) {
  Doors w("doors_from_on_nopick", "hall", [](DCopy& c) { chestFrom(c, "on"); });
  arrange(w, "tink", 2, 0);
  w.unit("tink").items.erase(w.unit("tink").items.begin() + w.slotOf("tink", "lockpick"));
  CHECK(w.why(use(w, "tink", "deft", 2, 0)).find("needs a Lockpick") != std::string::npos);
  // Sneak's pick (range 1, no item) on the same chest: from on it, fine.
  arrange(w, "sneak", 2, 0);
  CHECK(w.run(use(w, "sneak", "pick", 2, 0)));
  CHECK_STR(w.kind("c1"), "chest_open");
}

TEST(doors_from_next_door_the_trait_works_from_beside_it_and_not_from_on_it) {
  // The cell door (5,1) says next; the trait works from next to it as before. (A door cannot be stood on, so the "on it" side is the chest.)
  Doors w("doors_from_next_door", "hall", [](DCopy& c) {
    c.swap("data/structures.json", "\"keys\": [\"brass_key\", \"master_key\"] }", "\"keys\": [\"brass_key\", \"master_key\"], \"from\": \"next\" }");
  });
  w.put("hero", 3, 1);
  arrange(w, "sneak", 4, 1);
  CHECK(inTargets(w, "sneak", "pick", w.at(4, 1), w.at(5, 1)));
  CHECK(w.run(use(w, "sneak", "pick", 5, 1)));
  CHECK_STR(w.kind("d1"), "cell_door_open");
  // A "next" chest: from on it refused (with the reason); from beside it as before; from two away (deft range 1-2) still refused? no: range reaches.
  Doors c("doors_from_next_chest", "hall", [](DCopy& d) { chestFrom(d, "next"); });
  arrange(c, "tink", 2, 0);
  CHECK(c.why(use(c, "tink", "deft", 2, 0)).find("it opens from the next square") != std::string::npos);
  CHECK(!inTargets(c, "tink", "deft", c.at(2, 0), c.at(2, 0)));
  arrange(c, "tink", 2, 2);  // two away: deft's range reaches, "next" does not forbid it (the ability's range still decides the distance)
  CHECK(c.why(use(c, "tink", "deft", 2, 0)).find("out of range") == std::string::npos);
  arrange(c, "tink", 2, 1);
  CHECK(c.run(use(c, "tink", "deft", 2, 0)));
  CHECK_STR(c.kind("c1"), "chest_open");
}

TEST(doors_from_either_is_unchanged_including_a_range_of_two) {
  Doors w("doors_from_either");  // the fixture's chest has no "from": either
  arrange(w, "tink", 2, 2);      // deft reaches 1-2
  CHECK(inTargets(w, "tink", "deft", w.at(2, 2), w.at(2, 0)));
  arrange(w, "tink", 2, 0);      // on it: the range minimum is 1, so deft cannot (as before)
  CHECK(w.why(use(w, "tink", "deft", 2, 0)).find("out of range") != std::string::npos);
  arrange(w, "tink", 2, 2);
  const int slot = w.slotOf("tink", "lockpick");
  CHECK(slot >= 0 && w.run(use(w, "tink", "deft", 2, 0)));
  CHECK_STR(w.kind("c1"), "chest_open");
  // pick (range 1) works from beside, not from on it.
  Doors v("doors_from_either2");
  arrange(v, "sneak", 2, 0);
  CHECK(v.why(use(v, "sneak", "pick", 2, 0)).find("out of range") != std::string::npos);
  arrange(v, "sneak", 2, 1);
  CHECK(v.run(use(v, "sneak", "pick", 2, 0)));
}

TEST(doors_from_on_chest_a_computer_thief_steps_onto_it_and_opens_it) {
  // market: r1 (goesFor chests) is a thief with the trait; c1 is locked and "on".
  Doors w("doors_from_on_ai", "market", [](DCopy& c) {
    chestFrom(c, "on");
    c.swap("maps/market.json", "\"contents\": [ \"potion\" ], \"locked\": false", "\"contents\": [ \"potion\" ]");
    c.swap("maps/market.json", "\"type\": \"raider\", \"id\": \"r1\"", "\"type\": \"thief\", \"id\": \"r1\"");
  });
  CHECK(w.b().isClosedChest(w.place("c1")) && w.st("c1").locked);
  bool stood = false;
  for (int t = 0; t < 4 && !w.b().over; ++t) {
    w.redTurn();
    stood = stood || w.unit("r1").cell == w.at(4, 1);
    if (w.kind("c1") == "chest_open") break;
    w.unit("lord").done = false;
  }
  CHECK_STR(w.kind("c1"), "chest_open");
  CHECK(w.has("r1", "potion"));
  CHECK_EQ(w.unit("r1").cell, w.at(4, 1));  // it opened it from the chest's own square
  CHECK(stood);
}

TEST(doors_from_either_chest_a_computer_thief_opens_it_with_its_trait_from_beside) {
  Doors w("doors_from_next_ai", "market", [](DCopy& c) {
    c.swap("maps/market.json", "\"contents\": [ \"potion\" ], \"locked\": false", "\"contents\": [ \"potion\" ]");
    c.swap("maps/market.json", "\"type\": \"raider\", \"id\": \"r1\"", "\"type\": \"thief\", \"id\": \"r1\"");
  });
  for (int t = 0; t < 4 && !w.b().over && w.kind("c1") != "chest_open"; ++t) {
    w.redTurn();
    w.unit("lord").done = false;
  }
  CHECK_STR(w.kind("c1"), "chest_open");
  CHECK_EQ(w.b().grid.distance(w.unit("r1").cell, w.at(4, 1)), 1);
}
