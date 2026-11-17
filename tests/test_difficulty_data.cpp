// docs/DIFFICULTY.md on a game on disk: the loader and its refusals, the arena at every difficulty (levels, flat and percent
// stats, exemptions, units for some difficulties only, addUnit, the story condition, resources), saves and campaigns.
#include "difficulty_fixture.h"
#include "json_edit.h"
#include "core/campaign/difficulty.h"

using namespace ote;

namespace {

bool has(const std::string& hay, const std::string& needle) { return hay.find(needle) != std::string::npos; }

void refusedWith(const std::string& err, const std::string& needle) {
  if (!has(err, needle)) std::printf("  got: %s\n  wanted: %s\n", err.c_str(), needle.c_str());
  CHECK(has(err, needle));
}

const Unit& unit(const Battle& b, const char* id) { return b.units[(size_t)b.unitByStoryId(id)]; }

// kDifficulties with one piece of text replaced.
std::string diffsWith(const std::string& from, const std::string& to) {
  std::string d = difftest::kDifficulties;
  const size_t at = d.find(from);
  if (at == std::string::npos) { std::printf("  fixture text not found: %s\n", from.c_str()); return d; }
  d.replace(at, from.size(), to);
  return d;
}

}  // namespace

TEST(difficulty_data_loads_the_list_default_and_change_rule) {
  difftest::Game g;
  CHECK(g.load("difficulty_load"));
  const DifficultyRules& r = g.g.db.difficulties;
  CHECK(r.any() && r.list.size() == 3);
  CHECK(r.defaultId == "normal" && r.defaultIndex == 1 && r.change == DifficultyChange::LowerOnly);
  CHECK(r.list[0].id == "easy" && r.list[0].name == "Easy" && r.list[0].description == "Gentler foes.");
  CHECK(r.list[0].enemy.levels == -2 && r.list[0].xpPercent == 150 && r.list[0].resourcePercent == 150);
  CHECK(r.list[2].enemy.levels == 2 && r.list[2].enemy.statsPercent == 110 && r.list[2].enemy.stats[(size_t)g.g.db.findStat("def")] == 1);
  CHECK(r.list[1].xpPercent == 100 && r.list[1].enemy.statsPercent == 100 && r.list[1].enemy.levels == 0);
  // every stat but the move stat is scaled by "statsPercent" unless the game lists them
  CHECK_EQ(r.percentStatIndex.size(), (size_t)3);
  CHECK(!r.percentStatsGiven);
}

TEST(difficulty_data_a_game_without_the_key_has_no_difficulty_anywhere) {
  difftest::Game g;
  CHECK(g.load("difficulty_none", "", camptest::mapJson(8, 3, camptest::sides("0", "[]"), difftest::arenaUnits().substr(0, 0) + camptest::heroesOnBlue(""))));
  CHECK(!g.g.db.difficulties.any());
  MapDef map;
  std::string err;
  CHECK(loadMap(g.g.db, "skirmish", map, err));
  Battle b;
  CHECK(b.init(g.g.db, map, g.g.formulas, 1, err));
  CHECK_EQ(b.difficulty, -1);
  Json j;
  saveBattleState(b, j);
  CHECK(!j.has("difficulty"));
  CHECK(unit(b, "eirik").level == 1);
}

