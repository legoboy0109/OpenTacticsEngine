// PUBLIC members of class Battle for facing (contract: docs/SRPG.md "Facing"). Included INSIDE the class body in battle.h (public section) and nowhere
// else. Definitions: facing_rules.cpp. In a game without "rules.facing" every query answers as if facing did not exist (every attack is a front
// attack, units have no facing), so callers may use them without checking facingGame() first.

// The game plays with facing: every unit has one (Unit::facing, a direction number of Grid::neighbor).
bool facingGame() const { return db->rules.facing.on; }
// The arc (kArcFront / kArcSide / kArcBack of grid/facing.h; formulas read it as `arc`) of an attack from the square `from` on a unit standing on
// `at` that faces `facing`. THE function every arc comes from (forecasts, strikes, skill conditions, the computer opponent): kArcFront without facing.
int arcAt(Cell at, int facing, Cell from) const;
// The arc of an attack on `defender` from the square `from` (the defender's own square and facing).
int arcOfAttack(const Unit& defender, Cell from) const { return arcAt(defender.cell, defender.facing, from); }
// The facing a unit gets when it enters the battle on `cell` without one the map gave it: toward the nearest unit of another team than `team` (the
// lowest unit id among equals; the lower direction number on an exact diagonal), else south (squares) / the direction opposite north (hexagons).
int defaultFacing(Cell cell, int team) const;
// The word the forecast shows for an arc ("Back"; game.json rules.facing.arcs may rename it).
const std::string& arcName(int arc) const { return db->rules.facing.arcName[arc < 0 ? 0 : arc > 2 ? 2 : arc]; }
// Gives every unit on the board that has no facing yet its default (Battle::init after all the map's units stand there; a save that has none).
void assignFacings();
