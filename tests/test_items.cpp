// Items in battle (docs/ITEMS.md "Rules"): accessories, usable items, trading,
// the shared storage, shops, carried weapons, wear, drops and saves.
#include "battle_fixture.h"
#include "core/battle/save.h"

using namespace ote;
using fixture::World;
using fixture::addItems;
using fixture::carry;
using namespace fixture::items;

namespace {

int count(const World& w, EventType t) {
  int n = 0;
  for (const Event& e : w.events) n += e.type == t;
  return n;
}
std::vector<int> ids(const std::vector<ItemStack>& v) {
  std::vector<int> out;
  for (const ItemStack& s : v) out.push_back(s.item);
  return out;
}

}  // namespace

TEST(items_accessories_add_to_stats_once_per_kind) {
  World w({"....."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 1, 0, false}, {"soldier", 0, 4, 0, false}});
  addItems(w);
  carry(w, 1, {Ring, Ring, Boots});
  CHECK(w.begin());
  const Battle& b = w.battle;
  CHECK_EQ(b.stat(b.units[1], 2), 4);       // def 2 + 2, the second ring adds nothing
  CHECK_EQ(b.movePoints(b.units[1]), 4);    // boots
  CHECK_EQ(b.maxHp(b.units[1]), 14);
  CHECK_EQ(b.units[1].hp, 14);              // starts full with the bonus
  CHECK_EQ(b.stat(b.units[0], 2), 2);
  Forecast f;
  CHECK(b.forecast(0, 1, 0, w.at(0, 0), f));
  CHECK_EQ(f.attack.damage, 5 - 4);         // formulas see the bonus: a.atk - b.def
  CHECK_EQ(f.counter.damage, 5 - 2);
}

TEST(items_using_heals_spends_uses_and_ends_the_turn) {
  World w({"....."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 4, 0, false}});
  addItems(w);
  carry(w, 0, {Potion});
  CHECK(w.begin());
  Battle& b = w.battle;
  std::vector<int> targets;
  b.useTargets(0, 0, targets);
  CHECK(targets.empty());                                   // full HP: nothing to do
  std::string why;
  CHECK(!b.apply(Action::useItem(0, 0, 0), w.events, &why));
  CHECK(why.find("nothing") != std::string::npos);
  b.units[0].hp = 3;
  CHECK(w.act(Action::useItem(0, 0, 0)));
  CHECK_EQ(b.units[0].hp, 8);
  CHECK(b.units[0].done);
  CHECK_EQ(b.units[0].items[0].uses, 1);
  CHECK_EQ(count(w, EventType::ItemUsed), 1);
  CHECK_EQ(count(w, EventType::UnitHealed), 1);
  CHECK(!w.act(Action::useItem(0, 0, 0)));                  // already acted
  CHECK(w.act(Action::endTurn()) && w.act(Action::endTurn()));
  CHECK(w.act(Action::useItem(0, 0, 0)));                   // 8 -> 10: only what is missing
  CHECK_EQ(b.units[0].hp, 10);
  CHECK(b.units[0].items.empty());                          // the last use: it is gone
  CHECK_EQ(count(w, EventType::ItemBroke), 1);
}

TEST(items_ally_items_need_a_neighbour_and_raise_is_for_good) {
  World w({"....."}, {{"soldier", 0, 0, 0, false}, {"soldier", 0, 1, 0, false}, {"soldier", 0, 3, 0, false},
                      {"soldier", 1, 4, 0, false}});
  addItems(w);
  carry(w, 0, {Salve, Tonic});
  CHECK(w.begin());
  Battle& b = w.battle;
  b.units[1].hp = 2;
  b.units[2].hp = 2;
  std::vector<int> targets;
  b.useTargets(0, 0, targets);
  CHECK(targets.size() == 1 && targets[0] == 1);            // unit 2 is too far, the user is not an ally of itself
  CHECK(!w.act(Action::useItem(0, 0, 2)));
  CHECK(!w.act(Action::useItem(0, 0, 0)));
  CHECK(w.act(Action::useItem(0, 0, 1)));
  CHECK_EQ(b.units[1].hp, 10);
  CHECK_EQ(b.units[0].items.size(), (size_t)2);             // unlimited uses: still there
  CHECK(w.act(Action::endTurn()) && w.act(Action::endTurn()));
  b.units[0].hp = 6;
  CHECK(w.act(Action::useItem(0, 1, 0)));                   // tonic: hp +3, atk +2, at full HP too
  CHECK_EQ(b.units[0].stats[0], 13);
  CHECK_EQ(b.units[0].stats[1], 7);
  CHECK_EQ(b.units[0].hp, 9);                               // the new hit points arrive filled
  CHECK_EQ(count(w, EventType::StatsRaised), 1);
  CHECK_EQ(b.units[0].items.size(), (size_t)1);
}

