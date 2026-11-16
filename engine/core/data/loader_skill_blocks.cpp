// The blocks of a skill (docs/SRPG.md "Skills": WHEN, CONDITIONS, CHANCE, EFFECTS, Aura): the whole format with
// every refusal, for all seven `when` points. Also the condition reader that menu abilities reuse (A1).
// Messages read `data/skills.json: skill "x": block 2: condition 1: ...` (LoadCtx adds the file name).
#include <algorithm>

#include "core/data/loader_srpg.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

// ---- when ----

const char* const kWhenNames[] = {"always", "turnStart", "combat", "strike", "struck", "defeat", "afterCombat"};
constexpr unsigned bit(SkillWhen w) { return 1u << (int)w; }
constexpr unsigned A = bit(SkillWhen::Always), T = bit(SkillWhen::TurnStart), C = bit(SkillWhen::Combat),
                   S = bit(SkillWhen::Strike), R = bit(SkillWhen::Struck), D = bit(SkillWhen::Defeat),
                   F = bit(SkillWhen::AfterCombat);

const char* whenName(SkillWhen w) { return kWhenNames[(int)w]; }

bool parseWhen(const std::string& s, SkillWhen& out) {
  for (int i = 0; i < 7; ++i)
    if (s == kWhenNames[i]) {
      out = (SkillWhen)i;
      return true;
    }
  return false;
}

std::string whenList(unsigned mask) {
  std::string out;
  for (int i = 0; i < 7; ++i)
    if (mask & (1u << i)) out += (out.empty() ? "" : ", ") + std::string(kWhenNames[i]);
  return out;
}

bool parseOp(const std::string& t, CmpOp& op) {
  if (t == "=" || t == "==") op = CmpOp::Eq;
  else if (t == "!=") op = CmpOp::Ne;
  else if (t == "<") op = CmpOp::Lt;
  else if (t == "<=") op = CmpOp::Le;
  else if (t == ">") op = CmpOp::Gt;
  else if (t == ">=") op = CmpOp::Ge;
  else return false;
  return true;
}

// A key list check: an object may only have these keys (a typo must not pass silently).
bool onlyKeys(const LoadCtx& c, const Json& o, std::initializer_list<const char*> keys, const std::string& where) {
  for (const Json::Member& m : o.members()) {
    bool known = false;
    for (const char* k : keys) known = known || m.first == k;
    if (known) continue;
    std::string list;
    for (const char* k : keys) list += (list.empty() ? "" : ", ") + q(k);
    return c.fail(where, "unknown key " + q(m.first) + " (expected " + list + ")");
  }
  return true;
}

// `key` of `o` as a whole number in lo..hi.
bool intIn(const LoadCtx& c, const Json& o, const char* key, const std::string& where, int lo, int hi, int& out,
           bool required = false) {
  if (!o.has(key)) {
    if (required) return c.fail(where, std::string("missing required ") + q(key));
    return true;
  }
  if (!LoadCtx::isWhole(o[key]) || o[key].asInt() < lo || o[key].asInt() > hi)
    return c.fail(where, q(key) + " must be a whole number from " + std::to_string(lo) + " to " + std::to_string(hi));
  out = o[key].asInt();
  return true;
}

bool readOp(const LoadCtx& c, const Json& o, const std::string& where, CmpOp& op) {
  std::string t;
  if (!c.str(o, "op", where, t)) return false;
  if (o.has("op") && !parseOp(t, op)) return c.fail(where, "\"op\" must be one of = != < <= > >=, not " + q(t));
  return true;
}

// ---- conditions ----

struct CondSpec {
  const char* key;
  SkillCond::Kind kind;
};
const CondSpec kConds[] = {{"hp", SkillCond::Kind::Hp},           {"near", SkillCond::Kind::Near},
                           {"weapon", SkillCond::Kind::Weapon},   {"terrain", SkillCond::Kind::Terrain},
                           {"structure", SkillCond::Kind::Structure}, {"unit", SkillCond::Kind::Unit},
                           {"carrying", SkillCond::Kind::Carrying}, {"round", SkillCond::Kind::Round},
                           {"initiating", SkillCond::Kind::Initiating}, {"distance", SkillCond::Kind::Distance},
                           {"formula", SkillCond::Kind::Formula}};

