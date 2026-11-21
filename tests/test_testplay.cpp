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

TEST(testlevel_auto_is_the_rounded_average_of_the_hostile_computer_units_plus_two) {
  std::vector<UnitSpec> units = {{"soldier", 0, 0, 0, false}, {"soldier", 1, 3, 0, false}, {"soldier", 1, 4, 0, false}, {"soldier", 1, 5, 0, false}};
  Arena a(units);
  a.map.units[1].level = 1;
  a.map.units[2].level = 2;
  a.map.units[3].level = 2;  // 5 / 3 = 1.67 -> 2
  a.battle.setTestLevel(-1);
  CHECK(a.begin());
  CHECK_EQ(a.battle.enemyAverageLevel(), 2);
  CHECK_EQ(Battle::kAutoTestLevelBonus, 2);
  CHECK_EQ(a.battle.testLevelUsed, 4);
  CHECK_EQ(a.battle.units[0].level, 4);
  Arena b(units);
  b.map.units[1].level = 2;
  b.map.units[2].level = 3;
  b.map.units[3].level = 3;  // 8 / 3 = 2.67 -> 3
  b.battle.setTestLevel(-1);
  CHECK(b.begin());
  CHECK_EQ(b.battle.units[0].level, 5);
  Arena c(std::vector<UnitSpec>({{"soldier", 0, 0, 0, false}, {"soldier", 1, 3, 0, false}, {"soldier", 1, 4, 0, false}}));
  c.map.units[1].level = 2;
  c.map.units[2].level = 3;  // 2.5 -> 3 (halves go up)
  CHECK_EQ(c.battle.autoTestLevel(), 0);  // before init there are no units
  c.battle.setTestLevel(-1);
  CHECK(c.begin());
  CHECK_EQ(c.battle.units[0].level, 5);  // 2.5 -> 3, plus 2
}

