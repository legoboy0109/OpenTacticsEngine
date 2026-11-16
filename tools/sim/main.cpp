// ote-sim: plays whole battles with the AI on every side (testing and
// balance tool). Usage:
//   ote-sim <gameDir> <mapId> [--seed N] [--runs N] [--max-rounds N] [--verbose] [--force-fog]
//       --force-fog plays the map with fog of war on even if the map file has none (single maps only)
//   ote-sim <gameDir> --campaign [--seed N] [--runs N] [--max-rounds N] [--verbose]
//       plays the game's campaign from chapter 1 (the human side played by the AI)
// Exit codes: 0 all runs had a winner, 1 load/usage error, 2 some run
// stalled (no winner within the round limit), 3 the AI misbehaved.
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "core/ai/ai.h"
#include "core/data/loader.h"
#include "core/script/formulas.h"
#include "campaign_sim.h"
#include "event_print.h"

using namespace ote;

namespace {

struct Options {
  std::string gameDir, mapId;
  unsigned seed = 1;
  int runs = 1;
  int maxRounds = 60;
  bool verbose = false;
  bool campaign = false;
  bool forceFog = false;  // switch fog of war on for the map, whatever the file says (to test the AI under fog)
};

// strtol-based, no exceptions. Returns false on junk.
bool parseInt(const char* text, long& out) {
  char* end = nullptr;
  out = std::strtol(text, &end, 10);
  return end && *end == '\0' && end != text;
}

bool parseArgs(int argc, char** argv, Options& o) {
  int positional = 0;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    long v = 0;
    if (a == "--verbose") o.verbose = true;
    else if (a == "--campaign") o.campaign = true;
    else if (a == "--force-fog") o.forceFog = true;
    else if ((a == "--seed" || a == "--runs" || a == "--max-rounds") && i + 1 < argc && parseInt(argv[i + 1], v)) {
      ++i;
      if (a == "--seed") o.seed = (unsigned)v;
      else if (a == "--runs") o.runs = (int)v;
      else o.maxRounds = (int)v;
    } else if (a.size() > 1 && a[0] == '-' && a[1] == '-') return false;
    else if (positional == 0) { o.gameDir = a; ++positional; }
    else if (positional == 1) { o.mapId = a; ++positional; }
    else return false;
  }
  return positional == (o.campaign ? 1 : 2) && o.runs > 0 && o.maxRounds > 0;
}

struct RunResult {
  bool won = false;
  bool aiError = false;
  int rounds = 0;
  int captures = 0, builds = 0;  // structures captured / units built during the run
  std::string winners;
  std::vector<int> survivors;  // per side
};

RunResult playOne(const Database& db, const MapDef& map, Formulas& formulas, unsigned seed,
                  int maxRounds, bool verbose) {
  RunResult r;
  Battle battle;
  std::string err;
  std::vector<Event> events;
  if (!battle.init(db, map, formulas, seed, err)) {
    std::printf("init failed: %s\n", err.c_str());
    r.aiError = true;
    return r;
  }
  battle.start(events);
  size_t printed = 0;
  auto flush = [&]() {
    if (verbose)
      for (; printed < events.size(); ++printed)
        std::printf("  %s\n", simtool::describeEvent(battle, events[printed]).c_str());
    printed = events.size();
  };
  flush();

  while (!battle.over && battle.round <= maxRounds) {
    // A sane turn needs at most a few steps per unit plus builds.
    long limit = 10L * (long)battle.units.size() + 100;
    long steps = 0;
    while (aiTakeStep(battle, events)) {
      flush();
      if (++steps > limit) {
        std::printf("ERROR: aiTakeStep returned true %ld times in one turn (round %d, side %s)\n",
                    steps, battle.round, battle.sides[battle.activeSide].name.c_str());
        r.aiError = true;
        return r;
      }
    }
    flush();
    for (const Event& e : events) {
      if (e.type == EventType::StructureCaptured) ++r.captures;
      if (e.type == EventType::UnitBuilt) ++r.builds;
    }
    events.clear();  // already printed; keeps memory flat on long runs
    printed = 0;
  }

  r.rounds = battle.round;
  r.won = battle.over && battle.winnerTeam >= 0;
  for (size_t s = 0; s < battle.sides.size(); ++s) {
    int alive = 0;
    for (const Unit& u : battle.units)
      if (battle.exists(u) && u.side == (int)s) ++alive;
    r.survivors.push_back(alive);
    if (r.won && battle.sides[s].team == battle.winnerTeam && !battle.sides[s].defeated) {
      if (!r.winners.empty()) r.winners += "+";
      r.winners += battle.sides[s].name;
    }
  }
  return r;
}

}  // namespace