bool readCond(const LoadCtx& c, Database& db, const Json& e, const std::string& where, const SkillCondScope& scope,
              const std::string& label, SkillCond& out) {
  if (!e.isObject()) return c.fail(where, "must be an object such as { \"hp\": { \"op\": \"<=\", \"percent\": 50 } }");
  const CondSpec* spec = nullptr;
  for (const CondSpec& s : kConds)
    if (e.has(s.key)) {
      if (spec) return c.fail(where, std::string("has two conditions (") + spec->key + " and " + s.key + "); use one entry for each");
      spec = &s;
    }
  if (!spec) {
    std::string list;
    for (const CondSpec& s : kConds) list += (list.empty() ? "" : ", ") + std::string(s.key);
    return c.fail(where, "is not a condition; it needs one of " + list);
  }
  const std::string key = spec->key;
  const Json& v = e[spec->key];
  out = SkillCond();
  out.kind = spec->kind;

  // Which kinds may say "who": "foe", and which only make sense where there is a combat.
  const bool foeable = out.kind == SkillCond::Kind::Hp || out.kind == SkillCond::Kind::Weapon ||
                       out.kind == SkillCond::Kind::Terrain || out.kind == SkillCond::Kind::Structure ||
                       out.kind == SkillCond::Kind::Unit || out.kind == SkillCond::Kind::Carrying;
  if (e.has("who")) {
    std::string who;
    if (!c.str(e, "who", where, who)) return false;
    if (!foeable) return c.fail(where, q(key) + " cannot have \"who\"");
    if (who != "foe") return c.fail(where, "\"who\" must be \"foe\" (a condition is about the owner unless it says so)");
    if (!scope.foe) return c.fail(where, "\"who\": \"foe\" cannot be used " + std::string(scope.place));
    out.foe = true;
  }
  if ((out.kind == SkillCond::Kind::Initiating || out.kind == SkillCond::Kind::Distance) && !scope.foePoint)
    return c.fail(where, q(key) + " cannot be used " + scope.place + " (there is no combat)");
  if (out.kind == SkillCond::Kind::Formula && !scope.formula)
    return c.fail(where, "\"formula\" cannot be used " + std::string(scope.place));

  switch (out.kind) {
    case SkillCond::Kind::Hp:
      if (!v.isObject()) return c.fail(where, "\"hp\" must be { \"op\": \"<=\", \"percent\": 50 }");
      if (!onlyKeys(c, v, {"op", "percent"}, where + ": \"hp\"")) return false;
      if (!readOp(c, v, where, out.op) || !intIn(c, v, "percent", where, 0, 100, out.value, true)) return false;
      return onlyKeys(c, e, {"hp", "who"}, where);
    case SkillCond::Kind::Near: {
      if (!onlyKeys(c, e, {"near", "within", "op", "count", "match"}, where)) return false;
      if (!v.isString() || (v.asString() != "ally" && v.asString() != "enemy"))
        return c.fail(where, "\"near\" must be \"ally\" or \"enemy\"");
      out.enemy = v.asString() == "enemy";
      out.op = CmpOp::Ge;
      out.value = 1;
      return intIn(c, e, "within", where, 1, 10, out.within) && readOp(c, e, where, out.op) &&
             intIn(c, e, "count", where, 0, 1000, out.value) && c.str(e, "match", where, out.key);
    }
    case SkillCond::Kind::Weapon:
    case SkillCond::Kind::Terrain:
    case SkillCond::Kind::Unit:
      if (!onlyKeys(c, e, {spec->key, "who"}, where)) return false;
      if (!v.isString() || v.asString().empty()) return c.fail(where, q(key) + " must be an id or a " + (out.kind == SkillCond::Kind::Weapon ? "group" : "tag"));
      out.key = v.asString();
      return true;
    case SkillCond::Kind::Structure:
      if (!onlyKeys(c, e, {"structure", "who"}, where)) return false;
      if (v.isBool()) {
        out.flag = v.asBool();  // true = on any structure, false = on none
      } else if (v.isString() && !v.asString().empty()) {
        out.key = v.asString();
      } else {
        return c.fail(where, "\"structure\" must be true, false or a structure id or tag");
      }
      return true;
    case SkillCond::Kind::Carrying:
    case SkillCond::Kind::Initiating:
      if (!onlyKeys(c, e, {spec->key, "who"}, where)) return false;
      if (!v.isBool()) return c.fail(where, q(key) + " must be true or false");
      out.flag = v.asBool();
      return true;
    case SkillCond::Kind::Round:
    case SkillCond::Kind::Distance:
      if (!v.isObject()) return c.fail(where, q(key) + " must be { \"op\": \">=\", \"value\": 5 }");
      if (!onlyKeys(c, e, {spec->key}, where) || !onlyKeys(c, v, {"op", "value"}, where + ": " + q(key))) return false;
      return readOp(c, v, where, out.op) && intIn(c, v, "value", where, 0, 1000000, out.value, true);
    case SkillCond::Kind::Formula: {
      if (!onlyKeys(c, e, {"formula"}, where)) return false;
      if (!v.isString() || v.asString().empty()) return c.fail(where, "\"formula\" must be the text of a formula");
      out.expr = db.addExpr(v.asString(), label + ": \"formula\"");
      return true;
    }
  }
  return true;
}

