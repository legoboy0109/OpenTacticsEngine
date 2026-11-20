// Which teams saw each event (Event::seen; docs/ECONOMY.md "Fog of war" and docs/EVENTS.md).
//
// Without fog every event keeps the default (all bits) and nothing here runs. With fog an event is stamped with the
// sight of the moment it happened: markSight() stamps everything pushed so far just BEFORE a change that alters
// sight, and apply() / start() stamp the rest when they finish. "Seen by a unit" always includes the unit's own team.
//
// Per type (a = first field, ...):
//   TurnStarted, SideDefeated, BattleEnded, Dialogue, Choice, Focus, ObjectiveChanged, MusicChanged, SoundRequested, TerrainChanged: all teams (public).
//   UnitMoved: the mover's team, and every team that saw the mover on at least one square of its path
//              (`seenSteps` says on which indices); a team that saw it only on the way gets the whole event and
//              decides for itself what to draw from `seenSteps`.
//   MoveBlocked: both units' teams and the teams that see either unit.
//   Attack: both fighters' teams and the teams that see either fighter.
//   UnitDied, UnitRemoved: the unit's team and the teams that saw it on its square (b).
//   UnitBuilt, UnitAdded, UnitHealed, CaptureProgress: the unit's team and the teams that see the unit.
//   ItemUsed: the teams that see the user or the target.
//   UnitRelocated: the unit's team and the teams that see either square (b, c).
//   SideChanged: both sides' teams and the teams that see the unit.
//   StructureCaptured: the new owner's team, the previous owner's team, and the teams that see any of its squares.
//   StructureUpgraded: the owner's team and the teams that see any of its squares.
//   Bookkeeping of one team's own units (XpGained, LevelUp, StatsRaised, ItemBroke, ItemEquipped, ItemLost,
//   UnitRepaired, UnitResupplied): only the unit's own team.
//   Resources and items of a side (ResourceChanged, ItemGained, ItemMoved, ResearchStarted, ResearchFinished, ResearchCancelled): only
//   the side's own team.
//   Strategy-RPG features (docs/SRPG.md "Events"):
//   SkillLearned, RankUp, SupportPoints, SupportReached: only the units' own team.
//   SkillActivated, UnitHurt, UnitPromoted: the unit's team and the teams that see it.
//   AbilityUsed: the user's team and the teams that see the user or the target unit.
//   StaffUsed (docs/SRPG.md section 8): the same; StaffMissed: the teams that see either of the two units.
//   ItemRepaired, AreaLit (stage 2): the unit's team (the user's team for a light).
//   UnitRescued, UnitHanded: the units' teams and the teams that see either of the two (a, b) before it happens.
//   UnitDropped, PairSwitched: the unit's team and the teams that see the square (c).
#include "core/battle/battle.h"

