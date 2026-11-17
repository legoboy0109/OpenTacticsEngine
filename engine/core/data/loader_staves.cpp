// Loader side of staves (docs/SRPG.md section 8): the keys of an item of kind "staff".
#include "core/data/loader_staves.h"

#include "core/data/loader_conditions.h"
#include "core/data/loader_ranks.h"
#include "core/data/loader_srpg.h"
#include "core/data/loader_util.h"
#include "core/data/loader_weapon_locks.h"

namespace ote {

namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

bool onlyKeys(const LoadCtx& c, const Json& o, const std::string& where) {
  static const char* const keys[] = {"id", "name", "description", "kind", "uses", "price", "group", "rank", "rankXp", "wieldedBy", "wieldedByTypes",
                                     "props", "range", "target", "area", "effects", "hit", "xp", "if", "repairable"};
  for (const Json::Member& m : o.members()) {
    bool known = false;
    for (const char* k : keys) known = known || m.first == k;
    if (!known) return c.fail(where, "unknown key " + q(m.first) + " of a staff");
  }
  return true;
}

// "range": 3 | [1, 3] | "a.mag / 2" (a formula for the maximum).
bool readRange(const LoadCtx& c, Database& db, const Json& e, const std::string& where, StaffDef& d) {
  if (!e.has("range")) return true;
  const Json& r = e["range"];
  const std::string bad = "\"range\" must be a whole number from 1 to " + std::to_string(kMaxStaffRange) +
                          ", a list [min, max] or a formula (text)";
  if (d.target == StaffTarget::Self) return c.fail(where, "\"range\" cannot be used with \"target\": \"self\"");
  if (r.isString()) {
    d.rangeExpr = db.addExpr(r.asString(), "data/items.json: " + where + ": \"range\"");
  } else if (LoadCtx::isWhole(r)) {
    if (r.asInt() < 1 || r.asInt() > kMaxStaffRange) return c.fail(where, bad);
    d.maxRange = r.asInt();
  } else if (r.isArray()) {
    if (r.size() != 2 || !LoadCtx::isWhole(r[0]) || !LoadCtx::isWhole(r[1]) || r[0].asInt() < 1 || r[1].asInt() < r[0].asInt() ||
        r[1].asInt() > kMaxStaffRange)
      return c.fail(where, bad + "; a list is whole numbers, min not above max");
    d.minRange = r[0].asInt();
    d.maxRange = r[1].asInt();
  } else {
    return c.fail(where, bad);
  }
  return true;
}

bool readTargetAndArea(const LoadCtx& c, const Json& e, const std::string& where, StaffDef& d) {
  std::string target = "ally";
  if (!c.str(e, "target", where, target)) return false;
  if (target == "ally") d.target = StaffTarget::Ally;
  else if (target == "self") d.target = StaffTarget::Self;
  else if (target == "team") d.target = StaffTarget::Team;
  else if (target == "foe") d.target = StaffTarget::Foe;
  else if (target == "any") d.target = StaffTarget::Any;
  else return c.fail(where, "\"target\" must be \"ally\", \"self\", \"team\", \"foe\" or \"any\", not " + q(target));
  if (!e.has("area")) return true;
  const Json& a = e["area"];
  if (a.isString() && a.asString() == "single") d.area = StaffArea::Single;
  else if (a.isString() && a.asString() == "all") d.area = StaffArea::All;
  else if (a.isObject()) {
    for (const Json::Member& m : a.members())
      if (m.first != "radius") return c.fail(where, "\"area\": unknown key " + q(m.first) + " (expected \"radius\")");
    if (!a.has("radius") || !LoadCtx::isWhole(a["radius"]) || a["radius"].asInt() < 1 || a["radius"].asInt() > kMaxStaffRadius)
      return c.fail(where, "\"area\": \"radius\" must be a whole number from 1 to " + std::to_string(kMaxStaffRadius));
    d.area = StaffArea::Radius;
    d.radius = a["radius"].asInt();
  } else {
    return c.fail(where, "\"area\" must be \"single\", \"all\" or { \"radius\": n }");
  }
  if (d.target == StaffTarget::Self && d.area != StaffArea::Single)
    return c.fail(where, "\"area\" has no meaning with \"target\": \"self\"");
  return true;
}

// "range" of a warp: the same three forms as the staff's own (a whole number, [min, max], a formula for the maximum).
bool readWarpRange(const LoadCtx& c, Database& db, const Json& v, const std::string& where, const std::string& at, StaffEffect& out) {
  const std::string bad = at + ": \"warp\": \"range\" must be a whole number from 1 to " + std::to_string(kMaxStaffRange) +
                          ", a list [min, max] or a formula (text)";
  if (!v.isObject()) return c.fail(where, at + ": \"warp\" must be { \"range\": n } (how far from the user the destination may be)");
  for (const Json::Member& m : v.members())
    if (m.first != "range") return c.fail(where, at + ": \"warp\": unknown key " + q(m.first) + " (expected \"range\")");
  if (!v.has("range")) return c.fail(where, at + ": \"warp\" needs \"range\" (how far from the user the destination may be)");
  const Json& r = v["range"];
  out.kind = StaffEffect::Kind::Warp;
  if (r.isString()) {
    out.warpExpr = db.addExpr(r.asString(), "data/items.json: " + where + ": " + at + ": \"warp\": \"range\"");
  } else if (LoadCtx::isWhole(r)) {
    if (r.asInt() < 1 || r.asInt() > kMaxStaffRange) return c.fail(where, bad);
    out.warpMax = r.asInt();
  } else if (r.isArray()) {
    if (r.size() != 2 || !LoadCtx::isWhole(r[0]) || !LoadCtx::isWhole(r[1]) || r[0].asInt() < 1 || r[1].asInt() < r[0].asInt() ||
        r[1].asInt() > kMaxStaffRange)
      return c.fail(where, bad + "; a list is whole numbers, min not above max");
    out.warpMin = r[0].asInt();
    out.warpMax = r[1].asInt();
  } else {
    return c.fail(where, bad);
  }
  return true;
}

bool readHeal(const LoadCtx& c, Database& db, const Json& h, const std::string& where, const std::string& at, StaffEffect& out) {
  if (h.isString() && h.asString() == "full") {
    out.amount = StaffEffect::Amount::Full;
  } else if (h.isString()) {
    out.amount = StaffEffect::Amount::Formula;
    out.expr = db.addExpr(h.asString(), "data/items.json: " + where + ": " + at + ": \"heal\"");
  } else if (LoadCtx::isWhole(h)) {
    if (h.asInt() < 1 || h.asInt() > 1000000) return c.fail(where, at + ": \"heal\" must be from 1 to 1000000");
    out.value = h.asInt();
  } else if (h.isObject()) {
    for (const Json::Member& m : h.members())
      if (m.first != "percent") return c.fail(where, at + ": \"heal\": unknown key " + q(m.first) + " (expected \"percent\")");
    if (!h.has("percent") || !LoadCtx::isWhole(h["percent"]) || h["percent"].asInt() < 1 || h["percent"].asInt() > 100)
      return c.fail(where, at + ": \"heal\": \"percent\" must be a whole number from 1 to 100");
    out.amount = StaffEffect::Amount::Percent;
    out.value = h["percent"].asInt();
  } else {
    return c.fail(where, at + ": \"heal\" must be a whole number, \"full\", { \"percent\": n } or a formula (text)");
  }
  return true;
}

bool readRepair(const LoadCtx& c, const Json& v, const std::string& where, const std::string& at, StaffEffect& out) {
  out.kind = StaffEffect::Kind::Repair;
  if (v.isString() && v.asString() == "full") {
    out.amount = StaffEffect::Amount::Full;
  } else if (LoadCtx::isWhole(v)) {
    if (v.asInt() < 1 || v.asInt() > 999) return c.fail(where, at + ": \"repair\" must be a whole number of uses from 1 to 999");
    out.value = v.asInt();
  } else {
    return c.fail(where, at + ": \"repair\" must be \"full\" or a whole number of uses from 1 to 999");
  }
  return true;
}

bool readLight(const LoadCtx& c, const Json& v, const std::string& where, const std::string& at, StaffEffect& out) {
  out.kind = StaffEffect::Kind::Light;
  if (!v.isObject()) return c.fail(where, at + ": \"light\" must be { \"radius\": n, \"turns\": n }");
  for (const Json::Member& m : v.members())
    if (m.first != "radius" && m.first != "turns")
      return c.fail(where, at + ": \"light\": unknown key " + q(m.first) + " (expected \"radius\" or \"turns\")");
  if (!v.has("radius") || !LoadCtx::isWhole(v["radius"]) || v["radius"].asInt() < 1 || v["radius"].asInt() > kMaxStaffRadius)
    return c.fail(where, at + ": \"light\": \"radius\" must be a whole number from 1 to " + std::to_string(kMaxStaffRadius));
  out.radius = v["radius"].asInt();
  if (v.has("turns")) {
    if (!LoadCtx::isWhole(v["turns"]) || v["turns"].asInt() < 1 || v["turns"].asInt() > kMaxStaffLightTurns)
      return c.fail(where, at + ": \"light\": \"turns\" must be a whole number from 1 to " + std::to_string(kMaxStaffLightTurns) +
                               " (leave it out for 1)");
    out.turns = v["turns"].asInt();
  }
  return true;
}

// What a staff's effects may be put together with (docs/SRPG.md "Staves: stage two, as built"), refused in plain words.
bool checkCombination(const LoadCtx& c, const Json& e, const std::string& where, const StaffDef& d) {
  int moves = 0, repairs = 0, lights = 0, inflicts = 0, cures = 0;
  for (const StaffEffect& fx : d.effects) {
    inflicts += fx.kind == StaffEffect::Kind::Inflict;
    cures += fx.kind == StaffEffect::Kind::Cure;
    moves += fx.movesUnit();
    repairs += fx.kind == StaffEffect::Kind::Repair;
    lights += fx.kind == StaffEffect::Kind::Light;
  }
  if (moves > 1) return c.fail(where, "a staff can have only one of \"warp\", \"pull\" and \"swap\" (each one decides where the picked unit ends up)");
  if (repairs > 1) return c.fail(where, "a staff can have only one \"repair\"");
  if (lights > 1) return c.fail(where, "a staff can have only one \"light\"");
  if (inflicts > 1) return c.fail(where, "a staff can have only one \"inflict\" (make two staves, or two conditions that replace each other)");
  if (cures > 1) return c.fail(where, "a staff can have only one \"cure\" (it takes a list of conditions)");
  if (repairs && moves)
    return c.fail(where, "\"repair\" cannot be put together with \"warp\", \"pull\" or \"swap\" (each one asks the player for a second pick)");
  if (lights) {
    if (d.effects.size() != 1) return c.fail(where, "\"light\" picks a square, not a unit, so it cannot be put together with another effect");
    if (e.has("target")) return c.fail(where, "\"light\" picks a square, so the staff has no \"target\"");
    if (e.has("area")) return c.fail(where, "\"light\" picks a square, so the staff has no \"area\" (the light has its own \"radius\")");
    if (e.has("hit")) return c.fail(where, "\"light\" picks a square, so there is no unit to roll \"hit\" for");
    if (e.has("if")) return c.fail(where, "\"light\" picks a square, so there is no unit for \"if\" to judge");
    return true;
  }
  if (moves || repairs) {
    const char* what = repairs ? "repair" : d.has(StaffEffect::Kind::Warp) ? "warp" : d.has(StaffEffect::Kind::Pull) ? "pull" : "swap";
    if (d.area != StaffArea::Single)
      return c.fail(where, std::string("\"") + what + "\" works on the one unit picked, so \"area\" must be \"single\"");
    if ((d.has(StaffEffect::Kind::Pull) || d.has(StaffEffect::Kind::Swap)) && d.target == StaffTarget::Self)
      return c.fail(where, std::string("\"") + what + "\" needs another unit, so \"target\" cannot be \"self\"");
  }
  return true;
}

// "effects": [ { "heal": ... } | { "warp": { "range": n } } | { "pull": true } | { "swap": true } | { "repair": "full" | n }
//              | { "light": { "radius": n, "turns": n } } ], 1 to 4 entries.
bool readEffects(const LoadCtx& c, Database& db, const Json& e, const std::string& where, StaffDef& d) {
  const Json* list;
  if (!c.array(e, "effects", where, list, true)) return false;
  if (list->size() < 1 || (int)list->size() > kMaxStaffEffects)
    return c.fail(where, "\"effects\" needs 1 to " + std::to_string(kMaxStaffEffects) + " effects");
  for (size_t i = 0; i < list->size(); ++i) {
    const Json& fx = (*list)[i];
    const std::string at = "\"effects\" entry " + std::to_string(i + 1);
    if (!fx.isObject() || fx.size() != 1) return c.fail(where, at + " must be an object with exactly one effect such as { \"heal\": 10 }");
    const std::string kind = fx.members()[0].first;
    const Json& v = fx[kind.c_str()];
    StaffEffect out;
    if (kind == "heal") {
      if (d.target == StaffTarget::Foe) return c.fail(where, "\"heal\" cannot target \"foe\"");
      if (!readHeal(c, db, v, where, at, out)) return false;
    } else if (kind == "warp") {
      if (!readWarpRange(c, db, v, where, at, out)) return false;
    } else if (kind == "pull" || kind == "swap") {
      if (!(v.isBool() && v.asBool())) return c.fail(where, at + ": \"" + kind + "\" must be true");
      out.kind = kind == "pull" ? StaffEffect::Kind::Pull : StaffEffect::Kind::Swap;
    } else if (kind == "repair") {
      if (!readRepair(c, v, where, at, out)) return false;
    } else if (kind == "light") {
      if (!readLight(c, v, where, at, out)) return false;
    } else if (kind == "inflict") {  // stage 3, docs/SRPG.md section 9
      InflictSpec spec;
      if (!readInflictSpec(c, db, v, where, at, false, spec)) return false;
      out.kind = StaffEffect::Kind::Inflict;
      out.conds = {spec.condition};
      out.turns = spec.turns;  // 0 = the condition's own (the default of 1 is a light's)
    } else if (kind == "cure") {
      out.kind = StaffEffect::Kind::Cure;
      if (!readCureList(c, db, v, where, at, out.conds, out.cureAll)) return false;
    } else {
      return c.fail(where, at + ": unknown effect " + q(kind) + " (a staff can: heal, warp, pull, swap, repair, light, inflict, cure)");
    }
    d.effects.push_back(out);
  }
  return checkCombination(c, e, where, d);
}

bool readHitXpConds(const LoadCtx& c, Database& db, const Json& e, const std::string& where, StaffDef& d) {
  if (e.has("hit")) {
    const Json& h = e["hit"];
    if (h.isString()) d.hitExpr = db.addExpr(h.asString(), "data/items.json: " + where + ": \"hit\"");
    else if (LoadCtx::isWhole(h) && h.asInt() >= 0 && h.asInt() <= 100) d.hitExpr = db.addExpr(std::to_string(h.asInt()), "data/items.json: " + where + ": \"hit\"");
    else return c.fail(where, "\"hit\" must be a percent (a whole number from 0 to 100) or a formula (text)");
  }
  if (e.has("xp")) {
    const Json& x = e["xp"];
    if (!db.rules.progression) return c.fail(where, "\"xp\" needs experience: set \"rules\": { \"progression\": { \"enabled\": true } } in game.json");
    if (x.isString()) d.xpExpr = db.addExpr(x.asString(), "data/items.json: " + where + ": \"xp\"");
    else if (LoadCtx::isWhole(x) && x.asInt() >= 0 && x.asInt() <= kMaxStaffXp) d.xp = x.asInt();
    else return c.fail(where, "\"xp\" must be a whole number from 0 to " + std::to_string(kMaxStaffXp) + " or a formula (text)");
  }
  if (e.has("if")) {
    // The conditions of an ability: "who": "foe" is the unit hit; there is no combat (no "initiating" / "distance").
    SkillCondScope scope = {true, false, true, "in a staff"};
    if (!readSkillConds(c, db, e["if"], where, scope, "data/items.json: ", d.conds)) return false;
  }
  return true;
}

}  // namespace

