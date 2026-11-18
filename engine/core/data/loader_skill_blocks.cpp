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

const char* const kWhenNames[] = {"always", "turnStart", "combat", "strike", "struck", "defeat", "afterCombat", "staffUse"};
constexpr int kWhenCount = 8;
constexpr unsigned bit(SkillWhen w) { return 1u << (int)w; }
constexpr unsigned A = bit(SkillWhen::Always), T = bit(SkillWhen::TurnStart), C = bit(SkillWhen::Combat),
                   S = bit(SkillWhen::Strike), R = bit(SkillWhen::Struck), D = bit(SkillWhen::Defeat),
                   F = bit(SkillWhen::AfterCombat), SU = bit(SkillWhen::StaffUse);

const char* whenName(SkillWhen w) { return kWhenNames[(int)w]; }

bool parseWhen(const std::string& s, SkillWhen& out) {
  for (int i = 0; i < kWhenCount; ++i)
    if (s == kWhenNames[i]) {
      out = (SkillWhen)i;
      return true;
    }
  return false;
}

std::string whenList(unsigned mask) {
  std::string out;
  for (int i = 0; i < kWhenCount; ++i)
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
                           {"formula", SkillCond::Kind::Formula}, {"condition", SkillCond::Kind::Condition},
                           {"weaponKind", SkillCond::Kind::WeaponKind}, {"turn", SkillCond::Kind::Turn},
                           {"counterattack", SkillCond::Kind::Counterattack}, {"carries", SkillCond::Kind::Carries},
                           {"inventory", SkillCond::Kind::Inventory}, {"strike", SkillCond::Kind::Strike},
                           {"marked", SkillCond::Kind::Marked}};

