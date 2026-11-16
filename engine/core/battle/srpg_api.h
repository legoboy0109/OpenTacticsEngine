// PUBLIC members of class Battle for the strategy-RPG features (contract: docs/SRPG.md). This file is included
// INSIDE the class body in battle.h (public section) and nowhere else; it is not a header of its own.
//
// It exists so that the steps after F1 never edit battle.h: a step that needs one more public query appends it
// here (APPEND ONLY; do not change what other steps are built against). Definitions live in each area's file.

// ---- characters (characters.cpp) ----
// The chance in percent for `stat` to rise on a level up: the unit type's growth, plus the character's and the
// standing skills' once those exist. The one place this rule lives (progression.cpp reads it).
int growthOf(const Unit& u, int stat) const;
// The character a unit with this story id is (index into db->characters), -1 = none. Unit::character is this
// value, derived whenever a unit is created, dressed from a roster or restored from a save.
int characterOf(const Unit& u) const;

// ---- "does this unit still exist" (docs/SRPG.md "off the board") ----
// Not defeated: on the board, or carried / behind a partner (alive == false but it keeps its HP).
bool exists(const Unit& u) const { return u.alive || u.carriedBy >= 0; }

// ---- skills (skills.cpp and friends) ----
// The unit's skill list in judging order, each skill once: the type's, the learned ones (oldest first), the
// character's, accessories in inventory order, the weapon in use. `weapon` = the weapon it fights with (index into
// db->weapons), or -1 = outside a combat (the equipped weapon / the type's first weapon).
void skillsOf(const Unit& u, int weapon, std::vector<int>& out) const;
bool hasSkill(const Unit& u, int skill) const;
// Does one condition of a skill block or an ability hold for this owner (and foe)?
bool skillCondHolds(const SkillCond& cond, const SkillCtx& ctx) const;

// ---- promotion (promotion.cpp) ----
// The unit types the unit could become now: through the item in inventory slot `slot`, or (`slot` = -1) through an
// offer at its level. Empty = no Promote entry.
void promotionOptions(int unit, int slot, std::vector<int>& out) const;

// ---- menu abilities and summons (abilities.cpp and friends) ----
// The abilities the unit has: its type's, its character's, then its skills', each once.
void abilitiesOf(const Unit& u, std::vector<int>& out) const;
// One menu entry per ability it has, if it stood on `from`.
void abilityOptions(int unit, Cell from, std::vector<AbilityOption>& out) const;
// Targets if the unit stood on `from`: units for unit targets, squares for "cell"; a "self" ability returns the unit.
void abilityTargets(int unit, int ability, Cell from, std::vector<int>& units, std::vector<Cell>& cells) const;

// ---- rescue and pair up (rescue.cpp, pair_up.cpp) ----
void rescueTargets(int unit, Cell from, std::vector<int>& out) const;  // carry: Rescue; pairUp: PairUp partners
void dropCells(int unit, Cell from, std::vector<Cell>& out) const;
void giveTargets(int unit, Cell from, std::vector<int>& out) const;    // carry only
void takeTargets(int unit, Cell from, std::vector<int>& out) const;    // carry only
bool canSwitchPair(int unit) const;                                     // pairUp only

// ---- supports (supports.cpp, supports_campaign.cpp) ----
// The partners (unit ids) the unit could have a ready support conversation with if it stood on `from`.
void supportOptions(int unit, Cell from, std::vector<int>& out) const;
// Hands in a campaign's closeness. Call after init(), before start() (like the roster). A state that does not fit
// the game's pairs is ignored.
void setStartingSupports(const SupportState& carried);

// ---- combat pipeline (combat_mods.cpp, step F2) ----
// Fills both fighters' modifiers by asking the sources in order: the attacker's own skill blocks, the defender's,
// auras of other units, supports.
void combatMods(const CombatSetup& setup, CombatMods& attacker, CombatMods& defender) const;
// The order of strikes of a forecast fight (for the player's forecast and for applyAttack).
void strikeOrder(const Forecast& forecast, std::vector<StrikeStep>& out) const;

// ---- skills, step K1 (skills.cpp, skill_standing.cpp) ----
// The weapon a unit has in use outside a combat: the equipped weapon (carried weapons) or the type's first weapon
// (fixed weapons); -1 = unarmed. `skillsOf(u, -1, ...)` uses it. In `skillsOf` the weapon value -2 means "no weapon
// at all" (a defender with no answering weapon): no weapon skills.
int restingWeapon(const Unit& u) const;
