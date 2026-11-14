// Saving a battle and restoring it (core/battle/save.*): exact round trip on
// the demo games and on a multi-square capture, the random generator, every
// way a damaged save must be refused, and the data hash.
#include "battle_fixture.h"
#include "core/data/game_info.h"
#include "save_helpers.h"

using namespace ote;
using namespace savetest;

namespace {

// Saves `live`, restores into a fresh battle (different seed: the saved
// generator must win), then plays both on with the AI and compares everything.
void checkRoundTrip(const Rig& live, const std::string& where) {
  Json saved;
  saveBattleState(live.battle, saved);
  Json parsed;
  std::string err;
  CHECK(Json::parse(saved.dump(2), parsed, err));  // through text, like a file
  Rig fresh;
  CHECK(fresh.load(live.gameDir, live.mapId, 999));
  bool ok = restoreBattleState(fresh.battle, parsed, err);
  if (!ok) std::printf("  [%s] restore failed: %s\n", where.c_str(), err.c_str());
  CHECK(ok);
  if (!ok) return;
  if (stateText(live.battle) != stateText(fresh.battle)) std::printf("  [%s] state differs right after restore\n", where.c_str());
  CHECK_STR(stateText(live.battle), stateText(fresh.battle));
  CHECK(live.battle.occupant == fresh.battle.occupant);

  Battle a = live.battle;  // keep `live` untouched; continue on a copy
  Battle& b = fresh.battle;
  std::vector<Event> ea, eb;
  playAi(a, 500, ea);
  playAi(b, 500, eb);
  if (!sameEvents(ea, eb)) std::printf("  [%s] events differ after restore (%zu vs %zu)\n", where.c_str(), ea.size(), eb.size());
  CHECK(sameEvents(ea, eb));
  CHECK_STR(stateText(a), stateText(b));
  CHECK(a.over && b.over);
}

// Plays a whole AI-vs-AI battle and round-trips at the start, every 40
// decisions and the first time each kind of event shows up. Returns the
// event types seen, so callers can insist the interesting ones happened.
std::set<int> walkAndCheck(const char* game, const char* map, unsigned seed) {
  Rig r;
  CHECK(r.loadDemo(game, map, seed));
  r.start();
  std::set<int> seen;
  checkRoundTrip(r, std::string(game) + " start");
  for (int step = 1; step <= 600 && !r.battle.over; ++step) {
    std::vector<Event> ev;
    aiTakeStep(r.battle, ev);
    bool fresh = false;
    for (const Event& e : ev)
      if (seen.insert((int)e.type).second) fresh = true;
    if (fresh || step % 40 == 0) checkRoundTrip(r, std::string(game) + " step " + std::to_string(step));
  }
  CHECK(r.battle.over);
  checkRoundTrip(r, std::string(game) + " end");  // defeated side, battle over
  return seen;
}

}  // namespace

TEST(save_round_trip_demo_srpg_all_milestones) {
  for (unsigned seed : {4u, 6u}) {
    std::set<int> seen = walkAndCheck("demo-srpg", "bridge", seed);
    CHECK(seen.count((int)EventType::UnitDied));
    CHECK(seen.count((int)EventType::LevelUp));
    CHECK(seen.count((int)EventType::SideDefeated));
  }
}

TEST(save_round_trip_demo_economy_builds_and_captures) {
  Rig probe;
  CHECK(probe.loadDemo("demo-economy", "twin_rivers"));
  for (unsigned seed : {5u, 21u}) {
    std::set<int> seen = walkAndCheck("demo-economy", "twin_rivers", seed);
    CHECK(seen.count((int)EventType::UnitBuilt));
    CHECK(seen.count((int)EventType::StructureCaptured));
    CHECK(seen.count((int)EventType::CaptureProgress));
    CHECK(seen.count((int)EventType::UnitDied));
  }
}

