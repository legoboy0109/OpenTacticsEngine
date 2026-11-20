// docs/SRPG.md "Unit ranks", merit mode: "rules.ranks", the same rank table on its own saved counter, beside levelling. The rank read off `merit` (never
// off experience), merit paid per hit and per defeat, a level-up and a rank-up in one action, the cap at the top, saves and refusals, and the loader.
// Fixture tests/fixtures/merit-ranks: soldier hp 40 / str 6 / def 2, growth hp +1 per level, 15 xp per level, a hit pays 10 xp, a kill 30; merit: 5 per hit,
// 20 per defeat; ranks Veteran at 10 (str +1), Elite at 30 (str +2, def +1, hp +6, steady), Hero at 60.
#include "core/ai/ai.h"
#include "core/ai/ai_internal.h"
#include "core/battle/save.h"
#include "srpg_skills_fixture.h"

using namespace ote;
using skilltest::saysIn;

namespace {

struct Duel {
  skilltest::Copy c;
  skilltest::World w;
  explicit Duel(const char* name, const char* map = "duel") : c(name, "merit-ranks"), w(map, c.dir) {}
  Battle& b() { return w.b(); }
  Unit& unit(const char* id) { return w.unit(id); }
  bool hit(const char* who, const char* target, int fromX = -1, int fromY = -1) {
    if (fromX >= 0 && !w.apply(Action::move(w.id(who), w.at(fromX, fromY)))) return false;
    return w.apply(Action::attack(w.id(who), w.id(target), w.db().findWeapon("sword")));
  }
  int countFor(EventType t, const char* unit) {
    int n = 0;
    for (const Event& e : w.events) n += e.type == t && e.a == w.id(unit);
    return n;
  }
  // Index of the first event of the type in the last action (-1 = none).
  int indexOf(EventType t) const {
    for (size_t i = 0; i < w.events.size(); ++i)
      if (w.events[i].type == t) return (int)i;
    return -1;
  }
};

std::string loadError(const char* name, const char* from, const char* to, const char* file = "game.json") {
  skilltest::Copy c(name, "merit-ranks");
  c.swap(file, from, to);
  return c.gameError();
}

}  // namespace

TEST(the_rank_is_read_off_merit_not_experience_and_levelling_goes_on) {
  Duel d("mr_read");
  const Database& db = d.w.db();
  CHECK(db.rules.unitRanks.on() && db.rules.unitRanks.merit && !db.rules.unitRanks.replacesLevels());
  CHECK_STR(db.rules.unitRanks.counter, "Merit");
  Unit& sol = d.unit("sol");
  CHECK_EQ(d.b().unitRank(sol), -1);
  CHECK_EQ(d.b().unitRankPoints(sol), 0);
  sol.xp = 14;  // experience does not make a rank here
  CHECK_EQ(d.b().unitRank(sol), -1);
  CHECK_EQ(d.b().unitRank(d.unit("vet")), 0);
  CHECK_EQ(d.w.stat(d.unit("vet"), "str"), 7);
  Unit& elite = d.unit("elite");
  CHECK_EQ(d.b().unitRank(elite), 1);
  CHECK_EQ(d.w.stat(elite, "str"), 8);
  CHECK_EQ(d.w.stat(elite, "hp"), 46);
  CHECK_EQ(elite.hp, 46);
  CHECK_STR(d.w.skillIds(elite), "steady");
}

TEST(a_hit_pays_experience_and_merit_and_a_level_up_and_a_rank_up_both_happen) {
  Duel d("mr_both");
  Unit& ready = d.unit("ready");  // merit 5 (one hit from Veteran) ...
  ready.xp = 10;                   // ... and xp 10 (one hit from a level)
  CHECK(d.hit("ready", "foe", 3, 1));
  CHECK_EQ(ready.level, 2);
  CHECK_EQ(ready.xp, 5);
  CHECK_EQ(ready.merit, 10);
  CHECK_EQ(d.countFor(EventType::LevelUp, "ready"), 1);
  CHECK_EQ(d.countFor(EventType::UnitRankUp, "ready"), 1);
  CHECK(d.indexOf(EventType::LevelUp) >= 0 && d.indexOf(EventType::LevelUp) < d.indexOf(EventType::UnitRankUp));  // the level first, then the rank
  CHECK_EQ(d.b().unitRank(ready), 0);
  CHECK_EQ(d.w.stat(ready, "str"), 7);  // the rank's bonus on top of the (grown) stats
  // The struck dummy counters and earns merit of its own: every side's units earn it.
  CHECK_EQ(d.unit("foe").merit, 5);
}

