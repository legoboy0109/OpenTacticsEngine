// PUBLIC members of class Battle for the height rules (contract: docs/SRPG.md "Height rules"). Included INSIDE the class body in battle.h
// (public section) and nowhere else. Definitions: height_rules.cpp. In a game without "rules.height" every query answers as if the rules did not
// exist (no jump limit, no extra range, no height limit on a weapon), so callers may use them without checking heightGame() first.

// The game plays with height rules.
bool heightGame() const { return db->rules.height.on; }
// The most `u` may climb / drop in one step: its unit type's "jump", else the game's default; kNoHeightLimit for a flying move type and in a
// game without height rules.
void jumpOf(const Unit& u, int& up, int& down) const;
// Heights only: may a unit with this jump step from `from` to the square `to` (a neighbour)?
bool stepAllowed(int jumpUp, int jumpDown, Cell from, Cell to) const {
  const int d = heightAt(to) - heightAt(from);
  return d <= jumpUp && -d <= jumpDown;
}
// Extra squares of maximum range `weapon` gets when fired from `from` at `to` (rules.height.rangeBonus; 0 for a weapon that reaches one square).
int rangeBonusAt(int weapon, Cell from, Cell to) const;
// The most extra range the rules can ever give `weapon` (what a scan of the squares around a shooter has to cover).
int rangeBonusMost(int weapon) const;
// THE range test of an attack: the distance is within the weapon's range (the maximum raised by the height bonus) and the target's height is
// within the weapon's "heightRange". Every attack check (targets, forecasts, counters, the computer opponent, the shown ranges) goes through
// weaponAims, which asks this first; the walk of a unit through reachFor.
bool weaponReaches(int weapon, Cell from, Cell to) const;
// Is a square strictly between `from` and `to` (on their straight line; false when they are not on one) higher than both of them? Only line fire
// with rules.height.lineOfSight is stopped by it.
bool highGroundBetween(Cell from, Cell to) const;
// Why a shot of `weapon` from `from` at `to` is refused for a reason of height ("too high to reach", "too low to hit", "the high ground is in the
// way"), or null when height is not the reason (or the game has no height rules).
const char* heightAimRefusal(int weapon, Cell from, Cell to) const;
// Why a Move of `unit` to `cell` is refused for a reason of height ("too steep to climb here"), null when the walk is not what height stopped.
const char* heightMoveRefusal(int unit, Cell cell) const;
// The squares an attack of `weapon` from `from` aimed at `target` covers: grid/area.h's areaCells, less the squares more than the area's "vertical"
// levels above or below the aimed square. Everything that wants the squares of an area attack asks this, not areaCells.
void areaCover(int weapon, Cell from, Cell target, std::vector<Cell>& out) const;