bool readStaffItem(const LoadCtx& c, Database& db, const Json& e, const std::string& where, ItemDef& it) {
  if (!onlyKeys(c, e, where)) return false;
  StaffDef d;
  d.gear.id = it.id;
  d.gear.name = it.name;
  if (!e.has("group")) return c.fail(where, "\"group\" is required for a staff (what a unit type lists in \"wields\")");
  if (!c.str(e, "group", where, d.gear.group)) return false;
  if (d.gear.group.empty()) return c.fail(where, "\"group\" must not be empty");
  if (!c.props(e, "props", where, d.gear.props) || !readWeaponRank(c, db, e, where, d.gear) ||
      !readWeaponWielders(c, e, where, d.gear))
    return false;
  if (!readTargetAndArea(c, e, where, d) || !readRange(c, db, e, where, d) || !readEffects(c, db, e, where, d) ||
      !readHitXpConds(c, db, e, where, d))
    return false;
  it.staff = (int)db.staves.size();
  db.staves.push_back(d);
  return true;
}

void addStaffRankGroups(Database& db) {
  if (!db.ranks.on()) return;
  for (StaffDef& s : db.staves) {
    int g = db.ranks.findGroup(s.gear.group);
    if (g < 0) {
      g = (int)db.ranks.groups.size();
      db.ranks.groups.push_back(s.gear.group);
    }
    s.gear.rankGroup = g;
  }
}

bool resolveStaffRefs(const std::string& root, Database& db, std::string& err) {
  LoadCtx c(err, root + "/data/items.json");
  for (const ItemDef& it : db.items) {
    if (it.kind != ItemKind::Staff) continue;
    StaffDef& s = db.staves[(size_t)it.staff];
    const std::string where = "item \"" + it.id + "\"";
    if (!resolveGearWielders(c, db, where, s.gear) || !resolveSkillConds(c, db, where, s.conds)) return false;
  }
  return true;
}

}  // namespace ote
