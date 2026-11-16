// The strategy-RPG skeleton (docs/SRPG.md, step F1) is inert for games that use none of it: their battles make
// the same events, use the same random numbers and write the same save bytes as the build before it. The goldens
// below were taken from that build (main at d3c9515, before any F1 change) and must never move by accident.
#include "save_helpers.h"

#include "core/campaign/campaign_save.h"
#include "core/campaign/playthrough.h"

using namespace ote;

namespace {

uint64_t fnv(const std::string& s) {
  uint64_t h = 1469598103934665603ULL;
  for (char c : s) {
    h ^= (uint8_t)c;
    h *= 1099511628211ULL;
  }
  return h;
}

std::string hex(uint64_t v) {
  char buf[17];
  std::snprintf(buf, sizeof buf, "%016llx", (unsigned long long)v);
  return buf;
}

std::string eventsText(const std::vector<Event>& events) {
  std::string s;
  for (const Event& e : events) {
    s += std::to_string((int)e.type) + ":" + std::to_string(e.a) + "," + std::to_string(e.b) + "," +
         std::to_string(e.c) + "," + std::to_string(e.d) + "," + std::to_string(e.flags) + "," + std::to_string(e.seen) + "[";
    for (int d : e.data) s += std::to_string(d) + " ";
    s += "][";
    for (int d : e.seenSteps) s += std::to_string(d) + " ";
    s += "]\n";
  }
  return s;
}

// One battle played `steps` AI decisions: "save hash, event hash, rng state".
std::string played(const std::string& dir, const char* map, unsigned seed, int steps) {
  savetest::Rig rig;
  if (!rig.load(ote_test::sourceDir() + "/" + dir, map, seed)) return "load failed";
  std::vector<Event> ev;
  rig.battle.start(ev);
  savetest::playAi(rig.battle, steps, ev);
  return hex(fnv(savetest::stateText(rig.battle))) + " " + hex(fnv(eventsText(ev))) + " " + hex(rig.battle.rng.state);
}

std::string dataHash(const std::string& dir, const char* map) {
  Database db;
  std::string err, out;
  if (!loadGame(ote_test::sourceDir() + "/" + dir, db, err) || !computeDataHash(db, map, out, err)) return err;
  return out;
}

struct Golden {
  const char* dir;
  const char* map;
  unsigned seed;
  int steps;
  const char* want;
};

const Golden kGoldens[] = {
    {"tests/fixtures/plain-economy", "twin_rivers", 7, 60, "7bd8c1c0aa74081e 54e6cde857e7133a 3cbce4c25ee130dc"},
    {"tests/fixtures/plain-economy", "twin_rivers", 11, 200, "ae9930f575d28df2 c6c9ec072b46fec9 2323f84c5a8f2068"},
    {"tests/fixtures/plain-economy", "twin_rivers", 23, 2000, "de4a61837a815c28 dd223047f4e5d54d c1dc2797283923cc"},
    {"games/demo-srpg", "bridge", 7, 12, "f692674fccbef1d8 ec06ebf070eb8c1c fb3c194038081104"},
    {"games/demo-srpg", "bridge", 11, 40, "a3d6e1a8cb3a8e0c 2f9c80b4748f6273 3be74f6c0f73f357"},
    {"games/demo-srpg", "bridge", 23, 2000, "16966dfb9b92d8b0 8799248a3bddf077 60dd66bcad3c8250"},
    {"games/demo-srpg", "north_road", 7, 25, "48c8b8a666f818d4 6a3f11bc09065a37 8066885ce777d0b1"},
    {"games/demo-srpg", "north_road", 11, 2000, "affa6a218b3e7755 da87c644d32fccc5 de65284903666a6d"},
    {"games/demo-economy", "twin_rivers", 7, 150, "0a96109ba014b641 5e26a207d7e93500 3cbce4c25ee130dc"},
    {"games/demo-economy", "misty_ford", 11, 150, "90bee29c0d453f9a 308853d97fefb07d 2323f84c5a8f2068"},
    {"games/demo-hex", "fortress_ford", 7, 150, "0ae9c4eef41c3b84 9c098ccf301b69d2 3cbce4c25ee130dc"},
};

}  // namespace

TEST(srpg_skeleton_keeps_battles_and_saves_byte_for_byte) {
  for (const Golden& g : kGoldens) {
    std::string got = played(g.dir, g.map, g.seed, g.steps);
    if (got != g.want) std::printf("  {\"%s\", \"%s\", %u, %d, \"%s\"},\n", g.dir, g.map, g.seed, g.steps, got.c_str());
    CHECK_STR(got, g.want);
  }
}

TEST(srpg_skeleton_keeps_data_hashes) {
  CHECK_STR(dataHash("tests/fixtures/plain-economy", "twin_rivers"), "1e44dae676a88a38");
  CHECK_STR(dataHash("games/demo-srpg", "bridge"), "424b5fd70a0bf539");
  CHECK_STR(dataHash("games/demo-economy", "twin_rivers"), "89beafac5e6893cf");
  CHECK_STR(dataHash("games/demo-hex", "fortress_ford"), "b44fd3b7730564a8");
  Database db;
  std::string err, out;
  CHECK(loadGame(ote_test::sourceDir() + "/games/demo-srpg", db, err));
  CHECK(computeCampaignHash(db, out, err));
  CHECK_STR(out, "e4f411e0ca888510");
}

// The demo's campaign: a chapter battle under way saved as a whole campaign save file, and the playthrough after
// the chapter is won (the roster and what the sides carry).
TEST(srpg_skeleton_keeps_campaign_saves_byte_for_byte) {
  Database db;
  Formulas formulas;
  std::string err;
  CHECK(loadGame(ote_test::sourceDir() + "/games/demo-srpg", db, err) && formulas.init(db, err));
  Playthrough pt;
  CHECK(pt.begin(db));
  std::vector<SceneLine> lines;
  runScene(db, pt, SceneKind::Before, lines);
  Battle b;
  std::vector<Event> ev;
  CHECK(startChapterBattle(db, pt, formulas, 5, b, ev, err));
  savetest::playAi(b, 12, ev);
  CHECK(!b.over);
  Json file;
  CHECK(buildCampaignSave(db, "demo-srpg", pt, &b, 0, file, err));
  CHECK_STR(hex(fnv(file.dump(0))), "4899cc10dbbb4724");
  b.over = true;
  b.winnerTeam = b.humanTeam();
  CHECK(finishChapterBattle(db, pt, b) == ChapterResult::Won);
  Json after;
  savePlaythrough(db, pt, after);
  CHECK_STR(hex(fnv(after.dump(0))), "71019abb4e87edfc");
}
