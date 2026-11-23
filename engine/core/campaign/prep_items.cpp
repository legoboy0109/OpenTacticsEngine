// The preparation screen's items: trading between roster characters, the shared storage, equipping a weapon and the
// preparation shop, buying and selling (docs/EVENTS.md "Preparation"). Everything works on the roster and on what the
// prep side carries, so it all stays when the battle starts (and after a lost battle).
#include <algorithm>

#include "core/campaign/prep_internal.h"

namespace ote {

namespace {

bool refuse(std::string& why, const std::string& text) {
  why = text;
  return false;
}

RosterUnit* unitOf(Playthrough& pt, const std::string& id, std::string& why) {
  for (RosterUnit& r : pt.roster)
    if (r.id == id) {
      if (r.gone) {
        why = r.name + " has fallen.";
        return nullptr;
      }
      return &r;
    }
  why = "That character is not in your party.";
  return nullptr;
}

bool hasRoom(const Database& db, const RosterUnit& r) { return (int)r.items.size() < db.rules.items.inventorySize; }

CarriedSide* carriedOf(Playthrough& pt, const Database& db, const MapDef& map) {
  if (!map.prep) return nullptr;
  return &pt.sideCarry(map.sides[(size_t)map.prep->side].id, db.resources.size());
}

bool needItems(const MapDef& map, std::string& why) {
  if (!map.prep) return refuse(why, "This battle has no preparation.");
  return true;
}

// Is this carried weapon one `r` can wield now (its class, a character lock, its rank)? The battle's own test
// (Battle::canWieldNow), run on a stand-in unit so the screen and the battle never disagree. `why` gets the reason.
bool wieldable(const Database& db, const RosterUnit& r, int weapon, std::string* why) {
  Unit u;
  u.type = r.type;
  u.character = db.findCharacter(r.id);
  u.weaponXp = r.weaponXp;
  Battle judge;
  judge.db = &db;
  std::string reason;
  if (judge.canWieldNow(u, weapon, &reason)) return true;
  // The battle's words are for a unit ("the unit cannot wield that"); a lock or a rank already names itself.
  if (why) *why = db.canWield(u.type, weapon, u.character) ? reason + "." : r.name + " cannot wield that.";
  return false;
}

// The position of the weapon in hand: the first wieldable weapon item (docs/ITEMS.md "State"); -1 = none.
int equippedAt(const Database& db, const RosterUnit& r) {
  for (size_t i = 0; i < r.items.size(); ++i) {
    const ItemDef& d = db.items[(size_t)r.items[i].item];
    if (d.kind == ItemKind::Weapon && wieldable(db, r, d.weapon, nullptr)) return (int)i;
  }
  return -1;
}

}  // namespace

const std::vector<ItemGrant>& prepStorage(const Playthrough& pt, const MapDef& map) {
  static const std::vector<ItemGrant> none;
  if (!map.prep) return none;
  const std::string& side = map.sides[(size_t)map.prep->side].id;
  for (const CarriedSide& s : pt.sides)
    if (s.id == side) return s.storage;
  return none;
}

std::vector<int> prepResources(const Database& db, const Playthrough& pt, const MapDef& map) {
  std::vector<int> out(db.resources.size(), 0);
  if (!map.prep) return out;
  const std::string& side = map.sides[(size_t)map.prep->side].id;
  for (const CarriedSide& s : pt.sides)
    if (s.id == side)
      for (size_t i = 0; i < out.size() && i < s.resources.size(); ++i) out[i] = s.resources[i];
  return out;
}

bool prepGive(const Database& db, Playthrough& pt, const MapDef& map, const std::string& from, int slot, const std::string& to,
              std::string& why) {
  if (!needItems(map, why)) return false;
  RosterUnit* a = unitOf(pt, from, why);
  if (!a) return false;
  RosterUnit* b = unitOf(pt, to, why);
  if (!b) return false;
  if (a == b) return refuse(why, "That is the same character.");
  if (slot < 0 || slot >= (int)a->items.size()) return refuse(why, a->name + " does not carry that item.");
  if (!hasRoom(db, *b)) return refuse(why, b->name + " cannot carry more.");
  ItemGrant g = a->items[(size_t)slot];
  a->items.erase(a->items.begin() + slot);
  g.drop = false;
  b->items.push_back(g);
  return true;
}

bool prepSwap(const Database& db, Playthrough& pt, const MapDef& map, const std::string& id, int slot, const std::string& with, int withSlot,
              std::string& why) {
  (void)db;
  if (!needItems(map, why)) return false;
  RosterUnit* a = unitOf(pt, id, why);
  if (!a) return false;
  RosterUnit* b = unitOf(pt, with, why);
  if (!b) return false;
  if (slot < 0 || slot >= (int)a->items.size()) return refuse(why, a->name + " does not carry that item.");
  if (withSlot < 0 || withSlot >= (int)b->items.size()) return refuse(why, b->name + " does not carry that item.");
  if (a == b && slot == withSlot) return refuse(why, "That is the same item.");
  std::swap(a->items[(size_t)slot], b->items[(size_t)withSlot]);  // (one pack: two places trade; the packs keep their size)
  if (a != b) {
    a->items[(size_t)slot].drop = false;  // like a hand over: nothing carries a "drops it" mark across
    b->items[(size_t)withSlot].drop = false;
  }
  return true;
}

bool prepStore(const Database& db, Playthrough& pt, const MapDef& map, const std::string& id, int slot, std::string& why) {
  if (!needItems(map, why)) return false;
  if (db.rules.items.storage == ItemStorage::Off) return refuse(why, "This game has no shared storage.");
  RosterUnit* a = unitOf(pt, id, why);
  if (!a) return false;
  if (slot < 0 || slot >= (int)a->items.size()) return refuse(why, a->name + " does not carry that item.");
  ItemGrant g = a->items[(size_t)slot];
  a->items.erase(a->items.begin() + slot);
  g.drop = false;
  carriedOf(pt, db, map)->storage.push_back(g);
  return true;
}

bool prepFetch(const Database& db, Playthrough& pt, const MapDef& map, const std::string& id, int storageIndex, std::string& why) {
  if (!needItems(map, why)) return false;
  if (db.rules.items.storage == ItemStorage::Off) return refuse(why, "This game has no shared storage.");
  RosterUnit* a = unitOf(pt, id, why);
  if (!a) return false;
  CarriedSide* side = carriedOf(pt, db, map);
  if (storageIndex < 0 || storageIndex >= (int)side->storage.size()) return refuse(why, "That is not in the storage.");
  if (!hasRoom(db, *a)) return refuse(why, a->name + " cannot carry more.");
  a->items.push_back(side->storage[(size_t)storageIndex]);
  side->storage.erase(side->storage.begin() + storageIndex);
  return true;
}

int prepEquippedAt(const Database& db, const Playthrough& pt, const std::string& id) {
  const RosterUnit* a = pt.findUnit(id);
  return a && db.rules.items.carriedWeapons ? equippedAt(db, *a) : -1;
}

bool prepWieldReason(const Database& db, const Playthrough& pt, const std::string& id, int slot, std::string& why) {
  if (!db.rules.items.carriedWeapons) return refuse(why, "Weapons are not carried in this game.");
  const RosterUnit* a = pt.findUnit(id);
  if (!a || a->gone) return refuse(why, "That character is not in your party.");
  if (slot < 0 || slot >= (int)a->items.size()) return refuse(why, a->name + " does not carry that item.");
  const ItemDef& d = db.items[(size_t)a->items[(size_t)slot].item];
  if (d.kind != ItemKind::Weapon) return refuse(why, "That cannot be wielded.");
  return wieldable(db, *a, d.weapon, &why);
}

bool prepEquipReason(const Database& db, const Playthrough& pt, const std::string& id, int slot, std::string& why) {
  if (!prepWieldReason(db, pt, id, slot, why)) return false;
  if (slot == equippedAt(db, *pt.findUnit(id))) return refuse(why, "That is already equipped.");
  return true;
}

bool prepEquip(const Database& db, Playthrough& pt, const MapDef& map, const std::string& id, int slot, std::string& why) {
  if (!needItems(map, why)) return false;
  if (!prepEquipReason(db, pt, id, slot, why)) return false;
  RosterUnit* a = unitOf(pt, id, why);
  ItemGrant g = a->items[(size_t)slot];
  a->items.erase(a->items.begin() + slot);
  a->items.insert(a->items.begin(), g);  // the front: ahead of every other weapon, so it is the first wieldable one
  return true;
}

// ---- selling

bool prepSellValue(const Database& db, const ItemGrant& g, std::vector<Amount>& out, std::string& why) {
  out.clear();
  const ItemDef& d = db.items[(size_t)g.item];
  if (d.price.empty()) return refuse(why, d.name + " has no price, so nobody buys it.");
  if (!d.sellable) return refuse(why, d.name + " cannot be sold.");
  for (const Amount& a : d.price) {
    long long v = (long long)a.amount * db.rules.items.sellPercent;  // percent of the price; divided by 100 below
    if (d.uses > 0 && g.uses >= 0) v = v * std::min(g.uses, d.uses) / d.uses;  // scaled by the uses left
    out.push_back({a.resource, (int)(v / 100)});
  }
  return true;
}

bool prepSell(const Database& db, Playthrough& pt, const MapDef& map, const std::string& id, int slot, std::string& why, int shop) {
  if (!needItems(map, why)) return false;
  if (!map.prep->hasShop) return refuse(why, "This battle has no shop.");
  if (shop < 0 || shop >= (int)map.prep->shops.size()) return refuse(why, "There is no such shop.");
  if (!map.prep->shops[(size_t)shop].buys) return refuse(why, "This shop does not buy items.");
  CarriedSide* side = carriedOf(pt, db, map);
  RosterUnit* r = nullptr;
  std::vector<ItemGrant>* from = &side->storage;
  if (!id.empty()) {
    r = unitOf(pt, id, why);
    if (!r) return false;
    from = &r->items;
  }
  if (slot < 0 || slot >= (int)from->size())
    return refuse(why, r ? r->name + " does not carry that item." : "That is not in the storage.");
  std::vector<Amount> value;
  if (!prepSellValue(db, (*from)[(size_t)slot], value, why)) return false;
  for (const Amount& a : value)
    if (a.resource < (int)side->resources.size()) side->resources[(size_t)a.resource] += a.amount;
  from->erase(from->begin() + slot);
  return true;
}

bool prepBuy(const Database& db, Playthrough& pt, const MapDef& map, const std::string& id, int item, std::string& why, int shopIndex) {
  if (!needItems(map, why)) return false;
  if (!map.prep->hasShop) return refuse(why, "This battle has no shop.");
  if (shopIndex < 0 || shopIndex >= (int)map.prep->shops.size()) return refuse(why, "There is no such shop.");
  const std::vector<int>& shop = map.prep->shops[(size_t)shopIndex].sells;
  if (item < 0 || std::find(shop.begin(), shop.end(), item) == shop.end()) return refuse(why, "That is not sold here.");
  const ItemDef& def = db.items[(size_t)item];
  CarriedSide* side = carriedOf(pt, db, map);
  for (const Amount& a : def.price)
    if (a.resource >= (int)side->resources.size() || side->resources[(size_t)a.resource] < a.amount)
      return refuse(why, "Not enough " + db.resources[(size_t)a.resource].name + ".");
  RosterUnit* r = nullptr;
  if (!id.empty()) {
    r = unitOf(pt, id, why);
    if (!r) return false;
  }
  const bool toUnit = r && hasRoom(db, *r);
  if (!toUnit && db.rules.items.storage == ItemStorage::Off)
    return refuse(why, r ? r->name + " cannot carry more." : "This game has no shared storage.");
  ItemGrant g;
  g.item = item;
  g.uses = def.uses;
  for (const Amount& a : def.price) side->resources[(size_t)a.resource] -= a.amount;
  if (toUnit) r->items.push_back(g);
  else side->storage.push_back(g);
  return true;
}

}  // namespace ote
