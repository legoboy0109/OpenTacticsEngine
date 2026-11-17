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
// Stage 2 (warp, pull, swap, repair, light): the computer IGNORES any staff that has an effect other than a heal. Those effects
// need a second pick (a square, an item) or change the map for the whole side, and judging them takes a plan the stateless AI
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
    double v = h.amount / (double)max * typeValue(*b.db, t.type) * (h.hit / 100.0);
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
    double v = kConditionShare * typeValue(*b.db, t.type) * (h.hit / 100.0);
    if (t.leader && b.db->rules.defeatLeaderDead) v *= kLeaderHeal;
    if (!h.cures.empty() && !b.isEnemy(user, t)) value += v;
    if (h.inflict >= 0 && b.isEnemy(user, t)) value += v;
  }
  return value;
}

}  // namespace

void considerStaves(const Battle& b, const Unit& u, Cell s, int travel, Order& best) {
  if (b.db->staves.empty()) return;
  std::vector<StaffOption> options;
  b.staffOptions(u.id, s, options);
  for (const StaffOption& o : options) {
    if (!o.ok) continue;
    const StaffDef& def = b.db->staves[(size_t)b.db->items[(size_t)o.item].staff];
    bool healsOnly = true;
    for (const StaffEffect& fx : def.effects)
      healsOnly = healsOnly && (fx.kind == StaffEffect::Kind::Heal || fx.kind == StaffEffect::Kind::Inflict || fx.kind == StaffEffect::Kind::Cure);
    if (!healsOnly) continue;
    std::vector<int> targets;
    b.staffTargets(u.id, o.slot, s, targets);
    for (int t : targets) {
      StaffForecast f;
      if (!b.staffForecast(u.id, o.slot, t, s, f)) continue;
      double sc = healValue(b, f) + conditionValue(b, u, f);
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
