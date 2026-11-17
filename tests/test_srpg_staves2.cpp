// Staves, stage two (docs/SRPG.md section 8 "Staves: stage two, as built"): warp, pull, swap, repair and light, in a battle.
// Game: tests/fixtures/srpg-staves2 (the boards are described in tests/srpg_staves2_fixture.h).
#include <algorithm>

#include "srpg_staves2_fixture.h"

using namespace ote;
using namespace staves2test;

namespace {
bool has(const std::vector<Cell>& v, Cell c) { return std::find(v.begin(), v.end(), c) != v.end(); }
bool hasInt(const std::vector<int>& v, int x) { return std::find(v.begin(), v.end(), x) != v.end(); }
const Event* nth(const std::vector<Event>& ev, EventType t, int n = 0) {
  for (const Event& e : ev)
    if (e.type == t && n-- == 0) return &e;
  return nullptr;
}
// Four squares around the sage on the yard are taken, so nothing can land next to it (wisp is on the fourth).
void crowdTheSage(Yard& w) {
  w.place("dax", 1, 2);
  w.place("bram", 2, 3);
  w.place("g1", 2, 1);
}
}  // namespace

// ---- warp ----

TEST(staves2_a_warp_puts_the_target_on_a_free_square_in_reach_of_the_user_and_keeps_its_turn_state) {
  Yard w("s2_warp");
  w.unit("bram").done = true;  // it has already acted: that must stay
  const Cell from = w.cellOf("bram");
  CHECK(w.use("sage", "warp_staff", "bram", 1, 1));
  CHECK(w.cellOf("bram") == w.at(1, 1));
  CHECK_EQ(w.b().occupant[(size_t)from], -1);
  CHECK_EQ(w.b().occupant[(size_t)w.at(1, 1)], w.id("bram"));
  CHECK(w.unit("bram").done && !w.unit("bram").moved);
  CHECK_EQ(w.unit("sage").items[(size_t)w.slot("sage", "warp_staff")].uses, 4);
  CHECK(w.unit("sage").done);
  // The events, in order: the use, the move, the experience.
  CHECK_EQ((int)w.events.size(), 3);
  CHECK(w.events[0].type == EventType::StaffUsed && w.events[0].c == w.id("bram") && w.events[0].d == 1);
  CHECK(w.events[1].type == EventType::UnitRelocated && w.events[1].a == w.id("bram") && w.events[1].b == from && w.events[1].c == w.at(1, 1));
  CHECK(w.events[2].type == EventType::XpGained && w.events[2].a == w.id("sage") && w.events[2].b == 10);
}

TEST(staves2_a_warp_square_is_measured_from_the_user_and_must_be_free_and_standable) {
  Yard w("s2_warp_squares");
  // Reach 4 from the sage at 2,2: 6,2 is the last square of that row; 7,2 is too far.
  CHECK_STR(w.refusal("sage", "warp_staff", "bram", 6, 2), "");
  CHECK(w.refusal("sage", "warp_staff", "bram", 7, 2).find("cannot be warped to that square") != std::string::npos);
  // Measured from the user, not from the target: bram at 2,4 could be moved 4 from itself to 6,4 but that is 6 from the user.
  CHECK(!w.refusal("sage", "warp_staff", "bram", 6, 4).empty());
  CHECK(w.refusal("sage", "warp_staff", "bram", 3, 2).find("cannot be warped") != std::string::npos);  // wisp stands there
  CHECK(!w.refusal("sage", "warp_staff", "bram", 2, 2).empty());                                       // the user's own square
  CHECK(!w.refusal("sage", "warp_staff", "bram", 3, 1).empty());                                       // the pond: a foot unit cannot stand there
  CHECK(!w.refusal("sage", "warp_staff", "bram", 3, 3).empty());                                       // the wall
  CHECK_STR(w.refusal("sage", "warp_staff", "bram"), "pick the square to warp it to");
  const std::vector<Cell> cells = w.warpCells("sage", "warp_staff", "bram");
  CHECK(has(cells, w.at(1, 1)) && has(cells, w.at(6, 2)));
  CHECK(!has(cells, w.at(7, 2)) && !has(cells, w.at(3, 1)) && !has(cells, w.at(3, 3)) && !has(cells, w.at(3, 2)) &&
        !has(cells, w.at(2, 2)) && !has(cells, w.at(2, 4)));
  // A flyer may be put on the pond.
  CHECK(has(w.warpCells("sage", "warp_staff", "wisp"), w.at(4, 1)));
  CHECK(w.use("sage", "warp_staff", "wisp", 4, 1));
  CHECK(w.cellOf("wisp") == w.at(4, 1));
}

