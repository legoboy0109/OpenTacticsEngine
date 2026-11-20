// Battle setup from a loaded map.
#include "core/battle/battle.h"
#include "core/base/log.h"
#include "core/data/footprint.h"

namespace ote {

static bool hasTag(const std::vector<std::string>& tags, const char* tag) {
  for (const std::string& t : tags)
    if (t == tag) return true;
  return false;
}

bool Battle::init(const Database& database, const MapDef& map, Formulas& formulaHost,
                  uint32_t seed, std::string& err) {
  const bool askChoices = choice_.ask;  // the caller's choice of asking survives a new battle
  const int keepDifficulty = difficulty;  // so does the difficulty it is to be built at (docs/DIFFICULTY.md)
  const int keepTestLevel = testLevel;  // and the test level (docs/DIFFICULTY.md "Test level")
  *this = Battle();
  choice_.ask = askChoices;
  difficulty = keepDifficulty;
  testLevel = keepTestLevel;
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

  fog = startFog = map.fog;
  fogVision = startFogVision = map.fogVision;
  if (!map.changes.empty()) mapChanges = std::make_shared<const std::vector<MapChangeDef>>(map.changes);
  baseTerrain = std::make_shared<const std::vector<int>>(map.terrain);
  if (map.tileset >= 0 && map.tileCells.size() == map.terrain.size()) {  // the tile layer (docs/GRAPHICS.md "Tile layer")
    tileset = map.tileset;
    tiles = map.tileCells;
    baseTiles = std::make_shared<const std::vector<int>>(map.tileCells);
  }
  if (map.gridType != db->gridType) {
    err = "map '" + map.id + "' has a different kind of grid than the game";
    return false;
  }
  grid = Grid(map.gridType, map.width, map.height, map.hexLayout);
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
    s.difficultyShift = sd.difficultyShift;
    s.seesAll = sd.ai && (sd.aiSeesAll >= 0 ? sd.aiSeesAll == 1 : map.aiSeesAll);  // docs/ECONOMY.md "Fog of war"
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
    s.researched.assign(db->research.size(), 0);
    for (const Amount& a : sd.resources)
      if (a.resource >= 0 && a.resource < (int)s.resources.size()) s.resources[a.resource] += a.amount;
    // Factions (docs/ECONOMY.md): "choose" that nobody picked is settled here (a computer side by the seed); the faction's
    // start resources are added to the map's.
    int faction = sd.factionChoose ? defaultFactionChoice(*db, seed, (int)sides.size(), sd.ai) : sd.faction;
    if (faction >= (int)db->factions.size()) faction = -1;
    if (faction >= 0)
      for (const Amount& a : db->factions[(size_t)faction].start)
        if (a.resource >= 0 && a.resource < (int)s.resources.size()) s.resources[a.resource] += a.amount;
    for (const ItemGrant& g : sd.storage) {
      ItemStack st;
      st.item = g.item;
      st.uses = g.uses;
      s.storage.push_back(st);
    }
    sides.push_back(s);
    takeFaction((int)sides.size() - 1, faction);  // its side-wide skills (factions.cpp)
  }
  teamCount = (int)teamNames.size();
  // A team sees everything only when every one of its sides is a computer side that does.
  for (int t = 0; t < teamCount && t < 32; ++t) {
    bool all = true;
    for (const SideState& s : sides)
      if (s.team == t && !s.seesAll) all = false;
    if (all) seesAllTeams |= 1u << t;
  }

  for (int c = 0; c < cellCount; ++c) {
    CellState& cs = cells[c];
    cs.terrain = map.terrain[c];
    if (cs.terrain < 0 || cs.terrain >= (int)db->terrains.size()) {
      err = "map '" + map.id + "' references an unknown terrain";
      return false;
    }
    cs.height = c < (int)map.heights.size() ? map.heights[c] : 0;
  }

