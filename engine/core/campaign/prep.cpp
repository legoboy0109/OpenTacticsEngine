// The preparation screen's rules: candidates, the default pick, deploy / bench / move, and the map the battle starts
// from (see prep.h; docs/EVENTS.md "Preparation").
#include "core/campaign/prep.h"

#include <algorithm>

#include "core/campaign/prep_internal.h"
#include "core/data/footprint.h"

namespace ote {

bool prepOn(const MapDef& map) { return map.prep != nullptr; }

namespace {

bool contains(const std::vector<std::string>& v, const std::string& id) { return std::find(v.begin(), v.end(), id) != v.end(); }

}  // namespace

// ---- the map's own units, by role ----

// A placed unit the player chooses about: named, on the prep side, not fixed.
bool prepIsCandidate(const PrepDef& prep, const MapUnitDef& u) {
  return u.side == prep.side && !u.id.empty() && !contains(prep.fixed, u.id);
}

int prepStructureAt(const Database& db, const MapDef& map, int cell) {
  const Grid grid(map.gridType, map.width, map.height, map.hexLayout);
  std::vector<Cell> cells;
  for (const MapStructureDef& s : map.structures) {
    structureFootprint(grid, db.structures[(size_t)s.type], s.x, s.y, cells);
    for (Cell c : cells)
      if ((int)c == cell) return s.type;
  }
  return -1;
}

bool prepCanStand(const Database& db, const MapDef& map, int type, int slot) {
  if (!map.prep || slot < 0 || slot >= (int)map.prep->slots.size() || type < 0 || type >= (int)db.unitTypes.size()) return false;
  const PrepSlot& s = map.prep->slots[(size_t)slot];
  const int cell = s.y * map.width + s.x;
  return enterCostOf(db, map.terrain[(size_t)cell], prepStructureAt(db, map, cell), db.unitTypes[(size_t)type].moveType) > 0;
}

int prepSlotAt(const MapDef& map, int x, int y) {
  if (!map.prep) return -1;
  for (size_t i = 0; i < map.prep->slots.size(); ++i)
    if (map.prep->slots[i].x == x && map.prep->slots[i].y == y) return (int)i;
  return -1;
}

std::string prepLockReason(PrepLock lock) {
  switch (lock) {
    case PrepLock::Required: return "Must fight in this battle.";
    case PrepLock::Forbidden: return "Cannot take part in this battle.";
    case PrepLock::Fallen: return "Has fallen and cannot fight.";
    default: return "";
  }
}

std::vector<PrepEntry> prepEntries(const Database& db, const Playthrough& pt, const MapDef& map) {
  std::vector<PrepEntry> out;
  if (!map.prep) return out;
  const PrepDef& prep = *map.prep;
  // Ids the map places that stay where they are: a roster unit with the same id is that unit, not a candidate.
  std::vector<std::string> staying = prep.fixed;
  for (const MapUnitDef& u : map.units)
    if (!u.id.empty() && !prepIsCandidate(prep, u)) staying.push_back(u.id);
  auto finish = [&](PrepEntry& e) {
    if (contains(prep.forbidden, e.id)) e.lock = PrepLock::Forbidden;
    for (const PrepRequired& r : prep.required)
      if (r.id == e.id) {
        e.fixedSlot = r.slot;
        if (e.lock == PrepLock::None) e.lock = PrepLock::Required;
      }
    for (const PrepDeploy& d : pt.prep.deployed)
      if (d.id == e.id) e.slot = d.slot;
    out.push_back(e);
  };
  for (const RosterUnit& r : pt.roster) {
    if (contains(staying, r.id)) continue;
    PrepEntry e;
    e.id = r.id;
    e.name = r.name;
    e.type = r.type;
    e.level = r.level;
    e.inRoster = true;
    if (r.gone) e.lock = PrepLock::Fallen;
    finish(e);
  }
  for (const MapUnitDef& u : map.units) {
    if (!prepIsCandidate(prep, u)) continue;
    bool known = false;
    for (const PrepEntry& e : out) known = known || e.id == u.id;
    if (known) continue;
    PrepEntry e;
    e.id = u.id;
    e.name = u.name.empty() ? db.unitTypes[(size_t)u.type].name : u.name;
    e.type = u.type;
    e.level = u.level;
    finish(e);
  }
  return out;
}

namespace {

// A character that can be deployed: not forbidden, not fallen.
bool available(const PrepEntry& e) { return e.lock == PrepLock::None || e.lock == PrepLock::Required; }

}  // namespace

std::vector<PrepDeploy> prepDefaultDeployment(const Database& db, const Playthrough& pt, const MapDef& map) {
  std::vector<PrepDeploy> out;
  if (!map.prep) return out;
  const PrepDef& prep = *map.prep;
  const std::vector<PrepEntry> entries = prepEntries(db, pt, map);
  auto find = [&](const std::string& id) -> const PrepEntry* {
    for (const PrepEntry& e : entries)
      if (e.id == id) return &e;
    return nullptr;
  };
  std::vector<std::string> order;
  auto push = [&](const std::string& id) {
    const PrepEntry* e = find(id);
    if (e && available(*e) && !contains(order, id)) order.push_back(id);
  };
  for (const PrepRequired& r : prep.required) push(r.id);
  for (const std::string& id : pt.lastDeployed) push(id);
  for (const PrepEntry& e : entries) push(e.id);

  std::vector<int> taken(prep.slots.size(), 0), slotOf(order.size(), -1);
  // A required character with a slot of its own gets it first, so nobody else takes it.
  for (size_t i = 0; i < order.size(); ++i) {
    const PrepEntry* e = find(order[i]);
    if (e->lock == PrepLock::Required && e->fixedSlot >= 0 && prepCanStand(db, map, e->type, e->fixedSlot)) {
      slotOf[i] = e->fixedSlot;
      taken[(size_t)e->fixedSlot] = 1;
    }
  }
  int count = 0;
  for (int s : slotOf) count += s >= 0;
  // Slots still wanted by a required character that could not take its own (it cannot stand there) stay free.
  for (size_t i = 0; i < order.size(); ++i) {
    if (slotOf[i] >= 0) continue;
    if (count >= prep.limit) break;
    const PrepEntry* e = find(order[i]);
    for (size_t s = 0; s < prep.slots.size(); ++s)
      if (!taken[s] && prepCanStand(db, map, e->type, (int)s)) {
        slotOf[i] = (int)s;
        taken[s] = 1;
        ++count;
        break;
      }
  }
  for (size_t i = 0; i < order.size(); ++i)
    if (slotOf[i] >= 0) out.push_back({order[i], slotOf[i]});
  return out;
}

std::vector<PrepDeploy> prepCurrentDeployment(const Database& db, const Playthrough& pt, const MapDef& map) {
  if (!pt.prep.deployed.empty()) return pt.prep.deployed;
  return prepDefaultDeployment(db, pt, map);
}

int prepDeployedCount(const Playthrough& pt) { return (int)pt.prep.deployed.size(); }

// ---- the screen's state ----

void prepAddMapUnits(const Database& db, Playthrough& pt, const MapDef& map) {
  if (!map.prep) return;
  for (const MapUnitDef& u : map.units) {
    if (!prepIsCandidate(*map.prep, u) || pt.findUnit(u.id)) continue;
    pt.roster.push_back(prepRosterEntry(db, map, u));
  }
}

RosterUnit prepRosterEntry(const Database& db, const MapDef& map, const MapUnitDef& u) {
  RosterUnit r;
  r.id = u.id;
  r.type = u.type;
  r.name = u.name.empty() ? db.unitTypes[(size_t)u.type].name : u.name;
  r.level = u.level;
  r.xp = 0;
  r.stats = u.stats;
  r.stats.resize(db.stats.size(), 0);
  r.items = u.hasItems ? u.items : db.unitTypes[(size_t)u.type].items;
  r.jobs = u.jobs;
  r.skills = u.skills;
  r.side = map.sides[(size_t)u.side].id;
  return r;
}

void prepOpen(const Database& db, Playthrough& pt, const MapDef& map, bool carry) {
  if (!map.prep || pt.prep.open) return;
  const PrepDef& prep = *map.prep;
  if (carry) {
    prepAddMapUnits(db, pt, map);
    if (!pt.prep.folded) {
      // What the chapter's map gives the prep side (money, storage) is added to what it carries now, so the screen can
      // spend and handle it; the battle then starts from the carried amounts alone (prepApplyToMap).
      const MapSideDef& side = map.sides[(size_t)prep.side];
      CarriedSide& c = pt.sideCarry(side.id, db.resources.size());
      for (const Amount& a : side.resources)
        if (a.resource >= 0 && a.resource < (int)c.resources.size()) c.resources[(size_t)a.resource] += a.amount;
      for (const ItemGrant& g : side.storage) c.storage.push_back(g);
      pt.prep.folded = true;
    }
  }
  pt.prep.open = true;
  if (pt.prep.deployed.empty()) pt.prep.deployed = prepDefaultDeployment(db, pt, map);
}

void prepOpenFree(const Database& db, Playthrough& pt, const MapDef& map) {
  pt.prep.scratch = true;
  prepOpen(db, pt, map, true);
}

// ---- who fights and where ----

namespace {

const PrepEntry* entryOf(const std::vector<PrepEntry>& entries, const std::string& id) {
  for (const PrepEntry& e : entries)
    if (e.id == id) return &e;
  return nullptr;
}

int occupantOf(const Playthrough& pt, int slot) {
  for (size_t i = 0; i < pt.prep.deployed.size(); ++i)
    if (pt.prep.deployed[i].slot == slot) return (int)i;
  return -1;
}

int deployedIndex(const Playthrough& pt, const std::string& id) {
  for (size_t i = 0; i < pt.prep.deployed.size(); ++i)
    if (pt.prep.deployed[i].id == id) return (int)i;
  return -1;
}

bool refuse(std::string& why, const std::string& text) {
  why = text;
  return false;
}

std::string slotName(int slot) { return "slot " + std::to_string(slot + 1); }

// The character `id`, as long as the screen may touch it; else the reason.
const PrepEntry* pick(const std::vector<PrepEntry>& entries, const std::string& id, std::string& why) {
  const PrepEntry* e = entryOf(entries, id);
  if (!e) {
    why = "That character is not in your party.";
    return nullptr;
  }
  return e;
}

}  // namespace

bool prepDeploy(const Database& db, Playthrough& pt, const MapDef& map, const std::string& id, int slot, std::string& why) {
  if (!map.prep) return refuse(why, "This battle has no preparation.");
  const PrepDef& prep = *map.prep;
  const std::vector<PrepEntry> entries = prepEntries(db, pt, map);
  const PrepEntry* e = pick(entries, id, why);
  if (!e) return false;
  if (e->lock == PrepLock::Forbidden) return refuse(why, e->name + " cannot take part in this battle.");
  if (e->lock == PrepLock::Fallen) return refuse(why, e->name + " has fallen and cannot fight.");
  if (deployedIndex(pt, id) >= 0) return refuse(why, e->name + " is already deployed.");
  if ((int)pt.prep.deployed.size() >= prep.limit)
    return refuse(why, "The limit of " + std::to_string(prep.limit) + " is reached. Bench someone first.");
  if (slot < 0) {
    // A required character's own slot, else the first free slot it can stand on.
    if (e->fixedSlot >= 0 && occupantOf(pt, e->fixedSlot) < 0 && prepCanStand(db, map, e->type, e->fixedSlot)) slot = e->fixedSlot;
    for (size_t s = 0; slot < 0 && s < prep.slots.size(); ++s) {
      bool reserved = false;  // another required character's own slot stays free for it
      for (const PrepRequired& r : prep.required) reserved = reserved || (r.slot == (int)s && r.id != id);
      if (occupantOf(pt, (int)s) < 0 && !reserved && prepCanStand(db, map, e->type, (int)s)) slot = (int)s;
    }
    if (slot < 0) return refuse(why, "There is no free square that " + e->name + " can stand on.");
  } else {
    if (slot >= (int)prep.slots.size()) return refuse(why, "That is not a deployment square.");
    int o = occupantOf(pt, slot);
    if (o >= 0) return refuse(why, "That square is taken by " + pt.prep.deployed[(size_t)o].id + ".");
    if (!prepCanStand(db, map, e->type, slot)) return refuse(why, e->name + " cannot stand on " + slotName(slot) + ".");
  }
  pt.prep.deployed.push_back({id, slot});
  return true;
}

bool prepBench(const Database& db, Playthrough& pt, const MapDef& map, const std::string& id, std::string& why) {
  if (!map.prep) return refuse(why, "This battle has no preparation.");
  const std::vector<PrepEntry> entries = prepEntries(db, pt, map);
  const PrepEntry* e = pick(entries, id, why);
  if (!e) return false;
  const int i = deployedIndex(pt, id);
  if (i < 0) return refuse(why, e->name + " is not deployed.");
  if (e->lock == PrepLock::Required) return refuse(why, e->name + " must fight in this battle and cannot be benched.");
  pt.prep.deployed.erase(pt.prep.deployed.begin() + i);
  return true;
}

bool prepMove(const Database& db, Playthrough& pt, const MapDef& map, const std::string& id, int slot, std::string& why) {
  if (!map.prep) return refuse(why, "This battle has no preparation.");
  const PrepDef& prep = *map.prep;
  const std::vector<PrepEntry> entries = prepEntries(db, pt, map);
  const PrepEntry* e = pick(entries, id, why);
  if (!e) return false;
  const int i = deployedIndex(pt, id);
  if (i < 0) return refuse(why, e->name + " is not deployed.");
  if (slot < 0 || slot >= (int)prep.slots.size()) return refuse(why, "That is not a deployment square.");
  const int from = pt.prep.deployed[(size_t)i].slot;
  if (from == slot) return true;
  if (e->fixedSlot >= 0) return refuse(why, e->name + " must start on " + slotName(e->fixedSlot) + ".");
  const int o = occupantOf(pt, slot);
  if (o < 0) {
    if (!prepCanStand(db, map, e->type, slot)) return refuse(why, e->name + " cannot stand on " + slotName(slot) + ".");
    pt.prep.deployed[(size_t)i].slot = slot;
    return true;
  }
  const PrepEntry* other = entryOf(entries, pt.prep.deployed[(size_t)o].id);
  if (other && other->fixedSlot >= 0) return refuse(why, other->name + " must start on " + slotName(slot) + ".");
  if (!prepCanStand(db, map, e->type, slot)) return refuse(why, e->name + " cannot stand on " + slotName(slot) + ".");
  if (other && !prepCanStand(db, map, other->type, from))
    return refuse(why, other->name + " cannot stand on " + slotName(from) + ".");
  std::swap(pt.prep.deployed[(size_t)i].slot, pt.prep.deployed[(size_t)o].slot);
  return true;
}

bool prepBegin(const Database& db, Playthrough& pt, const MapDef& map, std::string& why) {
  if (!map.prep) return refuse(why, "This battle has no preparation.");
  const std::vector<PrepEntry> entries = prepEntries(db, pt, map);
  bool anyone = false;
  for (const PrepEntry& e : entries) {
    anyone = anyone || available(e);
    if (e.lock == PrepLock::Required && deployedIndex(pt, e.id) < 0) return refuse(why, e.name + " must fight in this battle.");
  }
  if (anyone && pt.prep.deployed.empty()) return refuse(why, "Deploy at least one character.");
  pt.prep.open = false;
  return true;
}

// ---- checking a deployment that came from outside (a save) ----

bool prepValidate(const Database& db, const Playthrough& pt, const MapDef& map, std::string& err) {
  if (!pt.prep.any()) return true;
  if (!map.prep) {
    err = "the chapter's map has no preparation, but the save has one";
    return false;
  }
  const PrepDef& prep = *map.prep;
  const std::vector<PrepEntry> entries = prepEntries(db, pt, map);
  if ((int)pt.prep.deployed.size() > prep.limit) {
    err = "more characters are deployed than the map's limit of " + std::to_string(prep.limit);
    return false;
  }
  std::vector<int> seenSlot;
  std::vector<std::string> seenId;
  for (const PrepDeploy& d : pt.prep.deployed) {
    const PrepEntry* e = entryOf(entries, d.id);
    if (!e || !available(*e)) {
      err = "\"" + d.id + "\" cannot be deployed on this map";
      return false;
    }
    if (d.slot < 0 || d.slot >= (int)prep.slots.size() || !prepCanStand(db, map, e->type, d.slot)) {
      err = "\"" + d.id + "\" is on a square it cannot stand on";
      return false;
    }
    if (std::find(seenSlot.begin(), seenSlot.end(), d.slot) != seenSlot.end() || contains(seenId, d.id)) {
      err = "two deployments share a slot or a character";
      return false;
    }
    seenSlot.push_back(d.slot);
    seenId.push_back(d.id);
  }
  return true;
}

// ---- starting the battle ----

void prepApplyToMap(const Database& db, const Playthrough& pt, const MapDef& in, MapDef& out) {
  out = in;
  if (!in.prep) return;
  const PrepDef& prep = *in.prep;
  const std::vector<PrepDeploy> deployment = prepCurrentDeployment(db, pt, in);
  std::vector<MapUnitDef> units;
  for (const MapUnitDef& u : in.units)
    if (!prepIsCandidate(prep, u)) units.push_back(u);  // the ones that stay as placed keep their order
  for (const PrepDeploy& d : deployment) {
    if (d.slot < 0 || d.slot >= (int)prep.slots.size()) continue;
    MapUnitDef u;
    bool placed = false;
    for (const MapUnitDef& m : in.units)
      if (prepIsCandidate(prep, m) && m.id == d.id) {
        u = m;
        placed = true;
      }
    if (!placed) {
      const RosterUnit* r = pt.findUnit(d.id);
      if (!r) continue;
      u.id = r->id;
      u.type = r->type;
      u.name = r->name;
      u.level = r->level;
      u.stats = r->stats;
      u.stats.resize(db.stats.size(), 0);
      u.side = prep.side;
    }
    if (pt.prep.scratch)  // a free battle: what the screen did to the character's items is final
      if (const RosterUnit* r = pt.findUnit(d.id)) {
        u.items = r->items;
        u.hasItems = true;
      }
    u.x = prep.slots[(size_t)d.slot].x;
    u.y = prep.slots[(size_t)d.slot].y;
    units.push_back(u);
  }
  out.units = units;
  if (pt.prep.scratch) {  // a free battle: the screen's storage and funds ARE the side's start (the map's were folded in)
    MapSideDef& side = out.sides[(size_t)prep.side];
    side.resources.clear();
    side.storage.clear();
    for (const CarriedSide& c : pt.sides)
      if (c.id == side.id) {
        for (size_t i = 0; i < c.resources.size(); ++i)
          if (c.resources[i] != 0) side.resources.push_back({(int)i, c.resources[i]});
        side.storage = c.storage;
      }
  } else if (pt.prep.folded) {  // the screen already took the side's map allowance into what it carries
    out.sides[(size_t)prep.side].resources.clear();
    out.sides[(size_t)prep.side].storage.clear();
  }
}

void prepFinishChapter(const Database& db, Playthrough& pt, const MapDef& map, const Battle& battle) {
  if (!map.prep) return;
  prepAddMapUnits(db, pt, map);  // a placed character the player left out still joins the party
  pt.lastDeployed.clear();
  for (const Unit& u : battle.units)
    if (!u.storyId.empty() && !battle.sides[(size_t)u.side].ai) pt.lastDeployed.push_back(u.storyId);
  pt.prep = PrepState();
}

}  // namespace ote
