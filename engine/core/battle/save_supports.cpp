// Supports in saves (docs/SRPG.md "Supports": Saves and hash): the closeness of the pairs, in a battle save and in
// a campaign's playthrough: "supports": { "points": [n, ...], "levels": [n, ...] }, one entry per pair in file
// order. Written only in a game with pairs and only when it differs from the starting state (so a save of a
// battle where nothing has happened yet is the same as without supports); a missing key reads as that start.
#include <cmath>

#include "core/battle/save_srpg.h"
#include "core/battle/support_rules.h"
#include "core/campaign/playthrough.h"

namespace ote {
namespace savejson {

namespace {

bool differs(const SupportState& a, const SupportState& b) { return a.points != b.points || a.level != b.level; }

void writeState(const SupportState& s, Json& out) {
  Json points = Json::makeArray(), levels = Json::makeArray();
  for (int p : s.points) points.push(Json::makeNumber(p));
  for (uint8_t l : s.level) levels.push(Json::makeNumber((int)l));
  Json o = Json::makeObject();
  o.set("points", points);
  o.set("levels", levels);
  out.set("supports", o);
}

// Reads `saved["supports"]` (known to exist) for a game that has pairs; every value is checked.
bool readState(Reader& rd, const Database& db, const Json& saved, const std::string& where, SupportState& out) {
  const Json* o;
  if (!rd.object(saved, "supports", where, o)) return false;
  const std::string at = where + ".supports";
  for (const Json::Member& m : o->members())
    if (m.first != "points" && m.first != "levels") return rd.fail(at, "unknown key \"" + m.first + "\"");
  const int n = (int)db.supports.pairs.size();
  const Json *points, *levels;
  if (!rd.array(*o, "points", at, n, 0, points) || !rd.array(*o, "levels", at, n, 0, levels)) return false;
  out.points.assign((size_t)n, 0);
  out.level.assign((size_t)n, 0);
  for (int i = 0; i < n; ++i) {
    const SupportPairDef& p = db.supports.pairs[(size_t)i];
    const Json& pv = (*points)[(size_t)i];
    const Json& lv = (*levels)[(size_t)i];
    const std::string pw = at + ".points[" + std::to_string(i) + "]", lw = at + ".levels[" + std::to_string(i) + "]";
    if (!pv.isNumber() || pv.asNumber() != std::floor(pv.asNumber()) || pv.asNumber() < 0 || pv.asNumber() > kMaxSupportPoints)
      return rd.fail(pw, "not a whole number from 0 to " + std::to_string(kMaxSupportPoints));
    if (!lv.isNumber() || lv.asNumber() != std::floor(lv.asNumber()) || lv.asNumber() < 0 || lv.asNumber() > (double)p.levels.size())
      return rd.fail(lw, "not a whole number from 0 to " + std::to_string(p.levels.size()) + " (the pair's levels)");
    out.points[(size_t)i] = (int)pv.asNumber();
    out.level[(size_t)i] = (uint8_t)lv.asNumber();
    if (out.level[(size_t)i] > 0 && out.points[(size_t)i] < p.levels[(size_t)out.level[(size_t)i] - 1].points)
      return rd.fail(lw, "level " + std::to_string((int)out.level[(size_t)i]) + " needs more points than the pair has");
  }
  return true;
}

}  // namespace

void writeBattleSupports(const Battle& b, Json& out) {
  if (b.db->supports.pairs.empty() || !differs(b.supports, startingSupports(*b.db))) return;
  writeState(b.supports, out);
}

bool readBattleSupports(Reader& rd, const Json& saved, Battle& b) {
  if (!saved.has("supports")) return true;  // the state Battle::init made: the pairs at their start
  if (b.db->supports.pairs.empty()) return rd.fail("battle.supports", "this game has no supports");
  return readState(rd, *b.db, saved, "battle", b.supports);
}

void writePlaythroughSupports(const Database& db, const Playthrough& pt, Json& out) {
  if (db.supports.pairs.empty() || !differs(pt.supports, startingSupports(db))) return;
  writeState(pt.supports, out);
}

bool readPlaythroughSupports(Reader& rd, const Database& db, const Json& saved, Playthrough& pt) {
  if (saved.has("supports") && db.supports.pairs.empty()) return rd.fail("playthrough.supports", "this game has no supports");
  if (!saved.has("supports")) {
    pt.supports = startingSupports(db);
    return true;
  }
  return readState(rd, db, saved, "playthrough", pt.supports);
}

}  // namespace savejson
}  // namespace ote