int main(int argc, char** argv) {
  Options opt;
  if (!parseArgs(argc, argv, opt)) {
    std::fprintf(stderr, "usage: ote-sim <gameDir> <mapId> [--seed N] [--runs N] [--max-rounds N] [--verbose] [--force-fog]\n"
                         "       ote-sim <gameDir> --campaign [--seed N] [--runs N] [--max-rounds N] [--verbose]\n");
    return 1;
  }
  Database db;
  MapDef map;
  Formulas formulas;
  std::string err;
  if (opt.campaign) {
    if (!loadGame(opt.gameDir, db, err) || !formulas.init(db, err)) {
      std::fprintf(stderr, "load error: %s\n", err.c_str());
      return 1;
    }
    simtool::CampaignSimOptions c;
    c.seed = opt.seed;
    c.runs = opt.runs;
    c.maxRounds = opt.maxRounds;
    c.verbose = opt.verbose;
    return simtool::runCampaignSim(db, formulas, c);
  }
  if (!loadGame(opt.gameDir, db, err) || !loadMap(db, opt.mapId, map, err) || !formulas.init(db, err)) {
    std::fprintf(stderr, "load error: %s\n", err.c_str());
    return 1;
  }

  if (opt.forceFog) map.fog = true;
  std::vector<int> wins(map.sides.size(), 0);
  std::vector<std::string> names;
  int stalls = 0;
  double totalRounds = 0, totalMs = 0, totalCaptures = 0, totalBuilds = 0;
  for (int i = 0; i < opt.runs; ++i) {
    unsigned seed = opt.seed + (unsigned)i;
    if (opt.verbose) std::printf("=== run seed %u ===\n", seed);
    auto t0 = std::chrono::steady_clock::now();
    RunResult r = playOne(db, map, formulas, seed, opt.maxRounds, opt.verbose);
    double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (r.aiError) return 3;
    totalMs += ms;
    totalRounds += r.rounds;
    totalCaptures += r.captures;
    totalBuilds += r.builds;
    std::string surv;
    for (size_t s = 0; s < r.survivors.size(); ++s)
      surv += (s ? ", " : "") + map.sides[s].name + "=" + std::to_string(r.survivors[s]);
    std::printf("seed %u: winner %s, rounds %d, survivors %s, captures %d, builds %d\n", seed,
                r.won ? r.winners.c_str() : "stalled", r.rounds, surv.c_str(), r.captures, r.builds);
    if (!r.won) ++stalls;
    else
      for (size_t s = 0; s < map.sides.size(); ++s)
        if (r.winners.find(map.sides[s].name) != std::string::npos) ++wins[s];
  }

  if (opt.runs > 1) {
    std::printf("summary: %d runs;", opt.runs);
    for (size_t s = 0; s < map.sides.size(); ++s)
      std::printf(" %s wins %d;", map.sides[s].name.c_str(), wins[s]);
    std::printf(" stalls %d; avg rounds %.1f; avg captures %.1f; avg builds %.1f; avg %.1f ms/run\n", stalls,
                totalRounds / opt.runs, totalCaptures / opt.runs, totalBuilds / opt.runs, totalMs / opt.runs);
  }
  return stalls > 0 ? 2 : 0;
}
