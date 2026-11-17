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

// ---- weapon ranks and stat caps (ranks.cpp, caps.cpp; docs/SRPG.md section 7) ----
// The unit's rank in a weapon group (index into db->ranks.ladder, 0 = lowest; 0 in a game without ranks).
int weaponRank(const Unit& u, int group) const;
// Does the unit's rank in the weapon's group reach the weapon's "rank"? False with "Needs rank B" in `why` (optional).
bool meetsRank(const Unit& u, int weapon, std::string* why = nullptr) const;
// Is the unit's character on the weapon's "wieldedBy" list (true when the weapon has none)? False with "Only Kell can
// wield this" in `why` (optional). Character-locked weapons: docs/SRPG.md.
bool meetsCharacterLock(const Unit& u, int weapon, std::string* why = nullptr) const;
// May the unit wield this weapon NOW: its class can (Database::canWield) and its rank is high enough. Weapons that
// fail only on rank can still be carried, traded and stored. The one test weaponsOf / weaponSlot / equippedSlot / Equip use.
bool canWieldNow(const Unit& u, int weapon, std::string* why = nullptr) const;
// The highest this unit's permanent stat can grow to (class cap, else game cap, plus the character's adjustment); -1 = none.
int statCap(const Unit& u, int stat) const;
// Would these per-stat gains change anything (stats at their cap do not rise)?
bool canRaiseAny(const Unit& u, const std::vector<int>& gains) const;

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
// Test-only: when set, its functions run after the real sources of modifiers and may change what they gave
// (core/battle/combat_mods.h). Null everywhere outside the unit tests; never saved; a copied battle shares it.
const CombatTestMods* testMods = nullptr;

// ---- skills, step K1 (skills.cpp, skill_standing.cpp) ----
// The weapon a unit has in use outside a combat: the equipped weapon (carried weapons) or the type's first weapon
// (fixed weapons); -1 = unarmed. `skillsOf(u, -1, ...)` uses it. In `skillsOf` the weapon value -2 means "no weapon
// at all" (a defender with no answering weapon): no weapon skills.
int restingWeapon(const Unit& u) const;

// ---- menu abilities and summons, step A1 (abilities.cpp, summons.cpp) ----
// What the unit has used of an ability this battle (null = never used it).
const AbilityUse* abilityUseOf(const Unit& u, int ability) const;
// The living summons (on the board or carried) the unit made with this ability.
int summonCount(int summoner, int ability) const;
// Is the unit a summoned unit (it does not keep its side in the battle)?
bool isSummoned(const Unit& u) const { return u.summoner >= 0; }

