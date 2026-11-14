#include "campaign_sim.h"

#include <cstdio>
#include <string>
#include <vector>

#include "core/ai/ai.h"
#include "core/campaign/playthrough.h"
#include "core/script/formulas.h"
#include "event_print.h"

using namespace ote;

namespace simtool {

namespace {

constexpr int kMaxAttempts = 3;  // a lost chapter ("retry") is tried this often, with other seeds

void printScene(const std::vector<SceneLine>& lines, const char* indent) {
  for (const SceneLine& l : lines) std::printf("%s%s%s%s\n", indent, l.speaker.c_str(), l.speaker.empty() ? "" : ": ", l.text.c_str());
}

// Plays the started battle to its end; false when the AI misbehaved. `decided` = false: no winner in time.
bool playBattle(Battle& battle, std::vector<Event>& events, int maxRounds, bool verbose, bool& decided) {
  size_t printed = 0;
  auto flush = [&]() {
    if (verbose)
      for (; printed < events.size(); ++printed)
        std::printf("    %s\n", describeEvent(battle, events[printed]).c_str());
    printed = events.size();
  };
  flush();
  while (!battle.over && battle.round <= maxRounds) {
    long limit = 10L * (long)battle.units.size() + 100, steps = 0;
    while (aiTakeStep(battle, events)) {
      flush();
      if (++steps > limit) {
        std::printf("ERROR: aiTakeStep returned true %ld times in one turn\n", steps);
        return false;
      }
    }
    flush();
    if (verbose) {
      events.clear();
      printed = 0;
    }
  }
  decided = battle.over;
  return true;
}

std::string partyText(const Playthrough& pt) {
  std::string s;
  for (const RosterUnit& r : pt.roster)
    if (!r.gone) s += (s.empty() ? "" : ", ") + r.name + " L" + std::to_string(r.level);
  return s.empty() ? "nobody" : s;
}

struct RunOutcome {
  bool finished = false;
  bool aiError = false;
  std::string why;  // when not finished
};

RunOutcome playRun(const Database& db, Formulas& formulas, const CampaignSimOptions& opt, unsigned seed) {
  RunOutcome out;
  Playthrough pt;
  pt.begin(db);
  std::vector<SceneLine> lines;
  int attempt = 0, lastChapter = -1;
  while (pt.active()) {
    const ChapterDef& ch = *currentChapter(db, pt);
    if (pt.phase == Phase::Before) {
      std::printf("-- chapter %d: %s\n", pt.chapter + 1, ch.name.c_str());
      lines.clear();
      runScene(db, pt, SceneKind::Before, lines);
      printScene(lines, "  ");
    } else if (pt.phase == Phase::After) {
      lines.clear();
      runScene(db, pt, SceneKind::After, lines);
      printScene(lines, "  ");
    } else {  // Battle
      attempt = pt.chapter == lastChapter ? attempt + 1 : 0;
      lastChapter = pt.chapter;
      Battle battle;
      std::vector<Event> events;
      std::string err;
      if (!startChapterBattle(db, pt, formulas, seed * 131u + (unsigned)attempt, battle, events, err)) {
        std::printf("ERROR: %s\n", err.c_str());
        out.aiError = true;
        return out;
      }
      bool decided = false;
      if (!playBattle(battle, events, opt.maxRounds, opt.verbose, decided)) {
        out.aiError = true;
        return out;
      }
      if (!decided) {
        std::printf("  battle on %s: stalled (no winner within %d rounds)\n", db.maps[(size_t)ch.map].id.c_str(), opt.maxRounds);
        out.why = "chapter " + std::to_string(pt.chapter + 1) + " stalled";
        return out;
      }
      ChapterResult res = finishChapterBattle(db, pt, battle);
      std::printf("  battle on %s: %s in round %d", db.maps[(size_t)ch.map].id.c_str(),
                  res == ChapterResult::Won ? "won" : "LOST", battle.round);
      if (res == ChapterResult::Won) std::printf("; party: %s\n", partyText(pt).c_str());
      else std::printf("\n");
      if (res == ChapterResult::Ended) {
        out.why = "campaign over: lost chapter " + std::to_string(pt.chapter + 1);
        return out;
      }
      if (res == ChapterResult::Retry && attempt + 1 >= kMaxAttempts) {
        out.why = "chapter " + std::to_string(pt.chapter + 1) + " lost " + std::to_string(kMaxAttempts) + " times";
        return out;
      }
    }
  }
  out.finished = pt.phase == Phase::Finished;
  return out;
}

}  // namespace

int runCampaignSim(const Database& db, Formulas& formulas, const CampaignSimOptions& opt) {
  if (!db.campaign) {
    std::fprintf(stderr, "this game has no campaign (data/campaign.json)\n");
    return 1;
  }
  int finished = 0;
  for (int i = 0; i < opt.runs; ++i) {
    unsigned seed = opt.seed + (unsigned)i;
    std::printf("=== campaign \"%s\", seed %u ===\n", db.campaign->name.c_str(), seed);
    RunOutcome r = playRun(db, formulas, opt, seed);
    if (r.aiError) return 3;
    std::printf("%s\n", r.finished ? "campaign finished" : ("campaign ended early: " + r.why).c_str());
    if (r.finished) ++finished;
  }
  if (opt.runs > 1) std::printf("summary: %d runs; finished %d; ended early %d\n", opt.runs, finished, opt.runs - finished);
  return finished == opt.runs ? 0 : 2;
}

}  // namespace simtool
