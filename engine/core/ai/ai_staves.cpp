// How the computer opponent uses staves (docs/SRPG.md section 8 "AI"). Stateless like the rest of the AI: every call looks at
// the squares the unit may stand on and the staves it carries and keeps the best use if it beats the order already chosen.
//
// Stage 1 knows only healing. A use is worth the HP it would really restore, as a share of the target's maximum, times the
// target's value (what attacks are worth, ai_score.cpp: production cost, default 1000), times the hit chance, summed over the
// units it hits. A leader whose death ends the game counts double. A unit above half HP that would get less than half of what
// the staff nominally gives is not worth a use (the staff has limited uses). Only a unit that would change is a legal target
// (Battle::staffTargets), so a use is never wasted on an unhurt unit.
//
// Stage 3 (conditions, docs/SRPG.md section 9): a staff whose effects are only heal, inflict and cure is used. A cure is worth a share
// (kConditionShare) of the target's value for each ally it would free of something, an inflict the same share for each foe it would
// really afflict (not immune, not afflicted already; the forecast only lists such units), both times the hit chance, a leader counting
// double as for a heal. Only a unit that would change is ever a target, so an unlimited staff cannot be used twice for nothing.
//
// The per-unit options of docs/AI.md "Healers that serve retreating allies" and section 10 (a StaffPolicy, filled from the unit's behaviour):
//  - "heals": "retreating": a staff with a heal effect (or a pull: a rescue staff) is used only when every unit it would help is an ally in the
//    retreat state, below full HP, and not the user;
//  - "staffPick": "lastPlaced": a heal, cure or afflicting staff goes to the unit placed last (the highest unit id) among its legal targets, which
//    for an afflicting staff are hostile units with no condition at all that the staff can hit, and for a cure any ally but the user;
//  - "inflictOnlyOn": "casters": a staff whose condition stops staves is used only on a unit that carries something the condition would stop.
// lastPlacedKeys finds the unit placed last once per kind of staff for the whole turn, considerStaves then allows only the uses that serve it. Other
// staves are as before. staffFirst (ai_staff_first.cpp) asks the same questions through staffUseAllowed.
//
// Stage 2 (warp, swap, repair, light): the computer IGNORES any staff that has an effect other than a heal (a pull only for "heals": "retreating").
// Those effects need a second pick (a square, an item) or change the map for the whole side, and judging them takes a plan the stateless AI
// does not make; ignoring them can never crash or stall it (docs/SRPG.md section 8).
#include <algorithm>

#include "core/ai/ai_internal.h"