TEST(items_trading_between_neighbours_of_one_side) {
  World w({"....."}, {{"soldier", 0, 0, 0, false}, {"soldier", 0, 1, 0, false}, {"soldier", 0, 3, 0, false},
                      {"soldier", 1, 4, 0, false}});
  addItems(w);
  w.db.rules.items.inventorySize = 2;
  carry(w, 0, {Potion, Boots});
  carry(w, 1, {Ring});
  CHECK(w.begin());
  Battle& b = w.battle;
  std::vector<int> partners;
  b.tradePartners(0, partners);
  CHECK(partners.size() == 1 && partners[0] == 1);
  CHECK(!w.act(Action::giveItem(0, 0, 2)));                 // not next to it
  CHECK(!w.act(Action::giveItem(0, 0, 3)));                 // an enemy
  CHECK(w.act(Action::giveItem(0, 1, 1)));                  // the boots go over
  CHECK(ids(b.units[0].items) == std::vector<int>({Potion}));
  CHECK(ids(b.units[1].items) == std::vector<int>({Ring, Boots}));
  CHECK_EQ(b.maxHp(b.units[0]), 10);
  CHECK_EQ(b.units[0].hp, 10);                              // cut down to the lower maximum
  CHECK_EQ(b.maxHp(b.units[1]), 14);
  CHECK_EQ(b.units[1].hp, 10);                              // a bonus does not heal
  CHECK(!b.units[0].done && !b.units[0].moved);             // trading is free
  CHECK(!w.act(Action::giveItem(0, 0, 1)));                 // the other one is full
  CHECK(w.act(Action::takeItem(0, 1, 0)));                  // takes the ring
  CHECK(ids(b.units[0].items) == std::vector<int>({Potion, Ring}));
  CHECK(!w.act(Action::takeItem(0, 1, 0)));                 // now full itself
  CHECK(w.act(Action::discardItem(0, 0)));
  CHECK(ids(b.units[0].items) == std::vector<int>({Ring}));
  CHECK_EQ(count(w, EventType::ItemMoved), 3);
  CHECK(w.act(Action::move(0, w.at(0, 0) + 0)) == false);   // (still where it was; a move elsewhere is allowed)
  CHECK(w.act(Action::wait(0)));
}

TEST(items_shared_storage_follows_the_setting) {
  World w({"c...."}, {{"soldier", 0, 0, 0, true}, {"soldier", 0, 1, 0, false}, {"soldier", 0, 3, 0, false},
                      {"soldier", 1, 4, 0, false}});
  addItems(w);
  w.own(0, 0, 0);
  carry(w, 0, {Potion});
  carry(w, 1, {Ring});
  carry(w, 2, {Boots});
  ItemGrant stored;
  stored.item = Tonic;
  stored.uses = 1;
  CHECK(w.begin());
  Battle& b = w.battle;
  CHECK(!w.act(Action::storeItem(0, 0)));                   // off
  w.db.rules.items.storage = ItemStorage::Structure;
  CHECK(w.act(Action::storeItem(0, 0)));                    // stands on its side's city
  CHECK(!w.act(Action::storeItem(1, 0)));                   // does not
  w.db.rules.items.storage = ItemStorage::Leader;
  CHECK(w.act(Action::storeItem(1, 0)));                    // next to the leader
  CHECK(!w.act(Action::storeItem(2, 0)));
  w.db.rules.items.storage = ItemStorage::Anywhere;
  CHECK(w.act(Action::storeItem(2, 0)));
  CHECK(ids(b.sides[0].storage) == std::vector<int>({Potion, Ring, Boots}));
  CHECK_EQ(b.maxHp(b.units[2]), 10);
  CHECK(w.act(Action::fetchItem(2, 1)));                    // the ring
  CHECK(ids(b.units[2].items) == std::vector<int>({Ring}));
  CHECK(ids(b.sides[0].storage) == std::vector<int>({Potion, Boots}));
  CHECK(!w.act(Action::fetchItem(2, 5)));
  CHECK(b.sides[1].storage.empty());
  CHECK(!b.units[2].done);
}