TEST(staves2_a_warp_with_no_free_square_to_send_the_target_to_is_not_usable) {
  Yard w("s2_warp_none", "yard", [](Copy& c) {
    c.swap("data/items.json", "\"effects\": [ { \"warp\": { \"range\": 4 } } ]", "\"effects\": [ { \"warp\": { \"range\": 1 } } ]");
  });
  crowdTheSage(w);  // the four squares at distance 1 are taken (wisp is on the fourth)
  CHECK(w.warpCells("sage", "warp_staff", "bram").empty());
  CHECK_STR(w.refusal("sage", "warp_staff", "bram", 1, 1), "there is no free square in reach of the user that it could stand on");
  const StaffOption o = w.option("sage", "warp_staff");
  CHECK(!o.ok);
  CHECK_STR(o.why, "nobody in reach can be moved");
}

TEST(staves2_a_warp_can_move_a_foe_and_the_user_itself) {
  Yard w("s2_warp_foe_self");
  w.place("g2", 3, 4);  // 3 away from the sage: in the banish staff's reach
  CHECK_STR(w.refusal("sage", "banish_staff", "g2", 2, 3), "it cannot be warped to that square (too far, not free, or it could not stand there)");  // 1 away: under its minimum
  CHECK(w.use("sage", "banish_staff", "g2", 6, 2));
  CHECK(w.cellOf("g2") == w.at(6, 2));
  // The hop staff warps its user: no unit to pick, only the square.
  Yard h("s2_hop");
  CHECK_STR(h.refusal("sage", "hop_staff", nullptr, 2, 0), "");
  CHECK(!h.refusal("sage", "hop_staff", nullptr, 6, 2).empty());  // 4 away: out of its reach of 3
  CHECK(!h.refusal("sage", "hop_staff", nullptr).empty());
  CHECK(h.use("sage", "hop_staff", nullptr, 2, 0));
  CHECK(h.cellOf("sage") == h.at(2, 0) && h.unit("sage").done);
  CHECK_EQ(h.b().occupant[(size_t)h.at(2, 2)], -1);
  CHECK(h.events[1].type == EventType::UnitRelocated && h.events[1].a == h.id("sage"));
}

TEST(staves2_a_warp_in_fog_needs_a_square_the_team_sees_and_an_unseen_enemy_there_blocks_it) {
  Yard w("s2_warp_fog", "mist");
  const std::vector<Cell> cells = w.warpCells("sage", "warp_staff", "ola");
  CHECK(!has(cells, w.at(5, 2)));  // in reach (4) but nobody on the team sees it
  CHECK(has(cells, w.at(3, 2)));
  CHECK(has(cells, w.at(4, 3)));  // looks empty: an enemy hides in the brush there, unseen
  CHECK(!w.refusal("sage", "warp_staff", "ola", 5, 2).empty());
  // Sent into the hiding enemy: the use is spent, nothing moves, the use is flagged blocked.
  CHECK(w.use("sage", "warp_staff", "ola", 4, 3));
  CHECK(w.cellOf("ola") == w.at(2, 3));
  CHECK_EQ(w.unit("sage").items[(size_t)w.slot("sage", "warp_staff")].uses, 4);
  CHECK(w.events[0].type == EventType::StaffUsed && (w.events[0].flags & kBlocked) && w.events[0].data[0] == w.id("g1"));
  CHECK_EQ(w.count(EventType::UnitRelocated), 0);
}

TEST(staves2_a_moved_unit_sees_from_its_new_square) {
  Yard w("s2_warp_sight", "mist");
  CHECK(!w.blueSees(5, 2));
  CHECK(w.use("sage", "warp_staff", "ola", 3, 2));  // ola's vision of 2 now reaches 5,2
  CHECK(w.blueSees(5, 2));
  CHECK(w.blueSees("g1") == false);  // the enemy in the brush stays hidden from a unit that is not next to it
}

// ---- pull ----

