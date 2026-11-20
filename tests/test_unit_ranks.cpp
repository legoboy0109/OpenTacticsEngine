// docs/SRPG.md "Unit ranks": the rank table of rules.progression.levels. The rank read off total experience, its stat bonus and skills, experience
// that stops at the top rank, the UnitRankUp event, a map unit's "xp", saves, other games untouched and the loader's refusals.
// Fixture tests/fixtures/unit-ranks: soldier str 6 / def 2 / hp 40, sword 4, hit 100, damage = str + might - def; ranks Veteran at 10 xp (str +1),
// Elite at 30 (str +2, def +1, hp +6, skill steady = 2 less damage taken), Hero at 60 (str +3, def +2, hp +10, steady); a hit pays 10 experience, a kill 30.
#include "core/battle/save.h"
#include "srpg_skills_fixture.h"

using namespace ote;
using skilltest::saysIn;

namespace {

struct Duel {
  skilltest::Copy c;
  skilltest::World w;
  explicit Duel(const char* name, const char* map = "duel") : c(name, "unit-ranks"), w(map, c.dir) {}
  Battle& b() { return w.b(); }
  Unit& unit(const char* id) { return w.unit(id); }
  bool hit(const char* who, const char* target, int fromX = -1, int fromY = -1) {
    if (fromX >= 0 && !w.apply(Action::move(w.id(who), w.at(fromX, fromY)))) return false;
    return w.apply(Action::attack(w.id(who), w.id(target), w.db().findWeapon("sword")));
  }
  // How many events of the type are about `unit` (the struck dummy answers and earns experience of its own).
  int countFor(EventType t, const char* unit) {
    int n = 0;
    for (const Event& e : w.events) n += e.type == t && e.a == w.id(unit);
    return n;
  }
  const Event* find(EventType t) const {
    for (const Event& e : w.events)
      if (e.type == t) return &e;
    return nullptr;
  }
};

std::string loadError(const char* name, const char* from, const char* to, const char* file = "game.json") {
  skilltest::Copy c(name, "unit-ranks");
  c.swap(file, from, to);
  return c.gameError();
}

}  // namespace

TEST(the_rank_is_read_off_total_experience_and_adds_stats_and_skills) {
  Duel d("ur_read");
  const Database& db = d.w.db();
  CHECK(db.rules.unitRanks.on());
  Unit& sol = d.unit("sol");
  CHECK_EQ(d.b().unitRank(sol), -1);  // no rank before the first entry
  CHECK_EQ(d.w.stat(sol, "str"), 6);
  CHECK_EQ(d.b().unitRank(d.unit("vet")), 0);  // map units may start with experience
  CHECK_EQ(d.w.stat(d.unit("vet"), "str"), 7);
  CHECK_STR(d.w.skillIds(d.unit("vet")), "");
  Unit& elite = d.unit("elite");
  CHECK_EQ(d.b().unitRank(elite), 1);
  CHECK_EQ(d.w.stat(elite, "str"), 8);  // the entry's own bonus, not added to the ranks below it
  CHECK_EQ(d.w.stat(elite, "def"), 3);
  CHECK_EQ(d.w.stat(elite, "hp"), 46);
  CHECK_EQ(elite.hp, 46);  // a unit placed with a rank starts at the full hit points of that rank
  CHECK_STR(d.w.skillIds(elite), "steady");
  CHECK_EQ(elite.level, 1);  // no levelling: the level stays a label
}

TEST(a_hit_pays_ten_and_crossing_a_threshold_is_a_rank_up_event) {
  Duel d("ur_hit");
  Unit& sol = d.unit("sol");
  CHECK(d.hit("sol", "foe"));
  CHECK_EQ(sol.xp, 10);
  CHECK_EQ(d.countFor(EventType::XpGained, "sol"), 1);
  CHECK_EQ(d.w.count(EventType::LevelUp), 0);
  CHECK_EQ(d.countFor(EventType::UnitRankUp, "sol"), 1);
  const Event* up = d.find(EventType::UnitRankUp);
  CHECK(up && up->a == d.w.id("sol") && up->b == 0 && up->c == -1);
  CHECK_EQ(up->data[(size_t)d.w.db().findStat("str")], 1);
  // The unit hit again below the next threshold: experience, no rank-up.
  CHECK(d.hit("vet", "foe", 3, 3));  // 10 -> 20
  CHECK_EQ(d.unit("vet").xp, 20);
  CHECK_EQ(d.w.count(EventType::UnitRankUp), 0);
}

TEST(a_rank_that_raises_hit_points_arrives_filled_and_a_kill_can_skip_ranks) {
  Duel d("ur_kill");
  Unit& sol = d.unit("sol");
  sol.hp = 30;  // hurt
  CHECK(d.hit("sol", "weak", 5, 0));
  CHECK_EQ(d.w.count(EventType::UnitDied), 1);
  CHECK_EQ(sol.xp, 30);
  const Event* up = d.find(EventType::UnitRankUp);
  CHECK(up && up->b == 1 && up->c == -1);  // none -> Elite at once
  CHECK_EQ(up->data[(size_t)d.w.db().findStat("hp")], 6);
  CHECK_EQ(up->data[(size_t)d.w.db().findStat("def")], 1);
  CHECK_EQ(sol.hp, 36);  // the 6 more hit points came with it
}