// ---- effects ----

struct EffectSpec {
  const char* key;
  SkillEffect::Kind kind;
  unsigned whens;  // the `when` points it may be used in
};
using K = SkillEffect::Kind;
const EffectSpec kEffects[] = {
    {"stats", K::Stats, A | C},       {"growths", K::Growths, A},          {"maxMoveCost", K::MaxMoveCost, A},
    {"pass", K::Pass, A},             {"moveAgain", K::MoveAgain, A},      {"xpPercent", K::XpPercent, A},
    {"ability", K::Ability, A},       {"hit", K::Hit, C | S},              {"crit", K::Crit, C | S},
    {"damage", K::Damage, C | S},     {"avoid", K::Avoid, C | R},          {"critAvoid", K::CritAvoid, C | R},
    {"damageTaken", K::DamageTaken, C | R}, {"damagePercent", K::DamagePercent, C | S},
    {"damageTakenPercent", K::DamageTakenPercent, C | R}, {"strikes", K::Strikes, C},
    {"multiStrike", K::MultiStrike, C}, {"strikeFirst", K::StrikeFirst, C}, {"counterAnyRange", K::CounterAnyRange, C},
    {"noCounter", K::NoCounter, C},   {"drain", K::Drain, S},              {"strikeAgain", K::StrikeAgain, S},
    {"survive", K::Survive, R},       {"heal", K::Heal, T | D | F},        {"hurt", K::Hurt, T | F},
    {"actAgain", K::ActAgain, D | F}};

// An aura's effects go to other units: only plain numbers that a fighter can carry.
bool auraKind(K k) {
  return k == K::Stats || k == K::Hit || k == K::Avoid || k == K::Crit || k == K::CritAvoid || k == K::Damage ||
         k == K::DamageTaken;
}

// "to": "foe" is allowed on these (in a combat block only).
bool foeKind(K k) { return auraKind(k) || k == K::Strikes; }

bool isFlag(K k) {
  return k == K::MoveAgain || k == K::StrikeFirst || k == K::CounterAnyRange || k == K::NoCounter ||
         k == K::StrikeAgain || k == K::Survive || k == K::ActAgain;
}

struct Range {
  int lo, hi;
};
// Whole-number effects and their range.
bool numberRange(K k, Range& r) {
  switch (k) {
    case K::MaxMoveCost: r = {1, 100}; return true;
    case K::XpPercent: r = {1, 1000}; return true;
    case K::Hit: case K::Crit: case K::Damage: case K::Avoid: case K::CritAvoid: case K::DamageTaken: r = {-1000, 1000}; return true;
    case K::DamagePercent: case K::DamageTakenPercent: r = {0, 1000}; return true;
    case K::Strikes: r = {-7, 7}; return true;
    case K::MultiStrike: r = {2, 4}; return true;
    case K::Drain: r = {1, 100}; return true;
    case K::Hurt: r = {1, 1000000}; return true;
    default: return false;
  }
}

bool readStatMap(const LoadCtx& c, const Database& db, const Json& v, const char* key, const std::string& where,
                 std::vector<int>& out) {
  if (!v.isObject() || v.size() == 0) return c.fail(where, q(key) + " must name at least one stat: { \"str\": 2 }");
  out.assign(db.stats.size(), 0);
  for (const Json::Member& m : v.members()) {
    int s = db.findStat(m.first);
    if (s < 0) return c.fail(where, q(key) + " names unknown stat " + q(m.first));
    if (!LoadCtx::isWhole(m.second) || m.second.asInt() < -1000 || m.second.asInt() > 1000)
      return c.fail(where, q(key) + "." + q(m.first) + " must be a whole number from -1000 to 1000");
    out[(size_t)s] = m.second.asInt();
  }
  return true;
}