TEST(staves2_a_pull_puts_the_target_on_the_free_square_next_to_the_user_nearest_to_it_ties_to_the_lower_cell) {
  Yard w("s2_pull");
  w.unit("ren").done = true;
  // 2,1 / 1,2 / 2,3 are free and all 7 from ren at 8,2; the east square is wisp's. The lowest cell number wins: 2,1.
  CHECK(w.use("sage", "recall_staff", "ren"));
  CHECK(w.cellOf("ren") == w.at(2, 1));
  CHECK(w.unit("ren").done);
  CHECK(w.events[1].type == EventType::UnitRelocated && w.events[1].b == w.at(8, 2) && w.events[1].c == w.at(2, 1));
  Yard v("s2_pull_nearest");
  v.place("ren", 2, 5);  // now 2,3 is nearest (2 away) of the free squares
  CHECK(v.b().staffPullCell(v.id("sage"), v.id("ren"), v.cellOf("sage")) == v.at(2, 3));
}

TEST(staves2_a_pull_needs_a_free_square_the_target_can_stand_on_and_a_target_that_is_not_already_next_to_the_user) {
  Yard w("s2_pull_refused");
  CHECK_STR(w.refusal("sage", "recall_staff", "wisp"), "that unit is already next to the user");
  crowdTheSage(w);
  CHECK_STR(w.refusal("sage", "recall_staff", "ren"), "there is no free square next to the user that it could stand on");
  CHECK_STR(w.option("sage", "recall_staff").why, "nobody in reach can be moved");
  // The only free square is the pond: a foot unit cannot stand there, a flyer can.
  w.place("wisp", 6, 0);
  CHECK(!w.refusal("sage", "recall_staff", "ren").empty());
  CHECK_STR(w.refusal("sage", "recall_staff", "wisp"), "");
  CHECK(w.use("sage", "recall_staff", "wisp"));
  CHECK(w.cellOf("wisp") == w.at(3, 2));
}

TEST(staves2_a_pull_can_drag_a_foe) {
  Yard w("s2_pull_foe");
  CHECK(w.use("sage", "shove_staff", "g2"));  // 3,2 is wisp's; of the free squares 2,3 is nearest to 5,5
  CHECK(w.cellOf("g2") == w.at(2, 3));
}

TEST(staves2_a_staff_with_a_heal_and_a_pull_does_both_and_each_alone_when_the_other_cannot_work) {
  Yard w("s2_heal_pull");
  w.unit("ren").hp = 10;
  CHECK(w.use("sage", "kindly_pull_staff", "ren"));
  CHECK_EQ(w.unit("ren").hp, 16);
  CHECK(w.cellOf("ren") == w.at(2, 1));
  CHECK(w.events[0].type == EventType::StaffUsed && w.events[1].type == EventType::UnitHealed && w.events[2].type == EventType::UnitRelocated);
  // Whole and already next to the user: nothing to do. The reason is the first effect's.
  Yard v("s2_heal_pull_none");
  CHECK_STR(v.refusal("sage", "kindly_pull_staff", "wisp"), "nobody in reach needs that");
  // Hurt, but no square to pull it to: only the heal happens.
  Yard x("s2_heal_pull_crowd");
  crowdTheSage(x);
  x.unit("ren").hp = 10;
  CHECK(x.use("sage", "kindly_pull_staff", "ren"));
  CHECK_EQ(x.unit("ren").hp, 16);
  CHECK(x.cellOf("ren") == x.at(8, 2));
  CHECK_EQ(x.count(EventType::UnitRelocated), 0);
  // Whole and far from the user: the pull is what the staff does.
  Yard y("s2_heal_pull_whole");
  CHECK(y.use("sage", "kindly_pull_staff", "dax"));
  CHECK_EQ(y.count(EventType::UnitHealed), 0);
  CHECK_EQ(y.count(EventType::UnitRelocated), 1);
}

// ---- swap ----

TEST(staves2_a_swap_trades_the_squares_of_the_user_and_the_target) {
  Yard w("s2_swap");
  w.unit("bram").done = true;
  CHECK(w.use("sage", "swap_staff", "bram"));
  CHECK(w.cellOf("sage") == w.at(2, 4) && w.cellOf("bram") == w.at(2, 2));
  CHECK_EQ(w.b().occupant[(size_t)w.at(2, 4)], w.id("sage"));
  CHECK_EQ(w.b().occupant[(size_t)w.at(2, 2)], w.id("bram"));
  CHECK(w.unit("bram").done && w.unit("sage").done);
  // The target first, then the user.
  CHECK_EQ(w.count(EventType::UnitRelocated), 2);
  CHECK_EQ(nth(w.events, EventType::UnitRelocated, 0)->a, w.id("bram"));
  CHECK_EQ(nth(w.events, EventType::UnitRelocated, 1)->a, w.id("sage"));
}

