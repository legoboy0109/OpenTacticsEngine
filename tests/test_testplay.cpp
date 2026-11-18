// The test level (docs/DIFFICULTY.md "Test level") on the in-code fixture game: the levelling function equals the difficulty's level
// shift, caps and maxLevel, units at or above the level are untouched, `auto`, and nothing changes without the option.
#include "battle_fixture.h"

using namespace fixture;

namespace {

// The soldier of the fixture: hp 10 atk 5 def 2 mov 3, growths hp 100 atk 100. Side 0 is human, side 1 the computer.
const std::vector<UnitSpec> kDuel = {{"soldier", 0, 0, 0, false}, {"soldier", 1, 3, 0, false}};

struct Arena : World {
  explicit Arena(const std::vector<UnitSpec>& units) : World({"......"}, units) {}
};

void addDifficulty(Database& db, int levels) {
  DifficultyDef up;
  up.id = "up";
  up.name = "Up";
  up.enemy.levels = levels;
  db.difficulties.list = {up};
  db.difficulties.defaultId = "up";
  db.difficulties.defaultIndex = 0;
  db.difficulties.percentStatIndex = {0, 1, 2};
}

}  // namespace

TEST(testlevel_levelling_equals_the_difficulty_shift_for_the_same_number_of_levels) {
  Arena shifted(kDuel);
  addDifficulty(shifted.db, 2);
  shifted.map.units[1].level = 3;
  CHECK(shifted.begin());
  Arena plain(kDuel);
  plain.map.units[1].level = 3;
  CHECK(plain.begin());
  Unit u = plain.battle.units[1];
  CHECK_EQ(plain.battle.levelUnitTo(u, 5), 2);
  CHECK_EQ(u.level, 5);
  CHECK(u.stats == shifted.battle.units[1].stats);
  CHECK_EQ(shifted.battle.units[1].level, 5);
  CHECK_EQ(u.hp, plain.battle.maxHp(u));
  CHECK_EQ(u.xp, 0);
}

TEST(testlevel_a_fractional_growth_rounds_like_the_difficulty_shift) {
  Arena a(kDuel);
  a.db.unitTypes[0].growths = {50, 40, 0, 0};
  CHECK(a.begin());
  Unit u = a.battle.units[0];
  CHECK_EQ(a.battle.levelUnitTo(u, 4), 3);
  CHECK_EQ(u.stats[0], 12);  // 50% x 3 = 1.5 -> 2
  CHECK_EQ(u.stats[1], 6);   // 40% x 3 = 1.2 -> 1
}

TEST(testlevel_stops_at_the_types_maxlevel_then_the_games) {
  Arena a(kDuel);
  a.db.rules.maxLevel = 6;
  a.db.unitTypes[0].maxLevel = -1;
  CHECK(a.begin());
  Unit u = a.battle.units[0];
  CHECK_EQ(a.battle.levelUnitTo(u, 9), 5);  // the game's top level 6: five levels from 1
  CHECK_EQ(u.level, 6);
  CHECK_EQ(u.stats[0], 15);
  Arena b(kDuel);
  b.db.rules.maxLevel = 6;
  b.db.unitTypes[0].maxLevel = 3;  // this type tops out lower
  CHECK(b.begin());
  Unit v = b.battle.units[0];
  CHECK_EQ(b.battle.levelUnitTo(v, 9), 2);
  CHECK_EQ(v.level, 3);
  CHECK_EQ(v.stats[0], 12);
}

TEST(testlevel_a_stat_cap_cuts_the_rise) {
  Arena a(kDuel);
  a.db.statCaps = {-1, 8, -1, -1};  // atk at most 8: 5 + 4 levels would be 9
  CHECK(a.begin());
  Unit u = a.battle.units[0];
  a.battle.levelUnitTo(u, 5);
  CHECK_EQ(u.stats[0], 14);  // hp has no cap
  CHECK_EQ(u.stats[1], 8);
}

