// PUBLIC members of class Battle for changing the map during a battle (contract: docs/EVENTS.md "Changing the map") and for the
// fog options that story commands switch (docs/ECONOMY.md "Fog of war"). Included INSIDE the class body in battle.h (public section)
// and nowhere else. Definitions: mapchange.cpp.

// The map's named changes (null = none) and the terrain the map started with (always set after init); shared by copies, not saved.
std::shared_ptr<const std::vector<MapChangeDef>> mapChanges;
std::shared_ptr<const std::vector<int>> baseTerrain;
// The changes applied and not reverted, in the order they were applied. Saved only when not empty.
std::vector<AppliedChange> appliedChanges;
// Fog: the vision adjustment added to every unit's vision now (the map's "fogVision" until a story command sets it), and what fog
// and the adjustment were when the battle began (a save only writes what differs). `seesAllTeams`: bit `t` = team `t` is made of computer
// sides that see everything (fixed at the start).
int fogVision = 0;
bool startFog = false;
int startFogVision = 0;
uint32_t seesAllTeams = 0;
// Is the change applied now?
bool changeApplied(int change) const;
// Story commands. All of them do nothing (and log) for an index that does not exist. setTerrain gives the squares the terrain at once;
// applyChange does nothing when the change is applied already, revertChange nothing when it is not. A unit left on a square it cannot
// stand on is put on the nearest free square it can (see mapchange.cpp); the squares' heights never change.
// `tile` / `keepTile`: the tile layer (docs/GRAPHICS.md "Tile layer"): the squares' new tile (kTileUnset = not given: a square whose terrain changes
// loses its tile), or keepTile = leave tiles as they are.
void storySetTerrain(const std::vector<Cell>& cells, int terrain, std::vector<Event>& events, int tile = -2, bool keepTile = false);
void storyApplyChange(int change, std::vector<Event>& events);
void storyRevertChange(int change, std::vector<Event>& events);
// Fog on / off: switching it on starts every team's memory of structures afresh from the truth, switching it off drops the lights.
void storySetFog(bool on);
// The vision adjustment (-kMaxFogVision..kMaxFogVision).
void storySetVision(int amount);
// For loading a save: what a square holds (checked by the caller); marks sight stale.
void restoreTerrain(Cell cell, int terrain) { cells[(size_t)cell].terrain = terrain; sight_.valid = false; }
// The kinds the map's changes turn this structure into (for checking a save).
std::vector<int> changeKindsOf(int structure) const;
// The pieces the commands are made of (mapchange.cpp): a structure becomes another kind (StructureUpgraded event); squares get a
// terrain (TerrainChanged event, then the stranded-unit rule; `touched` = extra squares to check, e.g. a changed structure's).
void setStructureKind(int structure, int kind, std::vector<Event>& events);
// `tileList` (optional, parallel to `list`): per square the new tile, -1 = none, kTileUnset = none given (cleared when the terrain really changes),
// kTileKeep = leave it. A square whose tile changes is reported by the TerrainChanged event even when its terrain did not.
void changeTerrainCells(const std::vector<std::pair<Cell, int>>& list, const std::vector<Cell>& touched, std::vector<Event>& events,
                        const std::vector<int>* tileList = nullptr);
// The tile layer (docs/GRAPHICS.md "Tile layer (hand-painted maps)"): presentation only, never hashed. `tiles` is empty when the map has none;
// else one entry per square, the tile number or -1. `tileset` = index into db->graphics.tilesets. `baseTiles` = the layer as the map file gave it
// (a save writes only what differs).
int tileset = -1;
std::vector<int> tiles;
std::shared_ptr<const std::vector<int>> baseTiles;
bool hasTiles() const { return !tiles.empty(); }
int tileAt(Cell c) const { return tiles.empty() ? -1 : tiles[(size_t)c]; }
// For loading a save (checked by the caller).
void restoreTile(Cell cell, int tile) { tiles[(size_t)cell] = tile; }
// A usable item with "light": lights the square the unit stands on for its side (the same light a staff makes; an AreaLit event).
void lightWithItem(int unit, const ItemDef& item, std::vector<Event>& events);
// The vision a unit's standing skills add (docs/ECONOMY.md "Fog of war"; 0 in a game with no such skill).
int skillVision(const Unit& u) const;
// A team that sees everything although the map has fog (a computer side marked "aiSeesAll").
bool teamSeesAll(int team) const { return team >= 0 && team < 32 && ((seesAllTeams >> team) & 1u); }
// How far the unit sees now: its type, the square, the map's adjustment and its skills (the rule sight uses).
int unitVision(const Unit& u) const { return visionOf(u); }
