// Map weapons: who may fire one, the forecast, the strikes, the uses and the counter rule (docs/SRPG.md "Map weapons").
// Game: tests/fixtures/srpg-mapweapon.
#include "srpg_mapweapon_fixture.h"

using namespace ote;
using namespace mwtest;

namespace {

// The archer steps onto the ballista b1 (4,1); g1 stands 5 squares away.
void archerToBallista(MapWorld& w) { CHECK(w.run(Action::move(w.id("ar"), w.at(4, 1)))); }

}  // namespace

TEST(mapweapon_a_unit_standing_on_a_ballista_may_attack_at_the_weapons_range) {
  MapWorld w("mapweapon_range");
  // Off the ballista the archer's bow (range 2) reaches nobody.
  CHECK_EQ(w.options("ar"), 0);
  archerToBallista(w);
  int mapOnes = 0;
  CHECK_EQ(w.options("ar", &mapOnes), 1);  // g1 at distance 5; g2 at 7 is too far
  CHECK_EQ(mapOnes, 1);
  std::vector<AttackOption> o;
  w.b().attackOptions(w.id("ar"), w.unit("ar").cell, o);
  CHECK_EQ(o[0].target, w.id("g1"));
  CHECK_EQ(o[0].weapon, w.bolt());
  // The forecast has the weapon's numbers and the unit's own stats: 5 + 10 - 1.
  Forecast f;
  CHECK(w.b().forecast(w.id("ar"), w.id("g1"), w.bolt(), w.unit("ar").cell, f));
  CHECK(f.attack.can);
  CHECK_EQ(f.attack.weapon, w.bolt());
  CHECK_EQ(f.attack.damage, 14);
  CHECK_EQ(f.attack.hit, 100);
  CHECK(!f.counter.can);  // a grunt's sword does not reach 5 squares
  // The attack goes through the normal action and is marked as the structure's.
  CHECK(w.run(Action::attack(w.id("ar"), w.id("g1"), w.bolt())));
  const Event* e = w.attackEvent();
  CHECK(e != nullptr);
  CHECK(e->flags & kMapWeapon);
  CHECK(!(e->flags & kMapWeaponSpent));
  CHECK(e->flags & kHit);
  CHECK_EQ(e->d, 14);
  CHECK_EQ(w.unit("g1").hp, 26);
  CHECK_EQ(w.st("b1").weaponUses, 2);
}

TEST(mapweapon_the_unit_off_the_structure_cannot_use_it) {
  MapWorld w("mapweapon_off");
  Forecast f;
  CHECK(!w.b().forecast(w.id("ar"), w.id("g1"), w.bolt(), w.unit("ar").cell, f));
  CHECK(!w.run(Action::attack(w.id("ar"), w.id("g1"), w.bolt()), true));
  // Forecast from a square it could stand on: the structure's weapon belongs to the square.
  CHECK(w.b().forecast(w.id("ar"), w.id("g1"), w.bolt(), w.at(4, 1), f));
  CHECK(!w.b().forecast(w.id("ar"), w.id("g1"), w.bolt(), w.at(3, 2), f));
}

TEST(mapweapon_a_move_then_attack_works_and_a_weapon_that_forbids_it_does_not) {
  MapWorld w("mapweapon_move");
  archerToBallista(w);
  CHECK(w.unit("ar").moved);
  CHECK_EQ(w.options("ar"), 1);  // the bolt may be fired after moving (afterMove is on by default)
  MapWorld v("mapweapon_nomove", "field", [](MCopy& c) { c.swap("data/weapons.json", "\"range\": [2, 6]", "\"range\": [2, 6], \"afterMove\": false"); });
  archerToBallista(v);
  CHECK_EQ(v.options("ar"), 0);
}

TEST(mapweapon_who_may_use_it_the_by_list_then_anyone) {
  MapWorld w("mapweapon_who");
  // The knight is no archer: the ballista is not his, the catapult (no list) is.
  w.put("g1", 7, 1);  // within 6 of both
  CHECK(w.run(Action::move(w.id("kn"), w.at(4, 0))));  // the catapult c1
  CHECK_EQ(w.options("kn"), 1);
  Forecast f;
  CHECK(w.b().forecast(w.id("kn"), w.id("g1"), w.bolt(), w.unit("kn").cell, f));
  CHECK_EQ(f.attack.damage, 17);  // 8 + 10 - 1
  MapWorld v("mapweapon_who2");
  v.put("g1", 7, 1);
  v.put("kn", 4, 1);  // the knight on the ballista
  CHECK_EQ(v.options("kn"), 0);
  std::string why;
  CHECK_EQ(v.b().mapWeaponStructure(v.unit("kn"), v.unit("kn").cell, &why), -1);
  CHECK_STR(why, "Only Archer can use this");
  CHECK_EQ(v.b().mapWeaponStructure(v.unit("ar"), v.at(4, 1)), v.place("b1"));
  CHECK(!v.b().forecast(v.id("kn"), v.id("g1"), v.bolt(), v.unit("kn").cell, f));
}