bool readEffect(const LoadCtx& c, const Database& db, const Json& e, const std::string& blockWhere, const std::string& where,
                SkillWhen when, bool aura, SkillEffect& out) {
  if (!e.isObject()) return c.fail(where, "must be an object such as { \"crit\": 25 }");
  const EffectSpec* spec = nullptr;
  for (const EffectSpec& s : kEffects)
    if (e.has(s.key)) {
      if (spec) return c.fail(where, std::string("has two effects (") + spec->key + " and " + s.key + "); use one entry for each");
      spec = &s;
    }
  if (!spec) {
    std::string list;
    for (const EffectSpec& s : kEffects) list += (list.empty() ? "" : ", ") + std::string(s.key);
    return c.fail(where, "is not an effect; it needs one of " + list);
  }
  const std::string key = spec->key;
  const Json& v = e[spec->key];
  if (!(spec->whens & bit(when)))
    return c.fail(blockWhere, q(key) + " cannot be used in " + (when == SkillWhen::Always ? "an " : "a ") + q(whenName(when)) +
                                  " block (it belongs to: " + whenList(spec->whens) + ")");
  if (aura && !auraKind(spec->kind))
    return c.fail(blockWhere, q(key) + " cannot be used in an aura block (an aura gives: stats, hit, avoid, crit, critAvoid, damage, damageTaken)");
  out = SkillEffect();
  out.kind = spec->kind;

  // "to" and "within": only where the table says so.
  const bool usesTo = !aura && (foeKind(spec->kind) || spec->kind == K::Heal || spec->kind == K::Hurt);
  if (e.has("to") && !usesTo) return c.fail(where, "\"to\" cannot be used with " + q(key) + (aura ? " in an aura block" : ""));
  if (e.has("to")) {
    std::string to;
    if (!c.str(e, "to", where, to)) return false;
    if (foeKind(spec->kind)) {
      if (to != "foe" && to != "self") return c.fail(where, "\"to\" must be \"foe\" (the default is the owner)");
      if (to == "foe" && when != SkillWhen::Combat) return c.fail(where, "\"to\": \"foe\" belongs to \"combat\" blocks");
      out.to = to == "foe" ? SkillEffect::To::Foe : SkillEffect::To::Self;
    } else if (spec->kind == K::Heal) {
      if (to != "self" && to != "allies") return c.fail(where, "\"to\" must be \"self\" or \"allies\"");
      out.to = to == "self" ? SkillEffect::To::Self : SkillEffect::To::Allies;
    } else {
      if (to != "foe" && to != "enemies") return c.fail(where, "\"to\" must be \"foe\" or \"enemies\"");
      out.to = to == "foe" ? SkillEffect::To::Foe : SkillEffect::To::Enemies;
    }
  } else if (spec->kind == K::Hurt) {
    out.to = when == SkillWhen::AfterCombat ? SkillEffect::To::Foe : SkillEffect::To::Enemies;
  }
  if (spec->kind == K::Hurt) {
    if (out.to == SkillEffect::To::Foe && when != SkillWhen::AfterCombat)
      return c.fail(where, "\"to\": \"foe\" belongs to \"afterCombat\" blocks");
    if (out.to == SkillEffect::To::Enemies && when != SkillWhen::TurnStart)
      return c.fail(where, "\"to\": \"enemies\" belongs to \"turnStart\" blocks");
  }
  const bool usesWithin = out.to == SkillEffect::To::Allies || out.to == SkillEffect::To::Enemies;
  if (e.has("within") && !usesWithin) return c.fail(where, "\"within\" belongs to \"to\": \"allies\" or \"enemies\"");
  if (usesWithin) {
    out.within = 1;
    if (!intIn(c, e, "within", where, 1, 10, out.within)) return false;
  }
  if (!onlyKeys(c, e, {spec->key, "to", "within"}, where)) return false;

  Range r;
  if (spec->kind == K::Stats || spec->kind == K::Growths) {
    if (!readStatMap(c, db, v, spec->key, where, out.stats)) return false;
    return true;
  }
  if (spec->kind == K::Pass) {
    if (!v.isString() || v.asString() != "enemies") return c.fail(where, "\"pass\" must be \"enemies\"");
    return true;
  }
  if (spec->kind == K::Ability) {
    if (!v.isString() || v.asString().empty()) return c.fail(where, "\"ability\" must be an ability id");
    out.abilityId = v.asString();
    return true;
  }
  if (isFlag(spec->kind)) {
    if (!v.isBool() || !v.asBool()) return c.fail(where, q(key) + " must be true (leave the effect out to turn it off)");
    return true;
  }
  if (spec->kind == K::Heal) {
    if (v.isObject()) {
      if (!onlyKeys(c, v, {"percent"}, where + ": \"heal\"")) return false;
      out.percent = true;
      return intIn(c, v, "percent", where, 1, 100, out.value, true);
    }
    if (!LoadCtx::isWhole(v) || v.asInt() < 1 || v.asInt() > 1000000)
      return c.fail(where, "\"heal\" must be a whole number of 1 or more, or { \"percent\": 20 }");
    out.value = v.asInt();
    return true;
  }
  if (numberRange(spec->kind, r)) {
    if (!LoadCtx::isWhole(v) || v.asInt() < r.lo || v.asInt() > r.hi)
      return c.fail(where, q(key) + " must be a whole number from " + std::to_string(r.lo) + " to " + std::to_string(r.hi));
    out.value = v.asInt();
  }
  return true;
}