TEST(a_defeat_pays_the_defeat_merit_and_can_skip_ranks) {
  Duel d("mr_kill");
  Unit& ready = d.unit("ready");
  ready.hp = 30;
  CHECK(d.hit("ready", "weak", 5, 0));
  CHECK_EQ(d.w.count(EventType::UnitDied), 1);
  CHECK_EQ(ready.merit, 5 + 5 + 20);  // 5 before, the hit, the defeat
  CHECK_EQ(d.b().unitRank(ready), 1);
  CHECK_EQ(d.w.count(EventType::UnitRankUp), 2);  // Veteran after the hit, Elite after the defeat
  // The hit points of Elite (+6) came with the rank: levels (+1 each) and the rank raised the maximum and the unit with it.
  CHECK(ready.hp > 30);
}

TEST(merit_stops_at_the_top_rank_and_nothing_more_is_paid) {
  Duel d("mr_top");
  Unit& elite = d.unit("elite");
  elite.merit = 58;
  CHECK(d.hit("elite", "foe", 3, 1));
  CHECK_EQ(elite.merit, 60);
  const Event* up = nullptr;
  for (const Event& e : d.w.events)
    if (e.type == EventType::UnitRankUp) up = &e;
  CHECK(up && up->b == 2 && up->c == 1);
  Duel e("mr_top2");
  e.unit("elite").merit = 60;
  CHECK(e.hit("elite", "foe", 3, 1));
  CHECK_EQ(e.unit("elite").merit, 60);
  CHECK_EQ(e.w.count(EventType::UnitRankUp), 0);
}

TEST(a_save_keeps_merit_only_in_a_game_with_the_block_and_refuses_it_otherwise) {
  Duel d("mr_save");
  d.unit("ready").xp = 10;
  CHECK(d.hit("ready", "foe", 3, 1));
  Json j;
  saveBattleState(d.b(), j);
  const std::string text = j.dump(0);
  CHECK(text.find("\"merit\":10") != std::string::npos);
  Duel t("mr_save2");
  std::string err;
  CHECK(restoreBattleState(t.b(), j, err));
  CHECK_STR(savetest::stateText(t.b()), text);
  CHECK_EQ(t.unit("ready").merit, 10);
  CHECK_EQ(t.b().unitRank(t.unit("ready")), 0);
  auto tryWith = [&](const char* from, const char* to) -> std::string {
    std::string s = text;
    const size_t at = s.find(from);
    if (at == std::string::npos) return "(missing)";
    s.replace(at, std::string(from).size(), to);
    Json bad;
    std::string perr;
    if (!Json::parse(s, bad, perr)) return "(parse) " + perr;
    Duel fresh("mr_save_bad");
    return restoreBattleState(fresh.b(), bad, err) ? "" : err;
  };
  CHECK_STR(tryWith("\"merit\":10", "\"merit\":10"), "");
  CHECK(saysIn(tryWith("\"merit\":10", "\"merit\":61"), "merit: value 61 is outside 0..60"));
  CHECK(saysIn(tryWith("\"merit\":10", "\"zz\":10"), "missing"));
  // The games without the block write no key at all.
  skilltest::World plain("duel", ote_test::sourceDir() + "/tests/fixtures/unit-ranks");
  CHECK(savetest::stateText(plain.b()).find("merit") == std::string::npos);
}

