// Story events in a battle (docs/EVENTS.md): which events wait for what happened, the queue that
// runs them, conditions, and the Talk action. What the commands do is in story_commands.cpp,
// objectives in objectives.cpp.
//
// Events never wait for input: they run to the end inside the action that triggered them and
// leave entries in the event stream (dialogue lines, focus, units added...). All of it is
// deterministic: events are taken in file order, queued ones in the order they fired.
#include <algorithm>

#include "core/base/log.h"
#include "core/battle/battle.h"

namespace ote {

namespace {

bool compare(CmpOp op, int a, int b) {
  switch (op) {
    case CmpOp::Eq: return a == b;
    case CmpOp::Ne: return a != b;
    case CmpOp::Lt: return a < b;
    case CmpOp::Le: return a <= b;
    case CmpOp::Gt: return a > b;
    case CmpOp::Ge: return a >= b;
  }
  return false;
}

bool inRect(const Grid& grid, const Rect& r, Cell c) {
  if (!grid.valid(c)) return false;
  int x = grid.x(c), y = grid.y(c);
  return x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h;
}

const std::string kNoText;

}  // namespace

const std::string& Battle::storyText(int index) const {
  // Quote lines (data/characters.json "quotes") have their own table too, above the supports'.
  if (index >= kQuoteTextBase) {
    const std::vector<std::string>& texts = db->quotes.texts;
    return index - kQuoteTextBase < (int)texts.size() ? texts[(size_t)(index - kQuoteTextBase)] : kNoText;
  }
  // The lines of support conversations have their own table (docs/SRPG.md "Levels and conversations").
  if (index >= kSupportTextBase) {
    const std::vector<std::string>& texts = db->supports.texts;
    return index - kSupportTextBase < (int)texts.size() ? texts[(size_t)(index - kSupportTextBase)] : kNoText;
  }
  if (!story || index < 0 || index >= (int)story->texts.size()) return kNoText;
  return story->texts[(size_t)index];
}

int Battle::unitByStoryId(const std::string& id) const {
  if (id.empty()) return -1;
  for (const Unit& u : units)
    if (u.storyId == id) return u.id;
  return -1;
}

bool Battle::unitMatches(const UnitFilter& f, const Unit& u) const {
  switch (f.kind) {
    case UnitFilter::Kind::Any: return true;
    case UnitFilter::Kind::Unit: return !u.storyId.empty() && u.storyId == f.unit;
    case UnitFilter::Kind::Side: return u.side == f.index;
    case UnitFilter::Kind::Team: return teamOf(u) == f.index;
    case UnitFilter::Kind::Type: return u.type == f.index;
    case UnitFilter::Kind::Tag: {
      for (const std::string& t : typeOf(u).tags)
        if (t == f.unit) return true;
      return false;
    }
  }
  return false;
}

int Battle::humanTeam() const {
  for (const SideState& s : sides)
    if (!s.ai) return s.team;
  return sides.empty() ? 0 : sides[0].team;
}

const std::string& Battle::objectiveText() const {
  if (!story) return kNoText;
  return storyText(story->objectives[(size_t)storyState.objectives].text);
}

bool Battle::setStartingStory(const std::vector<uint8_t>& switches, const std::vector<int>& variables) {
  if (switches.size() != storyState.switches.size() || variables.size() != storyState.variables.size()) return false;
  storyState.switches = switches;
  storyState.variables = variables;
  return true;
}

// ---- conditions ----

bool Battle::conditionHolds(const Condition& c) const {
  switch (c.kind) {
    case Condition::Kind::Switch:
      return (storyState.switches[(size_t)c.index] != 0) == c.is;
    case Condition::Kind::Variable:
      return compare(c.op, storyState.variables[(size_t)c.index], c.value);
    case Condition::Kind::Round:
      return compare(c.op, round, c.value);
    case Condition::Kind::Alive:
    case Condition::Kind::Dead: {
      bool any = false, alive = false;
      for (const Unit& u : units) {
        if (!unitMatches(c.filter, u) || summonIgnored(c.filter, u)) continue;  // A1: summons are not counted by side / team / any
        any = true;
        alive = alive || exists(u);  // R1: a carried unit exists
      }
      // "dead" needs a unit that exists (or existed): a unit not added yet is neither.
      return c.kind == Condition::Kind::Alive ? alive : (any && !alive);
    }
    case Condition::Kind::Researched:
      return hasTechnology(c.side, c.index);
    case Condition::Kind::Sees:
      // Judged on the team's sight (the truth of what it sees); without fog it sees every living unit.
      for (const Unit& u : units)
        if (u.alive && unitMatches(c.filter, u) && unitVisibleTo(c.team, u)) return true;
      return false;
    case Condition::Kind::Carries:
      for (const Unit& u : units) {
        if (!exists(u) || !unitMatches(c.filter, u)) continue;  // R1: a carried unit keeps its items
        for (const ItemStack& s : u.items)
          if (s.item == c.index) return true;
      }
      return false;
    case Condition::Kind::HasSkill:
      return storyHasSkill(c);
    case Condition::Kind::Support:
      return storySupportHolds(c);
    case Condition::Kind::HasCondition:
      return storyHasCondition(c);
  }
  return false;
}

bool Battle::conditionsHold(const std::vector<Condition>& list) const {
  for (const Condition& c : list)
    if (!conditionHolds(c)) return false;
  return true;
}

// ---- triggers and the queue ----

bool Battle::triggerMatches(const Trigger& t, const StoryOccurrence& o) const {
  using K = Trigger::Kind;
  if (t.kind != o.kind) return false;
  switch (t.kind) {
    case K::Turn:
      return (t.round == 0 || t.round == o.round) && t.side == o.side;
    case K::Reach:
      return o.unit >= 0 && inRect(grid, t.rect, o.cell) && unitMatches(t.who, units[(size_t)o.unit]);
    case K::Defeated:
      if (o.unit < 0 || !unitMatches(t.who, units[(size_t)o.unit])) return false;
      // "by" left out = by anything (including nobody: a side that is wiped out by the rules).
      return t.other.kind == UnitFilter::Kind::Any || (o.other >= 0 && unitMatches(t.other, units[(size_t)o.other]));
    case K::Fight: {
      if (o.unit < 0 || o.other < 0) return false;
      const Unit& a = units[(size_t)o.unit];
      const Unit& b = units[(size_t)o.other];
      // Either one may be the unit meant; "with" is then the other.
      return (unitMatches(t.who, a) && unitMatches(t.other, b)) || (unitMatches(t.who, b) && unitMatches(t.other, a));
    }
    case K::Promoted:  // docs/SRPG.md: fired by Battle::promote with the unit
      return o.unit >= 0 && unitMatches(t.who, units[(size_t)o.unit]);
    case K::Support:   // fired when a pair gains a level; "level" left out = any level
      return t.pair == o.pair && (t.level == 0 || t.level == o.level);
    case K::Place:     // docs/SRPG.md section 10: a door / chest opened, a place visited / destroyed; "by" left out = by anyone (or the story)
      if (o.place < 0 || o.place >= (int)structures.size() || structures[(size_t)o.place].id != t.place || t.placeIs != o.placeIs) return false;
      return t.who.kind == UnitFilter::Kind::Any || (o.unit >= 0 && unitMatches(t.who, units[(size_t)o.unit]));
    default:
      return true;  // battle start / won / lost / end: the kind says it all
  }
}

void Battle::fireStory(const StoryOccurrence& o, std::vector<Event>& events) {
  if (!story) return;
  for (size_t i = 0; i < story->events.size(); ++i) {
    const StoryEvent& ev = story->events[i];
    if (ev.when.kind == Trigger::Kind::Talk) continue;  // only the Talk action starts these
    if (storyState.fired[i] && !ev.repeat) continue;
    if (triggerMatches(ev.when, o)) storyRun_.queue.push_back({(int)i, -1});
  }
  drainStory(events);
}

// Runs queued events until none is left. While events run, new ones only join the queue, so
// every event sees the world the previous one left. `running` guards against re-entry.
void Battle::drainStory(std::vector<Event>& events) {
  StoryRun& run = storyRun_;
  if (run.running) return;
  run.running = true;
  run.ran = 0;
  for (size_t head = 0; head < run.queue.size(); ++head) {
    if (choice_.halted) break;  // a question is waiting (story_choice.cpp): the rest of the queue does not run
    StoryRun::Queued q = run.queue[head];
    const StoryEvent& ev = story->events[(size_t)q.event];
    if (storyState.fired[(size_t)q.event] && !ev.repeat) continue;  // queued twice in this go
    // Conditions are judged when the event is about to run, so earlier events count; one that
    // fails does not use up a non-repeating event.
    if (!conditionsHold(ev.cond)) continue;
    if (run.ran >= kStoryChainLimit) {
      OTE_ERROR("story: more than %d events in one go; cut at event '%s' (an event that sets off itself?)",
                kStoryChainLimit, ev.id.c_str());
      break;
    }
    ++run.ran;
    storyState.fired[(size_t)q.event] = 1;
    run.talker = q.talker;
    runCommands(ev.commands, events);
    run.talker = -1;
  }
  run.queue.clear();
  run.running = false;
  endStoryRun(events);  // a fade does not outlive the run (story_scene_fx.cpp)
}

// ---- Talk ----

void Battle::talkOptions(int unit, Cell from, std::vector<TalkOption>& out) const {
  out.clear();
  if (!story || over || !grid.valid(from) || !canAct(unit)) return;
  const Unit& u = units[(size_t)unit];
  if (u.done || u.storyId.empty()) return;
  for (size_t i = 0; i < story->events.size(); ++i) {
    const StoryEvent& ev = story->events[i];
    if (ev.when.kind != Trigger::Kind::Talk || ev.when.talker != u.storyId) continue;
    if (storyState.fired[i] && !ev.repeat) continue;
    int t = unitByStoryId(ev.when.talkee);
    if (t < 0 || !units[(size_t)t].alive || grid.distance(from, units[(size_t)t].cell) != 1) continue;
    if (!unitVisibleTo(teamOf(u), units[(size_t)t])) continue;  // fog: nobody to talk to that the team cannot see
    if (!conditionsHold(ev.cond)) continue;
    TalkOption opt;
    opt.target = t;
    opt.event = (int)i;
    opt.label = ev.when.label;
    out.push_back(opt);
  }
}

void Battle::applyTalk(const Action& a, std::vector<Event>& events) {
  std::vector<TalkOption> options;
  talkOptions(a.unit, units[(size_t)a.unit].cell, options);
  for (const TalkOption& o : options) {
    if (o.target != a.target || (a.slot >= 0 && a.slot != o.event)) continue;  // validated: one exists
    storyRun_.queue.push_back({o.event, a.unit});
    drainStory(events);
    return;
  }
}

}  // namespace ote
