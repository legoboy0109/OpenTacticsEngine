// Repair at the start of a side's turn (docs/ECONOMY.md "Repair and supply"): the amount, the price and its
// rounding, partial repairs, the structure filter, the order of things at the turn start, and the interplay
// with maximum HP from research and accessories.
#include "supply_fixture.h"

using namespace ote;
using namespace suptest;

namespace {

// Blue's soldier (10 HP, cost 100) at (0,0) on a hospital of blue; red's soldier far away.
struct Hurt : SupplyWorld {
  Hurt() : SupplyWorld(field(), {{"soldier", 0, 0, 0, false}, {"soldier", 1, 6, 2, false}}) { place("hospital", 0, 0, 0); }
  // Starts, hurts the soldier to `hp`, plays a round: blue's next turn start has repaired it.
  bool turn(int hp) {
    if (!begin()) return false;
    unit(0, 0).hp = hp;
    events.clear();
    return nextTurn();
  }
  int hp() { return unit(0, 0).hp; }
};

}  // namespace

TEST(repair_heals_the_percentage_and_pays_in_proportion) {
  Hurt w;
  CHECK(w.turn(3));
  CHECK_EQ(w.hp(), 5);                // 20% of 10 = 2
  CHECK_EQ(w.funds(), 1000 - 20);     // 100 x 2/10
  std::vector<Event> rep = w.all(EventType::UnitRepaired);
  CHECK_EQ(rep.size(), size_t(1));
  CHECK_EQ(rep[0].a, 0);
  CHECK_EQ(rep[0].b, 2);
  CHECK_EQ(rep[0].c, 5);
  CHECK_EQ(rep[0].data.size(), size_t(1));
  CHECK_EQ(rep[0].data[0], 20);
  // The payment is announced first, as ResourceChanged.
  int paid = -1, healed = -1;
  for (size_t i = 0; i < w.events.size(); ++i) {
    if (w.events[i].type == EventType::ResourceChanged && w.events[i].c == -20) paid = (int)i;
    if (w.events[i].type == EventType::UnitRepaired) healed = (int)i;
  }
  CHECK(paid >= 0 && healed > paid);
  CHECK_EQ(w.events[(size_t)paid].d, 980);
}

TEST(repair_amount_is_rounded_up_and_capped_at_what_is_missing) {
  Hurt w;
  w.db.rules.supply.repairAmount = 15;  // 1.5 of 10 -> 2
  CHECK(w.turn(1));
  CHECK_EQ(w.hp(), 3);
  Hurt v;
  v.db.rules.supply.repairAmount = 1;   // 0.1 -> 1: always at least a point
  CHECK(v.turn(1));
  CHECK_EQ(v.hp(), 2);
  Hurt c;
  CHECK(c.turn(9));                     // 2 would be too much: only 1 is missing
  CHECK_EQ(c.hp(), 10);
  CHECK_EQ(c.funds(), 1000 - 10);
  Hurt full;
  CHECK(full.turn(10));                 // nothing missing: nothing happens
  CHECK_EQ(full.count(EventType::UnitRepaired), 0);
  CHECK_EQ(full.funds(), 1000);
}

TEST(repair_price_is_rounded_up_per_resource) {
  Hurt w;
  w.db.unitTypes[0].cost = {{0, 105}};
  w.db.rules.supply.repairAmount = 10;  // 1 HP of 10
  CHECK(w.turn(4));
  CHECK_EQ(w.hp(), 5);
  CHECK_EQ(w.funds(), 1000 - 11);       // 10.5 -> 11
}

TEST(repair_cost_share_scales_the_price_and_zero_is_free) {
  Hurt half;
  half.db.rules.supply.repairCostShare = 50;
  CHECK(half.turn(3));
  CHECK_EQ(half.funds(), 1000 - 10);
  Hurt free;
  free.db.rules.supply.repairCostShare = 0;
  CHECK(free.turn(3));
  CHECK_EQ(free.hp(), 5);
  CHECK_EQ(free.funds(), 1000);
  std::vector<Event> rep = free.all(EventType::UnitRepaired);
  CHECK_EQ(rep.size(), size_t(1));
  CHECK(rep[0].data.empty());           // free: no price listed
  for (const Event& e : free.events) CHECK(e.type != EventType::ResourceChanged);
}