TEST(staves2_a_swap_needs_both_to_be_able_to_stand_on_each_others_square) {
  Yard w("s2_swap_flyer");
  // Wisp is a flyer on the pond; the sage walks and could not stand there.
  CHECK_STR(w.refusal("sage", "swap_staff", "wisp"), "they could not stand on each other's squares");
  CHECK(!hasInt(w.targets("sage", "swap_staff"), w.id("wisp")));
  // On firm ground the same pair may swap, and a flyer may be swapped onto the sage's square from the pond's far side.
  w.place("wisp", 1, 2);
  CHECK(w.use("sage", "swap_staff", "wisp"));
  CHECK(w.cellOf("wisp") == w.at(2, 2) && w.cellOf("sage") == w.at(1, 2));
  // The other way round: a flyer user is not in this fixture, so ask the rule directly: wisp could stand on the sage's square.
  CHECK(w.b().staffCanStand(w.unit("wisp"), w.at(3, 2)) && !w.b().staffCanStand(w.unit("sage"), w.at(3, 2)));
}

// ---- repair ----

TEST(staves2_a_repair_restores_uses_of_the_item_the_player_picked_and_never_offers_the_wrong_ones) {
  Yard w("s2_repair");
  w.place("ren", 2, 3);
  w.setUses("ren", "iron_blade", 3);
  w.setUses("ren", "glass_blade", 1);  // worn, but "repairable": false
  w.setUses("ren", "herb", 3);         // full
  // Not offered: the glass blade (repairable false), the short sword (unlimited), the herb (full).
  CHECK_EQ((int)w.repairSlots("sage", "fix_staff", "ren").size(), 1);
  CHECK_EQ(w.repairSlots("sage", "fix_staff", "ren")[0], w.slotOf("ren", "iron_blade"));
  CHECK_STR(w.refusal("sage", "fix_staff", "ren"), "pick the item to repair");
  CHECK_STR(w.refusal("sage", "fix_staff", "ren", -1, -1, w.slotOf("ren", "glass_blade")), "that item cannot be repaired");
  CHECK_STR(w.refusal("sage", "fix_staff", "ren", -1, -1, w.slotOf("ren", "short_sword")), "that item cannot be repaired");
  CHECK_STR(w.refusal("sage", "fix_staff", "ren", -1, -1, w.slotOf("ren", "herb")), "that item cannot be repaired");
  CHECK(w.use("sage", "fix_staff", "ren", -1, -1, w.slotOf("ren", "iron_blade")));
  CHECK_EQ(w.unit("ren").items[(size_t)w.slotOf("ren", "iron_blade")].uses, 8);
  CHECK_EQ(w.unit("ren").items[(size_t)w.slotOf("ren", "glass_blade")].uses, 1);
  const Event* r = nth(w.events, EventType::ItemRepaired);
  CHECK(r && r->a == w.id("ren") && r->b == w.item("iron_blade") && r->c == 5 && r->d == 8);
  CHECK(w.unit("sage").done);
  CHECK_EQ(w.count(EventType::XpGained), 1);
}

TEST(staves2_a_unit_with_nothing_to_repair_is_not_a_target_and_a_number_never_goes_above_the_full_uses) {
  Yard w("s2_repair_none");
  CHECK_STR(w.refusal("sage", "fix_staff", "bram"), "that unit carries nothing that needs repair");
  CHECK(!w.option("sage", "fix_staff").ok);
  CHECK_STR(w.option("sage", "fix_staff").why, "nobody in reach has an item to repair");
  // "repair": 2 on 7 of 8 uses gives 1.
  w.setUses("wisp", "iron_blade", 7);
  w.place("wisp", 2, 3);
  CHECK(w.use("sage", "patch_staff", "wisp", -1, -1, 0));
  CHECK_EQ(w.unit("wisp").items[0].uses, 8);
  CHECK_EQ(nth(w.events, EventType::ItemRepaired)->c, 1);
  // 2 of 8 gets two.
  Yard v("s2_repair_two");
  v.setUses("wisp", "iron_blade", 2);
  v.place("wisp", 2, 3);
  CHECK(v.use("sage", "patch_staff", "wisp", -1, -1, 0));
  CHECK_EQ(v.unit("wisp").items[0].uses, 4);
}

