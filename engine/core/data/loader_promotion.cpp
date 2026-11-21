// Loader of the "promotions" key on unit types and "promotes" on items, plus the story command promote and the
// trigger promoted (docs/SRPG.md "Promotion"). rules.promotion is read in loader_srpg_rules.cpp.
#include "core/data/loader_events.h"
#include "core/data/loader_srpg.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {

std::string quoted(const std::string& s) { return "\"" + s + "\""; }

bool knownKey(const std::string& k) {
  static const char* const keys[] = {"to", "fromLevel", "items", "atLevel", "levelAfter", "gains", "fromLevelFor"};
  for (const char* n : keys)
    if (k == n) return true;
  return false;
}

// One entry of "promotions". `where` names the unit type; the entry's own number goes in front of each message.
// The target type is only kept as written: it may be defined further down units.json (resolvePromotionRefs).
bool readPromotion(const LoadCtx& c, const Database& db, const Json& p, size_t index, const std::string& where,
                   PromotionDef& d) {
  const std::string at = "\"promotions\" entry " + std::to_string(index + 1);
  if (!p.isObject()) return c.fail(where, at + " must be an object");
  for (const Json::Member& m : p.members())
    if (!knownKey(m.first)) return c.fail(where, at + ": " + unknownKeyText(m.first, {"to", "fromLevel", "items", "atLevel", "levelAfter", "gains", "fromLevelFor"}));
  if (!c.str(p, "to", where + ": " + at, d.toId, true)) return false;

  if (p.has("fromLevel")) {
    if (!LoadCtx::isWhole(p["fromLevel"]) || p["fromLevel"].asInt() < 1 || p["fromLevel"].asInt() > db.rules.maxLevel)
      return c.fail(where, at + ": \"fromLevel\" must be from 1 to " + std::to_string(db.rules.maxLevel));
    d.fromLevel = p["fromLevel"].asInt();
  }

  if (p.has("fromLevelFor")) {  // { "eirik": 1, "mia": 8 }: other levels for named characters (the ids are resolved last)
    const Json& m = p["fromLevelFor"];
    if (!m.isObject() || m.size() == 0) return c.fail(where, at + ": \"fromLevelFor\" must be an object such as { \"eirik\": 1 } (character id: level)");
    for (const Json::Member& e : m.members()) {
      if (!LoadCtx::isWhole(e.second) || e.second.asInt() < 1 || e.second.asInt() > db.rules.maxLevel)
        return c.fail(where, at + ": \"fromLevelFor\" " + quoted(e.first) + " must be a level from 1 to " + std::to_string(db.rules.maxLevel));
      d.fromLevelForIds.push_back({e.first, e.second.asInt()});
    }
  }

  if (p.has("atLevel")) {
    const std::string v = p["atLevel"].isString() ? p["atLevel"].asString() : "";
    if (v == "off") d.atLevel = PromoteAtLevel::Off;
    else if (v == "offer") d.atLevel = PromoteAtLevel::Offer;
    else if (v == "auto") d.atLevel = PromoteAtLevel::Auto;
    else return c.fail(where, at + ": \"atLevel\" must be \"off\", \"offer\" or \"auto\"");
    if (d.atLevel != PromoteAtLevel::Off && db.rules.unitRanks.replacesLevels())
      return c.fail(where, at + ": \"atLevel\" needs levels, and this game uses the rank table (rules.progression \"levels\"): promote by item, by a story command or from a menu");
    if (d.atLevel != PromoteAtLevel::Off && !db.rules.progression)
      return c.fail(where, at + ": \"atLevel\" needs experience and levels (rules.progression)");
  }

  if (p.has("levelAfter")) {
    const std::string v = p["levelAfter"].isString() ? p["levelAfter"].asString() : "";
    if (v == "reset") d.levelAfter = LevelAfter::Reset;
    else if (v == "keep") d.levelAfter = LevelAfter::Keep;
    else return c.fail(where, at + ": \"levelAfter\" must be \"reset\" or \"keep\"");
  }

  // Items are loaded before unit types, so they are checked here.
  if (!c.stringList(p, "items", where + ": " + at, d.itemIds)) return false;
  for (const std::string& id : d.itemIds) {
    int it = db.findItem(id);
    if (it < 0 || !db.items[(size_t)it].promotes)
      return c.fail(where, at + ": \"items\" names " + quoted(id) +
                               ", which is not a promotion item (it needs \"promotes\": true in data/items.json)");
    d.items.push_back(it);
  }

  if (p.has("gains")) {
    d.hasGains = true;
    d.gains.assign(db.stats.size(), 0);
    if (!c.statMap(db, p, "gains", where + ": " + at, d.gains)) return false;
  }
  return true;
}

}  // namespace