// "turn": { "odd": true } | { "even": true } | { "from": 3, "to": 6 } (either alone) | { "every": 3, "offset": 1 }: the battle's turn number
// (the same number as "round"). Every k with offset j = the turns whose number divided by k leaves j (default 0).
bool readTurnCond(const LoadCtx& c, const Json& e, const Json& v, const std::string& where, SkillCond& out) {
  if (!onlyKeys(c, e, {"turn"}, where)) return false;
  if (!v.isObject()) return c.fail(where, "\"turn\" must be { \"odd\": true }, { \"even\": true }, { \"from\": 3, \"to\": 6 } or { \"every\": 3, \"offset\": 1 }");
  const std::string at = where + ": \"turn\"";
  if (!onlyKeys(c, v, {"odd", "even", "from", "to", "every", "offset"}, at)) return false;
  const int modes = (int)v.has("odd") + (int)v.has("even") + (int)(v.has("from") || v.has("to")) + (int)(v.has("every") || v.has("offset"));
  if (modes != 1) return c.fail(where, "\"turn\" needs exactly one of \"odd\", \"even\", \"from\" / \"to\" or \"every\" (with \"offset\")");
  if (v.has("odd") || v.has("even")) {
    const char* k = v.has("odd") ? "odd" : "even";
    if (!v[k].isBool() || !v[k].asBool()) return c.fail(where, q(k) + " must be true");
    out.turnMode = v.has("odd") ? SkillCond::TurnMode::Odd : SkillCond::TurnMode::Even;
    return true;
  }
  if (v.has("from") || v.has("to")) {
    out.turnMode = SkillCond::TurnMode::Range;
    out.turnA = 1;
    out.turnB = 1000000;
    if (!intIn(c, v, "from", at, 1, 1000000, out.turnA) || !intIn(c, v, "to", at, 1, 1000000, out.turnB)) return false;
    if (out.turnA > out.turnB) return c.fail(where, "\"turn\": \"from\" is later than \"to\"");
    return true;
  }
  if (!v.has("every")) return c.fail(where, "\"offset\" needs \"every\"");
  out.turnMode = SkillCond::TurnMode::Every;
  out.turnA = 0;
  out.turnB = 0;
  if (!intIn(c, v, "every", at, 2, 1000, out.turnA, true) || !intIn(c, v, "offset", at, 0, 999, out.turnB)) return false;
  if (out.turnB >= out.turnA) return c.fail(where, "\"turn\": \"offset\" must be less than \"every\"");
  return true;
}

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
                       out.kind == SkillCond::Kind::Unit || out.kind == SkillCond::Kind::Carrying ||
                       out.kind == SkillCond::Kind::Condition || out.kind == SkillCond::Kind::WeaponKind ||
                       out.kind == SkillCond::Kind::Carries || out.kind == SkillCond::Kind::Inventory || out.kind == SkillCond::Kind::Marked;
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
    case SkillCond::Kind::Condition:
      if (!onlyKeys(c, e, {"condition", "who"}, where)) return false;
      if (!v.isString() || v.asString().empty()) return c.fail(where, "\"condition\" must be a condition id (data/conditions.json)");
      out.key = v.asString();
      return true;
    case SkillCond::Kind::Turn: return readTurnCond(c, e, v, where, out);
    case SkillCond::Kind::Counterattack:
      if (!onlyKeys(c, e, {"counterattack"}, where)) return false;
      if (!v.isBool()) return c.fail(where, "\"counterattack\" must be true (the foe answers this fight) or false (it does not)");
      if (!scope.strike)
        return c.fail(where, "\"counterattack\" can only be asked in \"strike\" and \"struck\" blocks (a \"combat\" block is judged while the answer is still being worked out)");
      out.flag = v.asBool();
      return true;
    case SkillCond::Kind::Strike: {  // "first" | "followUp" | n | { "op": ">=", "value": 2 }: which strike of the fight (docs/SRPG.md)
      const std::string how = "\"strike\" must be \"first\", \"followUp\", the number of the strike (1 or more) or { \"op\": \">=\", \"value\": 2 }";
      if (!onlyKeys(c, e, {"strike"}, where)) return false;
      if (!scope.strike)
        return c.fail(where, "\"strike\" can only be asked in \"strike\" and \"struck\" blocks (it is the number of the strike being made)");
      out.op = CmpOp::Eq;
      out.value = 1;
      if (v.isString() && v.asString() == "first") out.value = 1;
      else if (v.isString() && v.asString() == "followUp") { out.op = CmpOp::Ge; out.value = 2; }
      else if (LoadCtx::isWhole(v) && v.asInt() >= 1 && v.asInt() <= 1000) out.value = v.asInt();
      else if (v.isObject()) {
        if (!onlyKeys(c, v, {"op", "value"}, where + ": \"strike\"") || !readOp(c, v, where, out.op) || !intIn(c, v, "value", where, 1, 1000, out.value, true)) return false;
      } else return c.fail(where, how);
      db.strikeNoUsed = true;
      return true;
    }
    case SkillCond::Kind::Marked:  // "name", with "is": false for the opposite (docs/SRPG.md "Linking two strike blocks")
      if (!onlyKeys(c, e, {"marked", "is", "who"}, where)) return false;
      if (!scope.marks)
        return c.fail(where, "\"marked\" can only be asked in \"strike\", \"struck\", \"defeat\" and \"afterCombat\" blocks (marks live for one fight)");
      if (!v.isString() || v.asString().empty() || v.asString().size() > 40)
        return c.fail(where, "\"marked\" must be the name of a mark (1 to 40 characters), the name a \"mark\" effect sets");
      out.key = v.asString();
      db.marksUsed = true;
      return c.boolean(e, "is", where, out.flag);
    case SkillCond::Kind::Carries:
      if (!onlyKeys(c, e, {"carries", "who"}, where)) return false;
      if (!v.isString() || v.asString().empty()) return c.fail(where, "\"carries\" must be an item id");
      out.key = v.asString();
      return true;
    case SkillCond::Kind::Inventory:
      if (!onlyKeys(c, e, {"inventory", "who"}, where)) return false;
      out.value = -1;
      if (v.isString()) {
        if (v.asString() != "full" && v.asString() != "notFull") return c.fail(where, "\"inventory\" must be \"full\", \"notFull\" or { \"op\": \">=\", \"value\": 3 }");
        out.flag = v.asString() == "full";
        return true;
      }
      if (!v.isObject() || !onlyKeys(c, v, {"op", "value"}, where + ": \"inventory\"")) return c.fail(where, "\"inventory\" must be \"full\", \"notFull\" or { \"op\": \">=\", \"value\": 3 }");
      return readOp(c, v, where, out.op) && intIn(c, v, "value", where, 0, 1000, out.value, true);
    case SkillCond::Kind::WeaponKind:
      if (!onlyKeys(c, e, {"weaponKind", "who"}, where)) return false;
      if (!v.isString() || v.asString().empty()) return c.fail(where, "\"weaponKind\" must be a damage kind, the word a weapon's \"damageKind\" says");
      out.key = v.asString();
      return true;
    case SkillCond::Kind::Terrain: {  // an id or tag, or a list of them: any one will do
      if (!onlyKeys(c, e, {"terrain", "who"}, where)) return false;
      if (v.isString() && !v.asString().empty()) {
        out.keys.push_back(v.asString());
      } else if (v.isArray() && v.size() > 0) {
        for (size_t i = 0; i < v.size(); ++i) {
          if (!v[i].isString() || v[i].asString().empty()) return c.fail(where, "\"terrain\" must be an id or a tag, or a list of them");
          out.keys.push_back(v[i].asString());
        }
      } else {
        return c.fail(where, "\"terrain\" must be an id or a tag, or a list of them");
      }
      out.key = out.keys[0];
      return true;
    }
    case SkillCond::Kind::Weapon:
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
    {"survive", K::Survive, R},       {"heal", K::Heal, T | D | F | SU},        {"hurt", K::Hurt, T | F},
    {"actAgain", K::ActAgain, D | F},
    // Conditions (docs/SRPG.md section 9).
    {"cannotAct", K::CannotAct, A},   {"cannotMove", K::CannotMove, A},    {"forbid", K::Forbid, A},
    {"berserk", K::Berserk, A},       {"immune", K::Immune, A},            {"inflict", K::Inflict, S | R},
    // General blocks (docs/SRPG.md "More skill blocks").
    {"followUp", K::FollowUp, C},     {"followShift", K::FollowShift, C},  {"trianglePercent", K::TrianglePercent, C},
    {"weaponXpPercent", K::WeaponXpPercent, A}, {"noCarryPenalty", K::NoCarryPenalty, A}, {"cancelEffective", K::CancelEffective, A},
    {"cure", K::Cure, T | D | F | SU},
    // Second set (docs/SRPG.md "Skill and ability blocks, second set").
    {"nullifySkills", K::NullifySkills, C}, {"staffRange", K::StaffRange, A}, {"carryBonus", K::CarryBonus, A}, {"vision", K::Vision, A},
    // Third set (docs/SRPG.md "Skill blocks, third set").
    {"recoil", K::Recoil, S},
    // Linking two strike blocks (docs/SRPG.md): the block that takes effect leaves a mark for the owner's other blocks of this fight.
    {"mark", K::Mark, S | R}};