TEST(staves2_the_staff_in_use_is_not_offered_to_itself) {
  // (A "self" staff: a "team" one never picks its user, whose own square is below the reach's minimum of 1.)
  Yard w("s2_repair_self", "yard", [](Copy& c) {
    c.swap("data/items.json", "\"range\": 2, \"target\": \"team\", \"effects\": [ { \"repair\": 2 } ]",
           "\"target\": \"self\", \"effects\": [ { \"repair\": 2 } ]");
  });
  const int patch = w.slot("sage", "patch_staff");
  w.setUses("sage", "patch_staff", 2);
  // Only the patch staff itself is worn: the user is not a target.
  CHECK(w.repairSlots("sage", "patch_staff", "sage").empty());
  CHECK(w.targets("sage", "patch_staff").empty());
  // Another staff of the user's is mended.
  w.setUses("sage", "warp_staff", 2);
  const std::vector<int> slots = w.repairSlots("sage", "patch_staff", "sage");
  CHECK_EQ((int)slots.size(), 1);
  CHECK_EQ(slots[0], w.slot("sage", "warp_staff"));
  CHECK(w.use("sage", "patch_staff", "sage", -1, -1, slots[0]));
  CHECK_EQ(w.unit("sage").items[(size_t)w.slot("sage", "warp_staff")].uses, 4);
  CHECK_EQ(w.unit("sage").items[(size_t)patch].uses, 1);  // the use was spent
}

TEST(staves2_a_staff_that_heals_and_repairs_asks_for_an_item_only_when_there_is_one) {
  auto both = [](Copy& c) {
    c.swap("data/items.json", "\"effects\": [ { \"repair\": \"full\" } ]", "\"effects\": [ { \"heal\": 5 }, { \"repair\": 1 } ]");
  };
  Yard w("s2_heal_repair", "yard", both);
  w.place("ren", 2, 3);
  w.unit("ren").hp = 10;
  CHECK(w.use("sage", "fix_staff", "ren"));  // nothing worn: the heal alone
  CHECK_EQ(w.unit("ren").hp, 15);
  CHECK_EQ(w.count(EventType::ItemRepaired), 0);
  Yard v("s2_heal_repair2", "yard", both);
  v.place("ren", 2, 3);
  v.unit("ren").hp = 10;
  v.setUses("ren", "iron_blade", 3);
  CHECK_STR(v.refusal("sage", "fix_staff", "ren"), "pick the item to repair");
  CHECK(v.use("sage", "fix_staff", "ren", -1, -1, v.slotOf("ren", "iron_blade")));
  CHECK_EQ(v.unit("ren").hp, 15);
  CHECK_EQ(v.unit("ren").items[(size_t)v.slotOf("ren", "iron_blade")].uses, 4);
}

// ---- light ----

TEST(staves2_a_light_shows_the_squares_around_the_picked_square_for_the_staffs_turns) {
  Yard w("s2_light", "mist");
  CHECK(!w.blueSees("g2"));
  CHECK(w.option("sage", "lantern_staff").ok);
  CHECK(w.use("sage", "lantern_staff", nullptr, 7, 2));
  CHECK_EQ((int)w.b().lights.size(), 1);
  CHECK(w.blueSees("g2"));  // standing on 7,2: inside the radius
  CHECK(w.blueSees(9, 2) && !w.blueSees(10, 2));
  CHECK(w.unit("sage").done);
  CHECK_EQ(w.unit("sage").items[(size_t)w.slot("sage", "lantern_staff")].uses, 2);
  const Event* lit = nth(w.events, EventType::AreaLit);
  CHECK(lit && lit->a == w.id("sage") && lit->b == w.at(7, 2) && lit->c == 2 && lit->d == 2);
  CHECK_EQ(w.count(EventType::XpGained), 1);
  // Two turns of the user's side: red's turn and blue's next turn still see it; it goes when blue's SECOND next turn starts.
  CHECK(w.run(Action::endTurn()));  // red's turn
  CHECK(w.blueSees("g2") && w.b().lights.size() == 1);
  CHECK(w.run(Action::endTurn()));  // blue's turn again: turns 2 -> 1
  CHECK(w.blueSees("g2") && w.b().lights.size() == 1);
  CHECK(w.run(Action::endTurn()));
  CHECK(w.run(Action::endTurn()));  // blue's second new turn: turns 1 -> 0, gone
  CHECK(w.b().lights.empty() && !w.blueSees("g2"));
}

