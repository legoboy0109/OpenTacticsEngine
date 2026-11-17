// docs/DIFFICULTY.md on the in-code fixture game (battle_fixture.h): how an enemy is built at another level, flat and
// percent stats, the exemptions, the experience and resource multipliers, the Lua values, the hexagon demo.
#include "battle_fixture.h"
#include "core/data/loader.h"

using namespace fixture;

namespace {

// easy -1 level / xp 50% / resources 50%; normal; hard +2 levels / def +2 / x150 / xp 150% / resources 150%
void addDifficulties(Database& db) {
  DifficultyDef easy, normal, hard;
  easy.id = "easy"; easy.name = "Easy"; easy.enemy.levels = -1; easy.xpPercent = 50; easy.resourcePercent = 50;
  normal.id = "normal"; normal.name = "Normal";
  hard.id = "hard"; hard.name = "Hard"; hard.enemy.levels = 2; hard.enemy.stats = {0, 0, 2, 0};
  hard.enemy.statsPercent = 150; hard.xpPercent = 150; hard.resourcePercent = 150;
  DifficultyRules& r = db.difficulties;
  r.list = {easy, normal, hard};
  r.defaultId = "normal";
  r.defaultIndex = 1;
  r.percentStatIndex = {0, 1, 2};  // every stat but the move stat (index 3)
}

struct Arena : World {
  Arena(const std::vector<UnitSpec>& units) : World({"......"}, units) { addDifficulties(db); }
  bool beginAt(const char* difficulty, uint32_t seed = 1) {
    battle.setDifficulty(db.difficulties.find(difficulty));
    return begin(seed);
  }
};

// The soldier of the fixture: hp 10 atk 5 def 2 mov 3, growths hp 100 atk 100.
const std::vector<UnitSpec> kDuel = {{"soldier", 0, 0, 0, false}, {"soldier", 1, 3, 0, false}};
const std::vector<UnitSpec> kClose = {{"soldier", 0, 0, 0, false}, {"soldier", 1, 1, 0, false}};  // side by side: they can fight

}  // namespace

TEST(difficulty_a_game_without_difficulties_builds_units_as_before) {
  World w({"......"}, kDuel);
  w.map.units[1].level = 3;
  CHECK(w.begin());
  CHECK_EQ(w.battle.difficulty, -1);
  CHECK(w.battle.units[1].level == 3 && w.battle.units[1].stats[0] == 10 && w.battle.units[1].stats[1] == 5);
}

TEST(difficulty_unset_means_the_default) {
  Arena a(kDuel);
  a.map.units[1].level = 3;
  CHECK(a.begin());
  CHECK_EQ(a.battle.difficulty, 1);  // normal: nothing shifts
  CHECK(a.battle.units[1].level == 3 && a.battle.units[1].stats[0] == 10);
}

TEST(difficulty_hard_builds_an_enemy_as_if_placed_levels_higher_then_adds_then_scales) {
  Arena a(kDuel);
  a.map.units[1].level = 3;
  CHECK(a.beginAt("hard"));
  const Unit& foe = a.battle.units[1];
  CHECK_EQ(foe.level, 5);
  // +2 levels of growth 100 = hp +2, atk +2 (12, 7); def +2 flat (4); then x150 on hp, atk, def; move untouched.
  CHECK_EQ(foe.stats[0], 18);
  CHECK_EQ(foe.stats[1], 11);
  CHECK_EQ(foe.stats[2], 6);
  CHECK_EQ(foe.stats[3], 3);
  CHECK_EQ(foe.hp, 18);  // built at full hit points
  // The human side is untouched.
  CHECK(a.battle.units[0].level == 1 && a.battle.units[0].stats[0] == 10 && a.battle.units[0].stats[2] == 2);
}

