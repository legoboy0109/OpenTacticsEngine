// The event printer's wording for the strategy-RPG entries (docs/SRPG.md "Events"); event_print.cpp hands them
// here. Every index in an event is checked so a bad event cannot crash the printer.
#include <cstdio>

#include "event_print.h"

using namespace ote;

namespace simtool {

namespace {

std::string unit(const Battle& b, int id) {
  if (id < 0 || id >= (int)b.units.size()) return "?";
  const Unit& u = b.units[(size_t)id];
  char buf[256];
  std::snprintf(buf, sizeof buf, "%s#%d[%s/%s]", u.name.c_str(), id, b.sides[(size_t)u.side].id.c_str(), b.typeOf(u).id.c_str());
  return buf;
}

std::string cell(const Battle& b, Cell c) {
  if (!b.grid.valid(c)) return "(?)";
  return "(" + std::to_string(b.grid.x(c)) + "," + std::to_string(b.grid.y(c)) + ")";
}

std::string skill(const Battle& b, int i) { return i >= 0 && i < (int)b.db->skills.size() ? b.db->skills[(size_t)i].id : "skill?"; }
std::string ability(const Battle& b, int i) {
  return i >= 0 && i < (int)b.db->abilities.size() ? b.db->abilities[(size_t)i].id : "ability?";
}
std::string item(const Battle& b, int i) { return i >= 0 && i < (int)b.db->items.size() ? b.db->items[(size_t)i].id : "item?"; }
std::string type(const Battle& b, int i) {
  return i >= 0 && i < (int)b.db->unitTypes.size() ? b.db->unitTypes[(size_t)i].id : "type?";
}

// "eirik and lyra": the two characters of a support pair.
std::string pairName(const Battle& b, int i) {
  if (i < 0 || i >= (int)b.db->supports.pairs.size()) return "pair?";
  const SupportPairDef& p = b.db->supports.pairs[(size_t)i];
  auto name = [&](int c) { return c >= 0 && c < (int)b.db->characters.size() ? b.db->characters[(size_t)c].id : std::string("?"); };
  return name(p.a) + " and " + name(p.b);
}

}  // namespace

bool describeSrpgEvent(const Battle& b, const Event& e, std::string& out) {
  switch (e.type) {
    case EventType::SkillLearned:
      out = unit(b, e.a) + " learns " + skill(b, e.b);
      return true;
    case EventType::RankUp: {
      const RankRules& r = b.db->ranks;
      auto rank = [&](int i) { return i >= 0 && i < (int)r.ladder.size() ? r.ladder[(size_t)i].id : std::string("?"); };
      out = unit(b, e.a) + " ranks up in " + (e.b >= 0 && e.b < (int)r.groups.size() ? r.groups[(size_t)e.b] : std::string("?")) +
            ": " + rank(e.d) + " to " + rank(e.c);
      return true;
    }
    case EventType::SkillActivated:
      out = unit(b, e.a) + ": " + skill(b, e.b) + " activates" + (e.c >= 0 ? " against " + unit(b, e.c) : "");
      return true;
    case EventType::UnitHurt:
      out = unit(b, e.a) + " is hurt for " + std::to_string(e.b) + " by a skill of " + unit(b, e.c);
      return true;
    case EventType::UnitPromoted: {
      out = unit(b, e.a) + " is promoted from " + type(b, e.b) + " to " + type(b, e.c) + " (level " + std::to_string(e.d) + ":";
      bool any = false;
      for (size_t i = 0; i < e.data.size() && i < b.db->stats.size(); ++i)
        if (e.data[i] != 0) {
          out += std::string(" ") + (e.data[i] > 0 ? "+" : "") + std::to_string(e.data[i]) + " " + b.db->stats[i].id;
          any = true;
        }
      out += any ? ")" : " nothing)";
      return true;
    }
    case EventType::AbilityUsed:
      out = unit(b, e.a) + " uses " + ability(b, e.b);
      if (e.c >= 0) out += " on " + unit(b, e.c);
      if (e.d >= 0) out += " at " + cell(b, e.d);
      if (e.flags & kBlocked) out += " (blocked" + (e.data.empty() ? std::string() : " by " + unit(b, e.data[0])) + ")";
      return true;
    case EventType::UnitRescued:
      out = unit(b, e.a) + ((e.flags & kPaired) ? " is joined by " : " rescues ") + unit(b, e.b) + " from " + cell(b, e.c);
      return true;
    case EventType::UnitDropped:
      out = unit(b, e.a) + " drops " + unit(b, e.b) + " at " + cell(b, e.c) + ((e.flags & kForced) ? " (forced)" : "") +
            ((e.flags & kBlocked) ? " (blocked)" : "");
      return true;
    case EventType::UnitHanded:
      out = unit(b, e.a) + " hands " + unit(b, e.c) + " to " + unit(b, e.b);
      return true;
    case EventType::PairSwitched:
      out = unit(b, e.b) + " comes forward at " + cell(b, e.c) + ", " + unit(b, e.a) + " goes behind";
      return true;
    case EventType::SupportPoints:
      out = "support " + pairName(b, e.c) + ": " + std::to_string(e.d) + " points" + ((e.flags & kReady) ? " (a conversation is ready)" : "");
      return true;
    case EventType::SupportReached:
      out = "support " + pairName(b, e.c) + " reaches level " + std::to_string(e.d);
      return true;
    case EventType::StaffUsed:
      out = unit(b, e.a) + " uses " + item(b, e.b) + (e.c >= 0 ? " on " + unit(b, e.c) : std::string(" on its area")) + " (" + std::to_string(e.d) + " affected)";
      return true;
    case EventType::StaffMissed:
      out = item(b, e.c) + " of " + unit(b, e.b) + " misses " + unit(b, e.a);
      return true;
    case EventType::ItemRepaired:
      out = unit(b, e.a) + "'s " + item(b, e.b) + " is repaired by " + std::to_string(e.c) + " (" + std::to_string(e.d) + " uses)";
      return true;
    case EventType::AreaLit:
      out = unit(b, e.a) + " lights " + cell(b, e.b) + " (radius " + std::to_string(e.c) + ", " + std::to_string(e.d) + " turns)";
      return true;
    case EventType::ConditionApplied: {
      const std::string name = e.b >= 0 && (size_t)e.b < b.db->conditions.size() ? b.db->conditions[(size_t)e.b].name : "?";
      out = unit(b, e.a) + " gets " + name + (e.c >= 0 ? " for " + std::to_string(e.c) + " turns" : std::string(" until cured")) +
            ((e.flags & kRefreshed) ? " (again)" : "") + (e.d >= 0 ? " from " + unit(b, e.d) : std::string());
      return true;
    }
    case EventType::ConditionEnded: {
      static const char* const kWhy[] = {"ran out", "wore off", "was cured", "was replaced", "was woken by a strike"};
      const std::string name = e.b >= 0 && (size_t)e.b < b.db->conditions.size() ? b.db->conditions[(size_t)e.b].name : "?";
      out = unit(b, e.a) + "'s " + name + " " + (e.c >= 0 && e.c < 5 ? kWhy[e.c] : "ended");
      return true;
    }
    case EventType::ConditionResisted: {
      const std::string name = e.b >= 0 && (size_t)e.b < b.db->conditions.size() ? b.db->conditions[(size_t)e.b].name : "?";
      out = (e.flags & kMissed) ? name + " missed " + unit(b, e.a) + " (" + std::to_string(e.d) + "%)" : unit(b, e.a) + " is immune to " + name;
      return true;
    }
    case EventType::ConditionTick: {
      const std::string name = e.b >= 0 && (size_t)e.b < b.db->conditions.size() ? b.db->conditions[(size_t)e.b].name : "?";
      out = unit(b, e.a) + (e.c == kTickActsAlone ? " acts on its own (" : " cannot act (") + name + ")";
      return true;
    }
    default:
      return false;
  }
}

}  // namespace simtool