TEST(items_shops_sell_for_resources) {
  World w({"c.c.."}, {{"soldier", 0, 0, 0, false}, {"soldier", 0, 1, 0, false}, {"soldier", 0, 2, 0, false},
                      {"soldier", 1, 4, 0, false}});
  addItems(w, true);
  w.db.rules.items.inventorySize = 2;
  w.own(2, 0, 1);                                           // the second city is the enemy's
  w.map.sides[0].resources = {{0, 260}};
  CHECK(w.begin());
  Battle& b = w.battle;
  std::vector<int> shop;
  b.shopItems(0, shop);
  CHECK(shop == std::vector<int>({Potion, SwordItem}));
  b.shopItems(1, shop);
  CHECK(shop.empty());                                      // no structure
  b.shopItems(2, shop);
  CHECK(shop.empty());                                      // an enemy's
  CHECK(!w.act(Action::buyItem(1, Potion)));
  CHECK(!w.act(Action::buyItem(0, Ring)));                  // not sold here
  CHECK(w.act(Action::buyItem(0, Potion)));
  CHECK_EQ(b.sides[0].resources[0], 210);
  CHECK(w.act(Action::buyItem(0, SwordItem)));
  CHECK_EQ(b.sides[0].resources[0], 10);
  CHECK(ids(b.units[0].items) == std::vector<int>({Potion, SwordItem}));
  CHECK_EQ(b.units[0].items[1].uses, 2);                    // new
  CHECK(!w.act(Action::buyItem(0, Potion)));                // cannot pay
  b.sides[0].resources[0] = 500;
  CHECK(!w.act(Action::buyItem(0, Potion)));                // no room and no storage
  w.db.rules.items.storage = ItemStorage::Leader;           // (no leader near: the storage still takes deliveries)
  CHECK(w.act(Action::buyItem(0, Potion)));
  CHECK(ids(b.sides[0].storage) == std::vector<int>({Potion}));
  CHECK(!b.units[0].done);
}

TEST(items_carried_weapons_are_what_a_unit_fights_with) {
  // Blue soldier 0 (sword item + bow item), red soldier 1 next to it (no weapon), red archer 3 two away.
  World w({"....."}, {{"soldier", 0, 1, 0, false}, {"soldier", 1, 2, 0, false}, {"soldier", 0, 0, 0, false},
                      {"archer", 1, 3, 0, false}});
  addItems(w, true);
  w.db.weapons[1].group = "bow";
  w.db.unitTypes[0].wields = {"bow"};                       // soldiers: sword by list, bows by group
  carry(w, 0, {Potion, BowItem, SwordItem, CannonItem});
  carry(w, 1, {});
  carry(w, 2, {CannonItem});                                // cannot wield it
  carry(w, 3, {BowItem});
  CHECK(w.begin());
  Battle& b = w.battle;
  std::vector<int> weapons;
  b.weaponsOf(b.units[0], weapons);
  CHECK(weapons == std::vector<int>({1, 0}));               // bow first (inventory order), the cannon is not for it
  CHECK_EQ(b.equippedSlot(b.units[0]), 1);
  CHECK_EQ(b.equippedSlot(b.units[2]), -1);
  std::vector<AttackOption> opts;
  b.attackOptions(2, w.at(1, 0), opts);
  CHECK(opts.empty());                                      // unarmed
  b.attackOptions(0, w.at(1, 0), opts);
  CHECK_EQ(opts.size(), (size_t)2);                         // bow -> archer (2 away), sword -> soldier
  Forecast f;
  CHECK(b.forecast(0, 1, 0, w.at(1, 0), f));
  CHECK(!f.counter.can);                                    // the defender carries nothing
  CHECK(!b.forecast(0, 1, 2, w.at(1, 0), f));               // the cannon is not usable
  CHECK(!w.act(Action::equip(0, 0)));                       // a potion
  CHECK(!w.act(Action::equip(0, 3)));                       // not wieldable
  CHECK(!w.act(Action::equip(0, 1)));                       // already in hand
  CHECK(w.act(Action::attack(0, 1, 0)));                    // attacking with the sword takes it in hand
  CHECK(ids(b.units[0].items) == std::vector<int>({SwordItem, Potion, BowItem, CannonItem}));
  CHECK_EQ(b.units[0].items[0].uses, 1);                    // one strike made
  CHECK_EQ(b.units[1].hp, 7);
  // Red's turn: the archer shoots; blue answers only with what is in hand (the sword cannot reach).
  CHECK(w.act(Action::endTurn()));
  CHECK(b.forecast(3, 0, 1, w.at(3, 0), f));
  CHECK(!f.counter.can);
  CHECK(w.act(Action::endTurn()));
  CHECK(w.act(Action::equip(0, 2)));                        // the bow
  CHECK_EQ(b.equippedSlot(b.units[0]), 0);
  CHECK(ids(b.units[0].items) == std::vector<int>({BowItem, SwordItem, Potion, CannonItem}));
  CHECK_EQ(count(w, EventType::ItemEquipped), 1);
}