TEST(repair_a_unit_type_without_cost_repairs_free) {
  SupplyWorld w(field(), {{"archer", 0, 0, 0, false}, {"soldier", 1, 6, 2, false}});  // the archer has no cost
  w.place("hospital", 0, 0, 0);
  CHECK(w.begin());
  w.unit(0, 0).hp = 2;
  w.events.clear();
  CHECK(w.nextTurn());
  CHECK_EQ(w.unit(0, 0).hp, 4);         // 20% of 8 = 1.6 -> 2
  CHECK_EQ(w.funds(), 1000);
  CHECK_EQ(w.all(EventType::UnitRepaired)[0].data.size(), size_t(0));
}

TEST(repair_cannot_pay_it_all_heals_what_it_can_afford) {
  Hurt w;
  w.map.sides[0].resources = {{0, 15}};  // 2 HP cost 20; 1 HP costs 10
  CHECK(w.turn(3));
  CHECK_EQ(w.hp(), 4);
  CHECK_EQ(w.funds(), 5);
  CHECK_EQ(w.all(EventType::UnitRepaired)[0].b, 1);
  Hurt broke;
  broke.map.sides[0].resources = {{0, 9}};  // not even 1 HP
  CHECK(broke.turn(3));
  CHECK_EQ(broke.hp(), 3);
  CHECK_EQ(broke.funds(), 9);
  CHECK_EQ(broke.count(EventType::UnitRepaired), 0);
}

TEST(repair_with_several_resources_each_is_proportional_and_all_must_be_affordable) {
  Hurt w;
  w.db.resources.push_back({"ore", "Ore"});
  w.db.unitTypes[0].cost = {{0, 100}, {1, 50}};
  w.map.sides[0].resources = {{0, 1000}, {1, 5}};  // ore pays for only 1 HP (5 per HP)
  CHECK(w.turn(3));
  CHECK_EQ(w.hp(), 4);
  CHECK_EQ(w.funds(0), 1000 - 10);
  CHECK_EQ(w.battle.sides[0].resources[1], 0);
  std::vector<Event> rep = w.all(EventType::UnitRepaired);
  CHECK_EQ(rep[0].data.size(), size_t(2));
  CHECK_EQ(rep[0].data[0], 10);
  CHECK_EQ(rep[0].data[1], 5);
  Hurt full;
  full.db.resources.push_back({"ore", "Ore"});
  full.db.unitTypes[0].cost = {{0, 100}, {1, 50}};
  full.map.sides[0].resources = {{0, 1000}, {1, 500}};
  CHECK(full.turn(3));
  CHECK_EQ(full.hp(), 5);
  CHECK_EQ(full.battle.sides[0].resources[1], 500 - 10);
}

TEST(repair_serves_units_in_index_order_with_what_is_left) {
  SupplyWorld w(field(), {{"soldier", 0, 0, 0, false}, {"soldier", 0, 1, 0, false}, {"soldier", 1, 6, 2, false}});
  w.place("hospital", 0, 0, 0);
  w.place("hospital", 1, 0, 0);
  w.map.sides[0].resources = {{0, 30}};
  CHECK(w.begin());
  w.unit(0, 0).hp = 2;
  w.unit(1, 0).hp = 2;
  w.events.clear();
  CHECK(w.nextTurn());
  CHECK_EQ(w.unit(0, 0).hp, 4);   // 2 HP for 20
  CHECK_EQ(w.unit(1, 0).hp, 3);   // 10 left: 1 HP
  CHECK_EQ(w.funds(), 0);
}

TEST(repair_only_own_structures_and_only_the_active_side) {
  SupplyWorld w(field(), {{"soldier", 0, 0, 0, false}, {"soldier", 0, 1, 0, false}, {"soldier", 1, 5, 2, false}, {"soldier", 1, 6, 2, false}});
  w.place("hospital", 0, 0, 1);    // red's hospital under blue's soldier
  w.place("hospital", 1, 0, -1);   // a neutral one under the other
  w.place("hospital", 5, 2, 1);    // red's own, under red's soldier
  CHECK(w.begin());
  for (Unit& u : w.battle.units) u.hp = 3;
  w.events.clear();
  CHECK(w.act(Action::endTurn()));  // red's turn begins
  CHECK_EQ(w.unit(5, 2).hp, 5);     // red's soldier on red's structure
  CHECK_EQ(w.unit(6, 2).hp, 3);     // no structure
  CHECK_EQ(w.unit(0, 0).hp, 3);     // blue's turn has not come
  CHECK(w.act(Action::endTurn()));
  CHECK_EQ(w.unit(0, 0).hp, 3);     // an enemy structure repairs nothing
  CHECK_EQ(w.unit(1, 0).hp, 3);     // neither does a neutral one
  CHECK_EQ(w.count(EventType::UnitRepaired), 1);
}