// An aura's effects go to other units: only plain numbers that a fighter can carry.
bool auraKind(K k) {
  return k == K::Stats || k == K::Hit || k == K::Avoid || k == K::Crit || k == K::CritAvoid || k == K::Damage ||
         k == K::DamageTaken;
}

// "to": "foe" is allowed on these (in a combat block only).
bool foeKind(K k) { return auraKind(k) || k == K::Strikes || k == K::FollowUp || k == K::FollowShift || k == K::TrianglePercent; }

// Combat numbers that may be written as a formula (text) instead of a whole number.
bool formulaKind(K k) {
  return k == K::Hit || k == K::Crit || k == K::Damage || k == K::Avoid || k == K::CritAvoid || k == K::DamageTaken ||
         k == K::DamagePercent || k == K::DamageTakenPercent;
}

bool isFlag(K k) {
  return k == K::NoCarryPenalty || k == K::CancelEffective || k == K::StrikeFirst || k == K::CounterAnyRange || k == K::NoCounter ||
         k == K::StrikeAgain || k == K::Survive || k == K::ActAgain || k == K::CannotMove || k == K::Berserk;
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
    case K::FollowShift: r = {-100, 100}; return true;
    case K::TrianglePercent: r = {0, 1000}; return true;
    case K::WeaponXpPercent: r = {1, 1000}; return true;
    default: return false;
  }
}

// `exprs` (combat blocks only): a stat may be a formula (text); its slot in `exprs` then holds the expression index.
bool readStatMap(const LoadCtx& c, Database& db, const Json& v, const char* key, const std::string& where,
                 std::vector<int>& out, std::vector<int>* exprs = nullptr, const std::string& label = "") {
  if (!v.isObject() || v.size() == 0) return c.fail(where, q(key) + " must name at least one stat: { \"str\": 2 }");
  out.assign(db.stats.size(), 0);
  if (exprs) exprs->assign(db.stats.size(), -1);
  for (const Json::Member& m : v.members()) {
    int s = db.findStat(m.first);
    if (s < 0) return c.fail(where, q(key) + " names unknown stat " + q(m.first));
    if (exprs && m.second.isString()) {
      if (m.second.asString().empty()) return c.fail(where, q(key) + "." + q(m.first) + " formula must not be empty");
      (*exprs)[(size_t)s] = db.addExpr(m.second.asString(), label + ": " + q(key) + "." + q(m.first));
      continue;
    }    if (!LoadCtx::isWhole(m.second) || m.second.asInt() < -1000 || m.second.asInt() > 1000)
      return c.fail(where, q(key) + "." + q(m.first) + " must be a whole number from -1000 to 1000");
    out[(size_t)s] = m.second.asInt();
  }
  return true;
}