bool readUnitPromotions(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, UnitTypeDef& u) {
  const Json* list;
  if (!c.array(e, "promotions", where, list)) return false;
  if ((int)list->size() > kMaxPromotions)
    return c.fail(where, "too many \"promotions\" (at most " + std::to_string(kMaxPromotions) + ")");
  for (size_t i = 0; i < list->size(); ++i) {
    PromotionDef d;
    if (!readPromotion(c, db, (*list)[i], i, where, d)) return false;
    u.promotions.push_back(d);
  }
  return true;
}

bool readItemPromotes(const LoadCtx& c, const Json& e, const std::string& where, ItemDef& it) {
  return c.boolean(e, "promotes", where, it.promotes);
}

// Target types become indices. Run once every unit type is loaded (a promotion may name a type further down).
bool resolvePromotionRefs(const std::string& root, Database& db, std::string& err) {
  LoadCtx c(err, root + "/data/units.json");
  for (UnitTypeDef& u : db.unitTypes) {
    const std::string where = "unit " + quoted(u.id);
    for (size_t i = 0; i < u.promotions.size(); ++i) {
      PromotionDef& d = u.promotions[i];
      const std::string at = "\"promotions\" entry " + std::to_string(i + 1);
      d.to = db.findUnitType(d.toId);
      if (d.to < 0) return c.fail(where, at + ": unknown unit type " + quoted(d.toId));
      if (db.unitTypes[(size_t)d.to].id == u.id) return c.fail(where, at + ": \"to\" is the unit type itself");
      for (const auto& f : d.fromLevelForIds) {
        const int ch = db.findCharacter(f.first);
        if (ch < 0) return c.fail(where, at + ": \"fromLevelFor\" names unknown character " + quoted(f.first) + " (data/characters.json)");
        d.fromLevelFor.push_back({ch, f.second});
      }
    }
  }
  return true;
}

// { "promote": "eirik", "to": "captain" }. `to` is any unit type; without it the unit's type must have exactly one
// promotion. The unit's type is only known here for units the map places itself; for one that an addUnit command or
// a character brings in, the same rule is checked when the command runs (Battle::storyPromote).
bool StoryLoader::promoteCommand(const Json& e, const std::string& at, Command& out) {
  if (!onlyKeys(e, {"promote", "to"}, at) || !unitId(e["promote"], at, "\"promote\"", out.unit)) return false;
  out.index = -1;
  if (e.has("to")) {
    std::string to;
    if (!c_.str(e, "to", at, to)) return false;
    out.index = db_.findUnitType(to);
    if (out.index < 0) return fail(at, "\"to\" names unknown unit type " + quoted(to));
    return true;
  }
  for (const MapUnitDef& u : map_.units) {
    if (u.id != out.unit) continue;
    const UnitTypeDef& t = db_.unitTypes[(size_t)u.type];
    if (t.promotions.size() != 1)
      return fail(at, "\"promote\" needs \"to\": " + quoted(t.id) + " has " + std::to_string(t.promotions.size()) +
                          " promotions");
  }
  return true;
}

bool StoryLoader::promotedTrigger(const Json& w, const std::string& at, Trigger& out) {
  return onlyKeys(w, {"promoted"}, at) && filter(w["promoted"], at, "\"promoted\"", out.who);
}

}  // namespace ote