TEST(experience_stops_at_the_top_rank) {
  Duel d("ur_top");
  Unit& elite = d.unit("elite");
  elite.xp = 55;
  CHECK(d.hit("elite", "foe", 3, 1));
  CHECK_EQ(elite.xp, 60);  // clamped to the last "at"
  const Event* gained = d.find(EventType::XpGained);
  CHECK(gained && gained->b == 5);
  const Event* up = d.find(EventType::UnitRankUp);
  CHECK(up && up->b == 2 && up->c == 1);
  CHECK_EQ(d.b().unitRank(elite), 2);
  // At the top nothing more is paid and nothing is announced.
  Duel e("ur_top2");
  e.unit("elite").xp = 60;
  CHECK(e.hit("elite", "foe", 3, 1));
  CHECK_EQ(e.countFor(EventType::XpGained, "elite"), 0);
  CHECK_EQ(e.unit("elite").xp, 60);
}

TEST(the_steady_skill_of_a_rank_counts_in_a_fight) {
  Duel d("ur_skill");
  // The foe (str 3, sword 4) hits the elite (def 3 + steady -2): 7 - 3 - 2 = 2; the plain soldier (def 2): 5.
  CHECK(d.hit("elite", "foe", 3, 1));
  CHECK_EQ(d.unit("elite").hp, 46 - 2);
  Duel p("ur_skill2");
  CHECK(p.hit("sol", "foe"));
  CHECK_EQ(p.unit("sol").hp, 40 - 5 + 0);
}

TEST(a_save_keeps_the_rank_because_it_keeps_the_experience) {
  Duel d("ur_save");
  CHECK(d.hit("sol", "foe"));
  Json j;
  saveBattleState(d.b(), j);
  Duel t("ur_save2");
  std::string err;
  CHECK(restoreBattleState(t.b(), j, err));
  CHECK_STR(savetest::stateText(t.b()), savetest::stateText(d.b()));
  CHECK_EQ(t.b().unitRank(t.unit("sol")), 0);
  CHECK_EQ(t.w.stat(t.unit("sol"), "str"), 7);
}

TEST(a_game_without_the_table_levels_up_as_before_and_refuses_xp_on_the_map) {
  skilltest::Copy c("ur_plain", "unit-ranks");
  c.swap("game.json", "\"levels\": [", "\"xpPerLevel\": 15, \"ignored\": [");  // the table is not read: ordinary levelling
  CHECK_STR(c.mapError("field"), "");
  CHECK(saysIn(c.mapError("duel"), "\"xp\" needs a rank table"));
  skilltest::World w("field", c.dir);
  CHECK(!w.db().rules.unitRanks.on());
  Unit& sol = w.unit("sol");
  CHECK(w.apply(Action::move(w.id("sol"), w.at(6, 3))));  // beside the dummy
  CHECK(w.apply(Action::attack(w.id("sol"), w.id("foe"), w.db().findWeapon("sword"))));
  CHECK_EQ(w.count(EventType::UnitRankUp), 0);
  CHECK_EQ(w.count(EventType::LevelUp), 0);
  CHECK_EQ(sol.xp, 10);
  CHECK_EQ(w.b().unitRank(sol), -1);
  CHECK_EQ(w.stat(sol, "str"), 6);
}

TEST(the_loader_refuses_wrong_rank_tables_naming_the_file_the_entry_and_the_key) {
  CHECK_STR(loadError("url0", "\"name\": \"Veteran\"", "\"name\": \"Veteran\""), "");
  CHECK(saysIn(loadError("url1", "\"xpHit\": \"10\",", "\"xpHit\": \"10\", \"xpPerLevel\": 50,"), "game.json: rules.progression: \"xpPerLevel\" belongs to levelling"));
  CHECK(saysIn(loadError("url2", "\"at\": 30", "\"at\": 10"), "game.json: rules.progression, \"levels\" entry 2 (\"Elite\"): \"at\" must be higher than the one before it (10)"));
  CHECK(saysIn(loadError("url3", "\"steady\"]", "\"nosuch\"]"), "game.json: rules.progression, \"levels\" entry 2 (\"Elite\"): \"skills\" names unknown skill \"nosuch\""));
  CHECK(saysIn(loadError("url4", "\"bonus\": { \"str\": 1 }", "\"bonus\": { \"luck\": 1 }"), "entry 1 (\"Veteran\"): \"bonus\" names unknown stat \"luck\""));
  CHECK(saysIn(loadError("url5", "\"badge\": \"I\"", "\"badge\": \"Vets\""), "entry 1 (\"Veteran\"): \"badge\" must be 0 to 3 plain characters"));
  CHECK(saysIn(loadError("url6", "\"name\": \"Veteran\"", "\"name\": \"\""), "\"name\" must be 1 to 24 characters"));
  CHECK(saysIn(loadError("url7", "\"badge\": \"I\", ", "\"badge\": \"I\", \"colour\": 3, "), "entry 1: unknown key \"colour\""));
  CHECK(saysIn(loadError("url8", "\"at\": 10", "\"at\": -4"), "\"at\" must be a whole number from 0 to"));
  CHECK(saysIn(loadError("url9", "\"enabled\": true,", "\"enabled\": false,"), "\"levels\" needs experience"));
  CHECK(saysIn(loadError("url10", "\"weapons\": [\"sword\"] },\n  { \"id\": \"dummy\"", "\"weapons\": [\"sword\"], \"maxLevel\": 5 },\n  { \"id\": \"dummy\"", "data/units.json"), "\"maxLevel\" belongs to levelling"));
}

TEST(a_map_unit_may_only_say_xp_when_the_game_has_the_table_and_within_it) {
  skilltest::Copy c("ur_map", "unit-ranks");
  c.swap("maps/duel.json", "\"id\": \"vet\", \"xp\": 10", "\"id\": \"vet\", \"xp\": 61");
  CHECK(saysIn(c.mapError("duel"), "\"xp\" must be a whole number from 0 to 60"));
}
