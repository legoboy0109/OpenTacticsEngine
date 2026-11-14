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

}  // namespace

std::string describeEvent(const Battle& b, const Event& e) {
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
  }
  return "?";
}

}  // namespace simtool