TEST(mapweapon_the_weapons_own_rank_counts_unless_the_structure_ignores_it) {
  auto ranked = [](MCopy& c) { c.swap("data/weapons.json", "\"group\": \"ballista\"", "\"group\": \"ballista\", \"rank\": \"c\""); };
  MapWorld w("mapweapon_rank", "field", ranked);
  w.put("g1", 7, 1);
  w.put("ar", 4, 1);
  CHECK_EQ(w.options("ar"), 0);  // the archer has no rank in that weapon group
  std::string why;
  CHECK_EQ(w.b().mapWeaponStructure(w.unit("ar"), w.unit("ar").cell, &why), -1);
  CHECK_STR(why, "Needs rank C");
  MapWorld v("mapweapon_rank2", "field", [&](MCopy& c) {
    ranked(c);
    c.swap("data/structures.json", "\"weapon\": \"ballista_bolt\", \"uses\": 3", "\"weapon\": \"ballista_bolt\", \"ignoreRank\": true, \"uses\": 3");
  });
  v.put("g1", 7, 1);
  v.put("ar", 4, 1);
  CHECK_EQ(v.options("ar"), 1);
}

TEST(mapweapon_a_wielder_lock_of_the_weapon_counts_too) {
  MapWorld w("mapweapon_lock", "field", [](MCopy& c) { c.swap("data/weapons.json", "\"group\": \"ballista\"", "\"group\": \"ballista\", \"wieldedByTypes\": [\"knight\"]"); });
  w.put("g1", 7, 1);
  w.put("ar", 4, 1);
  w.put("kn", 4, 0);
  CHECK_EQ(w.options("ar"), 0);  // the archer fits the ballista's list, but the weapon is the knights'
  CHECK_EQ(w.options("kn"), 1);  // the catapult has no list; the knight is who the weapon allows
}

TEST(mapweapon_uses_count_down_a_spent_weapon_is_gone_and_becomes_swaps_the_kind) {
  MapWorld w("mapweapon_uses");
  archerToBallista(w);
  const int ar = w.id("ar"), g1 = w.id("g1");
  CHECK(w.run(Action::attack(ar, g1, w.bolt())));
  CHECK_EQ(w.st("b1").weaponUses, 2);
  w.fresh("ar");
  CHECK(w.run(Action::attack(ar, g1, w.bolt())));
  CHECK_EQ(w.st("b1").weaponUses, 1);
  CHECK_STR(w.kind("b1"), "ballista");
  w.fresh("ar");
  CHECK(w.run(Action::attack(ar, g1, w.bolt())));
  const Event* e = w.attackEvent();
  CHECK(e && (e->flags & kMapWeapon) && (e->flags & kMapWeaponSpent));
  CHECK_EQ(w.count(EventType::StructureUpgraded), 1);  // it becomes the wreck
  CHECK_STR(w.kind("b1"), "ballista_wreck");
  CHECK_EQ(w.b().mapWeaponUses(w.place("b1")), -2);  // no weapon any more
  CHECK(!w.unit("g1").alive);  // 14 + 14 + 14 is more than its 40 hit points: it fell on the third
  w.fresh("ar");
}

TEST(mapweapon_a_spent_weapon_stays_when_the_kind_does_not_become_anything) {
  MapWorld w("mapweapon_stay", "field", [](MCopy& c) { c.swap("data/structures.json", ", \"becomes\": \"ballista_wreck\"", ""); });
  w.put("g1", 7, 1);
  w.unit("g1").hp = 99;
  w.unit("g1").stats[0] = 99;
  w.put("ar", 4, 2);  // b2, one use
  CHECK_EQ(w.options("ar"), 1);
  CHECK(w.run(Action::attack(w.id("ar"), w.id("g1"), w.bolt())));
  CHECK(w.attackEvent()->flags & kMapWeaponSpent);
  CHECK_STR(w.kind("b2"), "ballista");  // the structure stays
  CHECK_EQ(w.st("b2").weaponUses, 0);
  CHECK_EQ(w.b().mapWeaponUses(w.place("b2")), 0);
  w.fresh("ar");
  CHECK_EQ(w.options("ar"), 0);  // spent: not offered any more
  CHECK(!w.run(Action::attack(w.id("ar"), w.id("g1"), w.bolt()), true));
  std::string why;
  CHECK_EQ(w.b().mapWeaponStructure(w.unit("ar"), w.unit("ar").cell, &why), -1);
  CHECK_STR(why, "spent");
}

TEST(mapweapon_an_unlimited_weapon_never_runs_out) {
  MapWorld w("mapweapon_unlimited");
  w.put("g1", 7, 1);
  w.unit("g1").stats[0] = 500;
  w.unit("g1").hp = 500;
  w.put("kn", 4, 0);
  for (int i = 0; i < 6; ++i) {
    w.fresh("kn");
    CHECK(w.run(Action::attack(w.id("kn"), w.id("g1"), w.bolt())));
  }
  CHECK_EQ(w.st("c1").weaponUses, -1);
  CHECK_STR(w.kind("c1"), "catapult");
}