bool isStat(const Database& db, const SkillEffect& e, int stat) {
  return stat >= 0 && (size_t)stat < e.stats.size() && e.stats[(size_t)stat] != 0 && (size_t)stat < db.stats.size();
}

}  // namespace

bool readSkillConds(const LoadCtx& c, Database& db, const Json& list, const std::string& where, const SkillCondScope& scope,
                    const std::string& labelPrefix, std::vector<SkillCond>& out) {
  if (!list.isArray()) return c.fail(where, "\"if\" must be a list of conditions");
  if ((int)list.size() > kMaxBlockConditions)
    return c.fail(where, "too many conditions (at most " + std::to_string(kMaxBlockConditions) + ")");
  for (size_t i = 0; i < list.size(); ++i) {
    SkillCond cond;
    const std::string at = where + ": condition " + std::to_string(i + 1);
    if (!readCond(c, db, list[i], at, scope, labelPrefix + at, cond)) return false;
    out.push_back(cond);
  }
  return true;
}

bool readSkillBlock(const LoadCtx& c, Database& db, const Json& e, const std::string& where, const std::string& labelPrefix,
                    SkillBlock& out) {
  if (!e.isObject()) return c.fail(where, "must be an object such as { \"when\": \"combat\", \"do\": [ { \"crit\": 25 } ] }");
  if (!onlyKeys(c, e, {"when", "if", "chance", "aura", "do"}, where)) return false;
  std::string when;
  if (!c.str(e, "when", where, when, true)) return false;
  if (!parseWhen(when, out.when))
    return c.fail(where, "\"when\" must be one of " + whenList(0x7f) + ", not " + q(when));
  const SkillWhen w = out.when;

  if (e.has("aura")) {
    const Json& a = e["aura"];
    if (w != SkillWhen::Combat) return c.fail(where, "\"aura\" cannot be used in " + std::string(w == SkillWhen::Always ? "an " : "a ") + q(whenName(w)) + " block (it belongs to: combat)");
    if (!a.isObject()) return c.fail(where, "\"aura\" must be { \"to\": \"allies\", \"within\": 2 }");
    if (!onlyKeys(c, a, {"to", "within"}, where + ": \"aura\"")) return false;
    std::string to;
    if (!c.str(a, "to", where + ": \"aura\"", to, true)) return false;
    if (to != "allies" && to != "enemies") return c.fail(where, "\"aura\".\"to\" must be \"allies\" or \"enemies\"");
    out.aura = true;
    out.auraEnemies = to == "enemies";
    out.auraWithin = 1;
    if (!intIn(c, a, "within", where + ": \"aura\"", 1, 10, out.auraWithin)) return false;
  }

  if (e.has("if")) {
    SkillCondScope scope;
    if (out.aura) scope = {false, false, true, "in an aura block"};
    else if (w == SkillWhen::Always) scope = {false, false, false, "in an \"always\" block"};
    else if (w == SkillWhen::TurnStart) scope = {false, false, true, "in a \"turnStart\" block"};
    else scope = {true, true, true, "here"};
    if (!readSkillConds(c, db, e["if"], where, scope, labelPrefix, out.conds)) return false;
  }

  if (e.has("chance")) {
    if (w == SkillWhen::Always || w == SkillWhen::Combat)
      return c.fail(where, "\"chance\" cannot be used in " + std::string(w == SkillWhen::Always ? "an " : "a ") + q(whenName(w)) +
                               " block (it must be the same every time it is asked)");
    const Json& ch = e["chance"];
    out.hasChance = true;
    if (ch.isString() && !ch.asString().empty()) out.chanceExpr = db.addExpr(ch.asString(), labelPrefix + where + ": \"chance\"");
    else if (LoadCtx::isWhole(ch)) out.chance = ch.asInt();
    else return c.fail(where, "\"chance\" must be a percent or a formula such as \"a.skl * 2\"");
  }

  const Json* list;
  if (!c.array(e, "do", where, list, true)) return false;
  if (list->size() == 0) return c.fail(where, "\"do\" needs at least one effect");
  if ((int)list->size() > kMaxBlockEffects) return c.fail(where, "too many effects (at most " + std::to_string(kMaxBlockEffects) + ")");
  for (size_t i = 0; i < list->size(); ++i) {
    SkillEffect fx;
    const std::string at = where + ": effect " + std::to_string(i + 1);
    if (!readEffect(c, db, (*list)[i], where, at, w, out.aura, fx)) return false;
    // Rules that need the block as a whole.
    if (fx.kind == K::Stats && w == SkillWhen::Always && isStat(db, fx, db.rules.statHp) && !out.conds.empty())
      return c.fail(where, "a bonus to " + q(db.stats[(size_t)db.rules.statHp].id) + " cannot have a condition");
    if (fx.kind == K::Stats && w == SkillWhen::Combat &&
        (isStat(db, fx, db.rules.statHp) || isStat(db, fx, db.rules.statMove)))
      return c.fail(at, "a combat bonus cannot change the hit points or the movement stat");
    if ((fx.kind == K::Growths || fx.kind == K::Ability) && !out.conds.empty())
      return c.fail(at, q(fx.kind == K::Growths ? "growths" : "ability") + " cannot have a condition");
    out.effects.push_back(fx);
  }
  return true;
}

