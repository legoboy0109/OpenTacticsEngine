// Changing the map during a battle (docs/EVENTS.md "Changing the map") and the fog switches of story commands (docs/ECONOMY.md "Fog of
// war"): setTerrain, applyChange, revertChange, setFog, setVision, a torch item and the vision skill.
//
// Terrain lives in Battle::cells and everything derived from it asks there (movement cost, sight, terrain numbers, the computer
// opponent, capture squares), so changing a square is only: write it, drop the sight cache, say so with a TerrainChanged event, and
// deal with a unit left on a square it can no longer stand on.
//
// THE STRANDED-UNIT RULE. After a change, a living unit on a changed square (or on a structure that changed kind) whose move type cannot
// enter that square (cost 0 or less) is moved at once to the nearest free square it can stand on (grid distance, the lowest square
// number on a tie), exactly like a story `moveUnit` that found its target blocked: a UnitRelocated event follows the TerrainChanged one
// and "reach" events fire for the new square. Units are handled in unit order. A unit that has no free square to go to stays where it
// is (it can still move off). Units that can still stand there never move. Heights are never changed by a map change.
#include <algorithm>

#include "core/base/log.h"
#include "core/battle/battle.h"

namespace ote {

namespace {

Event ev(EventType t, int a = -1, int b = -1, int c = -1, int d = -1) {
  Event e;
  e.type = t;
  e.a = a;
  e.b = b;
  e.c = c;
  e.d = d;
  return e;
}

}  // namespace

std::vector<int> Battle::changeKindsOf(int structure) const {
  std::vector<int> out;
  if (mapChanges)
    for (const MapChangeDef& ch : *mapChanges)
      for (const MapChangeStructure& s : ch.structures)
        if (s.index == structure) out.push_back(s.kind);
  return out;
}

bool Battle::changeApplied(int change) const {
  for (const AppliedChange& a : appliedChanges)
    if (a.change == change) return true;
  return false;
}

// A structure becomes another kind of the same size (the upgrade mechanism: art, movement cost, sight and the fog memory follow the
// kind). The door / chest numbers start over for the new kind.
void Battle::setStructureKind(int s, int kind, std::vector<Event>& events) {
  StructureState& st = structures[(size_t)s];
  if (st.type == kind) return;
  const int from = st.type;
  markSight();
  st.type = kind;
  const StructureDef& def = db->structures[(size_t)kind];
  st.capture = def.capturePoints;
  st.capturer = -1;
  st.locked = (def.place.isDoor() || def.place.isChest()) && def.place.locked;
  st.hp = def.place.isDoor() && def.place.hp > 0 ? def.place.hp : -1;
  events.push_back(ev(EventType::StructureUpgraded, s, from, kind));
}

// Writes the terrain of the squares (only those that really change), says so, then moves the units that cannot stand where they are.
// `touched` lists extra squares to check for stranded units (the footprints of structures that changed kind).
void Battle::changeTerrainCells(const std::vector<std::pair<Cell, int>>& list, const std::vector<Cell>& touched, std::vector<Event>& events) {
  std::vector<Cell> changed;
  int common = -2;
  for (const std::pair<Cell, int>& p : list) {
    if (!grid.valid(p.first) || p.second < 0 || p.second >= (int)db->terrains.size() || cells[(size_t)p.first].terrain == p.second) continue;
    if (std::find(changed.begin(), changed.end(), p.first) != changed.end()) continue;  // a square twice: the first entry counts
    changed.push_back(p.first);
  }
  if (!changed.empty()) markSight();  // the events so far describe the world as it was
  for (const std::pair<Cell, int>& p : list) {
    if (std::find(changed.begin(), changed.end(), p.first) == changed.end()) continue;
    cells[(size_t)p.first].terrain = p.second;
    common = common == -2 || common == p.second ? p.second : -1;
  }
  if (!changed.empty()) {
    sight_.valid = false;
    Event e = ev(EventType::TerrainChanged, (int)changed.size(), common < 0 ? -1 : common);
    for (Cell c : changed) e.data.push_back(c);
    events.push_back(e);
    OTE_INFO("map: %d square(s) changed terrain", (int)changed.size());
  }
  // Stranded units (see the rule at the top of the file).
  std::vector<Cell> check = changed;
  check.insert(check.end(), touched.begin(), touched.end());
  if (check.empty()) return;
  const int unitCount = (int)units.size();
  for (int i = 0; i < unitCount; ++i) {
    const Unit& u = units[(size_t)i];
    if (!u.alive || u.cell == kNoCell || std::find(check.begin(), check.end(), u.cell) == check.end()) continue;
    if (enterCost(u.cell, typeOf(u).moveType) > 0) continue;
    OTE_INFO("map: unit %d cannot stand on square %d any more; moved to the nearest square it can", i, u.cell);
    storyRelocate(i, u.cell, events);  // the nearest free square it can stand on; stays when there is none
  }
}

void Battle::storySetTerrain(const std::vector<Cell>& squares, int terrain, std::vector<Event>& events) {
  if (terrain < 0 || terrain >= (int)db->terrains.size()) {
    OTE_WARN("story: setTerrain: no terrain number %d", terrain);
    return;
  }
  std::vector<std::pair<Cell, int>> list;
  for (Cell c : squares) list.push_back({c, terrain});
  changeTerrainCells(list, {}, events);
}

void Battle::storyApplyChange(int change, std::vector<Event>& events) {
  if (!mapChanges || change < 0 || change >= (int)mapChanges->size()) {
    OTE_WARN("story: applyChange: the map has no change number %d", change);
    return;
  }
  if (changeApplied(change)) {
    OTE_INFO("story: applyChange '%s': it is applied already", (*mapChanges)[(size_t)change].id.c_str());
    return;
  }
  const MapChangeDef& def = (*mapChanges)[(size_t)change];
  AppliedChange rec;
  rec.change = change;
  std::vector<std::pair<Cell, int>> list;
  for (const MapChangeCell& mc : def.cells) {
    const Cell c = grid.at(mc.x, mc.y);
    rec.before.push_back(c == kNoCell ? 0 : cells[(size_t)c].terrain);
    list.push_back({c, mc.terrain});
  }
  for (const MapChangeStructure& ms : def.structures) rec.beforeKinds.push_back(structures[(size_t)ms.index].type);
  appliedChanges.push_back(rec);  // before the units are moved: an event they start may look at what is applied
  std::vector<Cell> touched;
  for (const MapChangeStructure& ms : def.structures) {
    setStructureKind(ms.index, ms.kind, events);
    std::vector<Cell> fp;
    structureCells(ms.index, fp);
    touched.insert(touched.end(), fp.begin(), fp.end());
  }
  changeTerrainCells(list, touched, events);
  OTE_INFO("story: applyChange '%s'", def.id.c_str());
}

void Battle::storyRevertChange(int change, std::vector<Event>& events) {
  if (!mapChanges || change < 0 || change >= (int)mapChanges->size()) {
    OTE_WARN("story: revertChange: the map has no change number %d", change);
    return;
  }
  size_t at = appliedChanges.size();
  for (size_t i = 0; i < appliedChanges.size(); ++i)
    if (appliedChanges[i].change == change) at = i;
  if (at == appliedChanges.size()) {
    OTE_INFO("story: revertChange '%s': it was not applied", (*mapChanges)[(size_t)change].id.c_str());
    return;
  }
  const MapChangeDef& def = (*mapChanges)[(size_t)change];
  const AppliedChange rec = appliedChanges[at];
  appliedChanges.erase(appliedChanges.begin() + (long)at);
  std::vector<std::pair<Cell, int>> list;
  for (size_t i = 0; i < def.cells.size() && i < rec.before.size(); ++i) list.push_back({grid.at(def.cells[i].x, def.cells[i].y), rec.before[i]});
  std::vector<Cell> touched;
  for (size_t i = 0; i < def.structures.size() && i < rec.beforeKinds.size(); ++i) {
    setStructureKind(def.structures[i].index, rec.beforeKinds[i], events);
    std::vector<Cell> fp;
    structureCells(def.structures[i].index, fp);
    touched.insert(touched.end(), fp.begin(), fp.end());
  }
  changeTerrainCells(list, touched, events);
  OTE_INFO("story: revertChange '%s'", def.id.c_str());
}

void Battle::storySetFog(bool on) {
  if (on == fog) return;
  if (!on) {
    markSight();  // what was pushed so far was seen as it was
    fog = false;
    lights.clear();  // lights only mean something with fog (and a save refuses them without it)
    sight_.valid = false;
    OTE_INFO("story: setFog: the fog lifts");
    return;
  }
  fog = true;
  sight_.valid = false;
  // Everybody knows who owns what, and what kind it is, at the moment the fog comes down (the same start as a fog map has).
  std::vector<int> owners, kinds;
  for (const StructureState& st : structures) {
    owners.push_back(st.owner);
    kinds.push_back(st.type);
  }
  seenOwners_.assign((size_t)teamCount, owners);
  seenKinds_.assign((size_t)teamCount, kinds);
  if (sightEvents_) stampFrom_ = sightEvents_->size();  // the events so far happened without fog: every team saw them
  OTE_INFO("story: setFog: fog comes down");
}

void Battle::storySetVision(int amount) {
  amount = std::max(-kMaxFogVision, std::min(kMaxFogVision, amount));
  if (amount == fogVision) return;
  markSight();
  fogVision = amount;
  sight_.valid = false;
  OTE_INFO("story: setVision %d", amount);
}

int Battle::skillVision(const Unit& u) const {
  if (!db->visionSkills) return 0;
  std::vector<const SkillEffect*> fx;
  std::vector<int> scale;
  standingEffects(u, false, fx, &scale);
  int sum = 0;
  for (size_t i = 0; i < fx.size(); ++i)
    if (fx[i]->kind == SkillEffect::Kind::Vision) sum += fx[i]->value * (scale.empty() ? 1 : scale[i]);
  return sum;
}

void Battle::lightWithItem(int unit, const ItemDef& item, std::vector<Event>& events) {
  const Unit& u = units[(size_t)unit];
  markSight();  // the events so far describe the world as it was; the light changes who sees what
  LightArea light;
  light.side = u.side;
  light.cell = u.cell;
  light.radius = item.lightRadius;
  light.turns = item.lightTurns;
  light.shrink = item.lightShrink;
  lights.push_back(light);
  sight_.valid = false;
  events.push_back(ev(EventType::AreaLit, unit, u.cell, light.radius, light.turns));
}

}  // namespace ote
