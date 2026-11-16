// Weapon ranks and stat caps stay out of games that do not use them (docs/SRPG.md section 7, "Saves and hash"), and a
// whole battle on the fixture plays the same way twice without stalling.
#include "core/ai/ai.h"
#include "srpg_chars_fixture.h"
#include "srpg_ranks_fixture.h"

using namespace ote;

namespace {
std::string hashOf(const std::string& dir) {
  Database db;
  std::string err, out;
  if (!loadGame(dir, db, err) || !computeDataHash(db, "field", out, err)) return "error: " + err;
  return out;
}
}  // namespace

TEST(ranks_files_that_hold_nothing_keep_the_hash_and_real_ones_change_it) {
  charstest::Copy c("ranks_hash");
  const std::string without = hashOf(c.dir);
  CHECK(without.rfind("error", 0) != 0);
  c.put("data/ranks.json", R"({ "growth": "use", "ranks": [] })");
  c.put("data/caps.json", R"({ "caps": {} })");
  CHECK_STR(hashOf(c.dir), without);  // the editor writes such files to remember a switch; saves stay valid
  c.put("data/caps.json", R"({ "caps": { "atk": 20 } })");
  const std::string capped = hashOf(c.dir);
  CHECK(capped != without);
  c.put("data/caps.json", R"({ "caps": { "atk": 21 } })");
  CHECK(hashOf(c.dir) != capped);  // a cap is a rule
  c.drop("data/caps.json");
  CHECK_STR(hashOf(c.dir), without);
}

TEST(ranks_a_rank_name_is_cosmetic_but_the_experience_is_not) {
  rankstest::Copy c("ranks_hash2");
  const std::string base = hashOf(c.dir);
  c.swap("data/ranks.json", "\"name\": \"B\"", "\"name\": \"Master\"");
  CHECK_STR(hashOf(c.dir), base);
  c.swap("data/ranks.json", "\"xp\": 12", "\"xp\": 13");
  CHECK(hashOf(c.dir) != base);
}

TEST(ranks_a_save_with_ranks_is_refused_by_a_game_without) {
  charstest::Copy c("ranks_refused");
  savetest::Rig rig;
  CHECK(rig.load(c.dir, "field"));
  rig.start();
  Json j;
  saveBattleState(rig.battle, j);
  std::string text = j.dump(0);
  const size_t at = text.find("\"level\":");
  CHECK(at != std::string::npos);
  text.insert(at, "\"ranks\":{\"sword\":3},");
  Json bad;
  std::string err;
  CHECK(Json::parse(text, bad, err));
  CHECK(!restoreBattleState(rig.battle, bad, err));
  CHECK(rankstest::saysIn(err, "this game has no weapon ranks"));
}

namespace {
struct Played {
  bool over = false, hung = false;
  int rankUps = 0;
  std::string trace;
};

Played play(uint32_t seed, const char* map) {
  savetest::Rig rig;
  Played r;
  if (!rig.load(ote_test::sourceDir() + "/tests/fixtures/srpg-ranks", map, seed)) return r;
  Battle& b = rig.battle;
  std::vector<Event> events;
  b.start(events);
  const long limit = 10L * (long)b.units.size() + 100;
  while (!b.over && b.round <= 60) {
    long steps = 0;
    while (aiTakeStep(b, events))
      if (++steps > limit) {
        r.hung = true;
        return r;
      }
  }
  for (const Event& e : events) {
    r.rankUps += e.type == EventType::RankUp;
    r.trace += std::to_string((int)e.type) + ":" + std::to_string(e.a) + "," + std::to_string(e.b) + "," +
               std::to_string(e.c) + "," + std::to_string(e.d) + ";";
  }
  r.trace += savetest::stateText(b);
  r.over = b.over;
  return r;
}
}  // namespace

TEST(ranks_fixture_battles_end_and_repeat_on_20_seeds) {
  int withRankUps = 0;
  for (uint32_t seed = 1; seed <= 20; ++seed) {
    const Played a = play(seed, "field"), b = play(seed, "field");
    CHECK(!a.hung);
    CHECK(a.over);
    CHECK(a.trace == b.trace);
    withRankUps += a.rankUps > 0;
  }
  CHECK(withRankUps > 3);  // the ranks really took part
}
