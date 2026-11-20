// Loader of the unit rank table. Every refusal names the file (through the LoadCtx), the entry and the key.
#include "core/data/loader_unit_ranks.h"

#include <algorithm>

namespace ote {

namespace {

constexpr int kMaxUnitRanks = 20;
constexpr int kMaxAt = 1000000;  // total experience; well under the save reader's number limit

std::string q(const std::string& s) { return "\"" + s + "\""; }

std::string roman(int n) {  // 1..20 -> I..XX, the default badge of the n-th rank
  static const char* const kDigits[] = {"", "I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX"};
  std::string out;
  for (; n >= 10; n -= 10) out += "X";
  return out + kDigits[n];
}

bool onlyThese(const LoadCtx& c, const Json& e, const std::string& at, std::initializer_list<const char*> keys) {
  for (const Json::Member& m : e.members()) {
    bool known = false;
    for (const char* k : keys) known = known || m.first == k;
    if (known) continue;
    std::string list;
    for (const char* k : keys) list += (list.empty() ? "" : ", ") + q(k);
    return c.fail(at, "unknown key " + q(m.first) + " (expected " + list + ")");
  }
  return true;
}

bool printable(const std::string& s) {
  for (unsigned char ch : s)
    if (ch < 0x20 || ch > 0x7e) return false;
  return true;
}

}  // namespace

bool loadUnitRankRules(const LoadCtx& c, const Json& block, Database& db) {
  RulesConfig& r = db.rules;
  r.unitRanks = UnitRankRules();
  if (!block.has("levels")) return true;
  const std::string at = "rules.progression";
  if (!r.progression) return c.fail(at, "\"levels\" needs experience: set \"enabled\": true");
  // The two ways to gain power exclude each other: the keys of the other one are refused so a maker is never left wondering why they do nothing.
  for (const char* k : {"xpPerLevel", "maxLevel"})
    if (block.has(k)) return c.fail(at, q(k) + " belongs to levelling, and this game uses the rank table (\"levels\"): a game uses one or the other");
  const Json* list;
  if (!c.array(block, "levels", at, list)) return false;
  if (list->size() == 0 || list->size() > (size_t)kMaxUnitRanks)
    return c.fail(at, "\"levels\" must list 1 to " + std::to_string(kMaxUnitRanks) + " ranks");
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& e = (*list)[i];
    std::string where = at + ", \"levels\" entry " + std::to_string(i + 1);
    if (!e.isObject()) return c.fail(where, "must be an object");
    UnitRankDef d;
    if (!onlyThese(c, e, where, {"at", "name", "badge", "bonus", "skills"})) return false;
    if (!c.str(e, "name", where, d.name, true)) return false;
    where += " (" + q(d.name) + ")";
    if (d.name.empty() || d.name.size() > 24) return c.fail(where, "\"name\" must be 1 to 24 characters");
    if (!c.integerIn(e, "at", where, 0, kMaxAt, d.at, true)) return false;
    if (i > 0 && d.at <= r.unitRanks.levels.back().at)
      return c.fail(where, "\"at\" must be higher than the one before it (" + std::to_string(r.unitRanks.levels.back().at) + "): the ranks go up in order");
    d.badge = roman((int)i + 1);
    if (e.has("badge")) {
      if (!c.str(e, "badge", where, d.badge)) return false;
      if (d.badge.size() > 3 || !printable(d.badge)) return c.fail(where, "\"badge\" must be 0 to 3 plain characters (the mark on the unit; \"\" shows none)");
    }
    if (e.has("bonus")) {
      d.bonus.assign(db.stats.size(), 0);
      if (!c.statMap(db, e, "bonus", where, d.bonus)) return false;
    }
    std::vector<std::string> skills;
    if (e.has("skills") && !c.stringList(e, "skills", where, skills)) return false;
    r.unitRanks.levels.push_back(d);
    r.unitRanks.skillIds.push_back(skills);
  }
  return true;
}

bool resolveUnitRankSkills(const std::string& root, Database& db, std::string& err) {
  UnitRankRules& t = db.rules.unitRanks;
  const LoadCtx c(err, root + "/game.json");
  for (size_t i = 0; i < t.levels.size(); ++i)
    for (const std::string& id : t.skillIds[i]) {
      const int s = db.findSkill(id);
      if (s < 0)
        return c.fail("rules.progression, \"levels\" entry " + std::to_string(i + 1) + " (" + q(t.levels[i].name) + ")", "\"skills\" names unknown skill " + q(id) + " (data/skills.json)");
      if (std::find(t.levels[i].skills.begin(), t.levels[i].skills.end(), s) == t.levels[i].skills.end()) t.levels[i].skills.push_back(s);
    }
  t.skillIds.clear();
  return true;
}

bool readUnitXp(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, MapUnitDef& u) {
  u.xp = 0;
  if (!e.has("xp")) return true;
  if (!db.rules.unitRanks.on())
    return c.fail(where, "\"xp\" needs a rank table: \"rules\": { \"progression\": { \"levels\": [ ... ] } } in game.json (levelling keeps its experience per level)");
  return c.integerIn(e, "xp", where, 0, db.rules.unitRanks.topAt(), u.xp);
}

}  // namespace ote
