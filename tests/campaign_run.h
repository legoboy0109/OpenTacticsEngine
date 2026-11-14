// A playthrough with its game and (when one is under way) its battle, for the campaign tests.
#pragma once
#include "campaign_fixture.h"

namespace camptest {

struct Run {
  Game g;
  Playthrough pt;
  Battle b;
  std::vector<Event> ev;
  std::vector<SceneLine> lines;

  bool begin(const char* folder, const std::string& campaign = kCampaignJson) {
    return g.load(folder, campaign) && pt.begin(g.db);
  }
  bool scene(SceneKind k) {
    lines.clear();
    return runScene(g.db, pt, k, lines);
  }
  // Runs the before scene (if still due) and starts the chapter's battle.
  bool startBattle(uint32_t seed = 1) {
    if (pt.phase == Phase::Before && !scene(SceneKind::Before)) return false;
    std::string err;
    ev.clear();
    if (!startChapterBattle(g.db, pt, g.formulas, seed, b, ev, err)) {
      std::printf("  startChapterBattle: %s\n", err.c_str());
      return false;
    }
    return true;
  }
  int unit(const char* id) const { return b.unitByStoryId(id); }
  // Decides the battle for the human team (or the other one) without playing it.
  void decide(bool humanWins) {
    b.over = true;
    b.winnerTeam = humanWins ? b.humanTeam() : 1 - b.humanTeam();
  }
  // Takes a unit off the board as a defeat does.
  void kill(const char* id) {
    Unit& u = b.units[(size_t)unit(id)];
    b.occupant[(size_t)u.cell] = -1;
    u.alive = false;
    u.hp = 0;
    u.cell = kNoCell;
  }
  // Win the current chapter as it stands and run its after scene. Returns the result.
  ChapterResult winAndFinish() {
    decide(true);
    return finishChapterBattle(g.db, pt, b);
  }
  std::string state() const {
    Json j;
    savePlaythrough(g.db, pt, j);
    return j.dump(0);
  }
  std::string said() const {
    std::string s;
    for (const SceneLine& l : lines) s += l.speaker + "|" + l.text + ";";
    return s;
  }
};

}  // namespace camptest
