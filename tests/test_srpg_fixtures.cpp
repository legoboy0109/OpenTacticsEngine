// Step D1 of docs/SRPG.md: the four frozen fixtures made last (srpg-carry, srpg-pair, srpg-supports, srpg-hex). Each loads,
// shows its one rule working, and (with the demo and the template) plays AI against AI for 200 rounds without a stall,
// the same text twice for the same seed.
#include "campaign_run.h"
#include "core/ai/ai.h"
#include "save_helpers.h"

using namespace ote;
using savetest::Rig;

namespace {

// The side to move plays one whole turn with the AI; false if the turn does not end.
bool aiTurn(Battle& b, std::vector<Event>& events) {
  const long limit = 20L * (long)b.units.size() + 100;
  long steps = 0;
  while (aiTakeStep(b, events))
    if (++steps > limit) return false;
  return true;
}

std::string fixture(const char* name) { return ote_test::sourceDir() + "/tests/fixtures/" + name; }

// One battle with every side played by the computer; the text of everything that happened, "" when it did not load.
std::string simulate(const std::string& dir, const std::string& map, uint32_t seed, bool& stalled) {
  Rig rig;
  stalled = false;
  if (!rig.load(dir, map, seed)) {
    stalled = true;
    return "";
  }
  std::vector<Event> ev;
  rig.battle.start(ev);
  while (!rig.battle.over && rig.battle.round <= 200)
    if (!aiTurn(rig.battle, ev)) {
      stalled = true;
      break;
    }
  if (!rig.battle.over) stalled = true;
  std::string out;
  for (const Event& e : ev)
    out += std::to_string((int)e.type) + ":" + std::to_string(e.a) + "," + std::to_string(e.b) + "," + std::to_string(e.c) + "," +
           std::to_string(e.d) + ";";
  return out + savetest::stateText(rig.battle);
}

struct Duel {
  Rig rig;
  std::vector<Event> ev;
  bool load(const char* game, const char* map) {
    if (!rig.load(fixture(game), map)) return false;
    rig.battle.start(ev);
    return true;
  }
  int id(const char* story) const { return rig.battle.unitByStoryId(story); }
  bool act(const Action& a) {
    ev.clear();
    return rig.battle.apply(a, ev);
  }
  int stat(const char* story, const char* s) const {
    return rig.battle.stat(rig.battle.units[(size_t)id(story)], rig.db.findStat(s));
  }
};

}  // namespace

TEST(fixtures_new_srpg_games_load_and_play_to_the_end_without_stalls) {
  struct Case { const char* game; const char* map; };
  const Case cases[] = {{"srpg-carry", "field"}, {"srpg-pair", "field"}, {"srpg-hex", "field"},
                        {"srpg-supports", "ch1"}, {"srpg-supports", "ch2"}};
  for (const Case& c : cases)
    for (uint32_t seed = 1; seed <= 5; ++seed) {
      bool s1, s2;
      const std::string a = simulate(fixture(c.game), c.map, seed, s1), b = simulate(fixture(c.game), c.map, seed, s2);
      CHECK(!s1 && !s2);
      CHECK(a == b);
    }
}

TEST(fixtures_carry_lifts_a_neighbour_but_never_a_mounted_one) {
  Duel d;
  CHECK(d.load("srpg-carry", "field"));
  const int eirik = d.id("eirik"), bram = d.id("bram"), kell = d.id("kell");
  CHECK(!d.act(Action::rescue(eirik, kell)));  // the rider is tagged mounted
  CHECK_EQ(d.stat("eirik", "spd"), 6);
  CHECK(d.act(Action::rescue(eirik, bram)));  // Strength 6 is at least Bram's Defense 4
  const Unit& u = d.rig.battle.units[(size_t)eirik];
  CHECK_EQ(u.carrying, bram);
  CHECK_EQ(d.stat("eirik", "spd"), 3);  // halved while carrying
  CHECK_EQ(d.stat("eirik", "mov"), 3);  // one less
  CHECK(d.rig.battle.exists(d.rig.battle.units[(size_t)bram]) && !d.rig.battle.units[(size_t)bram].alive);
}

TEST(fixtures_pair_the_units_own_bonus_replaces_the_games) {
  Duel d;
  CHECK(d.load("srpg-pair", "field"));
  const int eirik = d.id("eirik"), bram = d.id("bram");
  CHECK(d.act(Action::pairUp(eirik, bram)));  // Eirik joins Bram: Hero gives the game's bonus (Strength +2, Defense +2)
  CHECK_EQ(d.stat("bram", "str"), 8 + 2);
  CHECK_EQ(d.stat("bram", "def"), 4 + 2);
  Duel e;
  CHECK(e.load("srpg-pair", "field"));
  CHECK(e.act(Action::pairUp(e.id("bram"), e.id("eirik"))));  // Bram joins Eirik: its own bonus (Strength +3, Speed +1)
  CHECK_EQ(e.stat("eirik", "str"), 6 + 3);
  CHECK_EQ(e.stat("eirik", "spd"), 6 + 1);
  CHECK_EQ(e.stat("eirik", "def"), 3);
}

TEST(fixtures_supports_two_pairs_and_a_two_chapter_campaign) {
  Database db;
  Formulas formulas;
  std::string err;
  CHECK(loadGame(fixture("srpg-supports"), db, err) && formulas.init(db, err));
  CHECK_EQ(db.supports.pairs.size(), size_t(2));
  CHECK(db.campaign && db.campaign->chapters.size() == 2);
  Playthrough pt;
  CHECK(pt.begin(db));
  std::vector<SceneLine> lines;
  runScene(db, pt, SceneKind::Before, lines);
  Battle b;
  std::vector<Event> ev;
  CHECK(startChapterBattle(db, pt, formulas, 1, b, ev, err));
  CHECK_EQ(b.supports.points.size(), size_t(2));
  CHECK_EQ((int)b.supports.level[0], 0);
  b.over = true;
  b.winnerTeam = b.humanTeam();
  CHECK(finishChapterBattle(db, pt, b) == ChapterResult::Won);
  CHECK_EQ(pt.supports.points.size(), size_t(2));
}

TEST(fixtures_hex_abilities_use_the_six_directions) {
  Duel d;
  CHECK(d.load("srpg-hex", "field"));
  const int eirik = d.id("eirik"), gorm = d.id("gorm"), bram = d.id("bram");
  const Cell east2 = d.rig.battle.units[(size_t)gorm].cell + 2;  // same row, two squares further east
  CHECK(d.act(Action::useAbility(eirik, d.rig.db.findAbility("shove"), gorm)));
  CHECK_EQ(d.rig.battle.units[(size_t)gorm].cell, east2);  // pushed straight away from Eirik, two squares
  Duel e;
  CHECK(e.load("srpg-hex", "field"));
  const Cell eirikCell = e.rig.battle.units[(size_t)e.id("eirik")].cell, bramCell = e.rig.battle.units[(size_t)bram].cell;
  CHECK(e.act(Action::useAbility(e.id("eirik"), e.rig.db.findAbility("swap"), e.id("bram"))));
  CHECK_EQ(e.rig.battle.units[(size_t)e.id("eirik")].cell, bramCell);
  CHECK_EQ(e.rig.battle.units[(size_t)e.id("bram")].cell, eirikCell);
}