TEST(difficulty_data_the_loader_refuses_wrong_difficulties) {
  difftest::Game g;
  refusedWith(g.loadError("difficulty_bad1", diffsWith("\"default\": \"normal\"", "\"default\": \"nope\"")), "\"default\" names unknown difficulty \"nope\"");
  refusedWith(g.loadError("difficulty_bad2", diffsWith("\"default\": \"normal\",", "")), "missing required \"default\"");
  refusedWith(g.loadError("difficulty_bad3", diffsWith("\"lowerOnly\"", "\"sometimes\"")), "\"change\" must be \"never\", \"lowerOnly\" or \"free\"");
  refusedWith(g.loadError("difficulty_bad4", diffsWith("\"id\": \"hard\"", "\"id\": \"easy\"")), "duplicate id");
  refusedWith(g.loadError("difficulty_bad5", diffsWith("\"def\": 1", "\"luck\": 1")), "unknown stat \"luck\"");
  refusedWith(g.loadError("difficulty_bad6", diffsWith("\"levels\": 2", "\"levels\": 2.5")), "\"levels\" must be a whole number");
  refusedWith(g.loadError("difficulty_bad7", diffsWith("\"levels\": 2", "\"levels\": 500")), "\"levels\" must be from -98 to 98");
  refusedWith(g.loadError("difficulty_bad8", diffsWith("\"xpPercent\": 50", "\"xpPercent\": -5")), "\"xpPercent\" must be from 0 to 1000");
  refusedWith(g.loadError("difficulty_bad9", diffsWith("\"statsPercent\": 110", "\"statsPercent\": 110, \"surprise\": 1")), "unknown key \"surprise\"");
  refusedWith(g.loadError("difficulty_bad10", "\"difficulties\": { \"default\": \"a\", \"list\": [] }"), "needs at least one difficulty");
  refusedWith(g.loadError("difficulty_bad11", diffsWith("\"id\": \"normal\"", "\"id\": \"no rmal\"")), "\"id\" must be letters, digits");
  refusedWith(g.loadError("difficulty_bad12", diffsWith("\"change\": \"lowerOnly\",", "\"change\": \"lowerOnly\", \"percentStats\": [\"hp\", \"luck\"],")), "\"percentStats\" names unknown stat \"luck\"");
}

TEST(difficulty_data_percent_stats_can_be_listed) {
  difftest::Game g;
  CHECK(g.load("difficulty_pctlist", diffsWith("\"change\": \"lowerOnly\",", "\"change\": \"lowerOnly\", \"percentStats\": [\"hp\"],")));
  CHECK(g.g.db.difficulties.percentStatsGiven && g.g.db.difficulties.percentStatIndex.size() == 1);
  Battle b;
  std::vector<Event> ev;
  CHECK(g.arena(b, ev, "hard"));
  CHECK_EQ(unit(b, "foe").stats[(size_t)g.g.db.findStat("hp")], 24);   // (20 + 2) x 110%
  CHECK_EQ(unit(b, "foe").stats[(size_t)g.g.db.findStat("atk")], 8);   // not in the list: only the level growth
}

TEST(difficulty_data_the_map_and_unit_keys_are_checked) {
  difftest::Game g;
  const std::string hardOnlyBad = difftest::arenaUnits(R"(,{"type":"grunt","side":"red","x":5,"y":0,"difficulty":["extreme"]})");
  refusedWith(g.loadError("difficulty_u1", difftest::kDifficulties, camptest::mapJson(8, 3, camptest::sides("0", "[]"), hardOnlyBad)),
              "\"difficulty\" names unknown difficulty \"extreme\"");
  const std::string emptyList = difftest::arenaUnits(R"(,{"type":"grunt","side":"red","x":5,"y":0,"difficulty":[]})");
  refusedWith(g.loadError("difficulty_u2", difftest::kDifficulties, camptest::mapJson(8, 3, camptest::sides("0", "[]"), emptyList)),
              "must list at least one difficulty");
  // a map's own numbers: unknown id, unknown key
  std::string map = camptest::mapJson(8, 3, camptest::sides("0", "[]"), difftest::arenaUnits(), difftest::kArenaEvents);
  std::string bad = map;
  bad.insert(bad.rfind('}'), ", \"difficulty\": { \"extreme\": { \"enemy\": { \"levels\": 1 } } }");
  refusedWith(g.loadError("difficulty_m1", difftest::kDifficulties, bad), "unknown difficulty \"extreme\"");
  bad = map;
  bad.insert(bad.rfind('}'), ", \"difficulty\": { \"hard\": { \"enemy\": { \"levels\": 1, \"speed\": 2 } } }");
  refusedWith(g.loadError("difficulty_m2", difftest::kDifficulties, bad), "unknown key \"speed\"");
  // the story condition names a real difficulty
  std::string ev = difftest::kArenaEvents;
  ev.replace(ev.find("\"hard\""), 6, "\"extreme\"");
  refusedWith(g.loadError("difficulty_c1", difftest::kDifficulties, camptest::mapJson(8, 3, camptest::sides("0", "[]"), difftest::arenaUnits(), ev)),
              "\"difficulty\" names unknown difficulty \"extreme\"");
  // in a game without difficulties every difficulty key is refused
  refusedWith(g.loadError("difficulty_n1", "", camptest::mapJson(8, 3, camptest::sides("0", "[]"), difftest::arenaUnits())),
              "the game has no difficulties");
  refusedWith(g.loadError("difficulty_n2", "", camptest::mapJson(8, 3, camptest::sides("0", "[]"), camptest::heroesOnBlue(""), difftest::kArenaEvents)),
              "the game has no difficulties");
}