TEST(difficulty_the_shift_equals_the_growths_of_the_levels_and_never_uses_the_dice) {
  Arena a(kDuel), b(kDuel);
  a.db.difficulties.list[2].enemy.stats.assign(4, 0);
  a.db.difficulties.list[2].enemy.statsPercent = 100;
  a.map.units[1].level = 4;
  CHECK(a.beginAt("hard", 1));
  b.db.difficulties.list[2] = a.db.difficulties.list[2];
  b.map.units[1].level = 4;
  CHECK(b.beginAt("hard", 999));
  CHECK_EQ(a.battle.units[1].stats[0], 12);  // exactly growth 100% x 2 levels
  CHECK(a.battle.units[1].stats == b.battle.units[1].stats);  // another seed, the same unit
  // The battle's random generator was not touched by building it.
  World plain({"......"}, kDuel);
  CHECK(plain.begin(1));
  CHECK(plain.battle.rng.state == a.battle.rng.state);
}

TEST(difficulty_a_fractional_growth_is_rounded_to_the_nearest_whole_stat) {
  Arena a(kDuel);
  a.db.unitTypes[0].growths = {50, 40, 0, 0};
  a.db.difficulties.list[2].enemy.stats.assign(4, 0);
  a.db.difficulties.list[2].enemy.statsPercent = 100;
  a.db.difficulties.list[2].enemy.levels = 3;
  CHECK(a.beginAt("hard"));
  CHECK_EQ(a.battle.units[1].stats[0], 12);  // 50% x 3 = 1.5 -> 2
  CHECK_EQ(a.battle.units[1].stats[1], 6);   // 40% x 3 = 1.2 -> 1
}

TEST(difficulty_easy_lowers_the_level_but_never_below_one) {
  Arena a(kDuel);
  a.map.units[1].level = 3;
  a.map.units[0].level = 3;
  CHECK(a.beginAt("easy"));
  CHECK(a.battle.units[1].level == 2 && a.battle.units[1].stats[0] == 9 && a.battle.units[1].stats[1] == 4);
  Arena b(kDuel);  // already at level 1: stays there, and its stats stay as placed
  CHECK(b.beginAt("easy"));
  CHECK(b.battle.units[1].level == 1 && b.battle.units[1].stats[0] == 10 && b.battle.units[1].stats[1] == 5);
}

TEST(difficulty_a_unit_is_not_built_above_the_games_top_level) {
  Arena a(kDuel);
  a.db.rules.maxLevel = 5;
  a.map.units[1].level = 4;
  CHECK(a.beginAt("hard"));
  CHECK_EQ(a.battle.units[1].level, 5);
  CHECK_EQ(a.battle.units[1].stats[0], 17);  // only ONE level of growth (hp 11), then x150% = 16.5 -> 17
}

TEST(difficulty_exemptions_boss_exact_stats_allies_and_opted_out_sides) {
  Arena a({{"soldier", 0, 0, 0, false}, {"soldier", 1, 3, 0, false}, {"soldier", 1, 4, 0, false}, {"soldier", 1, 5, 0, false}});
  a.map.units[1].difficultyShift = false;                 // a boss
  a.map.units[2].statExact = {0, 1, 0, 0};                // its attack is written on the map: exact
  a.map.units[2].stats[1] = 9;
  CHECK(a.beginAt("hard"));
  const Unit& boss = a.battle.units[1];
  CHECK(boss.level == 1 && boss.stats[0] == 10 && boss.stats[1] == 5 && boss.stats[2] == 2);
  const Unit& exact = a.battle.units[2];
  CHECK_EQ(exact.stats[1], 9);       // exact
  CHECK_EQ(exact.stats[0], 18);      // the rest is shifted
  CHECK_EQ(exact.level, 3);
  // A computer side the map opted out.
  Arena b(kDuel);
  b.map.sides[1].difficultyShift = false;
  CHECK(b.beginAt("hard"));
  CHECK(b.battle.units[1].level == 1 && b.battle.units[1].stats[0] == 10);
}