TEST(items_weapons_wear_out_and_break) {
  World w({"....."}, {{"soldier", 0, 1, 0, false}, {"tank", 1, 2, 0, false}});
  addItems(w, true);
  w.db.weapons[0].versus.clear();                           // swords may hit tanks here (for 0 damage)
  w.db.rules.fStrikes = "2";
  carry(w, 0, {SwordItem});                                 // 2 uses
  carry(w, 1, {CannonItem});
  w.db.unitTypes[2].stats[1] = 3;                           // the tank's answer does not kill
  w.map.units[1].stats[1] = 3;
  CHECK(w.begin());
  Battle& b = w.battle;
  CHECK(w.act(Action::attack(0, 1, 0)));
  CHECK(b.units[0].items.empty());                          // two strikes, two uses: broken
  CHECK_EQ(count(w, EventType::ItemBroke), 1);
  int strikes = 0;
  for (const Event& e : w.events) strikes += e.type == EventType::Attack && !(e.flags & kCounter);
  CHECK_EQ(strikes, 2);
  CHECK(w.act(Action::endTurn()) && w.act(Action::endTurn()));
  std::vector<AttackOption> opts;
  b.attackOptions(0, b.units[0].cell, opts);
  CHECK(opts.empty());
}

TEST(items_drops_go_to_the_one_who_strikes_the_carrier_down) {
  World w({"....."}, {{"tank", 0, 1, 0, false}, {"soldier", 1, 2, 0, false}, {"soldier", 1, 4, 0, false}});
  addItems(w);
  w.db.rules.items.inventorySize = 2;
  carry(w, 0, {Ring});
  carry(w, 1, {Potion, Boots, Tonic}, 1);                   // the boots drop; (3 items: set before the limit matters)
  w.map.units[1].items[2].drop = true;                      // and the tonic
  w.db.rules.items.inventorySize = 3;
  CHECK(w.begin());
  Battle& b = w.battle;
  w.db.rules.items.inventorySize = 2;
  b.units[1].hp = 1;
  CHECK(w.act(Action::attack(0, 1, 2)));
  CHECK(!b.units[1].alive);
  CHECK(ids(b.units[0].items) == std::vector<int>({Ring, Boots}));  // room for one
  CHECK(!b.units[0].items[1].drop);
  CHECK_EQ(count(w, EventType::ItemLost), 1);               // no storage: the tonic is lost
  CHECK_EQ(count(w, EventType::ItemGained), 1);
  CHECK_EQ(b.maxHp(b.units[0]), 24);
}

TEST(items_survive_a_save) {
  World w({"....."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 4, 0, false}});
  addItems(w, true);
  w.db.rules.items.storage = ItemStorage::Anywhere;
  carry(w, 0, {SwordItem, Potion, Boots}, 1);
  CHECK(w.begin());
  CHECK(w.act(Action::storeItem(0, 1)));
  w.battle.units[0].items[0].uses = 1;
  Json saved;
  saveBattleState(w.battle, saved);

  World w2({"....."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 4, 0, false}});
  addItems(w2, true);
  w2.db.rules.items.storage = ItemStorage::Anywhere;
  std::string err;
  CHECK(w2.formulas.init(w2.db, err));
  CHECK(w2.battle.init(w2.db, w2.map, w2.formulas, 1, err));
  CHECK(restoreBattleState(w2.battle, saved, err));
  const Battle& b = w2.battle;
  CHECK(ids(b.units[0].items) == std::vector<int>({SwordItem, Boots}));
  CHECK_EQ(b.units[0].items[0].uses, 1);
  CHECK(ids(b.sides[0].storage) == std::vector<int>({Potion}));
  CHECK_EQ(b.units[0].hp, 14);                              // above the bare maximum: allowed by the boots

  // Damage: more uses than a new one has, an item that does not exist.
  Json bad = saved;
  Json units = bad["units"], u0 = units[size_t(0)], items = u0["items"], it0 = items[size_t(0)];
  it0.set("uses", Json::makeNumber(99));
  Json list = Json::makeArray();
  list.push(it0);
  u0.set("items", list);
  Json ulist = Json::makeArray();
  ulist.push(u0);
  ulist.push(units[size_t(1)]);
  bad.set("units", ulist);
  CHECK(!restoreBattleState(w2.battle, bad, err));
  CHECK(err.find("uses") != std::string::npos);
}