TEST(difficulty_data_enemy_levels_and_stats_by_difficulty) {
  difftest::Game g;
  CHECK(g.load("difficulty_arena"));
  const int hp = g.g.db.findStat("hp"), atk = g.g.db.findStat("atk"), def = g.g.db.findStat("def"), mov = g.g.db.findStat("mov");
  Battle normal, hard, easy;
  std::vector<Event> ev;
  CHECK(g.arena(normal, ev, "normal"));
  CHECK(g.arena(hard, ev, "hard"));
  CHECK(g.arena(easy, ev, "easy"));
  // normal: as placed
  CHECK(unit(normal, "foe").level == 3 && unit(normal, "foe").stats[(size_t)hp] == 20 && unit(normal, "foe").stats[(size_t)atk] == 6);
  // hard: +2 levels of hero growth (hp 100, atk 100) -> 22 / 8, def 3 + 1 flat = 4, then x110 on hp / atk / def; mov untouched
  const Unit& h = unit(hard, "foe");
  CHECK_EQ(h.level, 5);
  CHECK_EQ(h.stats[(size_t)hp], 24);
  CHECK_EQ(h.stats[(size_t)atk], 9);
  CHECK_EQ(h.stats[(size_t)def], 4);
  CHECK_EQ(h.stats[(size_t)mov], 4);
  CHECK_EQ(h.hp, 24);
  // easy: -2 levels -> level 1, hp 18 / atk 4
  const Unit& e = unit(easy, "foe");
  CHECK(e.level == 1 && e.stats[(size_t)hp] == 18 && e.stats[(size_t)atk] == 4 && e.stats[(size_t)def] == 3);
  // the human hero is never touched
  CHECK(unit(hard, "eirik").level == 1 && unit(hard, "eirik").stats[(size_t)hp] == 20);
  // the stats of a shifted unit equal those of a hero of the same level made by level-ups: growth 100 is exact, no dice
  CHECK(unit(hard, "foe").stats[(size_t)hp] == (20 + 2 + 0) * 110 / 100);
}

TEST(difficulty_data_exemptions) {
  difftest::Game g;
  CHECK(g.load("difficulty_exempt"));
  const int hp = g.g.db.findStat("hp"), atk = g.g.db.findStat("atk"), def = g.g.db.findStat("def");
  Battle hard;
  std::vector<Event> ev;
  CHECK(g.arena(hard, ev, "hard"));
  const Unit& boss = unit(hard, "boss");  // "difficultyShift": false
  CHECK(boss.level == 1 && boss.stats[(size_t)hp] == 30 && boss.stats[(size_t)atk] == 12 && boss.stats[(size_t)def] == 6);
  const Unit& ex = unit(hard, "exact");  // its attack was written on the map: exact; the rest is shifted
  CHECK_EQ(ex.stats[(size_t)atk], 20);
  CHECK_EQ(ex.stats[(size_t)hp], 4);   // grunt hp 4, no growth, x110 = 4.4 -> 4
  CHECK_EQ(ex.stats[(size_t)def], 2);  // 1 + 1 flat = 2, x110 = 2.2 -> 2
  CHECK_EQ(ex.level, 3);
}