TEST(repair_filter_lists_unit_types_and_tags) {
  SupplyWorld w(field(), {{"soldier", 0, 0, 0, false}, {"tank", 0, 1, 0, false}, {"plane", 0, 2, 0, false}, {"soldier", 1, 6, 2, false}});
  w.place("workshop", 0, 0, 0);    // repairs tag "armor"
  w.place("workshop", 1, 0, 0);
  w.place("workshop", 2, 0, 0);
  CHECK(w.begin());
  w.unit(0, 0).hp = 3;
  w.unit(1, 0).hp = 3;
  w.unit(2, 0).hp = 3;
  w.events.clear();
  CHECK(w.nextTurn());
  CHECK_EQ(w.unit(0, 0).hp, 3);    // soldier: not taken
  CHECK_EQ(w.unit(1, 0).hp, 7);    // tank (20 HP): 20% = 4; has no cost: free
  CHECK_EQ(w.unit(2, 0).hp, 3);    // plane: not taken
  // By unit type id: the airfield lists the tag "air", plane carries it.
  SupplyWorld a(field(), {{"plane", 0, 0, 0, false}, {"soldier", 0, 1, 0, false}, {"soldier", 1, 6, 2, false}});
  a.place("airfield", 0, 0, 0);
  a.place("airfield", 1, 0, 0);
  a.db.structures[Airfield].repairs = {"plane"};  // now by type id
  CHECK(a.begin());
  a.unit(0, 0).hp = 3;
  a.unit(1, 0).hp = 3;
  CHECK(a.nextTurn());
  CHECK_EQ(a.unit(0, 0).hp, 5);
  CHECK_EQ(a.unit(1, 0).hp, 3);
}

TEST(repair_follows_maximum_hp_from_research_and_accessories) {
  // Research: +2 HP for soldiers makes the maximum 12; 20% of 12 is 2.4 -> 3.
  Hurt r;
  TechnologyDef vigor;
  vigor.id = vigor.name = "vigor";
  vigor.turns = 0;
  vigor.bonusTypes = {0};
  vigor.bonus = {2, 0, 0, 0};
  r.db.research = {vigor};
  CHECK(r.begin());
  r.battle.sides[0].researched[0] = 1;
  CHECK_EQ(r.battle.maxHp(r.battle.units[0]), 12);
  r.unit(0, 0).hp = 3;
  r.events.clear();
  CHECK(r.nextTurn());
  CHECK_EQ(r.unit(0, 0).hp, 6);
  CHECK_EQ(r.funds(), 1000 - 25);  // 3 of 12 HP of 100: 25
  // It never heals past the maximum with the bonus.
  r.unit(0, 0).hp = 11;
  CHECK(r.nextTurn());
  CHECK_EQ(r.unit(0, 0).hp, 12);
  // An accessory (+4 HP) counts the same way.
  Hurt a;
  fixture::addItems(a);
  fixture::carry(a, 0, {fixture::items::Boots});
  CHECK(a.begin());
  CHECK_EQ(a.battle.maxHp(a.battle.units[0]), 14);
  a.unit(0, 0).hp = 13;
  CHECK(a.nextTurn());
  CHECK_EQ(a.unit(0, 0).hp, 14);
}