// Conditions (docs/SRPG.md section 9): the five effects that only make sense on a temporary state, and the readers of the
// forms the new effects take. `where` / `at` name the place for the message.
bool conditionOnly(K k) { return k == K::CannotAct || k == K::CannotMove || k == K::Forbid || k == K::Berserk; }

bool readBoolFlags(const LoadCtx& c, const Json& v, const std::string& where, const char* key, std::initializer_list<const char*> names,
                   std::initializer_list<bool*> outs) {
  if (v.isBool()) {
    if (!v.asBool()) return c.fail(where, q(key) + " must be true (leave the effect out to turn it off)");
    return true;
  }
  if (!v.isObject()) return c.fail(where, q(key) + " must be true or an object with the keys of its options");
  if (!onlyKeys(c, v, names, where + ": " + q(key))) return false;
  auto o = outs.begin();
  for (const char* n : names) {
    if (v.has(n) && !v[n].isBool()) return c.fail(where, q(key) + "." + q(n) + " must be true or false");
    if (v.has(n)) **o = v[n].asBool();
    ++o;
  }
  return true;
}

bool readNameList(const LoadCtx& c, const Json& v, const std::string& where, const std::string& what, std::vector<std::string>& out) {
  if (!v.isArray()) return c.fail(where, what + " must be a list of ids");
  for (size_t i = 0; i < v.size(); ++i) {
    if (!v[i].isString() || v[i].asString().empty()) return c.fail(where, what + " entry " + std::to_string(i + 1) + " must be an id (text)");
    out.push_back(v[i].asString());
  }
  return true;
}

// "forbid": { "groups": ["staff", "tome"], "kinds": ["staff", "usable"] }
bool readForbid(const LoadCtx& c, const Json& v, const std::string& where, SkillEffect& out) {
  if (!v.isObject()) return c.fail(where, "\"forbid\" must be { \"groups\": [\"magic\"], \"kinds\": [\"staff\"] }");
  if (!onlyKeys(c, v, {"groups", "kinds"}, where + ": \"forbid\"")) return false;
  if (v.has("groups") && !readNameList(c, v["groups"], where, "\"forbid\".\"groups\"", out.names)) return false;
  if (v.has("kinds") && !readNameList(c, v["kinds"], where, "\"forbid\".\"kinds\"", out.kinds)) return false;
  for (const std::string& k : out.kinds)
    if (k != "staff" && k != "usable" && k != "weapon")
      return c.fail(where, "\"forbid\".\"kinds\" has " + q(k) + "; the kinds are \"staff\", \"usable\" and \"weapon\"");
  if (out.names.empty() && out.kinds.empty())
    return c.fail(where, "\"forbid\" must name at least one weapon or staff group (\"groups\") or item kind (\"kinds\")");
  return true;
}