TEST(staves2_a_light_with_no_turns_given_lasts_until_the_users_next_turn_starts) {
  Yard w("s2_light_one", "mist");
  CHECK(w.use("sage", "spark_staff", nullptr, 6, 2));  // 5 away; radius 1 around it: nothing red there, but it is sight
  CHECK(w.blueSees(6, 2) && w.blueSees(7, 2));
  CHECK(w.run(Action::endTurn()));  // red's turn
  CHECK(w.blueSees(6, 2));
  CHECK(w.run(Action::endTurn()));  // blue's next turn starts: gone
  CHECK(w.b().lights.empty() && !w.blueSees(6, 2));
}

TEST(staves2_a_light_is_only_for_squares_in_reach_where_something_is_hidden) {
  Yard w("s2_light_cells", "mist");
  CHECK_STR(w.refusal("sage", "spark_staff", nullptr, 5, 2), "");  // 4 away: fine
  CHECK_STR(w.refusal("sage", "spark_staff", nullptr, 9, 2), "that square is out of range");
  CHECK_STR(w.refusal("sage", "spark_staff", nullptr, 1, 1), "nothing there is hidden from you");  // all of it is in sight already
  CHECK_STR(w.refusal("sage", "spark_staff", nullptr), "pick a square to light");
  // The squares offered are exactly those within reach that have something unseen in their area.
  const std::vector<Cell> cells = w.lightCells("sage", "lantern_staff");
  for (Cell c = 0; c < w.b().grid.cellCount(); ++c) {
    const int d = w.b().grid.distance(w.cellOf("sage"), c);
    bool unseen = false;
    std::vector<Cell> area;
    w.b().grid.within(c, 2, area);
    for (Cell a : area) unseen = unseen || !w.b().visibleTo(0, a);
    CHECK_EQ(has(cells, c), d >= 1 && d <= 6 && unseen);
  }
  // A light shows squares, not what hides: the enemy in the brush stays hidden unless a unit is next to it.
  CHECK(w.use("sage", "lantern_staff", nullptr, 4, 3));
  CHECK(w.blueSees(4, 3) && !w.blueSees("g1"));
}

TEST(staves2_a_light_staff_cannot_be_used_on_a_map_without_fog) {
  Yard w("s2_light_nofog", "yard", [](Copy& c) {
    c.swap("maps/yard.json", "\"warp_staff\", \"recall_staff\"", "\"lantern_staff\", \"warp_staff\", \"recall_staff\"");
  });
  const StaffOption o = w.option("sage", "lantern_staff");
  CHECK(!o.ok);
  CHECK_STR(o.why, "this map has no fog of war");
  CHECK_STR(w.refusal("sage", "lantern_staff", nullptr, 4, 2), "this map has no fog of war, so there is nothing to light");
  CHECK(w.lightCells("sage", "lantern_staff").empty());
}

// ---- story, objectives ----

TEST(staves2_a_unit_moved_by_a_staff_arrives_for_the_story_and_the_objectives) {
  Yard w("s2_story", "gate");
  const int sw = w.rig.db.findSwitch("arrived");
  CHECK(w.b().storyState.switches[(size_t)sw] == 0);
  CHECK(w.use("sage", "warp_staff", "bram", 1, 1));  // an event waits for bram to end a move on 1,1
  CHECK(w.b().storyState.switches[(size_t)sw] != 0);
  CHECK(!w.b().over);
  Yard v("s2_story_win", "gate");
  CHECK(v.use("sage", "warp_staff", "dax", 5, 3));  // dax reaching 5,3 wins the battle
  CHECK(v.b().over);
  CHECK_EQ(v.count(EventType::BattleEnded), 1);
}

// ---- nothing else changes ----

TEST(staves2_a_staff_with_no_second_pick_still_works_with_the_stage_one_action) {
  Yard w("s2_plain_action");
  CHECK(w.run(Action::useStaff(w.id("sage"), w.slot("sage", "recall_staff"), w.id("ren"))));
  CHECK(w.cellOf("ren") == w.at(2, 1));
}
