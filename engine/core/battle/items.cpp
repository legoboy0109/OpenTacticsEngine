// Items: inventories, the shared storage, shops and the item actions
// (docs/ITEMS.md). Weapons-as-items meet combat in combat.cpp through
// weaponsOf / weaponSlot / equippedSlot / spendUse.
#include <algorithm>

#include "core/battle/battle.h"
#include "core/battle/unit_stats.h"

namespace ote {

namespace {
bool fail(std::string* why, const char* reason) {
  if (why) *why = reason;
  return false;
}
Event event(EventType t, int a, int b, int c = -1, int d = -1, int flags = 0) {
  Event e;
  e.type = t;
  e.a = a;
  e.b = b;
  e.c = c;
  e.d = d;
  e.flags = flags;
  return e;
}
}  // namespace

// Own value, accessories and research (unitStat), then what skills and carrying change (srpgStat; docs/SRPG.md).
int Battle::stat(const Unit& u, int index) const { return srpgStat(u, index, unitStat(*db, u, index, &sides)); }

void Battle::clampHp(Unit& u) {
  if (!u.alive) return;
  u.hp = std::max(1, std::min(u.hp, maxHp(u)));
}

void Battle::weaponsOf(const Unit& u, std::vector<int>& out) const {
  out.clear();
  if (!db->rules.items.carriedWeapons) {
    // Fixed weapons; one with ammunition that has run out is not available (docs/ECONOMY.md).
    const std::vector<int>& list = typeOf(u).weapons;
    for (size_t i = 0; i < list.size(); ++i)
      if ((u.ammo.empty() || i >= u.ammo.size() || u.ammo[i] != 0) && !weaponForbidden(u, list[i], nullptr)) out.push_back(list[i]);
    return;
  }
  for (const ItemStack& s : u.items) {
    const ItemDef& d = itemOf(s);
    if (d.kind != ItemKind::Weapon || !canWieldNow(u, d.weapon)) continue;
    if (std::find(out.begin(), out.end(), d.weapon) == out.end()) out.push_back(d.weapon);
  }
}

int Battle::weaponSlot(const Unit& u, int weapon) const {
  for (size_t i = 0; i < u.items.size(); ++i) {
    const ItemDef& d = itemOf(u.items[i]);
    if (d.kind == ItemKind::Weapon && d.weapon == weapon && canWieldNow(u, weapon)) return (int)i;
  }
  return -1;
}

int Battle::equippedSlot(const Unit& u) const {
  for (size_t i = 0; i < u.items.size(); ++i) {
    const ItemDef& d = itemOf(u.items[i]);
    if (d.kind == ItemKind::Weapon && canWieldNow(u, d.weapon)) return (int)i;
  }
  return -1;
}

bool Battle::storageOpen(const Unit& u) const {
  if (!u.alive) return false;
  switch (db->rules.items.storage) {
    case ItemStorage::Off: return false;
    case ItemStorage::Anywhere: return true;
    case ItemStorage::Structure: {
      int si = structureAt[u.cell];
      return si >= 0 && structures[(size_t)si].owner == u.side;
    }
    case ItemStorage::Leader:
      if (u.leader) return true;
      for (const Unit& o : units)
        if (o.alive && o.leader && o.side == u.side && grid.distance(o.cell, u.cell) == 1) return true;
      return false;
  }
  return false;
}

void Battle::shopItems(int unit, std::vector<int>& items) const {
  items.clear();
  if (unit < 0 || unit >= (int)units.size() || !units[(size_t)unit].alive) return;
  const Unit& u = units[(size_t)unit];
  int si = structureAt[u.cell];
  if (si < 0) return;
  int owner = structures[(size_t)si].owner;
  if (owner >= 0 && sides[(size_t)owner].team != teamOf(u)) return;  // an enemy's shop does not serve
  items = defOf(structures[(size_t)si]).sells;
}

bool Battle::canPay(int side, const std::vector<Amount>& price) const {
  for (const Amount& a : price)
    if (sides[(size_t)side].resources[(size_t)a.resource] < a.amount) return false;
  return true;
}

void Battle::useTargets(int unit, int slot, std::vector<int>& targets) const {
  targets.clear();
  if (unit < 0 || unit >= (int)units.size()) return;
  const Unit& u = units[(size_t)unit];
  if (!u.alive || slot < 0 || slot >= (int)u.items.size()) return;
  const ItemDef& d = itemOf(u.items[(size_t)slot]);
  if (d.kind != ItemKind::Usable) return;
  // Using an item that would change nothing is not offered.
  // A promotion item also counts when the unit using it on itself could promote now (promotion.cpp).
  auto helps = [&](const Unit& t) {
    if ((!d.raise.empty() && canRaiseAny(t, d.raise)) || (d.heal > 0 && t.hp < maxHp(t))) return true;
    if ((d.cureAll || !d.cures.empty()) && cureWould(t, d.cures, d.cureAll)) return true;
    if (d.inflict >= 0 && inflictWhy(t, d.inflict, d.inflictTurns) == 0) return true;
    if (d.lightRadius > 0 && fog && t.id == unit) return true;  // a torch: offered whenever the map has fog (docs/ECONOMY.md "Fog of war")
    std::vector<const PromotionDef*> fits;
    if (d.promotes && t.id == unit) promotionCandidates(t, u.items[(size_t)slot].item, true, fits);
    return !fits.empty();
  };
  if (conditionForbidsKind(u, ItemKind::Usable) >= 0) return;  // e.g. a condition that stops items (docs/SRPG.md section 9)
  if (!d.targetAlly) {
    if (helps(u)) targets.push_back(unit);
    return;
  }
  for (const Unit& t : units)
    if (t.alive && t.id != unit && teamOf(t) == teamOf(u) && grid.distance(t.cell, u.cell) == 1 && helps(t))
      targets.push_back(t.id);
}

void Battle::tradePartners(int unit, std::vector<int>& out) const {
  out.clear();
  if (unit < 0 || unit >= (int)units.size() || !units[(size_t)unit].alive) return;
  const Unit& u = units[(size_t)unit];
  for (const Unit& t : units)
    if (t.alive && t.id != unit && t.side == u.side && grid.distance(t.cell, u.cell) == 1) out.push_back(t.id);
}

// The caller (validate) has checked that the unit can act and has not finished its turn.
bool Battle::validateItemAction(const Action& a, std::string* why) const {
  const Unit& u = units[(size_t)a.unit];
  auto ownSlot = [&]() { return a.slot >= 0 && a.slot < (int)u.items.size(); };
  auto partner = [&]() {
    if (a.target < 0 || a.target >= (int)units.size() || a.target == a.unit) return false;
    const Unit& t = units[(size_t)a.target];
    return t.alive && t.side == u.side && grid.distance(t.cell, u.cell) == 1;
  };
  switch (a.type) {
    case ActionType::UseItem: {
      if (!ownSlot()) return fail(why, "the unit does not carry that item");
      if (itemOf(u.items[(size_t)a.slot]).kind != ItemKind::Usable) return fail(why, "that item cannot be used like this");
      const int stop = conditionForbidsKind(u, ItemKind::Usable);
      if (stop >= 0) {
        if (why) *why = db->conditions[(size_t)stop].name + " stops the unit using items";
        return false;
      }
      const ItemDef& d = itemOf(u.items[(size_t)a.slot]);
      const int item = u.items[(size_t)a.slot].item;
      // Promotion (docs/SRPG.md): a promotion that was asked for by type must be possible; when the item would
      // otherwise do nothing, say why the promotion cannot happen.
      std::string promoWhy;
      if (d.promotes && a.target == a.unit && a.unitType >= 0 && !chosenPromotion(u, item, a.unitType, &promoWhy)) {
        if (why) *why = promoWhy;
        return false;
      }
      std::vector<int> targets;
      useTargets(a.unit, a.slot, targets);
      if (std::find(targets.begin(), targets.end(), a.target) == targets.end()) {
        if (d.promotes && a.target == a.unit && !chosenPromotion(u, item, -1, &promoWhy) && why) {
          *why = promoWhy;
          return false;
        }
        return fail(why, "the item would do nothing there");
      }
      return true;
    }
    case ActionType::Equip: {
      if (!db->rules.items.carriedWeapons) return fail(why, "weapons are not carried in this game");
      if (!ownSlot()) return fail(why, "the unit does not carry that item");
      const ItemDef& d = itemOf(u.items[(size_t)a.slot]);
      if (d.kind != ItemKind::Weapon) return fail(why, "the unit cannot wield that");
      if (!canWieldNow(u, d.weapon, why)) return false;  // the class cannot, or (ranks) "Needs rank B"
      if (a.slot == equippedSlot(u)) return fail(why, "that is already equipped");
      return true;
    }
    case ActionType::GiveItem:
      if (!ownSlot()) return fail(why, "the unit does not carry that item");
      if (!partner()) return fail(why, "there is nobody of your side next to the unit to give it to");
      if (!hasRoom(units[(size_t)a.target])) return fail(why, "the other unit cannot carry more");
      return true;
    case ActionType::TakeItem:
      if (!partner()) return fail(why, "there is nobody of your side next to the unit to take it from");
      if (a.slot < 0 || a.slot >= (int)units[(size_t)a.target].items.size())
        return fail(why, "the other unit does not carry that item");
      if (!hasRoom(u)) return fail(why, "the unit cannot carry more");
      return true;
    case ActionType::StoreItem:
      if (!storageOpen(u)) return fail(why, "the storage cannot be reached from here");
      if (!ownSlot()) return fail(why, "the unit does not carry that item");
      return true;
    case ActionType::FetchItem:
      if (!storageOpen(u)) return fail(why, "the storage cannot be reached from here");
      if (a.slot < 0 || a.slot >= (int)sides[(size_t)u.side].storage.size()) return fail(why, "that is not in the storage");
      if (!hasRoom(u)) return fail(why, "the unit cannot carry more");
      return true;
    case ActionType::BuyItem: {
      std::vector<int> shop;
      shopItems(a.unit, shop);
      if (std::find(shop.begin(), shop.end(), a.item) == shop.end()) return fail(why, "that is not sold here");
      if (!canPay(u.side, db->items[(size_t)a.item].price)) return fail(why, "not enough resources");
      if (!hasRoom(u) && db->rules.items.storage == ItemStorage::Off) return fail(why, "the unit cannot carry more");
      return true;
    }
    case ActionType::DiscardItem:
      if (!ownSlot()) return fail(why, "the unit does not carry that item");
      return true;
    default:
      return fail(why, "unknown action");
  }
}

void Battle::spendUse(int unit, int slot, std::vector<Event>& events) {
  Unit& u = units[(size_t)unit];
  ItemStack& s = u.items[(size_t)slot];
  if (s.uses < 0) return;  // unlimited
  if (--s.uses > 0) return;
  events.push_back(event(EventType::ItemBroke, unit, s.item));
  u.items.erase(u.items.begin() + slot);
  clampHp(u);
}

void Battle::giveTo(int unit, ItemStack stack, int flags, std::vector<Event>& events) {
  Unit& u = units[(size_t)unit];
  stack.drop = false;
  if (hasRoom(u)) {
    u.items.push_back(stack);
    events.push_back(event(EventType::ItemGained, unit, stack.item, u.side, -1, flags));
  } else if (db->rules.items.storage != ItemStorage::Off) {
    sides[(size_t)u.side].storage.push_back(stack);
    events.push_back(event(EventType::ItemGained, -1, stack.item, u.side, -1, flags));
  } else {
    events.push_back(event(EventType::ItemLost, unit, stack.item));
  }
}

void Battle::applyItemAction(const Action& a, std::vector<Event>& events) {
  Unit& u = units[(size_t)a.unit];
  std::vector<ItemStack>& storage = sides[(size_t)u.side].storage;
  switch (a.type) {
    case ActionType::UseItem: {
      const ItemDef d = itemOf(u.items[(size_t)a.slot]);  // a copy: the stack may go away below
      Unit& t = units[(size_t)a.target];
      events.push_back(event(EventType::ItemUsed, a.unit, u.items[(size_t)a.slot].item, a.target));
      if (!d.raise.empty()) {
        Event e = event(EventType::StatsRaised, a.target, -1);
        int before = maxHp(t);
        for (size_t s = 0; s < d.raise.size() && s < t.stats.size(); ++s) {
          const int applied = raiseStat(t, (int)s, d.raise[s]);  // a stat cap may cut a gain (docs/SRPG.md section 7)
          e.data.push_back(d.raise[s] > 0 ? applied : d.raise[s]);
        }
        events.push_back(e);
        if (maxHp(t) > before) t.hp += maxHp(t) - before;  // new hit points arrive filled
        clampHp(t);
      }
      bool healedAnother = false;  // supports earn after the item is spent: their story events may add units
      if (d.heal > 0 && t.hp < maxHp(t)) {
        int amount = std::min(d.heal, maxHp(t) - t.hp);
        t.hp += amount;
        events.push_back(event(EventType::UnitHealed, a.target, amount));
        healedAnother = a.target != a.unit;
      }
      // Conditions (docs/SRPG.md section 9): a cure, then an inflict, after the heal.
      if (d.cureAll || !d.cures.empty()) cureConditions(a.target, d.cures, d.cureAll, kConditionCured, events);
      if (d.inflict >= 0) inflictCondition(a.target, d.inflict, d.inflictTurns, a.unit, events);
      if (d.lightRadius > 0 && fog) lightWithItem(a.unit, d, events);  // a torch (mapchange.cpp)
      const int usedItem = u.items[(size_t)a.slot].item;
      spendUse(a.unit, a.slot, events);
      endOrder(a.unit);
      if (healedAnother) supportsHealed(a.unit, a.target, events);  // supports: one healed the other
      // A promotion item promotes its user last (heal and raise came first); story events it starts may add units,
      // so nothing below this line touches `u` or `t`.
      if (d.promotes && a.target == a.unit) promoteByItem(a.unit, usedItem, a.unitType, events);
      break;
    }
    case ActionType::Equip: {
      ItemStack s = u.items[(size_t)a.slot];
      u.items.erase(u.items.begin() + a.slot);
      u.items.insert(u.items.begin(), s);
      events.push_back(event(EventType::ItemEquipped, a.unit, s.item));
      break;
    }
    case ActionType::GiveItem:
    case ActionType::TakeItem: {
      Unit& o = units[(size_t)a.target];
      Unit& from = a.type == ActionType::GiveItem ? u : o;
      Unit& to = a.type == ActionType::GiveItem ? o : u;
      ItemStack s = from.items[(size_t)a.slot];
      from.items.erase(from.items.begin() + a.slot);
      to.items.push_back(s);
      clampHp(from);
      events.push_back(event(EventType::ItemMoved, from.id, to.id, s.item, u.side));
      break;
    }
    case ActionType::StoreItem: {
      ItemStack s = u.items[(size_t)a.slot];
      s.drop = false;
      u.items.erase(u.items.begin() + a.slot);
      storage.push_back(s);
      clampHp(u);
      events.push_back(event(EventType::ItemMoved, a.unit, -1, s.item, u.side));
      break;
    }
    case ActionType::FetchItem: {
      ItemStack s = storage[(size_t)a.slot];
      storage.erase(storage.begin() + a.slot);
      u.items.push_back(s);
      events.push_back(event(EventType::ItemMoved, -1, a.unit, s.item, u.side));
      break;
    }
    case ActionType::BuyItem: {
      const ItemDef& d = db->items[(size_t)a.item];
      for (const Amount& p : d.price) changeResource(u.side, p.resource, -p.amount, events);
      ItemStack s;
      s.item = a.item;
      s.uses = d.uses;
      giveTo(a.unit, s, kBought, events);
      break;
    }
    case ActionType::DiscardItem: {
      int item = u.items[(size_t)a.slot].item;
      u.items.erase(u.items.begin() + a.slot);
      clampHp(u);
      events.push_back(event(EventType::ItemMoved, a.unit, -2, item, u.side));
      break;
    }
    default: break;
  }
}

}  // namespace ote