  // Structures: the loader already checked these, but maps can also be built
  // in code (tests, tools), so refuse inconsistent ones here too.
  structureAt.assign(cellCount, -1);
  for (const MapStructureDef& ms : map.structures) {
    if (ms.type < 0 || ms.type >= (int)db->structures.size()) {
      err = "map '" + map.id + "' references an unknown structure type";
      return false;
    }
    const StructureDef& def = db->structures[ms.type];
    std::vector<Cell> fits;
    if (!structureFootprint(grid, def, ms.x, ms.y, fits)) {
      err = "map '" + map.id + "': structure '" + def.id + "' at " + std::to_string(ms.x) + "," +
            std::to_string(ms.y) + " does not fit inside the map";
      return false;
    }
    StructureState s;
    s.type = s.mapType = ms.type;
    s.x = ms.x;
    s.y = ms.y;
    s.owner = ms.side >= 0 && ms.side < (int)sides.size() ? ms.side : -1;
    s.capture = def.capturePoints;
    // Doors, chests and visitable places (docs/SRPG.md section 10).
    s.id = ms.id;
    s.loot = ms.loot;
    s.visitSides = ms.visitSides;
    s.visitUnits = ms.visitUnits;
    if (def.place.isDoor() || def.place.isChest()) s.locked = ms.locked >= 0 ? ms.locked == 1 : def.place.locked;
    if (def.place.isDoor() && def.place.hp > 0) s.hp = def.place.hp;
    if (def.mapWeapon.has()) {  // a usable weapon: its uses (the placement's own number, else the kind's), docs/SRPG.md "Map weapons"
      s.weaponFor = s.type;
      s.weaponUses = ms.weaponUses >= 0 ? ms.weaponUses : def.mapWeapon.uses;
    }
    int index = (int)structures.size();
    structures.push_back(s);
    std::vector<Cell> footprint;
    structureCells(index, footprint);
    for (Cell c : footprint) {
      if (structureAt[c] != -1) {
        err = "map '" + map.id + "': structure '" + def.id + "' at " + std::to_string(ms.x) + "," +
              std::to_string(ms.y) + " overlaps another structure";
        return false;
      }
      structureAt[c] = index;
    }
    if (s.owner >= 0 && hasTag(def.tags, "hq")) sides[s.owner].hadHq = true;
  }

  // Difficulty (docs/DIFFICULTY.md): the numbers for the chosen one, with this map's own on top; before any unit is built.
  if (!map.difficulty.empty()) diffMap = std::make_shared<const std::vector<MapDifficulty>>(map.difficulty);
  resolveDifficulty();
  if (difficulty >= 0)
    OTE_INFO("battle: difficulty \"%s\" (enemy levels %+d, xp %d%%, resources %d%%)", db->difficulties.list[(size_t)difficulty].id.c_str(),
             diffEnemy.levels, diffXpPercent, diffResourcePercent);

  for (const MapUnitDef& ud : map.units) {
    if (!appearsOnDifficulty(ud)) continue;  // a unit for other difficulties only
    Cell c = grid.at(ud.x, ud.y);
    if (c == kNoCell || occupant[c] != -1 || ud.side < 0 || ud.side >= (int)sides.size() ||
        ud.type < 0 || ud.type >= (int)db->unitTypes.size()) {
      err = "map '" + map.id + "' has an invalid unit placement";
      return false;
    }
    Unit u = makeUnit(ud);
    u.id = (int)units.size();
    u.cell = c;
    occupant[c] = u.id;
    sides[u.side].everHadUnits = true;
    units.push_back(u);
  }
  assignFacings();  // rules.facing: the units the map gave no facing look at the nearest foe (all of them stand on the board now)

  // Fog: every team starts out knowing who owns what at the map's start.
  if (fog) {
    std::vector<int> owners;
    for (const StructureState& st : structures) owners.push_back(st.owner);
    seenOwners_.assign((size_t)teamCount, owners);
    std::vector<int> kinds;
    for (const StructureState& st : structures) kinds.push_back(st.type);
    seenKinds_.assign((size_t)teamCount, kinds);
  }