TEST(difficulty_an_allied_or_neutral_computer_side_is_not_an_enemy) {
  Arena a({{"soldier", 0, 0, 0, false}, {"soldier", 1, 3, 0, false}, {"soldier", 2, 4, 0, false}});
  MapSideDef ally; ally.id = "green"; ally.name = "Green"; ally.ai = true; ally.team = "blue";
  a.map.sides[0].team = "blue";
  a.map.sides.push_back(ally);
  CHECK(a.beginAt("hard"));
  CHECK_EQ(a.battle.units[1].level, 3);  // red is hostile to the human blue side
  CHECK_EQ(a.battle.units[2].level, 1);  // green shares blue's team
  // Two computer sides and no human side: nobody is an enemy of a human side.
  Arena b(kDuel);
  b.map.sides[0].ai = true;
  CHECK(b.beginAt("hard"));
  CHECK(b.battle.units[0].level == 1 && b.battle.units[1].level == 1);
}

TEST(difficulty_hit_points_never_drop_below_one) {
  Arena a(kDuel);
  a.db.difficulties.list[0].enemy.stats = {-50, 0, 0, 0};
  CHECK(a.beginAt("easy"));
  CHECK_EQ(a.battle.units[1].stats[0], 1);
  CHECK_EQ(a.battle.units[1].hp, 1);
}

TEST(difficulty_a_maps_own_numbers_replace_the_games_for_what_they_name) {
  Arena a(kDuel);
  MapDifficulty md;
  md.difficulty = 2;
  md.enemy.levels = 3; md.enemy.hasLevels = true;           // "levels" replaces +2 (it does not add)
  md.enemy.stats.assign(4, 0); md.enemy.hasStat.assign(4, 0);
  md.enemy.stats[2] = 0; md.enemy.hasStat[2] = 1;           // def flat addition replaced by 0
  md.xpPercent = 200;
  a.map.difficulty = {md};
  CHECK(a.beginAt("hard"));
  CHECK_EQ(a.battle.units[1].level, 4);
  CHECK_EQ(a.battle.units[1].stats[0], 20);  // (10 + 3) x 150% = 19.5 -> 20
  CHECK_EQ(a.battle.units[1].stats[2], 3);   // 2 x 150% (the flat +2 was replaced by 0)
  CHECK_EQ(a.battle.diffXpPercent, 200);
  CHECK_EQ(a.battle.diffResourcePercent, 150);  // not named: the game's
  Arena b(kDuel);  // another difficulty: the map's numbers do not apply
  b.map.difficulty = {md};
  CHECK(b.beginAt("easy"));
  CHECK_EQ(b.battle.diffXpPercent, 50);
}

TEST(difficulty_experience_is_scaled_for_units_of_human_sides_only) {
  Arena a(kClose);
  a.db.rules.progression = true;
  a.db.rules.xpPerLevel = 100;
  a.db.rules.maxLevel = 20;
  a.db.rules.fXpHit = "30";
  a.db.rules.fXpKill = "60";
  CHECK(a.beginAt("easy"));
  CHECK(a.act(Action::attack(0, 1, 0)));
  int humanXp = 0, foeXp = 0;
  for (const Event& e : a.events)
    if (e.type == EventType::XpGained) (e.a == 0 ? humanXp : foeXp) += e.b;
  CHECK_EQ(humanXp, 15);  // 30 x 50%
  CHECK_EQ(foeXp, 30);    // the computer unit's counter-attack experience is not scaled
  Arena b(kClose);
  b.db.rules.progression = true; b.db.rules.fXpHit = "30"; b.db.rules.fXpKill = "60";
  b.map.units[1].difficultyShift = false;  // a foe that stays weak enough to be hurt
  CHECK(b.beginAt("hard"));
  CHECK(b.act(Action::attack(0, 1, 0)));
  humanXp = 0;
  for (const Event& e : b.events)
    if (e.type == EventType::XpGained && e.a == 0) humanXp += e.b;
  CHECK_EQ(humanXp, 45);  // 30 x 150%
}