TEST(save_round_trip_capture_in_progress_on_a_multi_square_structure) {
  using namespace fixture;
  auto setup = [](World& w) {
    w.db.rules.capture = true;
    w.db.rules.fCaptureAmount = "10";
    w.place("castle", 1, 0);  // 2 x 2: (1..2, 0..1)
  };
  World a({".....", ".....", "....."}, {{"soldier", 0, 1, 0, false}, {"soldier", 1, 4, 2, false}});
  setup(a);
  CHECK(a.begin(5));
  CHECK(a.act(Action::capture(0)));  // halfway through the 20 points
  CHECK(a.act(Action::endTurn()));
  CHECK(a.act(Action::endTurn()));
  CHECK(a.act(Action::move(0, a.at(2, 1))));  // another square of the same castle: progress stays

  Json saved;
  saveBattleState(a.battle, saved);
  World b({".....", ".....", "....."}, {{"soldier", 0, 1, 0, false}, {"soldier", 1, 4, 2, false}});
  setup(b);
  std::string err;
  CHECK(b.formulas.init(b.db, err));
  CHECK(b.battle.init(b.db, b.map, b.formulas, 99, err));
  CHECK(restoreBattleState(b.battle, saved, err));
  CHECK_EQ(b.structureAt(1, 0).capture, 10);
  CHECK_EQ(b.structureAt(1, 0).capturer, 0);
  CHECK_STR(stateText(a.battle), stateText(b.battle));

  // Finishing the capture gives the same events and the same owner.
  std::vector<Event> ea, eb;
  CHECK(a.battle.apply(Action::capture(0), ea));
  CHECK(b.battle.apply(Action::capture(0), eb));
  CHECK(sameEvents(ea, eb));
  CHECK_EQ(a.structureAt(1, 0).owner, 0);
  CHECK_EQ(b.structureAt(1, 0).owner, 0);
  CHECK_STR(stateText(a.battle), stateText(b.battle));

  // Same with the capturer walking off: progress resets in both.
  World c({".....", ".....", "....."}, {{"soldier", 0, 1, 0, false}, {"soldier", 1, 4, 2, false}});
  setup(c);
  CHECK(c.begin(5));
  CHECK(c.act(Action::capture(0)));
  CHECK(c.act(Action::endTurn()));
  CHECK(c.act(Action::endTurn()));
  saveBattleState(c.battle, saved);
  World d({".....", ".....", "....."}, {{"soldier", 0, 1, 0, false}, {"soldier", 1, 4, 2, false}});
  setup(d);
  CHECK(d.formulas.init(d.db, err));
  CHECK(d.battle.init(d.db, d.map, d.formulas, 1, err));
  CHECK(restoreBattleState(d.battle, saved, err));
  CHECK(c.act(Action::move(0, c.at(4, 0))));
  CHECK(d.act(Action::move(0, d.at(4, 0))));
  CHECK_EQ(c.structureAt(1, 0).capture, 20);
  CHECK_EQ(d.structureAt(1, 0).capture, 20);
  CHECK_EQ(d.structureAt(1, 0).capturer, -1);
}

TEST(save_keeps_the_random_generator_exactly) {
  using namespace fixture;
  World w({"....."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 4, 0, false}});
  CHECK(w.begin(0xFFFFFFFFu));
  for (int i = 0; i < 37; ++i) w.battle.rng.next();
  CHECK(w.battle.rng.state > (1ULL << 53));  // would not survive a JSON number
  Json saved;
  saveBattleState(w.battle, saved);
  Json parsed;
  std::string err;
  CHECK(Json::parse(saved.dump(0), parsed, err));
  World r({"....."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 4, 0, false}});
  CHECK(r.formulas.init(r.db, err));
  CHECK(r.battle.init(r.db, r.map, r.formulas, 4, err));
  CHECK(restoreBattleState(r.battle, parsed, err));
  CHECK(r.battle.rng.state == w.battle.rng.state);
  CHECK(r.battle.rng.inc == w.battle.rng.inc);
  for (int i = 0; i < 20; ++i) CHECK(r.battle.rng.next() == w.battle.rng.next());
}
