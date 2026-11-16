// The Support action (a conversation in battle) and the story's part of supports: the command addSupport and the
// condition support (docs/SRPG.md "Supports": Levels and conversations, Story and campaign).
#include "core/battle/battle.h"
#include "core/battle/support_rules.h"

namespace ote {

namespace {

bool compareLevel(CmpOp op, int a, int b) {
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

}  // namespace

// Why the order cannot be given (null = it can). Used by the action, its options and the player's menu.
const char* Battle::supportWhy(int unit, Cell from, int target) const {
  const SupportData& s = db->supports;
  if (s.pairs.empty()) return "this game has no supports";
  if (s.rules.conversations == SupportTalks::Camp) return "support conversations are played between chapters in this game";
  if (target < 0 || target >= (int)units.size() || target == unit) return "that is not a unit to talk to";
  const Unit& u = units[(size_t)unit];
  const Unit& t = units[(size_t)target];
  if (!exists(t) || u.character < 0 || t.character < 0) return "those two are not a support pair";
  const int pair = db->findSupportPair(u.character, t.character);
  if (pair < 0) return "those two are not a support pair";
  if (!supportTalkPending(*db, supports, pair)) return "those two have no support conversation waiting";
  if (teamOf(u) != teamOf(t)) return "those two are not on the same team";
  // Neighbours on the board; the two of a pair-up pair share a square, so the one behind is the front one's partner.
  const bool partners = db->rules.rescue.mode == RescueMode::PairUp && (u.carrying == target || t.carrying == unit);
  if (!partners && !(t.alive && grid.valid(from) && grid.distance(from, t.cell) == 1)) return "those two are not next to each other";
  return nullptr;
}

void Battle::supportOptions(int unit, Cell from, std::vector<int>& out) const {
  out.clear();
  if (db->supports.pairs.empty() || over || unit < 0 || unit >= (int)units.size() || !canAct(unit)) return;
  const Unit& u = units[(size_t)unit];
  if (u.done || u.acted || u.character < 0 || !grid.valid(from)) return;
  for (const Unit& t : units)
    if (t.id != unit && exists(t) && !supportWhy(unit, from, t.id)) out.push_back(t.id);
}

bool Battle::validateSupport(const Action& action, std::string* why) const {
  const char* reason = supportWhy(action.unit, units[(size_t)action.unit].cell, action.target);  // validate() checked the unit
  if (reason && why) *why = reason;
  return reason == nullptr;
}

// The conversation: its lines as Dialogue events, then the level is gained. Free: nothing about the unit changes.
void Battle::applySupport(const Action& action, std::vector<Event>& events) {
  const int pair = db->findSupportPair(units[(size_t)action.unit].character, units[(size_t)action.target].character);
  const int level = supports.level[(size_t)pair];
  for (const SupportLine& line : db->supports.pairs[(size_t)pair].levels[(size_t)level].talk) {
    // The speaker is a character: its unit when it is on the board, else the name stored with the line.
    int speaker = line.speaker.empty() ? -1 : unitByStoryId(line.speaker);
    if (speaker >= 0 && !units[(size_t)speaker].alive) speaker = -1;
    Event e;
    e.type = EventType::Dialogue;
    e.a = speaker;
    e.b = kSupportTextBase + line.text;
    e.c = speaker < 0 && line.speakerName >= 0 ? kSupportTextBase + line.speakerName : -1;
    e.d = line.portrait >= 0 ? kSupportTextBase + line.portrait : -1;
    events.push_back(e);
  }
  ++supports.level[(size_t)pair];
  supportReached(pair, level + 1, events);
  supportSettleLevels(events);
}

// ---- story ----

void Battle::storyAddSupport(const Command& c, std::vector<Event>& events) {
  if (c.index >= 0 && (size_t)c.index < db->supports.pairs.size() && supports.points.size() == db->supports.pairs.size())
    supportGain(c.index, c.value, events);
}

bool Battle::storySupportHolds(const Condition& c) const {
  if (c.index < 0 || (size_t)c.index >= supports.level.size()) return false;
  return compareLevel(c.op, supports.level[(size_t)c.index], c.value);
}

}  // namespace ote