// The amount of a heal or hurt: a whole number, { "percent": 10 }, or a formula; hurt also { "amount" | "percent" | "formula", "canKill": true }.
bool readAmount(const LoadCtx& c, Database& db, const Json& v, const std::string& where, const std::string& labelPrefix, const char* key,
                bool hurt, SkillEffect& out) {
  const std::string k = q(key);
  const Json* amount = &v;
  if (v.isObject()) {
    if (!onlyKeys(c, v, hurt ? std::initializer_list<const char*>{"amount", "percent", "formula", "canKill", "lethal"}
                             : std::initializer_list<const char*>{"amount", "percent", "formula"}, where + ": " + k)) return false;
    const int given = (int)v.has("amount") + (int)v.has("percent") + (int)v.has("formula");
    if (given != 1) return c.fail(where, k + " needs exactly one of \"amount\", \"percent\" or \"formula\"");
    if (v.has("canKill") && v.has("lethal")) return c.fail(where, k + ": give \"canKill\" or \"lethal\" (the same thing), not both");
    for (const char* kw : {"canKill", "lethal"})  // "lethal" is another name for "canKill" (docs/SRPG.md section 9); the default is false either way
      if (v.has(kw)) {
        if (!v[kw].isBool()) return c.fail(where, k + ": \"" + kw + "\" must be true or false");
        out.kill = v[kw].asBool();
      }
    if (v.has("percent")) {
      if (!LoadCtx::isWhole(v["percent"]) || v["percent"].asInt() < 1 || v["percent"].asInt() > 100)
        return c.fail(where, k + ": \"percent\" must be a whole number from 1 to 100");
      out.percent = true;
      out.value = v["percent"].asInt();
      return true;
    }
    amount = v.has("amount") ? &v["amount"] : &v["formula"];
  }
  if (hurt && amount->isArray()) {  // [lo, hi]: a roll of the battle's random numbers each time (docs/SRPG.md section 9 "Damage that varies")
    if (amount->size() != 2 || !LoadCtx::isWhole((*amount)[0]) || !LoadCtx::isWhole((*amount)[1]) || (*amount)[0].asInt() < 1 ||
        (*amount)[1].asInt() > 1000000 || (*amount)[1].asInt() <= (*amount)[0].asInt())
      return c.fail(where, k + ": a range must be [lowest, highest], whole numbers from 1 up with the highest above the lowest (a single number needs no range)");
    out.value = (*amount)[0].asInt();
    out.valueMax = (*amount)[1].asInt();
    return true;
  }
  if (amount->isString()) {
    if (amount->asString().empty()) return c.fail(where, k + " formula must not be empty");
    out.expr = db.addExpr(amount->asString(), labelPrefix + where + ": " + k);
    return true;
  }
  if (!LoadCtx::isWhole(*amount) || amount->asInt() < 1 || amount->asInt() > 1000000)
    return c.fail(where, k + " must be a whole number of 1 or more, { \"percent\": 20 } or a formula (text)");
  out.value = amount->asInt();
  return true;
}

