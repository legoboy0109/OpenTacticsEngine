// Promotion in a battle (docs/SRPG.md "Promotion"): one function, `promote`, that every trigger ends in (an item,
// an offer at a level, an automatic one at a level, a story command), and the questions that decide which
// promotions are possible. Nothing here is saved: type, stats, level and xp already are.
#include <algorithm>

#include "core/base/log.h"
#include "core/battle/battle.h"

namespace ote {

namespace {
Event promotedEvent(int unit, int oldType, int newType, int level, std::vector<int> change) {
  Event e;
  e.type = EventType::UnitPromoted;
  e.a = unit;
  e.b = oldType;
  e.c = newType;
  e.d = level;
  e.data = std::move(change);
  return e;
}
}  // namespace

bool Battle::canStandAs(const Unit& u, int type) const {
  return grid.valid(u.cell) && enterCost(u.cell, db->unitTypes[(size_t)type].moveType) > 0;
}

bool Battle::jobBlocksPromotion(const Unit& u) const {
  return db->jobsOn() && u.jobs.on() && u.jobs.job < (int)db->jobs.size() && !db->jobs[(size_t)u.jobs.job].overlay();
}

void Battle::promotionCandidates(const Unit& u, int item, bool needStand, std::vector<const PromotionDef*>& out) const {
  out.clear();
  if (jobBlocksPromotion(u)) return;
  for (const PromotionDef& d : typeOf(u).promotions) {
    if (u.level < promotionFromLevel(u, d)) continue;
    if (item >= 0) {
      if (std::find(d.items.begin(), d.items.end(), item) == d.items.end()) continue;
    } else if (d.atLevel != PromoteAtLevel::Offer) {
      continue;
    }
    if (needStand && !canStandAs(u, d.to)) continue;
    out.push_back(&d);
  }
}

const PromotionDef* Battle::chosenPromotion(const Unit& u, int item, int unitType, std::string* why) const {
  auto refuse = [&](const std::string& reason) -> const PromotionDef* {
    if (why) *why = reason;
    return nullptr;
  };
  std::vector<const PromotionDef*> fits;
  promotionCandidates(u, item, false, fits);
  if (fits.empty()) return refuse(item >= 0 ? "that item cannot promote this unit now" : "that unit has no promotion on offer");
  if (unitType >= 0) {
    fits.erase(std::remove_if(fits.begin(), fits.end(), [&](const PromotionDef* d) { return d->to != unitType; }), fits.end());
    if (fits.empty()) return refuse("that promotion is not on offer");
  }
  for (const PromotionDef* d : fits)
    if (canStandAs(u, d->to)) return d;
  return refuse("a " + db->unitTypes[(size_t)fits[0]->to].name + " could not stand on this square");
}

void Battle::promotionOptions(int unit, int slot, std::vector<int>& out) const {
  out.clear();
  if (unit < 0 || unit >= (int)units.size()) return;
  const Unit& u = units[(size_t)unit];
  if (!u.alive) return;
  int item = -1;
  if (slot >= 0) {
    if (slot >= (int)u.items.size()) return;
    item = u.items[(size_t)slot].item;
  }
  std::vector<const PromotionDef*> fits;
  promotionCandidates(u, item, true, fits);
  for (const PromotionDef* d : fits)
    if (std::find(out.begin(), out.end(), d->to) == out.end()) out.push_back(d->to);
}

bool Battle::promote(int unit, int toType, const PromotionDef* def, std::vector<Event>& events) {
  if (unit < 0 || unit >= (int)units.size() || toType < 0 || toType >= (int)db->unitTypes.size()) return false;
  Unit& u = units[(size_t)unit];
  if (!u.alive || !canStandAs(u, toType) || jobBlocksPromotion(u)) return false;

  const int oldType = u.type;
  markSight();  // the unit may see differently as the new type; what was reported so far is stamped first
  const std::vector<int> change = changeType(u, toType, def);
  events.push_back(promotedEvent(unit, oldType, toType, u.level, change));
  rankRaiseToClass(u, unit, &events);  // weapon ranks: raised to the new class's where that is higher (docs/SRPG.md section 7)

  // Events waiting for a promotion; they may add units, so `u` is not used after this.
  StoryOccurrence o;
  o.kind = Trigger::Kind::Promoted;
  o.unit = unit;
  fireStory(o, events);
  return true;
}

// The unit-only part of a promotion, shared with the test level (test_level.cpp), which promotes units that are not on a board (the
// campaign roster): the new type, the stat gains, hit points, the level rule, ammunition, skills. No events, no story, no sight.
// Returns the change per stat index.
std::vector<int> Battle::changeType(Unit& u, int toType, const PromotionDef* def) {
  const UnitTypeDef& from = typeOf(u);
  const UnitTypeDef& to = db->unitTypes[(size_t)toType];
  const int oldType = u.type;
  const std::vector<int> before = u.stats;

  u.type = toType;
  if (u.jobs.on() && u.jobs.baseType >= 0) u.jobs.baseType = toType;  // an overlay job plays as the promoted class from now on
  for (size_t s = 0; s < u.stats.size(); ++s) {
    int gain = def && def->hasGains ? def->gains[s] : to.stats[s] - from.stats[s];
    raiseStat(u, (int)s, gain);  // the NEW type's cap judges a positive gain (docs/SRPG.md section 7)
  }
  // Hit points rise with a positive HP gain; either way they stay within 1..maximum.
  const int hpStat = db->rules.statHp;
  if (hpStat >= 0 && (size_t)hpStat < u.stats.size() && u.stats[(size_t)hpStat] > before[(size_t)hpStat])
    u.hp += u.stats[(size_t)hpStat] - before[(size_t)hpStat];
  clampHp(u);

  LevelAfter rule = def ? def->levelAfter : LevelAfter::Game;
  if (rule == LevelAfter::Game) rule = db->rules.promotion.keepLevel ? LevelAfter::Keep : LevelAfter::Reset;
  if (rule == LevelAfter::Reset) {
    u.level = 1;
    u.xp = 0;
  }
  stockUnit(u);  // fuel and ammunition as for a new unit of the new type
  if (db->rules.promotion.replaceClassSkills) dropOldClassSkills(u, oldType);  // "classSkills": "replace"
  learnUpTo(u);  // the new type's "learns" at or below the level it now has (K1; learned skills stay)

  std::vector<int> change(u.stats.size(), 0);
  for (size_t s = 0; s < u.stats.size(); ++s) change[s] = u.stats[s] - before[s];
  return change;
}

bool Battle::validatePromote(const Action& action, std::string* why) const {
  bool any = false;
  for (const UnitTypeDef& t : db->unitTypes) any = any || !t.promotions.empty();
  if (!any) {
    if (why) *why = "this game has no promotions";
    return false;
  }
  return chosenPromotion(units[(size_t)action.unit], -1, action.unitType, why) != nullptr;
}

void Battle::applyPromote(const Action& action, std::vector<Event>& events) {
  const PromotionDef* d = chosenPromotion(units[(size_t)action.unit], -1, action.unitType, nullptr);
  if (!d) return;  // validated by the caller
  endOrder(action.unit);  // the offer costs the turn; before the story events a promotion may start
  promote(action.unit, d->to, d, events);
}

void Battle::promoteByItem(int unit, int item, int unitType, std::vector<Event>& events) {
  const PromotionDef* d = chosenPromotion(units[(size_t)unit], item, unitType, nullptr);
  if (d) promote(unit, d->to, d, events);  // nothing to do for an item that was only drunk for its heal
}

// "auto": right after the LevelUp that took the unit to the entry's level or above.
void Battle::promotionLevelUp(int unit, std::vector<Event>& events) {
  if (unit < 0 || unit >= (int)units.size() || !units[(size_t)unit].alive) return;
  const Unit& u = units[(size_t)unit];
  if (jobBlocksPromotion(u)) return;
  for (const PromotionDef& d : typeOf(u).promotions)
    if (d.atLevel == PromoteAtLevel::Auto && u.level >= promotionFromLevel(u, d) && canStandAs(u, d.to)) {
      promote(unit, d.to, &d, events);
      return;
    }
}

// An automatic promotion that could not happen (the unit could not stand there) is simply tried again: nothing is
// remembered, so every unit of the side whose level meets an "auto" entry is looked at each turn.
void Battle::promotionTurnStart(std::vector<Event>& events) {
  for (size_t i = 0; i < units.size(); ++i) {  // `units` may grow while a promotion's story events run
    if (!ownsTurn(units[i])) continue;
    promotionLevelUp((int)i, events);
  }
}

void Battle::storyPromote(const Command& c, std::vector<Event>& events) {
  const int id = unitByStoryId(c.unit);
  if (id < 0 || !units[(size_t)id].alive) {
    OTE_INFO("story: promote: unit \"%s\" is gone or not on the board; nothing happens", c.unit.c_str());
    return;
  }
  const Unit& u = units[(size_t)id];
  int to = c.index;
  if (to < 0) {  // no "to": the type must have exactly one promotion (checked when the map loads, if it could be)
    if (typeOf(u).promotions.size() != 1) {
      OTE_WARN("story: promote: \"%s\" is a %s, which has %d promotions, so \"to\" is needed", c.unit.c_str(),
               typeOf(u).id.c_str(), (int)typeOf(u).promotions.size());
      return;
    }
    to = typeOf(u).promotions[0].to;
  }
  if (to == u.type) return;
  const PromotionDef* def = nullptr;  // a pair the type lists keeps its gains and level rule; any other is free
  for (const PromotionDef& d : typeOf(u).promotions)
    if (d.to == to) {
      def = &d;
      break;
    }
  if (!promote(id, to, def, events))
    OTE_INFO("story: promote: unit \"%s\" cannot stand on its square as a %s; nothing happens", c.unit.c_str(),
             db->unitTypes[(size_t)to].id.c_str());
}

}  // namespace ote
