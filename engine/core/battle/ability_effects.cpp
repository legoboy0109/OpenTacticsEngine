// The abilities of the second set of skill and ability blocks (docs/SRPG.md "Steal", "Refresh an ally", "Cure"): what each
// needs from its target (asked by validation and the menus, so it judges what the unit's team can see) and what it does.
// Move-past is a member of the reposition family and lives with it (ability_moves.cpp).
#include <algorithm>

#include "core/battle/battle.h"

namespace ote {

namespace {

bool refuse(std::string* why, const std::string& reason) {
  if (why) *why = reason;
  return false;
}

}  // namespace

// ---- steal ----

void Battle::stealOptions(int unit, int ability, Cell from, int target, std::vector<StealOption>& out) const {
  out.clear();
  if (unit < 0 || unit >= (int)units.size() || target < 0 || target >= (int)units.size() || ability < 0 ||
      ability >= (int)db->abilities.size())
    return;
  const AbilityDef& d = db->abilities[(size_t)ability];
  if (d.effect != AbilityEffect::Steal) return;
  const Unit& foe = units[(size_t)target];
  const int equipped = db->rules.items.carriedWeapons ? equippedSlot(foe) : -1;
  for (size_t i = 0; i < foe.items.size(); ++i) {
    const ItemDef& it = itemOf(foe.items[i]);
    StealOption o;
    o.slot = (int)i;
    if (!it.stealable) o.why = "cannot be stolen";
    else if (it.kind == ItemKind::Weapon && !d.stealWeapons) o.why = "weapons cannot be stolen";
    else if (it.kind == ItemKind::Weapon && (int)i == equipped && !d.stealEquipped) o.why = "the equipped weapon";
    else if (d.stealIfItem) {  // the maker's condition, judged for this item (`it`): its reason greys the item
      SkillCtx ctx;
      ctx.unit = unit;
      ctx.cell = from;
      ctx.weapon = restingWeapon(units[(size_t)unit]);
      ctx.foe = target;
      ctx.item = &it;
      ctx.itemUses = foe.items[i].uses;
      if (skillExpr(d.stealIf, ctx, 0) == 0) o.why = d.stealWhy.empty() ? "cannot be taken" : d.stealWhy;
    }
    o.ok = o.why.empty();
    out.push_back(o);
  }
}

bool Battle::stealRoomOk(const Unit& u, std::string* why) const {
  // The chests' rule (docs/SRPG.md section 10): a full pack takes nothing unless the side has a storage to take the overflow.
  if (!hasRoom(u) && db->rules.items.storage == ItemStorage::Off) return refuse(why, "the unit cannot carry more: drop something first");
  return true;
}

// ---- what each needs from its target ----

bool Battle::abilityEffectOk(const Unit& u, const AbilityDef& d, Cell from, const Unit& t, std::string* why) const {
  switch (d.effect) {
    case AbilityEffect::Steal: {
      if (d.stealIf >= 0 && !d.stealIfItem) {  // a formula that reads `it` is judged per item (stealOptions)
        SkillCtx ctx;
        ctx.unit = u.id;
        ctx.cell = from;
        ctx.weapon = restingWeapon(u);
        ctx.foe = t.id;
        if (skillExpr(d.stealIf, ctx, 0) == 0) return refuse(why, "cannot steal from that unit");
      }
      std::vector<StealOption> opts;
      stealOptions(u.id, (int)(&d - db->abilities.data()), from, t.id, opts);
      bool any = false;
      for (const StealOption& o : opts) any = any || o.ok;
      if (!any) return refuse(why, opts.empty() ? "it carries nothing" : "there is nothing it can take");
      return true;
    }
    case AbilityEffect::Refresh:
      if (t.side != u.side) return refuse(why, "only a unit of the same side can be refreshed");
      if (speedGame()) {  // docs/TURN_ORDER.md: it gets its charge filled instead; not the unit that has the turn, nor one that is ready already
        if (t.id == activeUnit) return refuse(why, "that unit has its turn now");
        if (t.ct >= db->rules.speed.threshold) return refuse(why, "that unit is ready already");
      } else if (!t.done && !t.acted) return refuse(why, "that unit has not acted yet");
      if (conditionBlock(t, ConditionBlock::Act) >= 0) return refuse(why, "that unit cannot act");
      if ((int)t.refreshedBy.size() >= d.refreshPerTurn) return refuse(why, "that unit was already refreshed this turn");
      if (std::find(u.refreshedBy.begin(), u.refreshedBy.end(), t.id) != u.refreshedBy.end())
        return refuse(why, "that unit refreshed you this turn");
      return true;
    case AbilityEffect::Cure:
      for (const ConditionState& cs : t.conditions)
        if (d.cureAll || std::find(d.cures.begin(), d.cures.end(), cs.condition) != d.cures.end()) return true;
      return refuse(why, t.id == u.id ? "nothing to cure" : "that unit has nothing to cure");
    default: return true;
  }
}

// ---- doing it ----

void Battle::applyAbilityEffect(const AbilityDef& d, int uid, int target, int slot, std::vector<Event>& events) {
  switch (d.effect) {
    case AbilityEffect::Steal: {
      Unit& foe = units[(size_t)target];
      ItemStack stack = foe.items[(size_t)slot];  // validated
      foe.items.erase(foe.items.begin() + slot);  // what it holds in hand falls back as for any removed weapon (equippedSlot)
      clampHp(foe);                               // an accessory's bonus may have been the maximum
      Event e;
      e.type = EventType::ItemStolen;
      e.a = uid;
      e.b = target;
      e.c = stack.item;
      e.d = stack.uses;
      events.push_back(e);
      Unit& me = units[(size_t)uid];
      const size_t before = me.items.size();
      giveTo(uid, stack, 0, events);  // the pack, else the side's storage (validated: never lost)
      // Like a looted chest: what a computer unit takes it drops again when it is defeated.
      if (sides[(size_t)me.side].ai && me.items.size() > before) me.items.back().drop = true;
      break;
    }
    case AbilityEffect::Refresh: {
      Unit& t = units[(size_t)target];
      if (speedGame()) t.ct = std::max(t.ct, db->rules.speed.threshold);  // docs/TURN_ORDER.md: it acts as soon as the order reaches it
      else {
        t.moved = t.done = t.acted = false;
        t.moveSpent = 0;
      }
      t.refreshedBy.push_back(uid);
      Event e;
      e.type = EventType::UnitRefreshed;
      e.a = uid;
      e.b = target;
      e.c = (int)(&d - db->abilities.data());
      events.push_back(e);
      break;
    }
    case AbilityEffect::Cure:
      cureConditions(target, d.cures, d.cureAll, kConditionCured, events);
      break;
    default: break;
  }
}

}  // namespace ote