bool readEffect(const LoadCtx& c, Database& db, const Json& e, const std::string& blockWhere, const std::string& where,
                SkillWhen when, bool aura, bool forCondition, const std::string& labelPrefix, SkillEffect& out) {
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
  if (conditionOnly(spec->kind) && !forCondition)
    return c.fail(blockWhere, q(key) + " belongs to the blocks of a condition (data/conditions.json), not to a skill");
  if (forCondition && (spec->kind == K::NullifySkills || spec->kind == K::StaffRange || spec->kind == K::CarryBonus))
    return c.fail(blockWhere, q(key) + " belongs to skills (data/skills.json), not to a condition");
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
      if (to != "foe" && to != "enemies" && to != "self") return c.fail(where, "\"to\" must be \"foe\", \"enemies\" or \"self\"");
      out.to = to == "foe" ? SkillEffect::To::Foe : to == "self" ? SkillEffect::To::Self : SkillEffect::To::Enemies;
    }
  } else if (spec->kind == K::Hurt) {
    out.to = when == SkillWhen::AfterCombat ? SkillEffect::To::Foe : SkillEffect::To::Enemies;
  }
  if (spec->kind == K::Hurt) {
    if (out.to == SkillEffect::To::Self && when != SkillWhen::TurnStart)
      return c.fail(where, "\"to\": \"self\" belongs to \"turnStart\" blocks");
    if (out.to == SkillEffect::To::Foe && when != SkillWhen::AfterCombat)
      return c.fail(where, "\"to\": \"foe\" belongs to \"afterCombat\" blocks");
    if (out.to == SkillEffect::To::Enemies && when != SkillWhen::TurnStart)
      return c.fail(where, "\"to\": \"enemies\" belongs to \"turnStart\" blocks");
  }
  if (when == SkillWhen::StaffUse && out.to == SkillEffect::To::Allies)
    return c.fail(where, "\"to\": \"allies\" is not allowed in a \"staffUse\" block (it heals the unit that used the staff)");
  const bool usesWithin = out.to == SkillEffect::To::Allies || out.to == SkillEffect::To::Enemies;
  if (e.has("within") && !usesWithin) return c.fail(where, "\"within\" belongs to \"to\": \"allies\" or \"enemies\"");
  if (usesWithin) {
    out.within = 1;
    if (!intIn(c, e, "within", where, 1, 10, out.within)) return false;
  }
  if (!onlyKeys(c, e, {spec->key, "to", "within"}, where)) return false;

  Range r;
  if (spec->kind == K::Stats || spec->kind == K::Growths) {
    // A combat bonus to stats may be a formula; "always" blocks may not (a stat that asks itself would never settle).
    const bool formulas = spec->kind == K::Stats && when != SkillWhen::Always;
    if (!readStatMap(c, db, v, spec->key, where, out.stats, formulas ? &out.statExprs : nullptr, labelPrefix + where)) return false;
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
  if (spec->kind == K::CannotAct) return readBoolFlags(c, v, where, "cannotAct", {"counter", "dodge"}, {&out.counter, &out.dodge});
  if (spec->kind == K::Forbid) return readForbid(c, v, where, out);
  if (spec->kind == K::Immune) {
    if (!readNameList(c, v, where, "\"immune\"", out.names)) return false;
    if (out.names.empty()) return c.fail(where, "\"immune\" must name at least one condition");
    return true;
  }
  if (spec->kind == K::Cure) {  // "all", or a list of condition ids
    if (v.isString() && v.asString() == "all") { out.cureAll = true; return true; }
    if (!readNameList(c, v, where, "\"cure\"", out.names) || out.names.empty())
      return c.fail(where, "\"cure\" must be \"all\" or a list of condition ids");
    return true;
  }
  if (spec->kind == K::Inflict) {
    if (!v.isObject() || !onlyKeys(c, v, {"condition", "turns"}, where + ": \"inflict\"") || !v.has("condition") || !v["condition"].isString())
      return c.fail(where, "\"inflict\" must be { \"condition\": \"poison\" } (optionally with \"turns\")");
    out.names.push_back(v["condition"].asString());
    return intIn(c, v, "turns", where, 1, kMaxConditionTurns, out.turns);
  }
  if (spec->kind == K::NullifySkills) {  // true | [skill ids] | { "tag": "word" }
    if (v.isBool() && v.asBool()) { out.value = 0; return true; }
    if (v.isArray() && v.size() > 0) {
      out.value = 1;
      return readNameList(c, v, where, "\"nullifySkills\"", out.names) && !out.names.empty();
    }
    if (v.isObject() && v.size() == 1 && v.has("tag") && v["tag"].isString() && !v["tag"].asString().empty()) {
      out.value = 2;
      out.names.push_back(v["tag"].asString());
      return true;
    }
    return c.fail(where, "\"nullifySkills\" must be true (every skill of the foe), a list of skill ids, or { \"tag\": \"word\" } (the skills with that tag)");
  }
  if (spec->kind == K::StaffRange) {  // N | { "amount": N, "group": "staff group" }
    const std::string how = "\"staffRange\" must be a whole number from 1 to 10 (squares added to the reach of staves), or { \"amount\": 2, \"group\": \"heal\" }";
    if (LoadCtx::isWhole(v)) {
      if (v.asInt() < 1 || v.asInt() > 10) return c.fail(where, how);
      out.value = v.asInt();
      return true;
    }
    if (v.isObject() && !onlyKeys(c, v, {"amount", "group"}, where + ": \"staffRange\"")) return false;
    if (!v.isObject() || !v.has("amount") || !LoadCtx::isWhole(v["amount"]) || v["amount"].asInt() < 1 || v["amount"].asInt() > 10)
      return c.fail(where, how);
    out.value = v["amount"].asInt();
    if (v.has("group")) {
      if (!v["group"].isString() || v["group"].asString().empty()) return c.fail(where, how);
      out.names.push_back(v["group"].asString());
    }
    return true;
  }
  if (spec->kind == K::Recoil) {  // N | "formula" | { "amount": N | "formula" (default "dealt"), "instead": true }
    const std::string how = "\"recoil\" must be a whole number of 1 or more (HP the wielder loses), a formula such as \"dealt / 2\" (`dealt` = the strike's damage), or { \"amount\": \"dealt\", \"instead\": true }";
    const Json* amount = &v;
    bool hasAmount = true;
    if (v.isObject()) {
      if (!onlyKeys(c, v, {"amount", "instead"}, where + ": \"recoil\"")) return false;
      if (v.has("instead")) {
        if (!v["instead"].isBool()) return c.fail(where, "\"recoil\".\"instead\" must be true or false");
        out.instead = v["instead"].asBool();
      }
      hasAmount = v.has("amount");
      amount = hasAmount ? &v["amount"] : &v;
    }
    if (!hasAmount) {
      out.expr = db.addExpr("dealt", labelPrefix + where + ": \"recoil\"");
    } else if (amount->isString() && !amount->asString().empty()) {
      out.expr = db.addExpr(amount->asString(), labelPrefix + where + ": \"recoil\"");
    } else if (LoadCtx::isWhole(*amount) && amount->asInt() >= 1 && amount->asInt() <= 1000000) {
      out.value = amount->asInt();
    } else {
      return c.fail(where, how);
    }
    return true;
  }
  if (spec->kind == K::Mark) {
    if (!v.isString() || v.asString().empty() || v.asString().size() > 40)
      return c.fail(where, "\"mark\" must be the name of a mark (1 to 40 characters), which a \"marked\" condition of another block can ask");
    out.names.push_back(v.asString());
    db.marksUsed = true;
    return true;
  }
  if (spec->kind == K::Vision) {
    if (!LoadCtx::isWhole(v) || v.asInt() < -kMaxVisionBonus || v.asInt() > kMaxVisionBonus || v.asInt() == 0)
      return c.fail(where, "\"vision\" must be a whole number from -10 to 10, not 0 (added to the unit's vision in fog of war)");
    out.value = v.asInt();
    return true;
  }
  if (spec->kind == K::CarryBonus) {
    if (!LoadCtx::isWhole(v) || v.asInt() < -100 || v.asInt() > 100) return c.fail(where, "\"carryBonus\" must be a whole number from -100 to 100 (the canCarry formula reads it as a.carryBonus)");
    out.value = v.asInt();
    return true;
  }
  if (spec->kind == K::FollowUp) {
    if (!v.isString() || (v.asString() != "force" && v.asString() != "forbid"))
      return c.fail(where, "\"followUp\" must be \"force\" (the unit always makes a follow-up strike) or \"forbid\" (it never does)");
    out.value = v.asString() == "force" ? 1 : -1;
    return true;
  }
  if (spec->kind == K::MoveAgain) {  // true, or { "notAfterAttack": true }: no second move after a fight
    if (v.isBool() && v.asBool()) return true;
    if (!v.isObject() || !onlyKeys(c, v, {"notAfterAttack"}, where + ": \"moveAgain\"") || !v.has("notAfterAttack") || !v["notAfterAttack"].isBool())
      return c.fail(where, "\"moveAgain\" must be true or { \"notAfterAttack\": true }");
    out.value = v["notAfterAttack"].asBool() ? 1 : 0;
    return true;
  }
  if (spec->kind == K::CancelEffective && v.isArray()) {  // a list of the effectiveness tags it cancels (the keys of weapons' "versus")
    if (!readNameList(c, v, where, "\"cancelEffective\"", out.names) || out.names.empty())
      return c.fail(where, "\"cancelEffective\" must be true (every effectiveness) or a list of effectiveness tags such as [\"flier\"]");
    return true;
  }
  if (isFlag(spec->kind)) {
    if (!v.isBool() || !v.asBool()) return c.fail(where, q(key) + " must be true (leave the effect out to turn it off)" + (spec->kind == K::CancelEffective ? ", or a list of tags such as [\"flier\"]" : ""));
    return true;
  }
  if (spec->kind == K::Heal || spec->kind == K::Hurt) return readAmount(c, db, v, where, labelPrefix, spec->key, spec->kind == K::Hurt, out);
  if (formulaKind(spec->kind) && v.isString()) {
    if (v.asString().empty()) return c.fail(where, q(key) + " formula must not be empty");
    out.expr = db.addExpr(v.asString(), labelPrefix + where + ": " + q(key));
    return true;
  }
  if (numberRange(spec->kind, r)) {
    if (!LoadCtx::isWhole(v) || v.asInt() < r.lo || v.asInt() > r.hi)
      return c.fail(where, q(key) + " must be a whole number from " + std::to_string(r.lo) + " to " + std::to_string(r.hi) +
                               (formulaKind(spec->kind) ? ", or a formula (text)" : ""));
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
                    SkillBlock& out, bool forCondition) {
  if (!e.isObject()) return c.fail(where, "must be an object such as { \"when\": \"combat\", \"do\": [ { \"crit\": 25 } ] }");
  if (!onlyKeys(c, e, {"when", "if", "chance", "aura", "do"}, where)) return false;
  std::string when;
  if (!c.str(e, "when", where, when, true)) return false;
  if (!parseWhen(when, out.when))
    return c.fail(where, "\"when\" must be one of " + whenList((1u << kWhenCount) - 1) + ", not " + q(when));
  const SkillWhen w = out.when;

  if (e.has("aura")) {
    const Json& a = e["aura"];
    if (w != SkillWhen::Combat) return c.fail(where, "\"aura\" cannot be used in " + std::string(w == SkillWhen::Always ? "an " : "a ") + q(whenName(w)) + " block (it belongs to: combat)");
    if (!a.isObject()) return c.fail(where, "\"aura\" must be { \"to\": \"allies\", \"within\": 2 }");
    if (!onlyKeys(c, a, {"to", "within", "match", "weapon", "weaponKind", "test"}, where + ": \"aura\"")) return false;
    // The receiver's weapon in this fight (a weapon id or group), its damage kind, or a formula test (a = the owner, b = the receiver).
    if (!c.str(a, "weapon", where + ": \"aura\"", out.auraWeapon) || !c.str(a, "weaponKind", where + ": \"aura\"", out.auraWeaponKind)) return false;
    if (a.has("weapon") && out.auraWeapon.empty()) return c.fail(where, "\"aura\".\"weapon\" must be a weapon id or group (leave it out for any weapon)");
    if (a.has("weaponKind") && out.auraWeaponKind.empty()) return c.fail(where, "\"aura\".\"weaponKind\" must be a damage kind, the word a weapon's \"damageKind\" says");
    if (a.has("test")) {
      if (!a["test"].isString() || a["test"].asString().empty())
        return c.fail(where, "\"aura\".\"test\" must be the text of a formula (a = the owner, b = the unit that would receive it)");
      out.auraTest = db.addExpr(a["test"].asString(), labelPrefix + where + ": \"aura\".\"test\"");
    }
    if (!c.str(a, "match", where + ": \"aura\"", out.auraMatch)) return false;
    if (a.has("match") && out.auraMatch.empty()) return c.fail(where, "\"aura\".\"match\" must be a unit type id or tag (leave it out for every unit)");
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
    else if (w == SkillWhen::StaffUse) scope = {false, false, true, "in a \"staffUse\" block"};
    else {
      scope = {true, true, true, "here"};
      scope.strike = w == SkillWhen::Strike || w == SkillWhen::Struck;
      scope.marks = scope.strike || w == SkillWhen::Defeat || w == SkillWhen::AfterCombat;
    }
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
    if (!readEffect(c, db, (*list)[i], where, at, w, out.aura, forCondition, labelPrefix, fx)) return false;
    // Rules that need the block as a whole.
    if (forCondition && fx.kind == K::Stats && isStat(db, fx, db.rules.statHp))
      return c.fail(at, "a condition cannot change the hit points stat (use \"hurt\" and \"heal\" for damage and healing over time)");
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
        for (const StaffDef& st : db.staves) found = found || st.gear.group == k.key;  // a "staffUse" block tests the staff's group
        what = "\"weapon\" names no weapon id or group \"";
        break;
      case SkillCond::Kind::Terrain:
        for (const std::string& key : k.keys) {
          bool one = false;
          for (const TerrainDef& t : db.terrains)
            one = one || t.id == key || std::find(t.tags.begin(), t.tags.end(), key) != t.tags.end();
          if (!one) {
            found = false;
            return c.fail(at, "\"terrain\" names no terrain id or tag \"" + key + "\"");
          }
        }
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
        for (const CharacterDef& ch : db.characters) found = found || std::find(ch.tags.begin(), ch.tags.end(), k.key) != ch.tags.end();
        what = k.kind == SkillCond::Kind::Unit ? "\"unit\" names no unit type id or tag \"" : "\"match\" names no unit type id or tag \"";
        break;
      case SkillCond::Kind::Carries:
        found = db.findItem(k.key) >= 0;
        what = "\"carries\" names unknown item \"";
        break;
      case SkillCond::Kind::WeaponKind:
        found = false;
        for (const WeaponDef& w : db.weapons) found = found || w.damageKind == k.key;
        what = "\"weaponKind\" names no weapon damage kind \"";
        break;
      case SkillCond::Kind::Condition:
        found = db.findCondition(k.key) >= 0;
        what = "\"condition\" names unknown condition \"";
        break;
      default: break;
    }
    if (!found)
      return c.fail(at, what + k.key + "\"" + (k.kind == SkillCond::Kind::Condition ? " (data/conditions.json)" : ""));
  }
  return true;
}

}  // namespace ote