// ---- references of conditions (after everything is loaded) ----

bool resolveSkillConds(const LoadCtx& c, const Database& db, const std::string& where, const std::vector<SkillCond>& conds) {
  for (size_t i = 0; i < conds.size(); ++i) {
    const SkillCond& k = conds[i];
    const std::string at = where + ": condition " + std::to_string(i + 1);
    bool found = true;
    const char* what = "";
    switch (k.kind) {
      case SkillCond::Kind::Weapon:
        found = false;
        for (const WeaponDef& w : db.weapons) found = found || w.id == k.key || w.group == k.key;
        what = "\"weapon\" names no weapon id or group \"";
        break;
      case SkillCond::Kind::Terrain:
        found = false;
        for (const TerrainDef& t : db.terrains)
          found = found || t.id == k.key || std::find(t.tags.begin(), t.tags.end(), k.key) != t.tags.end();
        what = "\"terrain\" names no terrain id or tag \"";
        break;
      case SkillCond::Kind::Structure:
        found = k.key.empty();
        for (const StructureDef& s : db.structures)
          found = found || s.id == k.key || std::find(s.tags.begin(), s.tags.end(), k.key) != s.tags.end();
        what = "\"structure\" names no structure id or tag \"";
        break;
      case SkillCond::Kind::Unit:
      case SkillCond::Kind::Near:
        found = k.key.empty();
        for (const UnitTypeDef& t : db.unitTypes) found = found || unitTypeMatchesKey(t, k.key);
        what = k.kind == SkillCond::Kind::Unit ? "\"unit\" names no unit type id or tag \"" : "\"match\" names no unit type id or tag \"";
        break;
      default: break;
    }
    if (!found) return c.fail(at, what + k.key + "\"");
  }
  return true;
}

}  // namespace ote
