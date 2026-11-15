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

int Battle::stat(const Unit& u, int index) const { return unitStat(*db, u, index, &sides); }

void Battle::clampHp(Unit& u) {
  if (!u.alive) return;
  u.hp = std::max(1, std::min(u.hp, maxHp(u)));
}

void Battle::weaponsOf(const Unit& u, std::vector<int>& out) const {
  out.clear();
  if (!db->rules.items.carriedWeapons) {
    out = typeOf(u).weapons;
    return;
  }
  for (const ItemStack& s : u.items) {
    const ItemDef& d = itemOf(s);
    if (d.kind != ItemKind::Weapon || !db->canWield(u.type, d.weapon)) continue;
    if (std::find(out.begin(), out.end(), d.weapon) == out.end()) out.push_back(d.weapon);
  }
}

int Battle::weaponSlot(const Unit& u, int weapon) const {
  for (size_t i = 0; i < u.items.size(); ++i) {
    const ItemDef& d = itemOf(u.items[i]);
    if (d.kind == ItemKind::Weapon && d.weapon == weapon && db->canWield(u.type, weapon)) return (int)i;
  }
  return -1;
}

int Battle::equippedSlot(const Unit& u) const {
  for (size_t i = 0; i < u.items.size(); ++i) {
    const ItemDef& d = itemOf(u.items[i]);
    if (d.kind == ItemKind::Weapon && db->canWield(u.type, d.weapon)) return (int)i;
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
  auto helps = [&](const Unit& t) { return !d.raise.empty() || (d.heal > 0 && t.hp < maxHp(t)); };
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
      std::vector<int> targets;
      useTargets(a.unit, a.slot, targets);
      if (std::find(targets.begin(), targets.end(), a.target) == targets.end())
        return fail(why, "the item would do nothing there");
      return true;
    }
    case ActionType::Equip: {
      if (!db->rules.items.carriedWeapons) return fail(why, "weapons are not carried in this game");
      if (!ownSlot()) return fail(why, "the unit does not carry that item");
      const ItemDef& d = itemOf(u.items[(size_t)a.slot]);
      if (d.kind != ItemKind::Weapon || !db->canWield(u.type, d.weapon)) return fail(why, "the unit cannot wield that");
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
          t.stats[s] = std::max(0, t.stats[s] + d.raise[s]);
          e.data.push_back(d.raise[s]);
        }
        events.push_back(e);
        if (maxHp(t) > before) t.hp += maxHp(t) - before;  // new hit points arrive filled
        clampHp(t);
      }
      if (d.heal > 0 && t.hp < maxHp(t)) {
        int amount = std::min(d.heal, maxHp(t) - t.hp);
        t.hp += amount;
        events.push_back(event(EventType::UnitHealed, a.target, amount));
      }
      spendUse(a.unit, a.slot, events);
      u.done = true;
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