TEST(a_computer_game_with_merit_is_the_same_every_time) {
  auto play = [](const char* name) {
    skilltest::Copy c(name, "merit-ranks");
    c.swap("maps/duel.json", "\"id\": \"blue\", \"name\": \"Blue\", \"controller\": \"human\"", "\"id\": \"blue\", \"name\": \"Blue\", \"controller\": \"ai\"");
    skilltest::World w("duel", c.dir);
    std::vector<Event> events;
    int ups = 0, guard = 0;
    while (!w.b().over && w.b().round <= 30 && guard++ < 3000) {
      events.clear();
      aiTakeStep(w.b(), events);
      for (const Event& e : events) ups += e.type == EventType::UnitRankUp;
    }
    return std::make_pair(savetest::stateText(w.b()), ups);
  };
  const auto a = play("mr_ai_a"), b = play("mr_ai_b");
  CHECK_STR(a.first, b.first);
  CHECK_EQ(a.second, b.second);
  CHECK(a.second >= 1);
}

TEST(the_computer_values_a_ranked_unit_a_little_more_by_merit) {
  Duel d("mr_value");
  CHECK_EQ((int)ai::unitValue(d.b(), d.unit("sol")), 1000);
  CHECK_EQ((int)ai::unitValue(d.b(), d.unit("vet")), 1100);
  CHECK_EQ((int)ai::unitValue(d.b(), d.unit("elite")), 1200);
}

TEST(the_loader_refuses_wrong_merit_blocks_naming_the_file_the_entry_and_the_key) {
  CHECK_STR(loadError("mr0", "\"name\": \"Merit\"", "\"name\": \"Honour\""), "");
  CHECK(saysIn(loadError("mr1", "\"xpPerLevel\": 15, \"maxLevel\": 10,", "\"levels\": [ { \"at\": 5, \"name\": \"x\" } ],"),
               "game.json: rules.ranks: \"rules.progression\" already has a rank table"));
  CHECK(saysIn(loadError("mr2", "\"perHit\": 5,", "\"perHit\": 5, \"colour\": 1,"), "game.json: rules.ranks: unknown key \"colour\""));
  CHECK(saysIn(loadError("mr3", "\"perHit\": 5", "\"perHit\": -5"), "rules.ranks: \"perHit\" must be a whole number from 0 to"));
  CHECK(saysIn(loadError("mr4", "\"name\": \"Merit\"", "\"name\": \"\""), "rules.ranks: \"name\" (the counter's name) must be 1 to 16 plain characters"));
  CHECK(saysIn(loadError("mr5", "\"at\": 30", "\"at\": 10"), "game.json: rules.ranks, \"levels\" entry 2 (\"Elite\"): \"at\" must be higher than the one before it (10)"));
  CHECK(saysIn(loadError("mr6", "\"steady\"]", "\"nosuch\"]"), "game.json: rules.ranks, \"levels\" entry 2 (\"Elite\"): \"skills\" names unknown skill \"nosuch\""));
  CHECK(saysIn(loadError("mr7", "\"bonus\": { \"str\": 1 }", "\"bonus\": { \"luck\": 1 }"), "rules.ranks, \"levels\" entry 1 (\"Veteran\"): \"bonus\" names unknown stat \"luck\""));
  // Ranks beside levelling and promotion are the point: these load.
  CHECK_STR(loadError("mr8", "\"xpPerLevel\": 15", "\"xpPerLevel\": 20"), "");
  // A map unit: "merit" within the table, "xp" needs the experience-driven table.
  skilltest::Copy c("mr_map", "merit-ranks");
  c.swap("maps/duel.json", "\"merit\": 10", "\"merit\": 61");
  CHECK(saysIn(c.mapError("duel"), "\"merit\" must be a whole number from 0 to 60"));
  skilltest::Copy x("mr_map2", "merit-ranks");
  x.swap("maps/duel.json", "\"merit\": 30", "\"xp\": 30");
  CHECK(saysIn(x.mapError("duel"), "\"xp\" needs a rank table"));
  skilltest::Copy p("mr_map3", "unit-ranks");
  p.swap("maps/duel.json", "\"xp\": 10", "\"merit\": 10");
  CHECK(saysIn(p.mapError("duel"), "\"merit\" needs a rank counter"));
}