TEST(difficulty_data_units_for_some_difficulties_only) {
  difftest::Game g;
  CHECK(g.load("difficulty_only"));
  Battle normal, hard, easy;
  std::vector<Event> ev;
  CHECK(g.arena(normal, ev, "normal"));
  CHECK(g.arena(hard, ev, "hard"));
  CHECK(g.arena(easy, ev, "easy"));
  CHECK(normal.unitByStoryId("hardonly") < 0 && normal.unitByStoryId("easyonly") < 0);
  CHECK(hard.unitByStoryId("hardonly") >= 0 && hard.unitByStoryId("easyonly") < 0);
  CHECK(easy.unitByStoryId("hardonly") < 0 && easy.unitByStoryId("easyonly") >= 0);
  // addUnit with "difficulty" (the start event): only on hard
  CHECK(normal.unitByStoryId("reinf") < 0 && easy.unitByStoryId("reinf") < 0 && hard.unitByStoryId("reinf") >= 0);
}

TEST(difficulty_data_story_conditions_and_resources) {
  difftest::Game g;
  CHECK(g.load("difficulty_story"));
  const int cut = g.g.db.findSwitch("bridge_cut"), met = g.g.db.findSwitch("met_wanderer"), kills = g.g.db.findVariable("kills");
  Battle normal, hard, easy;
  std::vector<Event> ev;
  CHECK(g.arena(normal, ev, "normal"));
  CHECK(g.arena(hard, ev, "hard"));
  CHECK(g.arena(easy, ev, "easy"));
  CHECK(!normal.storyState.switches[(size_t)cut] && hard.storyState.switches[(size_t)cut] && !easy.storyState.switches[(size_t)cut]);
  CHECK(normal.storyState.switches[(size_t)met] && hard.storyState.switches[(size_t)met] && !easy.storyState.switches[(size_t)met]);  // atLeast normal
  CHECK(normal.storyState.variables[(size_t)kills] == 5 && easy.storyState.variables[(size_t)kills] == 5 && hard.storyState.variables[(size_t)kills] == 0);  // atMost normal
  // giveResource 100 on the blue side: scaled by resourcePercent
  CHECK_EQ(normal.sides[0].resources[0], 100);
  CHECK_EQ(hard.sides[0].resources[0], 50);
  CHECK_EQ(easy.sides[0].resources[0], 150);
}

TEST(difficulty_data_a_maps_numbers_replace_the_games) {
  difftest::Game g;
  std::string map = camptest::mapJson(8, 3, camptest::sides("0", "[]"), difftest::arenaUnits());
  map.insert(map.rfind('}'), ", \"difficulty\": { \"hard\": { \"enemy\": { \"levels\": 3 } } }");  // "hard adds 3 enemy levels here"
  CHECK(g.load("difficulty_override", difftest::kDifficulties, map));
  CHECK_EQ(g.g.db.difficulties.list[2].enemy.levels, 2);  // the game's own is untouched
  Battle hard, easy;
  std::vector<Event> ev;
  CHECK(g.arena(hard, ev, "hard"));
  CHECK(g.arena(easy, ev, "easy"));
  CHECK_EQ(unit(hard, "foe").level, 6);  // 3 + 3 (replaces +2, does not add to it)
  CHECK_EQ(unit(hard, "foe").stats[(size_t)g.g.db.findStat("hp")], 25);  // (20 + 3) x 110% = 25.3
  CHECK_EQ(unit(easy, "foe").level, 1);  // not named: the game's -2
}

TEST(difficulty_data_changing_the_rules_lowerOnly_never_free) {
  DifficultyRules r;
  r.list.resize(3);
  r.change = DifficultyChange::LowerOnly;
  CHECK(r.mayChange(2, 1) && r.mayChange(2, 0) && r.mayChange(1, 0));
  CHECK(!r.mayChange(0, 1) && !r.mayChange(1, 2) && !r.mayChange(1, 1) && !r.mayChange(1, 7) && !r.mayChange(1, -1));
  r.change = DifficultyChange::Never;
  CHECK(!r.mayChange(2, 0) && !r.mayChange(0, 2));
  r.change = DifficultyChange::Free;
  CHECK(r.mayChange(0, 2) && r.mayChange(2, 0) && !r.mayChange(1, 1));
}