namespace ote {
namespace ai {

namespace {

constexpr double kAttackBase = 200;       // as an attack's (ai_order.cpp): any worthwhile use outranks plain advancing
constexpr double kMoveCostWeight = 0.5;   // prefer the shorter trip between equal uses
constexpr double kLeaderHeal = 2.0;       // a leader that decides the game is healed first
constexpr double kConditionShare = 0.25;  // what freeing an ally of a condition, or afflicting a foe, is worth of the unit's value

double healValue(const Battle& b, const StaffForecast& f) {
  double value = 0;
  for (const StaffHit& h : f.hits) {
    const Unit& t = b.units[(size_t)h.unit];
    const int max = std::max(1, b.maxHp(t));
    if (h.amount <= 0) continue;
    if (t.hp * 2 > max && h.amount * 2 < h.nominal) continue;  // a scratch: keep the use for someone who needs it
    double v = h.amount / (double)max * unitValue(b, t) * (h.hit / 100.0);
    if (t.leader && b.db->rules.defeatLeaderDead) v *= kLeaderHeal;
    value += v;
  }
  return value;
}

// Curing allies and afflicting foes (not allies: the AI cannot tell a blessing from a curse).
double conditionValue(const Battle& b, const Unit& user, const StaffForecast& f) {
  double value = 0;
  for (const StaffHit& h : f.hits) {
    const Unit& t = b.units[(size_t)h.unit];
    double v = kConditionShare * unitValue(b, t) * (h.hit / 100.0);
    if (t.leader && b.db->rules.defeatLeaderDead) v *= kLeaderHeal;
    if (!h.cures.empty() && !b.isEnemy(user, t)) value += v;
    if (h.inflict >= 0 && b.isEnemy(user, t)) value += v;
  }
  return value;
}

// A pull of an ally that is not also healed (only a unit with "heals": "retreating" uses a pull): a share of the ally's value, as a cure is.
double pullValue(const Battle& b, const StaffForecast& f) {
  double value = 0;
  for (const StaffHit& h : f.hits) {
    if (h.moveTo == kNoCell || h.amount > 0) continue;
    const Unit& t = b.units[(size_t)h.unit];
    double v = kConditionShare * unitValue(b, t) * (h.hit / 100.0);
    if (t.leader && b.db->rules.defeatLeaderDead) v *= kLeaderHeal;
    value += v;
  }
  return value;
}

// The staves the computer uses at all: heal, inflict and cure effects; a pull too for a unit that heals only retreating allies (a rescue staff).
bool usable(const StaffDef& def, const StaffPolicy& pol) {
  for (const StaffEffect& fx : def.effects)
    if (!(fx.kind == StaffEffect::Kind::Heal || fx.kind == StaffEffect::Kind::Inflict || fx.kind == StaffEffect::Kind::Cure ||
          (pol.healsRetreating && fx.kind == StaffEffect::Kind::Pull)))
      return false;
  return true;
}

enum Kind { kHeal = 0, kInflict = 1, kCure = 2 };

// Which unit placed last a staff is chosen for: heal (a pull counts with it under "heals": "retreating"), then afflicting, then cure.
Kind kindOf(const StaffDef& def, const StaffPolicy& pol) {
  if (def.has(StaffEffect::Kind::Heal) || (pol.healsRetreating && def.has(StaffEffect::Kind::Pull))) return kHeal;
  return def.has(StaffEffect::Kind::Inflict) ? kInflict : kCure;
}

// Does the staff go to the unit placed last? ("heals": "retreating" always did; "staffPick": "lastPlaced" says so for every kind.)
bool picksLast(const StaffDef& def, const StaffPolicy& pol) { return pol.lastPlaced || (pol.healsRetreating && kindOf(def, pol) == kHeal); }

// Does the condition stop staves (it forbids the item kind "staff", or the group of a staff of the game)? Only such a condition is "silence-like".
bool stopsStaves(const Database& db, int condition) {
  for (const SkillBlock& blk : db.skills[(size_t)db.conditions[(size_t)condition].skill].blocks) {
    if (blk.when != SkillWhen::Always) continue;
    for (const SkillEffect& e : blk.effects) {
      if (e.kind != SkillEffect::Kind::Forbid) continue;
      if (std::find(e.kinds.begin(), e.kinds.end(), "staff") != e.kinds.end()) return true;
      for (const StaffDef& s : db.staves)
        if (std::find(e.names.begin(), e.names.end(), s.gear.group) != e.names.end()) return true;
    }
  }
  return false;
}

// Does the unit carry something the condition would stop it using: a staff it can use now, or a weapon it can use now, of a forbidden group / kind?
bool wouldLoseToCondition(const Battle& b, const Unit& t, int condition) {
  const Database& db = *b.db;
  std::vector<const SkillEffect*> forbids;
  for (const SkillBlock& blk : db.skills[(size_t)db.conditions[(size_t)condition].skill].blocks)
    if (blk.when == SkillWhen::Always)
      for (const SkillEffect& e : blk.effects)
        if (e.kind == SkillEffect::Kind::Forbid) forbids.push_back(&e);
  auto has = [](const std::vector<std::string>& v, const std::string& x) { return std::find(v.begin(), v.end(), x) != v.end(); };
  for (const ItemStack& st : t.items) {  // staves are always items
    const ItemDef& it = db.items[(size_t)st.item];
    if (it.kind != ItemKind::Staff || !b.canWieldStaff(t, st.item)) continue;
    const std::string& group = db.staves[(size_t)it.staff].gear.group;
    for (const SkillEffect* e : forbids)
      if (has(e->kinds, "staff") || has(e->names, group)) return true;
  }
  std::vector<int> weapons;  // the weapons it can use now (its type's fixed ones, or the wieldable ones it carries)
  b.weaponsOf(t, weapons);
  for (int w : weapons)
    for (const SkillEffect* e : forbids)
      if (has(e->kinds, "weapon") || has(e->names, db.weapons[(size_t)w].group)) return true;
  return false;
}

// Do the unit's options allow this use? Every unit it would hit has to pass: a heal under "heals": "retreating" needs a retreating, hurt ally that is not
// the user; an afflicting staff needs, under "lastPlaced", a hostile unit with no condition at all that the staff can hit, and, under "casters" for a
// condition that stops staves, a unit that would lose something to it; a cure needs, under "lastPlaced", an ally that is not the user. `key` = the highest unit
// id it serves (-1 = none: a use that only cures with a heal staff is not a heal).
bool allowedUse(const Battle& b, const Unit& user, const StaffDef& def, const StaffForecast& f, const StaffPolicy& pol, int& key) {
  key = -1;
  const Kind kind = kindOf(def, pol);
  const int cond = kind == kInflict ? def.find(StaffEffect::Kind::Inflict)->conds[0] : -1;
  const bool casters = pol.castersOnly && cond >= 0 && stopsStaves(*b.db, cond);
  for (const StaffHit& h : f.hits) {
    const Unit& t = b.units[(size_t)h.unit];
    if (kind == kHeal) {
      if (h.amount <= 0 && !(def.has(StaffEffect::Kind::Pull) && h.moveTo != kNoCell)) continue;  // (a cure or an inflict on someone is not a heal)
      if (pol.healsRetreating && (t.id == user.id || b.isEnemy(user, t) || !t.retreating || t.hp >= b.maxHp(t))) return false;
    } else if (kind == kInflict) {
      if (pol.lastPlaced && (!b.isEnemy(user, t) || !t.conditions.empty() || h.hit <= 0 || h.inflict < 0)) return false;
      if (casters && !wouldLoseToCondition(b, t, cond)) return false;
    } else if (pol.lastPlaced && (t.id == user.id || b.isEnemy(user, t) || h.cures.empty())) {
      return false;
    }
    key = std::max(key, t.id);
  }
  return true;
}

}  // namespace

StaffPolicy staffPolicyOf(const AiBehaviour* beh) {
  StaffPolicy p;
  if (beh) {
    p.healsRetreating = beh->healsRetreating;
    p.lastPlaced = beh->staffLastPlaced;
    p.castersOnly = beh->inflictCasters;
  }
  return p;
}

void lastPlacedKeys(const Battle& b, const Unit& u, const std::vector<Cell>& stops, StaffPolicy& pol) {
  if (!pol.healsRetreating && !pol.lastPlaced) return;
  std::vector<StaffOption> options;
  std::vector<int> targets;
  for (Cell s : stops) {
    b.staffOptions(u.id, s, options);
    for (const StaffOption& o : options) {
      if (!o.ok) continue;
      const StaffDef& def = b.db->staves[(size_t)b.db->items[(size_t)o.item].staff];
      if (!usable(def, pol) || !picksLast(def, pol)) continue;
      b.staffTargets(u.id, o.slot, s, targets);
      for (int t : targets) {
        StaffForecast f;
        int key = -1;
        if (b.staffForecast(u.id, o.slot, t, s, f) && allowedUse(b, u, def, f, pol, key) && key >= 0) {
          int& last = pol.keys[kindOf(def, pol)];
          last = std::max(last, key);
        }
      }
    }
  }
}

double staffUseValue(const Battle& b, const Unit& user, const StaffForecast& f) { return healValue(b, f) + conditionValue(b, user, f) + pullValue(b, f); }

bool staffUseAllowed(const Battle& b, const Unit& u, int item, const StaffForecast& f, const StaffPolicy& pol) {
  const StaffDef& def = b.db->staves[(size_t)b.db->items[(size_t)item].staff];
  if (!usable(def, pol)) return false;
  int key = -1;
  if (!allowedUse(b, u, def, f, pol, key)) return false;
  return !picksLast(def, pol) || (key >= 0 && key == pol.keys[kindOf(def, pol)]);
}

void considerStaves(const Battle& b, const Unit& u, Cell s, int travel, Order& best, const StaffPolicy& pol) {
  if (b.db->staves.empty()) return;
  std::vector<StaffOption> options;
  b.staffOptions(u.id, s, options);
  for (const StaffOption& o : options) {
    if (!o.ok) continue;
    std::vector<int> targets;
    b.staffTargets(u.id, o.slot, s, targets);
    for (int t : targets) {
      StaffForecast f;
      if (!b.staffForecast(u.id, o.slot, t, s, f) || !staffUseAllowed(b, u, o.item, f, pol)) continue;
      double sc = staffUseValue(b, u, f);
      if (sc <= 0) continue;
      sc -= exposureCost(b, u, s);
      if (sc <= 0) continue;
      sc += kAttackBase - kMoveCostWeight * travel;
      if (sc <= best.score) continue;
      best = Order();
      best.score = sc;
      best.kind = ActionType::UseStaff;
      best.moveTo = s == u.cell ? kNoCell : s;
      best.slot = o.slot;
      best.target = f.needsPick ? t : -1;
    }
  }
}

}  // namespace ai
}  // namespace ote
