// PRIVATE members of class Battle for area attacks and line fire (docs/SRPG.md "Area attacks and line fire"). Included INSIDE the class
// body in battle.h (private section) and nowhere else.

// area_aim.cpp: a square the shot cannot pass (terrain or a structure that blocks sight, "blocksSight").
bool shotBlockedAt(Cell c) const;
// Does a strike of `attacker` with `weapon` hit the unit `u` standing in the area (alive, not the attacker, an enemy or friendly fire / berserk,
// and a weapon that can hurt it at all)?
bool areaHits(const Unit& attacker, int weapon, const Unit& u) const;
// area_forecast.cpp: the options an area weapon adds to Battle::attackOptions.
void areaOptions(int unit, Cell from, int weapon, bool mapWeapon, std::vector<AttackOption>& out) const;
// area_attack.cpp: an Attack action with an area weapon.
void applyAreaAttack(const Action& action, std::vector<Event>& events);
// The weapon of an attack is taken in hand (a carried weapon moves to slot 0).
void takeWeaponInHand(int unit, int weapon);
// While an area attack resolves: doStrike defers deaths (listed in `pendingKills_`, handled in order afterwards) and the attacker's strikes
// do not wear the weapon (the attack wears it once). Never saved: an attack is one action.
struct PendingKill { int unit = -1, killer = -1; bool wielder = false; };  // wielder: the attacker, felled by its own weapon's recoil
bool fightArea_ = false;
std::vector<PendingKill> pendingKills_;
// The usual end of a unit struck down by `killer`: its drops go to the killer, the fall, its last words and the "defeated" story events.
void defeatStruck(int unit, int killer, std::vector<Event>& events);
