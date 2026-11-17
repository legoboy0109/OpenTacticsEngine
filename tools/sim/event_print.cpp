#include "event_print.h"

#include <cstdarg>
#include <cstdio>

using namespace ote;

namespace simtool {

namespace {

std::string fmt(const char* f, ...) __attribute__((format(printf, 1, 2)));
std::string fmt(const char* f, ...) {
  char buf[512];
  va_list ap;
  va_start(ap, f);
  std::vsnprintf(buf, sizeof buf, f, ap);
  va_end(ap);
  return buf;
}

std::string cellName(const Battle& b, Cell c) {
  if (!b.grid.valid(c)) return "(?)";
  return fmt("(%d,%d)", b.grid.x(c), b.grid.y(c));
}

// "city at (3,2)": structure index -> type id and top-left square.
std::string structureName(const Battle& b, int index) {
  if (index < 0 || index >= (int)b.structures.size()) return "structure?";
  const StructureState& s = b.structures[index];
  return fmt("%s at (%d,%d)", b.db->structures[s.type].id.c_str(), s.x, s.y);
}

std::string unitName(const Battle& b, int id) {
  if (id < 0 || id >= (int)b.units.size()) return "?";
  const Unit& u = b.units[id];
  return fmt("%s#%d[%s/%s]", u.name.c_str(), id, b.sides[u.side].id.c_str(), b.typeOf(u).id.c_str());
}


// An item's id; the index in an event is checked so a bad event cannot crash the printer.
std::string itemId(const Battle& b, int item) {
  return item >= 0 && item < (int)b.db->items.size() ? b.db->items[(size_t)item].id : "item?";
}

// "storage" for -1, "(thrown away)" for -2, else the unit.
std::string holderName(const Battle& b, int id) {
  if (id == -1) return "storage";
  if (id == -2) return "(thrown away)";
  return unitName(b, id);
}

// "+2 str +1 def": per-stat gains by stat id.
std::string gains(const Battle& b, const std::vector<int>& data) {
  std::string s;
  for (size_t i = 0; i < data.size() && i < b.db->stats.size(); ++i)
    if (data[i] != 0) s += (s.empty() ? "" : " ") + fmt("%+d %s", data[i], b.db->stats[i].id.c_str());
  return s.empty() ? "nothing" : s;
}

// Ids from event indices, checked so a bad event cannot crash the printer.
std::string techName(const Battle& b, int tech) {
  return tech >= 0 && tech < (int)b.db->research.size() ? b.db->research[(size_t)tech].id : "technology?";
}
std::string techKind(const Battle& b, int kind) {
  return kind >= 0 && kind < (int)b.db->structures.size() ? b.db->structures[(size_t)kind].id : "structure?";
}

std::string gained(const Battle& b, const Event& e) {
  std::string how = (e.flags & kDropped) ? " (dropped)" : (e.flags & kBought) ? " (bought)" : "";
  std::string who = e.a >= 0 ? unitName(b, e.a) : "storage of " + b.sides[(size_t)e.c].name;
  return who + " gets " + itemId(b, e.b) + how;
}

}  // namespace

static std::string describeEventText(const Battle& b, const Event& e) {
  switch (e.type) {
    case EventType::TurnStarted:
      return fmt("-- turn: %s, round %d", b.sides[e.a].name.c_str(), e.b);
    case EventType::UnitMoved: {
      std::string s = unitName(b, e.a) + " moves";
      for (size_t i = 0; i < e.data.size(); ++i) s += (i ? " > " : " ") + cellName(b, e.data[i]);
      return s;
    }
    case EventType::Attack: {
      const char* result = (e.flags & kHit) ? ((e.flags & kCrit) ? "CRIT" : "hit") : "miss";
      return fmt("%s %s %s with %s: %s%s, %d dmg%s", unitName(b, e.a).c_str(),
                 (e.flags & kCounter) ? "counters" : "attacks", unitName(b, e.b).c_str(),
                 b.db->weapons[e.c].id.c_str(), result, (e.flags & kCounter) ? " (counter)" : "", e.d,
                 (e.flags & kKill) ? " KILL" : "");
    }
    case EventType::UnitDied:
      return fmt("%s dies at %s", unitName(b, e.a).c_str(), cellName(b, e.b).c_str());
    case EventType::XpGained:
      return fmt("%s gains %d xp", unitName(b, e.a).c_str(), e.b);
    case EventType::LevelUp:
      return fmt("%s reaches level %d", unitName(b, e.a).c_str(), e.b);
    case EventType::CaptureProgress:
      return fmt("%s captures %s: %d points left", unitName(b, e.a).c_str(), structureName(b, e.d).c_str(), e.c);
    case EventType::StructureCaptured:
      return fmt("%s captured by %s (was %s)", structureName(b, e.a).c_str(), b.sides[e.b].name.c_str(),
                 e.c >= 0 ? b.sides[e.c].name.c_str() : "neutral");
    case EventType::ResourceChanged:
      return fmt("%s %s %+d -> %d", b.sides[e.a].name.c_str(), b.db->resources[e.b].id.c_str(), e.c, e.d);
    case EventType::UnitBuilt:
      return fmt("%s built at %s", unitName(b, e.a).c_str(), cellName(b, e.b).c_str());
    case EventType::SideDefeated:
      return fmt("side %s is defeated", b.sides[e.a].name.c_str());
    case EventType::BattleEnded:
      return fmt("== battle ended, winning team %d", e.a);
    case EventType::ItemUsed:
      return fmt("%s uses %s on %s", unitName(b, e.a).c_str(), itemId(b, e.b).c_str(), unitName(b, e.c).c_str());
    case EventType::UnitHealed:
      return fmt("%s heals %d", unitName(b, e.a).c_str(), e.b);
    case EventType::StatsRaised:
      return fmt("%s raised: %s", unitName(b, e.a).c_str(), gains(b, e.data).c_str());
    case EventType::ItemBroke:
      return fmt("%s's %s breaks", unitName(b, e.a).c_str(), itemId(b, e.b).c_str());
    case EventType::ItemGained:
      return gained(b, e);
    case EventType::ItemLost:
      return fmt("%s has no room: %s is lost", unitName(b, e.a).c_str(), itemId(b, e.b).c_str());
    case EventType::ItemMoved:
      return fmt("%s: %s -> %s", itemId(b, e.c).c_str(), holderName(b, e.a).c_str(), holderName(b, e.b).c_str());
    case EventType::ItemEquipped:
      return fmt("%s equips %s", unitName(b, e.a).c_str(), itemId(b, e.b).c_str());
    case EventType::Dialogue: {
      // The speaker is a unit (its current name), a free name, or nobody.
      std::string who = e.a >= 0 && e.a < (int)b.units.size() ? b.units[(size_t)e.a].name
                        : e.c >= 0 ? b.storyText(e.c) : "Narrator";
      std::string portrait = e.d >= 0 ? " [" + b.storyText(e.d) + "]" : "";
      return who + portrait + ": " + b.storyText(e.b);
    }
    case EventType::Focus:
      return e.b >= 0 ? fmt("view follows %s at %s", unitName(b, e.b).c_str(), cellName(b, e.a).c_str())
                      : fmt("view moves to %s", cellName(b, e.a).c_str());
    case EventType::UnitAdded:
      return fmt("%s appears at %s", unitName(b, e.a).c_str(), cellName(b, e.b).c_str());
    case EventType::UnitRemoved:
      return fmt("%s leaves the map from %s", unitName(b, e.a).c_str(), cellName(b, e.b).c_str());
    case EventType::UnitRelocated:
      return fmt("%s is moved %s > %s", unitName(b, e.a).c_str(), cellName(b, e.b).c_str(), cellName(b, e.c).c_str());
    case EventType::SideChanged:
      return fmt("%s joins side %s (was %s)", unitName(b, e.a).c_str(), b.sides[(size_t)e.c].name.c_str(),
                 b.sides[(size_t)e.b].name.c_str());
    case EventType::ObjectiveChanged:
      return "new objective: " + (e.a >= 0 ? b.storyText(e.a) : std::string("(none)"));
    case EventType::StructureUpgraded:
      return fmt("structure at %s upgraded: %s -> %s",
                 e.a >= 0 && e.a < (int)b.structures.size() ? cellName(b, b.grid.at(b.structures[(size_t)e.a].x, b.structures[(size_t)e.a].y)).c_str() : "(?)",
                 techKind(b, e.b).c_str(), techKind(b, e.c).c_str());
    case EventType::ResearchStarted:
      return fmt("%s starts researching %s (%d turns)", b.sides[(size_t)e.a].name.c_str(), techName(b, e.b).c_str(), e.c);
    case EventType::ResearchFinished:
      return fmt("%s has researched %s", b.sides[(size_t)e.a].name.c_str(), techName(b, e.b).c_str());
    case EventType::UnitRepaired: {
      std::string cost;
      for (size_t r = 0; r < e.data.size() && r < b.db->resources.size(); ++r)
        if (e.data[r] > 0) cost += (cost.empty() ? "" : ", ") + fmt("%d %s", e.data[r], b.db->resources[r].id.c_str());
      return fmt("%s is repaired +%d -> %d HP (%s)", unitName(b, e.a).c_str(), e.b, e.c, cost.empty() ? "free" : cost.c_str());
    }
    case EventType::UnitResupplied:
      return fmt("%s is resupplied", unitName(b, e.a).c_str());
    case EventType::MoveBlocked:
      return fmt("%s is stopped at %s by %s at %s", unitName(b, e.a).c_str(), cellName(b, e.c).c_str(),
                 unitName(b, e.b).c_str(), cellName(b, e.d).c_str());
    case EventType::MusicChanged:
      if (e.a < 0) return fmt("the story stops the music (fade %d ms)", e.b);
      return fmt("the story plays music '%s' (fade %d ms)", b.storyText(e.a).c_str(), e.b);
    case EventType::SoundRequested:
      return fmt("the story plays sound '%s'", b.storyText(e.a).c_str());
    case EventType::SceneEffect:  // presentation only (docs/EVENTS.md "Scene presentation"): the simulator shows no scenes
      return fmt("scene effect %d '%s' (%d ms)", e.a, e.b >= 0 ? b.storyText(e.b).c_str() : "", e.c);
    default:
      break;
  }
  // The strategy-RPG entries (docs/SRPG.md) are worded in event_print_srpg.cpp.
  std::string text;
  return describeSrpgEvent(b, e, text) ? text : "?";
}

std::string describeEvent(const Battle& b, const Event& e) {
  std::string text = describeEventText(b, e);
  if (e.seen != kSeenByAll) {  // fog of war: say which teams saw it
    text += " [seen by team";
    bool first = true;
    for (int t = 0; t < b.teamCount && t < kMaxFogTeams; ++t)
      if ((e.seen >> t) & 1u) { text += (first ? " " : ", ") + std::to_string(t); first = false; }
    text += first ? " none]" : "]";
  }
  return text;
}

}  // namespace simtool