TEST(mapweapon_a_defender_does_not_counter_with_the_structures_weapon) {
  // The grunt hits the archer standing on the ballista from next to it: the bow (range 2) does not answer at 1, and the
  // bolt (range 2 to 6) is never used to counter, so there is no counter at all.
  MapWorld w("mapweapon_counter");
  w.put("ar", 4, 1);
  w.put("g1", 5, 1);
  Forecast f;
  CHECK(w.b().forecast(w.id("g1"), w.id("ar"), w.sword(), w.unit("g1").cell, f));
  CHECK(!f.counter.can);
  // A grunt with a bow hits a knight with nothing in hand on the catapult from 2 squares: the catapult's range covers the grunt,
  // and still nothing answers, because only the equipped carried weapon counters.
  MapWorld v("mapweapon_counter2", "field", [](MCopy& c) {
    c.swap("data/units.json", "\"wields\": [\"sword\"], \"ranks\": { \"sword\": \"d\" }, \"items\": [\"sword\"]",
           "\"wields\": [\"sword\", \"bow\"], \"ranks\": { \"sword\": \"d\", \"bow\": \"d\" }, \"items\": [\"short_bow\"]");
  });
  v.put("kn", 4, 0);
  v.unit("kn").items.clear();
  v.put("g1", 6, 0);
  const int bow = v.db().findWeapon("short_bow");
  CHECK(v.b().forecast(v.id("g1"), v.id("kn"), bow, v.unit("g1").cell, f));
  CHECK(!f.counter.can);
  // With its sword in hand the knight answers a neighbour with the sword, as it would anywhere.
  MapWorld x("mapweapon_counter3");
  x.put("kn", 4, 0);
  x.put("g1", 5, 0);
  CHECK(x.b().forecast(x.id("g1"), x.id("kn"), x.sword(), x.unit("g1").cell, f));
  CHECK(f.counter.can);
  CHECK_EQ(f.counter.weapon, x.sword());
}

TEST(mapweapon_a_weapon_the_unit_carries_anyway_is_the_carried_one) {
  // (the knight's class has to be able to wield the bolt's group for the copy in its pack to count)
  MapWorld w("mapweapon_carried", "field", [](MCopy& c) { c.swap("data/units.json", "\"wields\": [\"sword\"], \"ranks\": { \"sword\": \"c\" }", "\"wields\": [\"sword\", \"ballista\"], \"ranks\": { \"sword\": \"c\" }"); });
  w.put("g1", 7, 1);
  w.put("kn", 4, 0);
  w.unit("kn").items.push_back(ItemStack{w.db().findItem("ballista_bolt"), 5, false});
  std::vector<AttackOption> o;
  w.b().attackOptions(w.id("kn"), w.unit("kn").cell, o);
  CHECK_EQ((int)o.size(), 1);
  CHECK(!o[0].mapWeapon);
  CHECK(w.run(Action::attack(w.id("kn"), w.id("g1"), w.bolt())));
  CHECK(!(w.attackEvent()->flags & kMapWeapon));
  CHECK_EQ(w.st("c1").weaponUses, -1);
  // It wore the carried copy (5 uses), not the structure.
  int uses = -9;
  for (const ItemStack& s : w.unit("kn").items)
    if (s.item == w.db().findItem("ballista_bolt")) uses = s.uses;
  CHECK_EQ(uses, 4);
}

TEST(mapweapon_the_weapon_group_earns_experience_as_with_a_carried_weapon) {
  MapWorld w("mapweapon_xp");
  archerToBallista(w);
  const int group = w.db().ranks.findGroup("ballista");
  CHECK(group >= 0);
  const int before = (int)w.unit("ar").weaponXp.size() > group ? w.unit("ar").weaponXp[(size_t)group] : 0;
  CHECK(w.run(Action::attack(w.id("ar"), w.id("g1"), w.bolt())));
  CHECK((int)w.unit("ar").weaponXp.size() > group);
  CHECK(w.unit("ar").weaponXp[(size_t)group] > before);
}

TEST(mapweapon_a_hexagon_game_works_the_same) {
  MapWorld w("mapweapon_hex", "hexfield", [](MCopy& c) {
    c.swap("game.json", "\"formatVersion\": 1,", "\"formatVersion\": 1, \"grid\": { \"type\": \"hex\", \"layout\": \"pointy\" },");
    c.swap("game.json", "\"maps\": [", "\"maps\": [ { \"id\": \"hexfield\", \"name\": \"Hex field\", \"file\": \"maps/hexfield.json\" },");
  });
  CHECK(w.b().grid.isHex());
  CHECK_EQ(w.options("ar"), 0);
  CHECK(w.run(Action::move(w.id("ar"), w.at(3, 2))));
  int mapOnes = 0;
  const int n = w.options("ar", &mapOnes);
  CHECK_EQ(n, 1);
  CHECK_EQ(mapOnes, 1);
  CHECK(w.run(Action::attack(w.id("ar"), w.id("g1"), w.bolt())));
  CHECK_EQ(w.unit("g1").hp, 40 - 14);
  CHECK_EQ(w.st("b1").weaponUses, 2);
}
