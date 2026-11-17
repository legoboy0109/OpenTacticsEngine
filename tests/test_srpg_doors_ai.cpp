// Doors, keys, chests and visiting: the computer opponent (docs/SRPG.md section 10 "Computer opponent"). Game: tests/fixtures/srpg-doors.
#include "core/ai/ai_internal.h"
#include "srpg_doors_fixture.h"

using namespace ote;
using namespace doorstest;

namespace {

// Plays `turns` full turns of red (blue just ends its turn); false if a turn did not end by itself (a stall).
bool playRed(Doors& w, int turns) {
  for (int t = 0; t < turns && !w.b().over; ++t) {
    w.redTurn();
    if (!w.b().over && w.b().activeSide != 0) return false;  // red never handed the turn back
  }
  return true;
}

}  // namespace

TEST(doors_ai_a_closed_door_is_a_wall_in_every_distance_field) {
  Doors w("doors_ai_field", "corridor");
  const int mt = w.b().typeOf(w.unit("r1")).moveType;
  std::vector<int> field;
  ai::distanceField(w.b(), mt, std::vector<Cell>(1, w.unit("lord").cell), field);
  CHECK(field[(size_t)w.unit("r1").cell] >= ai::kFar);  // no way round: the corridor holds nothing but the door
  CHECK(w.run(w.open("lord", "d1") , true) == false);   // (the lord is not next to it, and has no key)
  w.put("lord", 3, 1);
  w.unit("lord").items.push_back(ItemStack{w.item("brass_key"), 1, false});
  CHECK(w.run(w.open("lord", "d1", "brass_key")));
  ai::distanceField(w.b(), mt, std::vector<Cell>(1, w.unit("lord").cell), field);
  CHECK(field[(size_t)w.unit("r1").cell] < ai::kFar);
}

TEST(doors_ai_a_marked_unit_with_a_key_opens_the_door_that_is_in_its_way) {
  Doors w("doors_ai_open", "corridor");
  CHECK_EQ(w.unit("r1").aiGoals, kAiDoors);
  CHECK(playRed(w, 3));
  CHECK_STR(w.kind("d1"), "cell_door_open");
  CHECK(!w.has("r1", "brass_key"));
  // It opened the door because that was the way to the enemy: the next turns it walks through (or fights).
  CHECK(playRed(w, 3));
  CHECK(w.unit("r1").cell != w.at(1, 1) || w.b().over);
}

TEST(doors_ai_without_a_key_or_without_the_marker_it_leaves_the_door_alone_and_never_stalls) {
  // Marked but no key: it cannot open it.
  Doors a("doors_ai_nokey", "corridor", [](DCopy& c) { c.swap("maps/corridor.json", "\"iron_sword\", \"brass_key\"", "\"iron_sword\""); });
  CHECK(playRed(a, 6));
  CHECK_STR(a.kind("d1"), "cell_door");
  const Cell stay = a.unit("r1").cell;
  CHECK(playRed(a, 3));
  CHECK_EQ(a.unit("r1").cell, stay);  // it does not oscillate
  // A key but no marker: unmarked units ignore doors.
  Doors b("doors_ai_unmarked", "corridor", [](DCopy& c) { c.swap("maps/corridor.json", ", \"goesFor\": [\"doors\"]", ""); });
  CHECK_EQ(b.unit("r1").aiGoals, 0);
  CHECK(playRed(b, 6));
  CHECK_STR(b.kind("d1"), "cell_door");
  CHECK(b.has("r1", "brass_key"));
  // The only path is a locked door: the battle does not stall, the AI hands the turn back every time.
  Doors c("doors_ai_only_path", "corridor");
  c.unit("r1").items.erase(c.unit("r1").items.begin() + c.slotOf("r1", "brass_key"));
  for (int i = 0; i < 8; ++i) {
    CHECK(playRed(c, 1));
    c.unit("lord").done = false;
  }
  CHECK_STR(c.kind("d1"), "cell_door");
}

TEST(doors_ai_a_unit_marked_for_chests_takes_them_and_an_unmarked_one_does_not) {
  Doors w("doors_ai_chests", "market");
  CHECK(playRed(w, 4));
  CHECK_STR(w.kind("c1"), "chest_open");        // r1 (marked) went for the nearest chest
  CHECK(w.has("r1", "potion"));
  CHECK(w.unit("r1").items.back().drop);        // and drops it again if it is defeated
  CHECK_STR(w.kind("c2"), "chest_open");        // after the first it went for the second
  CHECK_EQ(w.gold("red"), 40);
  // r2 is unmarked: it never touched a chest, even where it stood next to one.
  Doors u("doors_ai_unmarked_chest", "market", [](DCopy& c) {
    c.swap("maps/market.json", ", \"goesFor\": [\"chests\"]", "");
  });
  CHECK(playRed(u, 6));
  CHECK_STR(u.kind("c1"), "chest");
  CHECK_STR(u.kind("c2"), "chest");
  CHECK(u.st("c1").loot.size() == 1 && u.st("c2").loot.size() == 1);
}

TEST(doors_ai_a_unit_marked_for_villages_destroys_the_enemys) {
  Doors w("doors_ai_villages", "market");
  CHECK(playRed(w, 3));
  CHECK_STR(w.kind("v1"), "hamlet_ruin");
  // Unmarked: it does not, even when it stands on the place.
  Doors u("doors_ai_villages_unmarked", "market", [](DCopy& c) { c.swap("maps/market.json", ", \"goesFor\": [\"villages\"]", ""); });
  CHECK(playRed(u, 4));
  CHECK_STR(u.kind("v1"), "hamlet");
}

TEST(doors_ai_a_defeated_looter_drops_the_loot_to_the_one_who_defeated_it) {
  Doors w("doors_ai_drop", "market");
  CHECK(playRed(w, 4));
  Unit& r1 = w.unit("r1");
  CHECK(w.has("r1", "potion"));
  // Blue's lord walks up to it (arranged) and defeats it: the potion goes to him.
  w.put("lord", 0, 0);
  w.put("r1", 1, 0);
  r1.hp = 1;
  CHECK(w.b().activeSide == 0);
  const bool ok = w.run(Action::attack(w.id("lord"), w.id("r1"), w.db().findWeapon("iron_sword")), true);
  CHECK(ok);
  if (ok) CHECK(w.has("lord", "potion"));
}

TEST(doors_ai_automatic_mode_opens_by_the_move) {
  const auto autoOn = [](DCopy& c) {
    c.swap("game.json", "\"rules\": { ", "\"rules\": { \"places\": { \"actions\": \"auto\" }, ");
  };
  Doors w("doors_ai_auto", "market", autoOn);
  CHECK(playRed(w, 4));
  CHECK_STR(w.kind("c1"), "chest_open");  // the marked unit's moves ended next to it
  Doors d("doors_ai_auto_door", "corridor", autoOn);
  CHECK(playRed(d, 3));
  CHECK_STR(d.kind("d1"), "cell_door_open");
}
