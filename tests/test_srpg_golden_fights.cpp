// Golden fights (docs/SRPG.md step F2): the combat pipeline was rebuilt around modifiers, and a game without any
// source of modifiers must still forecast and fight bit for bit as before. While the computer plays a battle, every
// attack any unit of the side to move could make is forecast (also from the squares around it) and then really
// fought on a copy of the battle; the forecast numbers, the fight's whole event stream, the random state after it
// and the two fighters' HP go into one digest per battle.
//
// The pins below were taken from the build BEFORE the pipeline changed (main at bebc0f3) and must never move by
// accident. A line that differs is printed in the table's own form.
#include "save_helpers.h"

using namespace ote;

namespace {

struct Digest {
  uint64_t h = 1469598103934665603ULL;
  void add(const std::string& s) {
    for (char c : s) {
      h ^= (uint8_t)c;
      h *= 1099511628211ULL;
    }
  }
};

std::string hex(uint64_t v) {
  char buf[17];
  std::snprintf(buf, sizeof buf, "%016llx", (unsigned long long)v);
  return buf;
}

std::string strikeText(const StrikeForecast& s) {
  return std::to_string(s.can) + "," + std::to_string(s.weapon) + "," + std::to_string(s.damage) + "," +
         std::to_string(s.hit) + "," + std::to_string(s.crit) + "," + std::to_string(s.strikes);
}

std::string eventsText(const std::vector<Event>& events) {
  std::string s;
  for (const Event& e : events) {
    s += std::to_string((int)e.type) + ":" + std::to_string(e.a) + "," + std::to_string(e.b) + "," +
         std::to_string(e.c) + "," + std::to_string(e.d) + "," + std::to_string(e.flags) + "," +
         std::to_string(e.seen) + "[";
    for (int d : e.data) s += std::to_string(d) + " ";
    s += "]\n";
  }
  return s;
}

struct Counts {
  int forecasts = 0, fights = 0;
  bool plain = true;  // no forecast showed anything only modifiers can cause
};

// Everything the side to move could attack right now.
void probe(const Battle& b, Digest& dg, Counts& n) {
  std::vector<AttackOption> opts;
  std::vector<Cell> around;
  for (const Unit& u : b.units) {
    if (!u.alive || u.side != b.activeSide) continue;
    b.grid.within(u.cell, 2, around);
    for (Cell from : around) {
      b.attackOptions(u.id, from, opts);
      for (const AttackOption& o : opts) {
        Forecast f;
        bool ok = b.forecast(u.id, o.target, o.weapon, from, f);
        dg.add("F" + std::to_string(u.id) + ">" + std::to_string(o.target) + " w" + std::to_string(o.weapon) + " @" +
               std::to_string(from) + " " + std::to_string(ok) + " " + strikeText(f.attack) + " | " +
               strikeText(f.counter) + "\n");
        ++n.forecasts;
        if (f.attack.multi != 1 || f.counter.multi != 1 || f.defenderFirst || !f.notes.empty()) n.plain = false;
        if (from != u.cell) continue;
        // The fight itself, on a copy: the real battle goes on undisturbed.
        Battle copy = b;
        std::vector<Event> ev;
        if (!copy.apply(Action::attack(u.id, o.target, o.weapon), ev)) continue;
        dg.add(eventsText(ev) + hex(copy.rng.state) + " " + std::to_string(copy.units[(size_t)u.id].hp) + " " +
               std::to_string(copy.units[(size_t)o.target].hp) + "\n");
        ++n.fights;
      }
    }
  }
}

// "digest forecasts fights" of one battle played by the computer for up to `steps` decisions.
std::string fought(const std::string& dir, const char* map, unsigned seed, int steps) {
  savetest::Rig rig;
  if (!rig.load(ote_test::sourceDir() + "/" + dir, map, seed)) return "load failed";
  std::vector<Event> ev;
  rig.battle.start(ev);
  Digest dg;
  Counts n;
  for (int i = 0; i < steps && !rig.battle.over; ++i) {
    probe(rig.battle, dg, n);
    aiTakeStep(rig.battle, ev);
  }
  dg.add(hex(rig.battle.rng.state));
  CHECK(n.plain);
  return hex(dg.h) + " " + std::to_string(n.forecasts) + " " + std::to_string(n.fights);
}

struct Golden {
  const char* dir;
  const char* map;
  unsigned seed;
  int steps;
  const char* want;
};

const Golden kGoldens[] = {
    {"games/demo-srpg", "bridge", 7, 400, "5563fd35e1b7ef17 938 85"},
    {"games/demo-srpg", "bridge", 11, 400, "4d1a23b460c3b490 393 14"},
    {"games/demo-srpg", "bridge", 23, 400, "308e32698d705b7c 940 58"},
    {"games/demo-srpg", "north_road", 7, 400, "60daa8b2b8fc9b4c 647 14"},
    {"games/demo-srpg", "north_road", 11, 400, "cc43d516d94a94bc 494 11"},
    {"games/demo-srpg", "north_road", 23, 400, "ce7eaf42efa80ccd 618 17"},
    {"games/demo-economy", "twin_rivers", 7, 300, "ed33a86d3439d235 1510 54"},
    {"games/demo-economy", "twin_rivers", 11, 300, "60794d2e778e55d9 1510 54"},
    {"games/demo-economy", "misty_ford", 7, 300, "2157d1d063775c95 2834 160"},
    {"games/demo-economy", "misty_ford", 23, 300, "0241debc712f6633 2834 160"},
    {"games/demo-hex", "fortress_ford", 7, 300, "c57820da0ef9b60a 10569 254"},
    {"games/demo-hex", "fortress_ford", 11, 300, "8e60e81bf7b70282 10569 254"},
    {"games/demo-hex", "foggy_hills", 7, 300, "443f83d035a7f57c 10773 308"},
    {"games/demo-hex", "foggy_hills", 23, 300, "c0c5e6478c0b4af6 10773 308"},
    {"tests/fixtures/plain-economy", "twin_rivers", 7, 300, "cffa1d22cc94f6a6 2949 169"},
    {"tests/fixtures/plain-economy", "twin_rivers", 11, 300, "3d0dcdeaa6cfe07a 2949 169"},
    {"tests/fixtures/plain-economy", "twin_rivers", 23, 300, "e6343f43a615021a 2949 169"},
    {"tests/fixtures/hex-pointy", "skirmish", 7, 300, "bc29314ab8c684a0 3046 166"},
    {"tests/fixtures/hex-pointy", "skirmish", 11, 300, "21d7f4b91ddf0dc8 3046 166"},
    {"tests/fixtures/hex-flat", "skirmish", 7, 300, "3e43d0a7287051f3 1031 57"},
    {"tests/fixtures/hex-flat", "skirmish", 23, 300, "a40feb19d66a1b17 1031 57"},
};

}  // namespace

TEST(golden_fights_forecast_and_fight_as_before_the_pipeline) {
  for (const Golden& g : kGoldens) {
    std::string got = fought(g.dir, g.map, g.seed, g.steps);
    if (got != g.want) std::printf("    {\"%s\", \"%s\", %u, %d, \"%s\"},\n", g.dir, g.map, g.seed, g.steps, got.c_str());
    CHECK_STR(got, g.want);
  }
}
