// Map weapons: the player's words (battle/mapweapon_text.h): uses text, the terrain info row, the forecast lines, the unit row and the log line.
// Game: tests/fixtures/srpg-mapweapon.
#include "battle/mapweapon_text.h"
#include "srpg_mapweapon_fixture.h"

using namespace ote;
using namespace mwtest;

TEST(mapweapon_player_words_for_uses_and_the_info_rows) {
  MapWorld w("mapweapon_words");
  const int b1 = w.place("b1"), b2 = w.place("b2"), c1 = w.place("c1");
  CHECK_STR(mapWeaponUsesText(w.b(), b1), "3/3");
  CHECK_STR(mapWeaponUsesText(w.b(), b2), "1/3");  // its own one use, of the kind's three
  CHECK_STR(mapWeaponUsesText(w.b(), c1), "");     // never runs out
  CHECK_STR(mapWeaponRowValue(w.b(), b1), "Bolt 3/3");
  CHECK_STR(mapWeaponRowValue(w.b(), c1), "Bolt");
  CHECK_STR(mapWeaponForecastName(w.b(), b1), "Bolt 3");
  CHECK_STR(mapWeaponForecastName(w.b(), c1), "Bolt");
  CHECK_STR(mapWeaponForecastFrom(w.b(), b1), "From Ballista");
  // A structure with no weapon says nothing.
  CHECK_STR(mapWeaponRowValue(w.b(), (int)w.b().structures.size() - 1), "");
}

TEST(mapweapon_player_the_unit_row_is_for_a_unit_standing_on_a_weapon_it_may_use) {
  MapWorld w("mapweapon_unit_row");
  std::string label, value;
  CHECK(!mapWeaponUnitRow(w.b(), w.unit("ar"), label, value));  // not on one
  w.put("ar", 4, 1);
  CHECK(mapWeaponUnitRow(w.b(), w.unit("ar"), label, value));
  CHECK_STR(label, "Bolt");
  CHECK_STR(value, "3/3 r2-6");
  w.put("kn", 4, 1);  // the knight may not use the ballista
  CHECK(!mapWeaponUnitRow(w.b(), w.unit("kn"), label, value));
  w.put("kn", 4, 0);  // the catapult
  CHECK(mapWeaponUnitRow(w.b(), w.unit("kn"), label, value));
  CHECK_STR(value, "r2-6");
}

TEST(mapweapon_player_the_log_line_names_the_structure_and_says_when_it_is_spent) {
  MapWorld w("mapweapon_log");
  w.put("ar", 4, 2);  // b2: one use
  w.put("g1", 7, 1);
  CHECK(w.run(Action::attack(w.id("ar"), w.id("g1"), w.bolt())));
  const Event* e = w.attackEvent();
  CHECK(e != nullptr);
  CHECK_STR(mapWeaponLogLine(w.b(), *e), "map weapon: Archer fires Bolt from Ballista (spent)");
  CHECK_STR(mapWeaponSpentFloat(w.b(), *e), "Ballista spent");
}
