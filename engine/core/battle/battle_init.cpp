// Battle setup from a loaded map.
#include "core/battle/battle.h"

namespace ote {

static bool hasTag(const std::vector<std::string>& tags, const char* tag) {
  for (const std::string& t : tags)
    if (t == tag) return true;
  return false;
}

bool Battle::init(const Database& database, const MapDef& map, Formulas& formulaHost,
                  uint32_t seed, std::string& err) {
  *this = Battle();
  db = &database;
  formulas = &formulaHost;
  rng.seed(seed);

  if (db->rules.statHp < 0 || db->rules.statMove < 0) {
    err = "game does not define the hp/move stat roles";
    return false;
  }
  int cellCount = map.width * map.height;
  if (cellCount <= 0 || (int)map.terrain.size() != cellCount) {
    err = "map '" + map.id + "' has inconsistent dimensions";
    return false;
  }
  if (map.sides.size() < 2) {
    err = "map '" + map.id + "' needs at least two sides";
    return false;
  }

  grid = Grid(map.gridType, map.width, map.height);
  cells.resize(cellCount);
  occupant.assign(cellCount, -1);

  // Team names -> dense indices, in order of first appearance.
  std::vector<std::string> teamNames;
  for (const MapSideDef& sd : map.sides) {
    SideState s;
    s.id = sd.id;
    s.name = sd.name.empty() ? sd.id : sd.name;
    s.color = sd.color;
    s.ai = sd.ai;
    std::string team = sd.team.empty() ? sd.id : sd.team;
    int teamIndex = -1;
    for (size_t i = 0; i < teamNames.size(); ++i)
      if (teamNames[i] == team) teamIndex = (int)i;
    if (teamIndex < 0) {
      teamIndex = (int)teamNames.size();
      teamNames.push_back(team);
    }
    s.team = teamIndex;
    s.resources.assign(db->resources.size(), 0);
    for (const Amount& a : sd.resources)
      if (a.resource >= 0 && a.resource < (int)s.resources.size()) s.resources[a.resource] += a.amount;
    sides.push_back(s);
  }
  teamCount = (int)teamNames.size();

  for (int c = 0; c < cellCount; ++c) {
    CellState& cs = cells[c];
    cs.terrain = map.terrain[c];
    if (cs.terrain < 0 || cs.terrain >= (int)db->terrains.size()) {
      err = "map '" + map.id + "' references an unknown terrain";
      return false;
    }
    cs.height = c < (int)map.heights.size() ? map.heights[c] : 0;
    cs.owner = c < (int)map.owner.size() ? map.owner[c] : -1;
    if (cs.owner >= (int)sides.size()) cs.owner = -1;
    const TerrainDef& t = db->terrains[cs.terrain];
    cs.capture = t.capturePoints;
    if (cs.owner >= 0 && hasTag(t.tags, "hq")) sides[cs.owner].hadHq = true;
  }

  for (const MapUnitDef& ud : map.units) {
    Cell c = grid.at(ud.x, ud.y);
    if (c == kNoCell || occupant[c] != -1 || ud.side < 0 || ud.side >= (int)sides.size() ||
        ud.type < 0 || ud.type >= (int)db->unitTypes.size()) {
      err = "map '" + map.id + "' has an invalid unit placement";
      return false;
    }
    Unit u;
    u.id = (int)units.size();
    u.type = ud.type;
    u.side = ud.side;
    u.cell = c;
    u.name = ud.name.empty() ? db->unitTypes[ud.type].name : ud.name;
    u.level = ud.level;
    u.leader = ud.leader;
    u.stats = ud.stats;
    u.stats.resize(db->stats.size(), 0);
    u.hp = u.stats[db->rules.statHp];
    occupant[c] = u.id;
    sides[u.side].everHadUnits = true;
    units.push_back(u);
  }
  return true;
}

void Battle::start(std::vector<Event>& events) {
  activeSide = 0;
  round = 1;
  beginTurn(events);
}

bool Battle::canAct(int unit) const {
  if (over || unit < 0 || unit >= (int)units.size()) return false;
  const Unit& u = units[unit];
  return u.alive && u.side == activeSide;
}

}  // namespace ote