TEST(repair_comes_after_income_and_before_story_events_of_the_turn) {
  Hurt w;
  w.db.rules.economy = true;
  w.place("city", 1, 0, 0);          // income 100
  w.map.sides[0].resources = {{0, 0}};
  CHECK(w.load(R"([{"id":"t","when":{"turn":"every"},"do":[{"say":"tick"}]}])").empty());
  CHECK(w.begin());
  // Turn 1: income pays for no repair yet (the unit is whole). Hurt it and play a round.
  w.unit(0, 0).hp = 3;
  w.events.clear();
  CHECK(w.nextTurn());
  // The income arrives first (a broke side can still pay), then the repair, then the story's line.
  std::vector<EventType> kinds;
  bool seenTurn = false;
  for (const Event& e : w.events) {
    if (e.type == EventType::TurnStarted && e.a == 0) seenTurn = true;
    if (seenTurn) kinds.push_back(e.type);
  }
  std::vector<EventType> want = {EventType::TurnStarted, EventType::ResourceChanged, EventType::ResourceChanged,
                                 EventType::UnitRepaired, EventType::Dialogue};
  CHECK(kinds == want);
  CHECK_EQ(w.unit(0, 0).hp, 5);
  CHECK_EQ(w.funds(), 100 + 100 - 20);  // turn 1 paid 100 as well
}

TEST(repair_after_the_research_countdown) {
  // A technology finishing at the same turn start is done before the repair, so its HP bonus counts.
  Hurt w;
  TechnologyDef vigor;
  vigor.id = vigor.name = "vigor";
  vigor.turns = 1;
  vigor.bonusTypes = {0};
  vigor.bonus = {10, 0, 0, 0};  // maximum 20
  w.db.research = {vigor};
  StructureDef lab;
  lab.id = lab.name = "lab";
  lab.moveCost = {-1};
  lab.research = true;
  w.db.structures.push_back(lab);
  w.place("lab", 3, 0, 0);
  CHECK(w.begin());
  CHECK(w.act(Action::research(0)));
  w.unit(0, 0).hp = 3;
  w.events.clear();
  CHECK(w.nextTurn());
  CHECK_EQ(w.count(EventType::ResearchFinished), 1);
  CHECK_EQ(w.unit(0, 0).hp, 7);  // 20% of the new maximum 20 = 4
  int finished = -1, healed = -1;
  for (size_t i = 0; i < w.events.size(); ++i) {
    if (w.events[i].type == EventType::ResearchFinished) finished = (int)i;
    if (w.events[i].type == EventType::UnitRepaired) healed = (int)i;
  }
  CHECK(finished >= 0 && healed > finished);
}

TEST(repair_preview_says_what_the_next_turn_start_will_do) {
  Hurt w;
  CHECK(w.begin());
  RepairPlan p;
  CHECK(!w.battle.repairPreview(0, p));     // not hurt
  w.unit(0, 0).hp = 3;
  CHECK(w.battle.repairPreview(0, p));
  CHECK_EQ(p.heal, 2);
  CHECK_EQ(p.cost.size(), size_t(1));
  CHECK_EQ(p.cost[0], 20);
  CHECK(!w.battle.repairPreviewAt(0, w.at(3, 3 - 1), p));  // a square without a structure
  CHECK(w.battle.repairPreviewAt(0, w.at(0, 0), p));
  w.battle.sides[0].resources[0] = 12;      // it follows the funds: only 1 HP is affordable
  CHECK(w.battle.repairPreview(0, p));
  CHECK_EQ(p.heal, 1);
  CHECK_EQ(p.cost[0], 10);
  w.battle.sides[0].resources[0] = 0;
  CHECK(!w.battle.repairPreview(0, p));
  CHECK(!w.battle.repairPreview(99, p));
}

TEST(repair_is_off_without_the_rule) {
  Hurt w;
  w.db.rules.supply.repair = false;
  CHECK(w.turn(3));
  CHECK_EQ(w.hp(), 3);
  CHECK_EQ(w.count(EventType::UnitRepaired), 0);
  RepairPlan p;
  CHECK(!w.battle.repairPreview(0, p));
}

TEST(repair_is_deterministic_and_a_copy_plays_the_same) {
  Hurt a, b;
  CHECK(a.turn(3));
  CHECK(b.turn(3));
  CHECK_EQ(a.hp(), b.hp());
  Battle copy = a.battle;
  std::vector<Event> e1, e2;
  CHECK(a.battle.apply(Action::endTurn(), e1) && a.battle.apply(Action::endTurn(), e1));
  CHECK(copy.apply(Action::endTurn(), e2) && copy.apply(Action::endTurn(), e2));
  CHECK_EQ(e1.size(), e2.size());
  CHECK_EQ(a.battle.units[0].hp, copy.units[0].hp);
  CHECK_EQ(a.battle.sides[0].resources[0], copy.sides[0].resources[0]);
}