namespace ote {

namespace {

uint32_t bit(int team) { return team >= 0 && team < kMaxFogTeams ? 1u << team : 0u; }

}  // namespace

// The unit's own team plus every team that sees it. Works for a dead unit too (its own team only).
uint32_t Battle::unitSeenBy(int unit) const {
  if (unit < 0 || unit >= (int)units.size()) return 0;
  const Unit& u = units[(size_t)unit];
  uint32_t mask = bit(teamOf(u));
  for (int t = 0; t < teamCount; ++t)
    if (unitVisibleTo(t, u)) mask |= bit(t);
  return mask;
}

// Teams that would see a unit of another team standing on the square.
uint32_t Battle::cellSeenBy(Cell cell) const {
  if (!grid.valid(cell)) return 0;
  ensureSight();
  uint32_t mask = 0;
  for (int t = 0; t < teamCount; ++t)
    if (sight_.unitSeen[(size_t)t][(size_t)cell]) mask |= bit(t);
  return mask;
}

uint32_t Battle::footprintSeenBy(int structure) const {
  uint32_t mask = 0;
  for (int t = 0; t < teamCount; ++t)
    if (structureVisibleTo(t, structure)) mask |= bit(t);
  return mask;
}

uint32_t Battle::eventSeenBy(const Event& e) const {
  ensureSight();
  auto sideTeam = [&](int side) { return side >= 0 && side < (int)sides.size() ? bit(sides[(size_t)side].team) : 0u; };
  auto unitTeam = [&](int unit) {
    return unit >= 0 && unit < (int)units.size() ? bit(teamOf(units[(size_t)unit])) : 0u;
  };
  switch (e.type) {
    case EventType::TurnStarted:
    case EventType::SideDefeated:
    case EventType::BattleEnded:
    case EventType::Dialogue:
    case EventType::Choice:
    case EventType::Focus:
    case EventType::ObjectiveChanged:
    case EventType::MusicChanged:
    case EventType::SoundRequested:
    case EventType::TerrainChanged:  // terrain is known to everybody anyway
    case EventType::SceneEffect:
      return kSeenByAll;
    case EventType::UnitMoved: {
      uint32_t mask = unitTeam(e.a);
      for (int t = 0; t < teamCount; ++t)
        for (int cell : e.data)
          if (grid.valid(cell) && sight_.unitSeen[(size_t)t][(size_t)cell]) { mask |= bit(t); break; }
      return mask;
    }
    case EventType::MoveBlocked:
    case EventType::Attack:
      return unitSeenBy(e.a) | unitSeenBy(e.b);
    case EventType::UnitDied:
    case EventType::UnitRemoved:
      return unitTeam(e.a) | cellSeenBy(e.b);
    case EventType::UnitRelocated:
      return unitTeam(e.a) | cellSeenBy(e.b) | cellSeenBy(e.c);
    case EventType::UnitBuilt:
    case EventType::UnitAdded:
    case EventType::UnitHealed:
    case EventType::CaptureProgress:
      return unitSeenBy(e.a);
    case EventType::ItemUsed:
      return unitSeenBy(e.a) | unitSeenBy(e.c);
    case EventType::SideChanged:
      return sideTeam(e.b) | sideTeam(e.c) | unitSeenBy(e.a);
    case EventType::StructureCaptured:
      return sideTeam(e.b) | sideTeam(e.c) | footprintSeenBy(e.a);
    case EventType::StructureUpgraded:
      return (e.a >= 0 && e.a < (int)structures.size() ? sideTeam(structures[(size_t)e.a].owner) : 0u) | footprintSeenBy(e.a);
    case EventType::XpGained:
    case EventType::LevelUp:
    case EventType::StatsRaised:
    case EventType::ItemBroke:
    case EventType::ItemEquipped:
    case EventType::ItemLost:
    case EventType::UnitRepaired:
    case EventType::UnitResupplied:
      return unitTeam(e.a);
    case EventType::ResourceChanged:
    case EventType::ResearchStarted:
    case EventType::ResearchFinished:
    case EventType::ResearchCancelled:
      return sideTeam(e.a);
    case EventType::ItemGained:
      return sideTeam(e.c);
    case EventType::ItemMoved:
      return sideTeam(e.d);
    case EventType::SkillLearned:
    case EventType::RankUp:
      return unitTeam(e.a);
    case EventType::SupportPoints:
    case EventType::SupportReached:
      return unitTeam(e.a) | unitTeam(e.b);
    case EventType::SkillActivated:
    case EventType::UnitHurt:
    case EventType::UnitPromoted:
      return unitSeenBy(e.a);
    case EventType::ItemStolen:    // like a swap: the teams that see either unit
    case EventType::UnitRefreshed:
      return unitSeenBy(e.a) | unitSeenBy(e.b);
    case EventType::AbilityUsed:
    case EventType::StaffUsed:
      return unitSeenBy(e.a) | unitSeenBy(e.c);
    case EventType::StaffMissed:
      return unitSeenBy(e.a) | unitSeenBy(e.b);
    case EventType::ConditionApplied:  // seen like a heal: by the teams that see the unit
    case EventType::ConditionEnded:
    case EventType::ConditionTick:
      return unitSeenBy(e.a);
    case EventType::ConditionResisted:
      return unitSeenBy(e.a) | unitSeenBy(e.c);
    case EventType::ItemRepaired:  // like an item break: the owner's team knows its items
    case EventType::AreaLit:       // the user's team (what the light shows is sight, not an event)
      return unitTeam(e.a);
    case EventType::UnitRescued:
    case EventType::UnitHanded:
      return unitSeenBy(e.a) | unitSeenBy(e.b);
    case EventType::UnitDropped:
      return unitTeam(e.b) | cellSeenBy(e.c);
    case EventType::PairSwitched:
      return unitTeam(e.a) | cellSeenBy(e.c);
    // Doors, chests and visiting (docs/SRPG.md section 10): the teams that see the structure, and the one that did it.
    case EventType::DoorOpened:
    case EventType::ChestOpened:
    case EventType::PlaceVisited:
    case EventType::PlaceDestroyed:
    case EventType::PlaceUnlocked:
      return footprintSeenBy(e.a) | unitTeam(e.b);
    case EventType::DoorClosed:
      return footprintSeenBy(e.a);
    case EventType::PlaceHit:
      return unitSeenBy(e.a) | footprintSeenBy(e.b);
  }
  return kSeenByAll;
}

void Battle::stampEvents() {
  if (!fog || !sightEvents_) return;
  std::vector<Event>& events = *sightEvents_;
  if (stampFrom_ >= events.size()) return;
  ensureSight();
  for (size_t i = stampFrom_; i < events.size(); ++i) {
    Event& e = events[i];
    e.seen = eventSeenBy(e);
    if (e.type == EventType::UnitMoved) {
      // For each team the first and last path index where it saw the mover (its own team saw every step).
      e.seenSteps.clear();
      const int mover = teamOf(units[(size_t)e.a]);
      for (int t = 0; t < teamCount; ++t) {
        int first = -1, last = -1;
        for (size_t k = 0; k < e.data.size(); ++k) {
          Cell c = e.data[k];
          if (t == mover || (grid.valid(c) && sight_.unitSeen[(size_t)t][(size_t)c])) {
            if (first < 0) first = (int)k;
            last = (int)k;
          }
        }
        e.seenSteps.push_back(first);
        e.seenSteps.push_back(last);
      }
    }
  }
  stampFrom_ = events.size();
}

Battle::EventScope::EventScope(Battle& battle, std::vector<Event>& events) : b(battle) {
  // Not nested (apply and start never call each other), but a scope that is already open keeps its own list.
  if (!b.sightEvents_) {
    b.sightEvents_ = &events;
    b.stampFrom_ = events.size();
    owns = true;
  }
}

Battle::EventScope::~EventScope() {
  if (!owns) return;
  b.stampEvents();
  b.sightEvents_ = nullptr;
}

}  // namespace ote
