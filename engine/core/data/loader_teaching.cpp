// Loaders of learning from equipment (loader_teaching.h; docs/JOBS.md "Learning from equipment (ability points)"). Every refusal names the file,
// the entry and the key.
#include "core/data/loader_teaching.h"

namespace ote {

namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

}  // namespace

bool readItemTeaches(const LoadCtx& c, Database& db, const Json& e, const std::string& where, ItemDef& it) {
  it.teaches.clear();
  if (!e.has("teaches")) return true;
  if (it.kind != ItemKind::Weapon && it.kind != ItemKind::Accessory) return c.fail(where, "\"teaches\" belongs to a weapon or an accessory");
  const Json& list = e["teaches"];
  const char* shape = "\"teaches\" lists 1 to 8 entries such as { \"ability\": \"first_aid\", \"points\": 100 }";
  if (!list.isArray() || list.size() == 0 || (int)list.size() > kMaxItemTeaches) return c.fail(where, shape);
  for (size_t i = 0; i < list.size(); ++i) {
    const Json& t = list[i];
    const std::string at = where + ", \"teaches\" entry " + std::to_string(i + 1);
    if (!t.isObject()) return c.fail(at, "must be an object: { \"ability\": \"first_aid\", \"points\": 100 }");
    if (!c.onlyKeys(t, {"skill", "ability", "points"}, at)) return false;
    if (t.has("skill") == t.has("ability")) return c.fail(at, "give \"skill\" (a skill id) or \"ability\" (a menu ability id), one of them");
    TeachDef d;
    d.ability = t.has("ability");
    const char* key = d.ability ? "ability" : "skill";
    if (!c.str(t, key, at, d.refId, true)) return false;
    d.ref = d.ability ? db.findAbility(d.refId) : db.findSkill(d.refId);
    if (d.ref < 0) return c.fail(at, std::string("\"") + key + "\" names unknown " + key + " " + q(d.refId) + (d.ability ? " (data/abilities.json)" : " (data/skills.json)"));
    if (!d.ability && db.skills[(size_t)d.ref].hidden) return c.fail(at, "\"skill\" " + q(d.refId) + " is a condition's own skill; it cannot be taught");
    if (!t.has("points") || !LoadCtx::isWhole(t["points"]) || t["points"].asInt() < 1 || t["points"].asInt() > kMaxTeachPoints)
      return c.fail(at, "\"points\" must be a whole number from 1 to " + std::to_string(kMaxTeachPoints));
    d.points = t["points"].asInt();
    // One entry per id in the whole game: the same cost everywhere, and a skill and an ability may not share an id.
    int index = -1;
    for (size_t k = 0; k < db.teachEntries.size(); ++k) {
      const TeachDef& o = db.teachEntries[k];
      if (o.refId != d.refId) continue;
      if (o.ability != d.ability) return c.fail(at, q(d.refId) + " is taught as a skill and as an ability: give them different ids");
      if (o.points != d.points) {
        std::string other;  // the first item that named it
        for (const ItemDef& x : db.items)
          for (int ti : x.teaches)
            if (ti == (int)k && other.empty()) other = x.id;
        return c.fail(at, q(d.refId) + " costs " + std::to_string(d.points) + " points here but " + std::to_string(o.points) + " on item " + q(other) +
                              ": an entry has one cost");
      }
      index = (int)k;
    }
    if (index < 0) {
      index = (int)db.teachEntries.size();
      db.teachEntries.push_back(d);
    }
    for (int ti : it.teaches)
      if (ti == index) return c.fail(at, q(d.refId) + " is listed twice");
    it.teaches.push_back(index);
  }
  return true;
}

bool loadAbilityPointRules(const LoadCtx& c, const Json& rules, Database& db) {
  db.rules.abilityPoints = AbilityPointRules();
  if (!rules.has("abilityPoints")) return true;
  const Json* blk;
  if (!c.object(rules, "abilityPoints", "rules", blk)) return false;
  AbilityPointRules& r = db.rules.abilityPoints;
  r.declared = true;
  const std::string at = "rules.abilityPoints";
  if (!c.onlyKeys(*blk, {"name", "perBattle", "perAction", "perDefeat"}, at)) return false;
  if (!c.str(*blk, "name", at, r.name)) return false;
  if (r.name.empty() || r.name.size() > 8) return c.fail(at, "\"name\" must be 1 to 8 characters");
  return c.integerIn(*blk, "perBattle", at, 0, kMaxTeachPoints, r.perBattle) && c.integerIn(*blk, "perAction", at, 0, kMaxTeachPoints, r.perAction) &&
         c.integerIn(*blk, "perDefeat", at, 0, kMaxTeachPoints, r.perDefeat);
}

bool resolveTeaching(const std::string& root, Database& db, std::string& err) {
  const LoadCtx game(err, root + "/game.json");
  const LoadCtx items(err, root + "/data/items.json");
  const LoadCtx jobs(err, root + "/data/jobs.json");
  if (db.rules.abilityPoints.declared && db.teachEntries.empty())
    return game.fail("rules.abilityPoints", "no item \"teaches\" anything (data/items.json); the block does nothing without it");
  // An entry that a job teaches too belongs to that job (docs/JOBS.md "With jobs"): the kinds must agree.
  for (TeachDef& t : db.teachEntries) {
    t.jobEntry = -1;
    for (const JobLearnDef& d : db.jobEntries) {
      if (d.refId != t.refId) continue;
      if (d.ability != t.ability)
        return items.fail("item teaching " + q(t.refId), q(t.refId) + " is taught as " + (t.ability ? "an ability" : "a skill") + " by an item but as " +
                                                             (d.ability ? "an ability" : "a skill") + " by job " + q(db.jobs[(size_t)d.job].id) + ": give them different ids");
      t.jobEntry = d.flat;
    }
  }
  // "spendJp": false: equipment is the only way to learn, so every learn entry needs an item that teaches it.
  if (!db.rules.jobs.spendJp)
    for (const JobLearnDef& d : db.jobEntries) {
      bool taught = false;
      for (const TeachDef& t : db.teachEntries) taught = taught || t.jobEntry == d.flat;
      if (!taught) {
        const JobDef& j = db.jobs[(size_t)d.job];
        return jobs.fail("job " + q(j.id) + ", \"learn\" entry " + std::to_string(d.flat - j.firstFlat + 1),
                         q(d.refId) + " is learned only from equipment (\"spendJp\": false), but no item teaches it");
      }
    }
  return true;
}

}  // namespace ote
