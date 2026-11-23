// Swapping items on the trade screen (docs/ITEMS.md "SwapItems"): the core action, with full packs, accessories, the weapon in hand,
// a reorder inside one pack, and that the same orders give the same result twice.
#include "battle_fixture.h"
#include "core/battle/save.h"

using namespace ote;
using fixture::World;
using fixture::addItems;
using fixture::carry;
using namespace fixture::items;

namespace {

std::vector<int> ids(const std::vector<ItemStack>& v) {
  std::vector<int> out;
  for (const ItemStack& s : v) out.push_back(s.item);
  return out;
}
int moved(const World& w) {
  int n = 0;
  for (const Event& e : w.events) n += e.type == EventType::ItemMoved;
  return n;
}

// Blue 0 and 1 side by side, blue 2 far away, red 3.
void two(World& w, bool carried = false) {
  addItems(w, carried);
  w.db.rules.items.inventorySize = 2;
}
#define TWO_WORLD(w) \
  World w({"....."}, {{"soldier", 0, 0, 0, false}, {"soldier", 0, 1, 0, false}, {"soldier", 0, 3, 0, false}, {"soldier", 1, 4, 0, false}}); \
  two(w)

}  // namespace

TEST(swap_items_works_between_two_full_packs) {
  TWO_WORLD(w);
  carry(w, 0, {Potion, Boots});
  carry(w, 1, {Ring, Salve});
  CHECK(w.begin());
  Battle& b = w.battle;
  CHECK(!w.act(Action::giveItem(0, 0, 1)));                       // a move still needs room
  CHECK(w.act(Action::swapItems(0, 1, 1, 0)));                    // boots <-> ring
  CHECK(ids(b.units[0].items) == std::vector<int>({Potion, Ring}));
  CHECK(ids(b.units[1].items) == std::vector<int>({Boots, Salve}));
  CHECK_EQ(moved(w), 2);                                          // one entry for each item
  CHECK(!b.units[0].done && !b.units[0].moved);                   // free, like the other trades
  CHECK_EQ(b.maxHp(b.units[0]), 10);                              // the boots left with their bonus ...
  CHECK_EQ(b.maxHp(b.units[1]), 14);                              // ... and arrived at the neighbour
  CHECK(w.act(Action::swapItems(0, 1, 1, 0)));                    // and back, as often as wanted
  CHECK(ids(b.units[0].items) == std::vector<int>({Potion, Boots}));
}

TEST(swap_items_is_refused_when_it_makes_no_sense) {
  TWO_WORLD(w);
  carry(w, 0, {Potion, Boots});
  carry(w, 1, {Ring});
  carry(w, 2, {Salve});
  CHECK(w.begin());
  Battle& b = w.battle;
  CHECK(!b.validate(Action::swapItems(0, 2, 1, 0)));              // no such slot of its own
  CHECK(!b.validate(Action::swapItems(0, 0, 1, 1)));              // no such slot of the neighbour's
  CHECK(!b.validate(Action::swapItems(0, -1, 1, 0)));
  CHECK(!b.validate(Action::swapItems(0, 0, 2, 0)));              // not next to it
  CHECK(!b.validate(Action::swapItems(0, 0, 3, 0)));              // an enemy
  CHECK(!b.validate(Action::swapItems(0, 0, 9, 0)));              // nobody
  CHECK(!b.validate(Action::swapItems(0, 0, -1, 0)));
  CHECK(!b.validate(Action::swapItems(0, 1, 0, 1)));              // the same item with itself
  CHECK(b.validate(Action::swapItems(0, 0, 1, 0)));
  CHECK(w.act(Action::wait(0)));
  CHECK(!b.validate(Action::swapItems(0, 0, 1, 0)));              // the unit has acted
}

TEST(swap_items_inside_one_pack_reorders_it) {
  TWO_WORLD(w);
  carry(w, 0, {Potion, Boots});
  CHECK(w.begin());
  Battle& b = w.battle;
  CHECK(w.act(Action::swapItems(0, 0, 0, 1)));
  CHECK(ids(b.units[0].items) == std::vector<int>({Boots, Potion}));
  CHECK_EQ(moved(w), 1);
  CHECK_EQ(b.maxHp(b.units[0]), 14);
}

TEST(swap_items_changes_the_weapon_in_hand_like_a_move_does) {
  World w({"....."}, {{"soldier", 0, 0, 0, false}, {"archer", 0, 1, 0, false}, {"soldier", 1, 4, 0, false}});
  addItems(w, true);
  w.db.rules.items.inventorySize = 2;
  w.db.weapons[1].group = "bow";
  w.db.unitTypes[0].wields = {"bow"};
  carry(w, 0, {SwordItem, Potion});
  carry(w, 1, {BowItem, CannonItem});
  CHECK(w.begin());
  Battle& b = w.battle;
  CHECK_EQ(b.equippedSlot(b.units[0]), 0);
  CHECK(w.act(Action::swapItems(0, 0, 1, 1)));                    // the sword goes, the cannon comes: nothing it can wield
  CHECK_EQ(b.equippedSlot(b.units[0]), -1);
  CHECK(ids(b.units[1].items) == std::vector<int>({BowItem, SwordItem}));
  CHECK(w.act(Action::swapItems(1, 0, 1, 1)));                    // a reorder puts the sword in hand
  CHECK(ids(b.units[1].items) == std::vector<int>({SwordItem, BowItem}));
}

TEST(swap_items_keeps_the_marks_and_uses_of_each_item) {
  TWO_WORLD(w);
  carry(w, 0, {Potion}, 0);                                       // marked to drop
  carry(w, 1, {Salve});
  CHECK(w.begin());
  Battle& b = w.battle;
  b.units[0].items[0].uses = 1;
  CHECK(w.act(Action::swapItems(0, 0, 1, 0)));
  CHECK(b.units[1].items[0].item == Potion && b.units[1].items[0].uses == 1 && b.units[1].items[0].drop);
  CHECK(b.units[0].items[0].item == Salve && !b.units[0].items[0].drop);
}

TEST(swap_items_plays_the_same_every_time_and_survives_a_save) {
  auto run = [](std::vector<int>& a, std::vector<int>& c, std::string& saved) {
    TWO_WORLD(w);
    carry(w, 0, {Potion, Boots});
    carry(w, 1, {Ring, Salve});
    CHECK(w.begin());
    CHECK(w.act(Action::swapItems(0, 1, 1, 0)));
    CHECK(w.act(Action::swapItems(0, 0, 0, 1)));
    CHECK(w.act(Action::swapItems(1, 1, 0, 0)));
    a = ids(w.battle.units[0].items);
    c = ids(w.battle.units[1].items);
    Json j;
    saveBattleState(w.battle, j);
    saved = j.dump();
  };
  std::vector<int> a1, c1, a2, c2;
  std::string s1, s2;
  run(a1, c1, s1);
  run(a2, c2, s2);
  CHECK(a1 == a2 && c1 == c2);
  CHECK(s1 == s2);
  CHECK(!s1.empty());
}