  storyState.weather = map.weather;  // the map's own weather ("" = clear); a story `weather` command changes it later
  // Story: the map's events and objectives, everything off / 0 / not yet run.
  story = map.story;
  storyState.switches.assign(db->switches.size(), 0);
  storyState.variables.assign(db->variables.size(), 0);
  if (story) {
    storyState.fired.assign(story->events.size(), 0);
    for (const StoryEvent& ev : story->events)
      if (ev.when.kind == Trigger::Kind::Turn && ev.when.side >= (int)sides.size()) {
        err = "map '" + map.id + "': event '" + ev.id + "' names a side that does not exist";
        return false;
      }
  }
  applyTestLevel();  // docs/DIFFICULTY.md "Test level": nothing unless asked for
  quotesInit();  // docs/EVENTS.md "Battle and defeat quotes": nothing said yet (none in a game without quotes)
  supportsInit();  // a single battle starts every support pair at its "start" points (docs/SRPG.md); none = nothing
  return true;
}

Unit Battle::makeUnit(const MapUnitDef& ud) const {
  Unit u;
  u.type = ud.type;
  u.side = ud.side;
  u.name = ud.name.empty() ? db->unitTypes[ud.type].name : ud.name;
  u.storyId = ud.id;
  const int placedLevel = difficultyLevel(ud);  // docs/DIFFICULTY.md: an enemy of a harder / easier difficulty is built at another level
  u.level = placedLevel;
  u.xp = ud.xp;  // 0 unless the map says (rank-table games)
  u.leader = ud.leader;
  u.facing = ud.facing;  // -1 unless the map said (rules.facing); the default is given once the unit stands on the board
  u.aiGoals = ud.aiGoals;  // docs/SRPG.md section 10
  u.behaviour = ud.behaviour;  // docs/AI.md
  u.jobs = ud.jobs;  // docs/JOBS.md
  u.stats = ud.stats;
  u.stats.resize(db->stats.size(), 0);
  for (const ItemGrant& g : ud.hasItems ? ud.items : db->unitTypes[ud.type].items) {
    ItemStack s;
    s.item = g.item;
    s.uses = g.uses;
    s.drop = g.drop;
    u.items.push_back(s);
  }
  dressUnit(u);  // its character and learned skills, before the hit points (a standing skill may raise the maximum)
  difficultyStats(u, ud, placedLevel - ud.level);  // flat and percent additions of the difficulty (nothing in a game without one)
  for (int s : ud.skills) learnSkill(u, s);  // the map unit's own skills ("skills" on the unit): learned for good, saved like any
  rankOverride(u, ud.weaponXp);  // a map unit's own ranks replace those of its class (docs/SRPG.md section 7)
  u.hp = maxHp(u);
  stockUnit(u);
  return u;
}

void Battle::structureCells(int s, std::vector<Cell>& out) const {
  out.clear();
  if (s < 0 || s >= (int)structures.size()) return;
  const StructureState& st = structures[s];
  const StructureDef& def = defOf(st);
  structureFootprint(grid, def, st.x, st.y, out);
}

void Battle::start(std::vector<Event>& events) {
  touch();
  if (askingChoices()) {  // a question may stop the start half way (story_choice.cpp)
    runAsking(true, Action(), nullptr, {}, 0, events);
    return;
  }
  startNow(events);
}

void Battle::startNow(std::vector<Event>& events) {
  EventScope scope(*this, events);
  sight_.valid = false;  // state may have been set up by hand since init (roster, research, tests)
  activeSide = 0;
  round = 1;
  StoryOccurrence o;  // "before the first turn": the first turn's own events come after TurnStarted
  o.kind = Trigger::Kind::BattleStart;
  fireStory(o, events);
  if (speedGame()) {  // docs/TURN_ORDER.md: round 1 begins for every side, then the clock runs to the first unit
    if (!over) speedStart(events);
    if (story) settle(events);
    return;
  }
  if (!over) beginTurn(events);  // an event may have decided the battle already
  if (story) settle(events);  // an event may already have decided things
  if (!over && sides[(size_t)activeSide].defeated) advanceTurn(events);  // the first side ran out of fuel
}

bool Battle::canAct(int unit) const {
  if (over || unit < 0 || unit >= (int)units.size()) return false;
  const Unit& u = units[unit];
  return u.alive && ownsTurn(u);
}

}  // namespace ote
