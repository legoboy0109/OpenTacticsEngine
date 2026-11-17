// The preparation screen's items: trading between roster characters, the shared storage and the preparation shop
// (docs/EVENTS.md "Preparation"). Everything works on the roster and on what the prep side carries, so it all stays
// when the battle starts (and after a lost battle).
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

bool prepEquip(const Database& db, Playthrough& pt, const MapDef& map, const std::string& id, int slot, std::string& why) {
  if (!needItems(map, why)) return false;
  RosterUnit* a = unitOf(pt, id, why);
  if (!a) return false;
  if (slot < 0 || slot >= (int)a->items.size()) return refuse(why, a->name + " does not carry that item.");
  const ItemDef& d = db.items[(size_t)a->items[(size_t)slot].item];
  if (d.kind != ItemKind::Weapon) return refuse(why, "That cannot be wielded.");
  if (slot == 0) return refuse(why, "That is already equipped.");
  ItemGrant g = a->items[(size_t)slot];
  a->items.erase(a->items.begin() + slot);
  a->items.insert(a->items.begin(), g);
  return true;
}

bool prepBuy(const Database& db, Playthrough& pt, const MapDef& map, const std::string& id, int item, std::string& why) {
  if (!needItems(map, why)) return false;
  if (!map.prep->hasShop) return refuse(why, "This battle has no shop.");
  const std::vector<int>& shop = map.prep->shop;
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