// ---- staves (staves.cpp; docs/SRPG.md section 8) ----
// May the unit use the staff (an index into db->items) now: its class has the staff's group, the character lock, the rank.
// False with the plain reason ("Needs rank C", "Only Mara can wield this", "the unit cannot wield that").
bool canWieldStaff(const Unit& u, int item, std::string* why = nullptr) const;
// The rank and lock tests of a weapon-like record (a weapon's data, or a staff's `gear`): the weapon-index versions above
// and canWieldNow delegate to these.
bool meetsRankFor(const Unit& u, const WeaponDef& w, std::string* why = nullptr) const;
bool meetsLockFor(const Unit& u, const WeaponDef& w, std::string* why = nullptr) const;
// The reach of the staff for this unit: min 1 .. max (a formula's value is clamped to 0..20; max 0 = nobody in reach).
void staffRange(const Unit& u, int item, int& minRange, int& maxRange) const;
// One entry per staff the unit carries, in inventory order, if it stood on `from`.
void staffOptions(int unit, Cell from, std::vector<StaffOption>& out) const;
// The units that may be picked for the staff in `slot` if the unit stood on `from`; a staff that picks nobody (area
// "all", target "self") returns the user alone when it would do something, meaning "confirm". Empty = unusable now.
void staffTargets(int unit, int slot, Cell from, std::vector<int>& out) const;
// What the use would do (no randomness). False when it is not legal (see validate).
// Stage 2 (docs/SRPG.md section 8): `dest` = a warp's chosen square and `itemSlot` = a repair's chosen item (slot in the target's
// inventory), both optional (the forecast says what is still to pick); for a light staff `target` is the picked SQUARE.
bool staffForecast(int unit, int slot, int target, Cell from, StaffForecast& out, Cell dest = kNoCell, int itemSlot = -1) const;
// A warp's destinations for the picked unit: free squares it could stand on (and, with fog, that the team sees) within the
// warp's reach of the user standing on `from`. Ascending cell index.
void staffWarpCells(int unit, int slot, int target, Cell from, std::vector<Cell>& out) const;
// The items of `target` a repair could mend (slots in its inventory, ascending): not unlimited, not full, not "repairable": false,
// and not the staff in use when the target is the user.
void staffRepairSlots(int unit, int slot, int target, std::vector<int>& out) const;
// A light staff's squares: those within its range of `from` where it would show the team something it does not see now.
// Empty on a map without fog.
void staffLightCells(int unit, int slot, Cell from, std::vector<Cell>& out) const;
// A pull's square: the free square next to the user (on `from`) the target could stand on that is nearest to it, ties to the
// lower cell number; kNoCell when there is none or the target already stands next to the user.
Cell staffPullCell(int unit, int target, Cell from) const;
// Why `target` is not a valid pick for the staff in `slot` ("" = valid): the words the player says when a unit is tapped.
std::string staffTargetWhy(int unit, int slot, int target, Cell from) const;
// May the unit stand on `cell`: on the map and its movement type can enter the terrain (ignores who is there). The one rule
// every staff move, drop and ability uses for "legal to stand on".
bool staffCanStand(const Unit& t, Cell cell) const { return grid.valid(cell) && enterCost(cell, typeOf(t).moveType) > 0; }
// The warp's reach (min .. max) from the user for this staff effect (a formula judged on the user's stats; max 0 = nowhere).
void staffWarpRange(const Unit& user, const StaffDef& d, const StaffEffect& fx, int& minRange, int& maxRange) const;
// The lights staves have made and that still last (docs/SRPG.md section 8, stage 2). Saved only when not empty.
std::vector<LightArea> lights;

// ---- conditions (conditions.cpp; docs/SRPG.md section 9) ----
// The unit has the condition (an index into db->conditions) / how many of its own turns it still lasts (-1 = no count; also -1 when absent).
bool hasCondition(const Unit& u, int condition) const;
int conditionLeft(const Unit& u, int condition) const;
// Does a standing effect (a skill's "immune") make the unit immune to the condition?
bool conditionImmune(const Unit& u, int condition) const;
// The first condition of the unit (in the order applied) whose blocks stop it acting / moving, or make it act by itself (berserk); -1 = none.
int conditionBlock(const Unit& u, ConditionBlock what) const;
// May an order be given to the unit: false with the plain reason when a condition stops it acting or acts for it.
bool unitControllable(const Unit& u, std::string* why = nullptr) const;
// What a unit that cannot act still does: answer a strike (its conditions' "counter" option), dodge one ("dodge"). True for a unit that is not stopped.
bool mayCounterWhileStopped(const Unit& u) const;
bool mayDodgeWhileStopped(const Unit& u) const;
// Does a condition forbid the unit this weapon (its group, or weapons at all)? `why` gets the plain reason.
bool weaponForbidden(const Unit& u, int weapon, std::string* why) const;
// The first condition that forbids the unit this weapon / staff group, or this kind of item ("staff", "usable", "weapon"); -1 = none.
int conditionForbidsGroup(const Unit& u, const std::string& group) const;
int conditionForbidsKind(const Unit& u, ItemKind kind) const;
// What inflicting `condition` (turns 0 = its own) on the unit would do: 0 = it would take hold (new, or a longer duration), 1 = the unit is
// immune, 2 = it has it already for as long. Judged now, no randomness.
int inflictWhy(const Unit& u, int condition, int turns) const;
// Would a cure of these conditions (or "all") remove at least one the unit has?
bool cureWould(const Unit& u, const std::vector<int>& ids, bool all) const;
// The own turns a fresh application lasts (-1 = no count): the override, else the condition's own.
int conditionDuration(int condition, int turns) const;