TEST(testlevel_a_unit_at_or_above_the_level_is_untouched) {
  Arena a(kDuel);
  a.map.units[0].level = 5;
  a.map.units[0].stats = {30, 12, 4, 3};
  CHECK(a.begin());
  Unit u = a.battle.units[0];
  u.xp = 40;
  CHECK_EQ(a.battle.levelUnitTo(u, 5), 0);
  CHECK_EQ(a.battle.levelUnitTo(u, 3), 0);
  CHECK(u.level == 5 && u.xp == 40 && u.stats == std::vector<int>({30, 12, 4, 3}));
}

TEST(testlevel_a_number_lifts_the_human_side_and_only_it) {
  Arena a(kDuel);
  a.map.units[1].level = 3;
  a.battle.setTestLevel(5);
  CHECK(a.begin());
  const Unit& mine = a.battle.units[0];
  CHECK_EQ(mine.level, 5);
  CHECK(mine.stats == std::vector<int>({14, 9, 2, 3}));
  CHECK_EQ(mine.hp, 14);
  CHECK_EQ(mine.xp, 0);
  CHECK(a.battle.units[1].level == 3 && a.battle.units[1].stats[0] == 10);  // the computer's unit is not touched
  CHECK_EQ(a.battle.testLevelUsed, 5);
}

TEST(testlevel_auto_is_the_rounded_average_of_the_hostile_computer_units) {
  std::vector<UnitSpec> units = {{"soldier", 0, 0, 0, false}, {"soldier", 1, 3, 0, false}, {"soldier", 1, 4, 0, false}, {"soldier", 1, 5, 0, false}};
  Arena a(units);
  a.map.units[1].level = 1;
  a.map.units[2].level = 2;
  a.map.units[3].level = 2;  // 5 / 3 = 1.67 -> 2
  a.battle.setTestLevel(-1);
  CHECK(a.begin());
  CHECK_EQ(a.battle.testLevelUsed, 2);
  CHECK_EQ(a.battle.units[0].level, 2);
  Arena b(units);
  b.map.units[1].level = 2;
  b.map.units[2].level = 3;
  b.map.units[3].level = 3;  // 8 / 3 = 2.67 -> 3
  b.battle.setTestLevel(-1);
  CHECK(b.begin());
  CHECK_EQ(b.battle.units[0].level, 3);
  Arena c(std::vector<UnitSpec>({{"soldier", 0, 0, 0, false}, {"soldier", 1, 3, 0, false}, {"soldier", 1, 4, 0, false}}));
  c.map.units[1].level = 2;
  c.map.units[2].level = 3;  // 2.5 -> 3 (halves go up)
  CHECK_EQ(c.battle.autoTestLevel(), 0);  // before init there are no units
  c.battle.setTestLevel(-1);
  CHECK(c.begin());
  CHECK_EQ(c.battle.units[0].level, 3);
}

TEST(testlevel_auto_without_hostile_computer_units_changes_nothing) {
  Arena a({{"soldier", 0, 0, 0, false}, {"soldier", 0, 1, 0, false}});
  a.battle.setTestLevel(-1);
  CHECK(a.begin());
  CHECK_EQ(a.battle.testLevelUsed, 0);
  CHECK(a.battle.units[0].level == 1 && a.battle.units[0].stats[0] == 10);
}

TEST(testlevel_plain_human_units_are_lifted_too_and_the_option_off_changes_nothing) {
  Arena a(kDuel);
  CHECK(a.begin());
  Arena b(kDuel);
  b.battle.setTestLevel(0);
  CHECK(b.begin());
  CHECK(a.battle.units[0].stats == b.battle.units[0].stats && a.battle.units[0].level == 1);
  CHECK(a.battle.rng.state == b.battle.rng.state);
  Arena c(kDuel);
  c.battle.setTestLevel(3);  // the units of this fixture are plain (no story id): they are lifted anyway
  CHECK(c.begin());
  CHECK_EQ(c.battle.units[0].level, 3);
  CHECK(c.battle.rng.state == a.battle.rng.state);  // no dice were used
}
