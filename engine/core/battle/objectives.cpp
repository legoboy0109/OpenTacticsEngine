// Objectives and the end of a battle (docs/EVENTS.md "Objectives").
//
// Objectives are judged for the human team (humanTeam()). They add ways to win or lose on top of
// the game's defeat rules (checkDefeat in turn.cpp), which keep applying to every side; a map
// without objectives therefore behaves exactly as before.
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

int Battle::winnerAgainst(int team) const {
  std::vector<int> others;  // teams other than `team` that still have an undefeated side
  for (const SideState& s : sides)
    if (!s.defeated && s.team != team && std::find(others.begin(), others.end(), s.team) == others.end())
      others.push_back(s.team);
  return others.size() == 1 ? others[0] : -1;
}

void Battle::endBattle(int winner, std::vector<Event>& events) {
  if (over) return;
  over = true;
  winnerTeam = winner;
  Event e;
  e.type = EventType::BattleEnded;
  e.a = winnerTeam;
  events.push_back(e);
  // Epilogue events: for the human team's result first, then the ones that fire either way.
  StoryOccurrence o;
  o.kind = winnerTeam == humanTeam() ? Trigger::Kind::BattleWon : Trigger::Kind::BattleLost;
  fireStory(o, events);
  o.kind = Trigger::Kind::BattleEnd;
  fireStory(o, events);
}

bool Battle::objectiveHolds(const ObjectiveRule& r, bool win) const {
  using K = ObjectiveRule::Kind;
  switch (r.kind) {
    case K::Round:
    case K::Survive:
      return round >= r.rounds;
    case K::Defeated: {
      // Everything the filter names is gone; something has to exist (or have existed) for that.
      bool any = false;
      for (const Unit& u : units) {
        if (u.hidden || !unitMatches(r.filter, u) || summonIgnored(r.filter, u)) continue;  // A1: summons are not counted by side / team / any; a hidden unit does not count (docs/EVENTS.md)
        if (exists(u)) return false;  // R1: a carried unit is not defeated
        any = true;
      }
      return any;
    }
    case K::Reach: {
      int human = humanTeam();
      for (const Unit& u : units) {
        if (!u.alive || !grid.valid(u.cell)) continue;
        // Without "by": winning is for the human team to do, losing for everybody else.
        bool who = r.hasBy ? unitMatches(r.filter, u) : (win ? teamOf(u) == human : teamOf(u) != human);
        int x = grid.x(u.cell), y = grid.y(u.cell);
        if (who && x >= r.rect.x && y >= r.rect.y && x < r.rect.x + r.rect.w && y < r.rect.y + r.rect.h) return true;
      }
      return false;
    }
  }
  return false;
}

void Battle::checkObjectives(std::vector<Event>& events) {
  if (over || !story) return;
  const ObjectiveSet& set = story->objectives[(size_t)storyState.objectives];
  // Losing is checked first: if both hold at the same moment the human team has lost.
  for (const ObjectiveRule& r : set.lose)
    if (objectiveHolds(r, false)) {
      endBattle(winnerAgainst(humanTeam()), events);
      return;
    }
  for (const ObjectiveRule& r : set.win)
    if (objectiveHolds(r, true)) {
      endBattle(humanTeam(), events);
      return;
    }
}

void Battle::settle(std::vector<Event>& events) {
  flushLostCarried(events);  // R1: story events of carried units that were lost with their carrier
  checkDefeat(events);
  checkObjectives(events);
}

}  // namespace ote