TEST(testlevel_auto_plus_two_is_still_capped_by_the_games_top_level) {
  Arena a(kDuel);
  a.db.rules.maxLevel = 4;
  a.map.units[1].level = 3;
  a.battle.setTestLevel(-1);
  CHECK(a.begin());
  CHECK_EQ(a.battle.testLevelUsed, 5);   // the aim is 3 + 2 ...
  CHECK_EQ(a.battle.units[0].level, 4);  // ... the unit stops at the cap
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

// ---- Weapon ranks follow the level (docs/DIFFICULTY.md "Test level") ----
namespace {

// Ladder E 0, D 30, C 70, B 120 (top 120); the game's top level 6, so perLevel = ceil(120 / 12) = 10. Groups: sword, bow, staff.
// The soldier wields sword (starts at D = 30) and staff (E = 0); the bow is the archer's. The soldier has no rank in the bow.
void addRanks(Database& db) {
  db.rules.maxLevel = 6;
  db.ranks.ladder = {{"e", "E", 0}, {"d", "D", 30}, {"c", "C", 70}, {"b", "B", 120}};
  db.ranks.groups = {"sword", "bow", "staff"};
  db.weapons[0].group = "sword";
  db.weapons[0].rankGroup = 0;
  db.weapons[1].group = "bow";
  db.weapons[1].rankGroup = 1;
  WeaponDef rod;
  rod.id = "rod";
  rod.name = "Rod";
  rod.group = "staff";
  rod.rankGroup = 2;
  db.weapons.push_back(rod);
  db.unitTypes[0].wields = {"sword", "staff"};
  db.unitTypes[0].rankStart = {30, 0, 0};
}

}  // namespace

TEST(testlevel_ranks_rise_per_level_for_the_groups_the_unit_wields) {
  Arena a(kDuel);
  addRanks(a.db);
  CHECK(a.begin());
  CHECK_EQ(a.battle.testRankXpPerLevel(), 10);
  Unit u = a.battle.units[0];
  CHECK(u.weaponXp == std::vector<int>({30, 0, 0}));
  const int gained = a.battle.levelUnitTo(u, 6);
  CHECK_EQ(gained, 5);
  const std::string note = a.battle.raiseTestRanks(u, gained);
  CHECK(u.weaponXp == std::vector<int>({80, 0, 50}));  // sword 30 + 50 (D -> C), staff 0 + 50 (E -> D), the bow's group untouched
  CHECK(note == ", sword D -> C, staff E -> D");
}

TEST(testlevel_ranks_stop_at_the_top_rank_and_a_game_without_ranks_gets_nothing) {
  Arena a(kDuel);
  addRanks(a.db);
  a.db.unitTypes[0].rankStart = {115, 0, 0};
  CHECK(a.begin());
  Unit u = a.battle.units[0];
  CHECK(a.battle.raiseTestRanks(u, 5) == ", sword C -> B, staff E -> D");  // 115 + 50 stops at 120
  CHECK_EQ(u.weaponXp[0], 120);
  CHECK(a.battle.raiseTestRanks(u, 5).find("sword") == std::string::npos);  // at the top: nothing more, no text
  CHECK_EQ(u.weaponXp[0], 120);
  Arena plain(kDuel);
  CHECK(plain.begin());
  Unit v = plain.battle.units[0];
  CHECK_EQ(plain.battle.testRankXpPerLevel(), 0);
  CHECK(plain.battle.raiseTestRanks(v, 5).empty() && v.weaponXp.empty());
}

TEST(testlevel_a_test_battle_raises_the_ranks_with_the_level_and_leaves_the_computer_alone) {
  Arena a(kDuel);
  addRanks(a.db);
  a.battle.setTestLevel(4);  // 3 levels = +30
  CHECK(a.begin());
  CHECK_EQ(a.battle.units[0].level, 4);
  CHECK(a.battle.units[0].weaponXp == std::vector<int>({60, 0, 30}));
  CHECK(a.battle.units[1].weaponXp == std::vector<int>({30, 0, 0}));  // the computer's unit keeps what it was placed with
  Arena off(kDuel);
  addRanks(off.db);
  CHECK(off.begin());
  CHECK(off.battle.units[0].weaponXp == std::vector<int>({30, 0, 0}));
}

// ---- Promotion by the test level (rules.promotion.testAt) ----
namespace {

// Soldier (0) can promote: the first entry (to the veteran, 3) wins over the second (to the tank, 2). The archer (1) has only an inert
// entry back to the soldier (no items, no atLevel): ignored. testAt 5, top level 20. The veteran stands on foot like the soldier.
void addPromotions(Database& db) {
  db.rules.maxLevel = 20;
  db.rules.promotion.testAt = 5;
  UnitTypeDef vet = db.unitTypes[0];
  vet.id = "veteran";
  vet.name = "Veteran";
  vet.stats = {12, 7, 3, 3};
  db.unitTypes.push_back(vet);
  PromotionDef up;
  up.to = 3;
  up.atLevel = PromoteAtLevel::Offer;
  up.fromLevel = 20;  // far away: nothing promotes by itself at a turn start (the test level ignores fromLevel)
  PromotionDef other;
  other.to = 2;
  other.atLevel = PromoteAtLevel::Auto;
  other.fromLevel = 20;
  db.unitTypes[0].promotions = {up, other};
  PromotionDef inert;
  inert.to = 0;
  db.unitTypes[1].promotions = {inert};
}

}  // namespace

TEST(testlevel_promotion_below_at_and_above_testat) {
  Arena a(kDuel);
  addPromotions(a.db);
  CHECK(a.begin());
  Unit below = a.battle.units[0];
  const Battle::TestRaise r4 = a.battle.raiseUnitForTest(below, 4, true);
  CHECK(r4.changed && below.type == 0 && below.level == 4 && r4.tail.empty());
  Unit at = a.battle.units[0];
  const Battle::TestRaise r5 = a.battle.raiseUnitForTest(at, 5, true);
  CHECK(r5.changed && at.type == 3 && at.level == 1 && r5.reached == 5);  // the first entry; a reset level
  CHECK(r5.tail == ", promoted to Veteran, Lv 1");
  CHECK(at.stats == std::vector<int>({16, 11, 3, 3}));  // 4 levels (+4 / +4), then the promotion's difference (+2 / +2 / +1)
  CHECK_EQ(at.hp, a.battle.maxHp(at));
  Unit above = a.battle.units[0];
  const Battle::TestRaise r8 = a.battle.raiseUnitForTest(above, 8, true);
  CHECK(above.type == 3 && above.level == 4 && r8.reached == 5);  // 8 - 5 = 3 more levels in the new class
  CHECK_EQ(a.battle.effectiveTestLevel(above), 9);
  CHECK(r8.tail == ", promoted to Veteran, Lv 4");
  CHECK(above.stats == std::vector<int>({19, 14, 3, 3}));
}

TEST(testlevel_promotion_already_promoted_inert_entries_and_no_key) {
  Arena a(kDuel);
  addPromotions(a.db);
  CHECK(a.begin());
  Unit vet = a.battle.units[0];
  vet.type = 3;
  vet.level = 2;
  CHECK(!a.battle.raiseUnitForTest(vet, 7, true).changed);  // effective 7 already
  CHECK(a.battle.raiseUnitForTest(vet, 8, true).changed);
  CHECK(vet.type == 3 && vet.level == 3);  // 8 - testAt, never promoted a second time
  Unit archer = a.battle.units[0];
  archer.type = 1;
  CHECK(a.battle.raiseUnitForTest(archer, 9, true).tail.empty());
  CHECK(archer.type == 1 && archer.level == 9);  // the inert entry back to the soldier is never followed
  Arena off(kDuel);  // no key: the plain levelling
  addPromotions(off.db);
  off.db.rules.promotion.testAt = 0;
  off.battle.setTestLevel(8);
  CHECK(off.begin());
  CHECK(off.battle.units[0].type == 0 && off.battle.units[0].level == 8);
}

TEST(testlevel_promotion_is_skipped_when_the_new_type_could_not_stand_there) {
  Arena a(kDuel);
  addPromotions(a.db);
  a.db.moveTypes.push_back({"boat", "Boat"});
  for (TerrainDef& t : a.db.terrains) t.moveCost.push_back(0);
  for (StructureDef& s : a.db.structures) s.moveCost.push_back(-1);
  a.db.unitTypes[3].moveType = 1;
  a.battle.setTestLevel(8);
  CHECK(a.begin());
  CHECK(a.battle.units[0].type == 0 && a.battle.units[0].level == 5);  // stopped at testAt, still a soldier
}

TEST(testlevel_auto_counts_effective_levels_and_promotes_in_a_battle) {
  Arena a({{"soldier", 0, 0, 0, false}, {"soldier", 1, 3, 0, false}, {"soldier", 1, 4, 0, false}});
  addPromotions(a.db);
  a.map.units[1].type = 3;  // a veteran of level 3 = effective 8
  a.map.units[1].level = 3;
  a.map.units[2].level = 4;  // (8 + 4) / 2 = 6, plus 2
  a.battle.setTestLevel(-1);
  CHECK(a.begin());
  CHECK_EQ(a.battle.testLevelUsed, 8);
  CHECK(a.battle.units[0].type == 3 && a.battle.units[0].level == 4);
}