TEST(difficulty_income_is_scaled_for_a_human_side_only) {
  Arena a({{"soldier", 0, 0, 0, false}, {"soldier", 1, 5, 0, false}});
  a.db.rules.economy = true;
  a.place("city", 1, 0, 0);
  a.place("city", 4, 0, 1);
  CHECK(a.beginAt("hard"));
  CHECK_EQ(a.battle.sides[0].resources[0], 150);  // 100 x 150%
  CHECK(a.act(Action::endTurn()));
  CHECK_EQ(a.battle.sides[1].resources[0], 100);  // the computer side's income is as the data says
  Arena b({{"soldier", 0, 0, 0, false}, {"soldier", 1, 5, 0, false}});
  b.db.rules.economy = true;
  b.place("city", 1, 0, 0);
  CHECK(b.beginAt("easy"));
  CHECK_EQ(b.battle.sides[0].resources[0], 50);
}

TEST(difficulty_formulas_read_the_position_and_the_id) {
  Arena a(kClose);
  a.db.rules.fDamage = "a.atk - b.def + difficulty + (difficultyId == 'hard' and 10 or 0)";
  CHECK(a.beginAt("hard"));
  CHECK(a.act(Action::attack(0, 1, 0)));
  // the hard enemy's def is 6 (2 + 2 flat = 4, x150): 5 - 6 + 2 + 10 = 11
  const int hurt = a.battle.units[1].stats[0] - a.battle.units[1].hp;
  CHECK_EQ(hurt, 11);
  Arena n(kClose);
  n.db.rules.fDamage = "a.atk - b.def + difficulty + (difficultyId == 'hard' and 10 or 0)";
  CHECK(n.beginAt("easy"));
  CHECK(n.act(Action::attack(0, 1, 0)));
  CHECK_EQ(n.battle.units[1].stats[0] - n.battle.units[1].hp, 3);  // 5 - 2 + 0
  World plain({"......"}, kClose);  // a game without difficulties: 0 and ""
  plain.db.rules.fDamage = "a.atk - b.def + difficulty + (difficultyId == '' and 1 or 0)";
  CHECK(plain.begin());
  CHECK(plain.act(Action::attack(0, 1, 0)));
  CHECK_EQ(plain.battle.units[1].stats[0] - plain.battle.units[1].hp, 4);  // 5 - 2 + 0 + 1
}

TEST(difficulty_the_hexagon_demo_shifts_its_enemies_by_the_same_rule) {
  Database db;
  std::string err;
  CHECK(loadGame(ote_test::sourceDir() + "/games/demo-hex", db, err));
  addDifficulties(db);
  db.difficulties.percentStatIndex.clear();
  for (size_t s = 0; s < db.stats.size(); ++s)
    if ((int)s != db.rules.statMove) db.difficulties.percentStatIndex.push_back((int)s);
  db.difficulties.list[2].enemy.stats.assign(db.stats.size(), 0);
  db.difficulties.list[2].enemy.statsPercent = 100;
  MapDef map;
  CHECK(loadMap(db, "fortress_ford", map, err));
  CHECK(db.skills.empty() && db.characters.empty());
  Formulas f;
  CHECK(f.init(db, err));
  Battle normal, hard;
  normal.setDifficulty(1);
  hard.setDifficulty(2);
  CHECK(normal.init(db, map, f, 1, err));
  CHECK(hard.init(db, map, f, 1, err));
  CHECK(normal.units.size() == hard.units.size());
  int shifted = 0;
  for (size_t i = 0; i < hard.units.size(); ++i) {
    const Unit& n = normal.units[i];
    const Unit& h = hard.units[i];
    const bool enemy = hard.sides[(size_t)h.side].ai;
    const int levels = enemy ? std::min(2, db.rules.maxLevel - n.level) : 0;
    CHECK_EQ(h.level, n.level + levels);
    for (size_t s = 0; s < h.stats.size(); ++s) {
      const int gain = (db.unitTypes[(size_t)h.type].growths[s] * levels + 50) / 100;
      CHECK_EQ(h.stats[s], n.stats[s] + gain);
    }
    if (enemy && levels > 0) ++shifted;
  }
  CHECK(shifted > 0);
}
