// The Database is everything a game defines in its data files: stats,
// terrain, weapons, unit types and the rule switches. It is read-only once
// loaded. Cross references are stored as indices into these vectors.
//
// File format: docs/GAME_FORMAT.md. Loader: core/data/loader.h.
#pragma once
#include <memory>

#include "core/data/action_points.h"
#include "core/data/ai_rules.h"
#include "core/data/behaviour.h"
#include "core/data/difficulty.h"
#include "core/data/facing_rules.h"
#include "core/data/unit_ranks.h"
#include "core/data/height_rules.h"
#include "core/data/jobs.h"
#include "core/data/graphics.h"
#include "core/data/map_change.h"
#include "core/data/map_weapon.h"
#include "core/data/places.h"
#include "core/data/speed_rules.h"
#include "core/data/weapon_area.h"
#include "core/data/ranks.h"
#include "core/base/types.h"
#include "core/grid/grid.h"

namespace ote {

struct StatDef { std::string id, name; };
// refill: "refill": { "perTurn": n, "carryOver": m } (docs/ECONOMY.md "Refilling resources"): at the start of the owning side's turn the amount becomes
// min(what is left, carryOver) + perTurn.
struct ResourceDef {
  std::string id, name;
  bool refill = false;
  int refillPerTurn = 0, refillCarryOver = 0;
};
struct MoveTypeDef { std::string id, name; };
// Story switches (off / on) and variables (whole numbers, start 0): docs/EVENTS.md.
struct SwitchDef { std::string id, name; };
struct VariableDef { std::string id, name; };

// An amount of one resource (costs, income, starting funds).
struct Amount {
  int resource = 0;
  int amount = 0;
};

struct TerrainDef {
  std::string id, name;
  uint32_t color = 0x808080;   // 0xRRGGBB placeholder draw color
  char glyph = ' ';            // optional placeholder letter
  // Movement cost to ENTER, indexed by move type. <= 0 means impassable.
  std::vector<int> moveCost;
  // "weatherCost" (docs/ECONOMY.md "Weather and movement cost"): [0] = while it rains, [1] = while it snows; per move type, -1 = no change, 0 = closed (cannot enter), n = cost.
  // Both empty = the terrain does not care about weather.
  std::vector<int> weatherCost[2];
  Props props;                 // formula-visible numbers (defense, avoid...)
  std::vector<std::string> tags;  // free-form labels (the "hq" tag moved to structures)
  // Fog of war (docs/ECONOMY.md "Fog of war"): squares a unit stands on add this to its vision (0..10); a unit on
  // a square that hides is seen only by units next to it.
  int visionBonus = 0;
  // Which units get the bonus: empty = every unit (the plain number), else unit type ids or tags (the same
  // matching as a weapon's "versus" keys). The loader refuses an empty list, so empty always means "all".
  std::vector<std::string> visionUnits;
  bool hides = false;
  // Fog: a unit looking along a straight line of squares does not see past a square of this terrain (a wall); the square itself is
  // seen (docs/ECONOMY.md "Fog of war", docs/EVENTS.md "Changing the map"). Only means something with fog.
  bool blocksSight = false;
};

// One way a structure kind can be built up into another (docs/ECONOMY.md "Upgradable structures").
struct UpgradeDef {
  int to = -1;                 // index into Database::structures; same footprint (checked by the loader)
  std::vector<Amount> cost;
  int needs = -1;           // index into Database::research, -1 = none
};

// A building-like thing standing on top of terrain squares (docs/STRUCTURES.md).
// It replaces only the rules it sets; everything else comes from the terrain
// under each of its squares. Capture, income, production and the "hq" tag
// live here and nowhere else.
struct StructureDef {
  std::string id, name;
  uint32_t color = 0x808080;   // placeholder block colour
  char glyph = ' ';            // placeholder letter
  int width = 1, height = 1;   // footprint in squares (1..kMaxStructureSize); always 1 x 1 in a hexagon game
  int rings = 0;               // hexagon games: 0 = one hexagon, 1 = the hexagon and its six neighbours (docs/HEX.md)
  // Per move type: -1 = not set (use the terrain's), 0 = cannot enter, > 0 = cost to enter.
  std::vector<int> moveCost;
  Props props;                 // only the numbers it sets
  int capturePoints = 0;       // > 0 makes it capturable
  std::vector<Amount> income;  // per turn to the owner (economy module), once per structure
  std::vector<int> produces;   // unit type indices buildable on its squares
  std::vector<int> sells;      // item indices a unit standing here can buy (docs/ITEMS.md "Shops")
  std::vector<std::string> tags;  // e.g. "hq"
  std::vector<UpgradeDef> upgrades;  // kinds it can be built up into (the structure changes kind during play)
  bool research = false;       // a side owning one may research (docs/ECONOMY.md "Research")
  // Repair (docs/ECONOMY.md "Repair and supply"): which units it repairs. Entries are unit type ids or tags
  // (like "versus" keys); repairsAll = no "repairs" key = every unit.
  bool repairsAll = true;
  std::vector<std::string> repairs;
  // Fog of war: like the terrain's keys; a structure's values ADD to those of the terrain under the unit
  // (bonuses are summed, "hides" on either one hides).
  int visionBonus = 0;
  std::vector<std::string> visionUnits;  // as TerrainDef::visionUnits
  bool hides = false;
  // Doors, chests and visitable places (docs/SRPG.md section 10): what a structure kind does when a unit interacts with it.
  PlaceDef place;
  // Fog: a unit looking along a straight line of squares does not see past the squares of this kind (a closed door); the
  // squares of the kind themselves are seen. Only means something with fog (a map's "fog").
  bool blocksSight = false;
  // Fog: a fixed light (a brazier): every square within this many squares of the structure's squares is in sight of EVERY team, all
  // the time. 0 = none, else 1..kMaxStaffRadius (docs/ECONOMY.md "Fog of war").
  int light = 0;
  // A weapon a unit standing here may attack with (docs/SRPG.md "Map weapons"); `has()` is false for any other structure.
  MapWeaponDef mapWeapon;
};

const int kMaxStructureSize = 8;

// Declared damage types (game.json "damageTypes", docs/SRPG.md "Damage types"). Empty list = the game has none and nothing changes.
struct DamageTypeDef {
  std::string id, name;
  bool ignoresTerrain = false;  // a strike of this type sees none of the defender's terrain numbers (`tb` is empty)
};
struct DamageTypes {
  std::vector<DamageTypeDef> list;
  bool hasRange = false;   // game.json "resistRange": [min, max] bounds every "resist" value
  int resistMin = 0, resistMax = 0;
  bool on() const { return !list.empty(); }
  int find(const std::string& id) const {
    for (size_t i = 0; i < list.size(); ++i)
      if (list[i].id == id) return (int)i;
    return -1;
  }
};
const int kResistUnset = -1000000;  // a character's "resist" entry that was not written (the unit type's value stands)

struct WeaponDef {
  std::string id, name;
  int minRange = 1, maxRange = 1;
  bool counter = true;     // may be used to counterattack
  bool afterMove = true;   // false = cannot attack after moving (artillery)
  Props props;             // formula-visible numbers (might, hit...)
  // Effectiveness table keyed by target unit type id or tag. The matching
  // number is passed to formulas as `vs`. When a weapon has a table and the
  // target matches no key and there is no default, it cannot target it.
  Props versus;
  bool hasVersusDefault = false;
  double versusDefault = 1;
  std::string damageFormula;  // optional per-weapon override (Lua expr)
  std::string group;          // optional, e.g. "sword": who can wield it as an item (docs/ITEMS.md)
  std::string damageKind;     // optional free-form word ("physical", "magic", ...): skill tests `weaponKind`, formulas `w.damageKind`
  int apCost = -1;            // weapons.json "apCost" (action points only): what one attack costs; -1 = the game's attack cost
  int damageType = -1;        // index into Database::damageTypes.list; -1 only in a game without damage types (else the first, by default)
  int ammo = 0;               // attacks per supply (1..99); 0 = unlimited (docs/ECONOMY.md "Repair and supply")
  // Area attacks and line fire (docs/SRPG.md "Area attacks and line fire"); both keys are optional, the defaults are the old single-target weapon.
  FireMode fire = FireMode::Direct;  // weapons.json "fire"
  AreaDef area;                      // weapons.json "area"
  ChargeDef charge;                  // weapons.json "chargeSpeed" etc. (speed turn order only; docs/TURN_ORDER.md "Charged actions")
  HeightRange heightRange;           // weapons.json "heightRange" (height rules only): how much higher / lower than the shooter the target may stand
  bool aimsStraight() const { return fire == FireMode::Line || area.shape == AreaShape::Line; }  // only along straight lines of the grid
  std::vector<int> skills;    // skills it gives while it is the weapon in use (docs/SRPG.md; indices into Database::skills)
  // Weapon ranks (docs/SRPG.md section 7); both -1 / 1 in a game without ranks.
  int rankGroup = -1;         // index into RankRules::groups (-1 = no group, or no ranks)
  int rankNeed = -1;          // lowest ladder index that may wield it; -1 = any
  int rankXp = 1;             // experience the user gains per use (growth "use")
  // Character lock (docs/SRPG.md "Character-locked weapons"): empty = anyone whose class can wield it.
  std::vector<std::string> wielderIds;  // weapons.json "wieldedBy", as written
  std::vector<int> wielders;            // the same as indices into Database::characters (filled once characters are loaded)
  // Unit-type lock (same section): "wieldedByTypes". With either list non-empty the unit's character OR its unit type must be on one.
  std::vector<std::string> wielderTypeIds;  // weapons.json "wieldedByTypes", as written
  std::vector<int> wielderTypes;            // the same as indices into Database::unitTypes (filled with `wielders`)
};

// ---- Items (docs/ITEMS.md) ----

enum class ItemKind { Usable, Weapon, Accessory, Staff };  // Staff: docs/SRPG.md section 8

struct ItemDef {
  std::string id, name, description;
  ItemKind kind = ItemKind::Usable;
  int uses = -1;               // how often it can be used when new; -1 = unlimited
  std::vector<Amount> price;   // empty = shops cannot sell it
  // Usable:
  bool targetAlly = false;     // false: used on the carrier; true: on a neighbouring unit of its team
  int heal = 0;                // HP restored; kHealFull = all of it
  std::vector<int> raise;      // per stat index: raised for good (empty or all 0 = none)
  // Weapon:
  int weapon = -1;             // index into Database::weapons
  // Accessory:
  std::vector<int> bonus;      // per stat index: added while carried
  // Strategy-RPG features (docs/SRPG.md):
  bool promotes = false;       // usable: a promotion item
  std::vector<int> skills;     // accessory: skills it gives while carried (indices into Database::skills)
  int staff = -1;              // staff: index into Database::staves (docs/SRPG.md section 8)
  bool repairable = true;      // false: a staff's "repair" never offers it (docs/SRPG.md section 8, stage 2)
  // Conditions (docs/SRPG.md section 9), a usable item: "cure": [ids] | "all" and "inflict": { "condition": id, "turns": n }
  // (on the user: an item that is used on an ally cures the ally, one that inflicts is always on the user).
  std::vector<int> cures;      // indices into Database::conditions
  bool cureAll = false;
  int inflict = -1;            // index into Database::conditions
  int inflictTurns = 0;        // 0 = the condition's own duration
  // Keys (docs/SRPG.md section 10): "opens": ["door", "chest", "<structure id>"]. `opensIds` as written; the loader turns them
  // into the two flags (any door / any chest) and structure kind indices once structures are loaded.
  std::vector<std::string> opensIds;
  bool opensDoors = false, opensChests = false;
  std::vector<int> opensKinds;
  bool isKey() const { return !opensIds.empty(); }
  bool stealable = true;       // false: a steal ability never takes it (docs/SRPG.md "Steal")
  Props props;                 // usable / accessory: numbers a steal condition reads as `it.weight` (a weapon or staff has its own props)
  bool sellable = true;        // false: a shop that buys items never takes it (docs/EVENTS.md "Preparation"); no price = never bought either
  // A usable item that lights the square its user stands on (a torch; docs/ECONOMY.md "Fog of war"): like a staff's "light". 0 = none.
  int lightRadius = 0, lightTurns = 1, lightShrink = 0;
};
const int kHealFull = 1000000000;

// One item as it is handed out by data: a unit's starting items, a side's starting storage.
struct ItemGrant {
  int item = 0;
  int uses = -1;      // what is left; -1 = unlimited (filled in from the item when the entry says nothing)
  bool drop = false;  // goes to whoever defeats the carrier
};

enum class ItemStorage { Off, Anywhere, Structure, Leader };

// What a FREE battle's shared storage holds when it starts (game.json rules.items.freeBattleStorage; docs/GAME_FORMAT.md).
// Resolved by loadFreeBattleStock after the items are known. `each` copies of every item (full uses) except `except`;
// `counts` overrides the number for the items it names (0 = none); `funds` is added to every human side's resources.
struct FreeStockCount {
  ItemGrant grant;  // item and the uses of one copy
  int count = 0;
};
struct FreeBattleStock {
  bool given = false;  // the key is present
  int each = 0;
  std::vector<int> except;  // item indices
  std::vector<FreeStockCount> counts;
  std::vector<Amount> funds;
};

struct ItemRules {
  bool carriedWeapons = false;  // false: a unit type's weapons, fixed; true: weapons are items
  int inventorySize = 5;
  ItemStorage storage = ItemStorage::Off;
  FreeBattleStock freeStock;    // see above
  int sellPercent = 50;         // what a shop that buys items pays: this share of the price, scaled by the uses left (0..100)
};
const int kMaxInventory = 20;

}  // namespace ote
// The strategy-RPG data structs (docs/SRPG.md). Here, not at the top: they hold ItemGrant lists.
#include "core/data/srpg.h"
#include "core/data/staff.h"
namespace ote {

struct UnitTypeDef {
  std::string id, name;
  char glyph = '?';
  int moveType = 0;
  std::vector<int> stats;    // base value per stat index
  std::vector<int> growths;  // % chance per stat to gain +1 on level up
  std::vector<int> weapons;  // weapon indices (with carried weapons: the ones it CAN wield)
  std::vector<std::string> wields;  // weapon groups it can wield (carried weapons)
  std::vector<ItemGrant> items;     // what a unit of this type starts with
  std::vector<Amount> cost;  // production cost
  std::vector<std::string> tags;
  bool canCapture = false;
  // Fuel (docs/ECONOMY.md "Repair and supply"): 0 = the type uses none.
  int fuel = 0;           // full tank (1..999)
  int fuelPerTurn = 0;    // lost at the start of each of its side's turns; 0 = only moving costs fuel
  bool supplies = false;  // refills fuel and ammunition of adjacent units of its side
  // Fog of war: how far its team sees from the square it stands on (grid distance), 0..kMaxVision.
  int vision = 2;
  // Strategy-RPG features (docs/SRPG.md); all empty in a game that uses none.
  std::vector<int> skills;               // skills a unit has while it is of this type (indices into Database::skills)
  std::vector<LearnDef> learns;          // skills learned for good at a level
  std::vector<int> abilities;            // menu abilities (indices into Database::abilities)
  std::vector<PromotionDef> promotions;  // the unit types it can become
  std::vector<int> pairBonus;            // pair up: per stat index, what a unit of this type gives when it is behind;
                                         // empty = rules.rescue.pairBonus
  // Weapon ranks and stat caps (docs/SRPG.md section 7); empty when the unit type says nothing.
  std::vector<int> rankStart;            // per RankRules::groups index: starting weapon experience
  std::vector<int> caps;                 // per stat index: this class's cap, -1 = none (overrides the game-wide one)
  std::vector<int> resist;               // per damage type (Database::damageTypes): flat resistance, read as `rs`; empty = all 0
  int apStep = -1;                       // units.json "ap": { "step" }: AP per movement point (action points only); -1 = the game's costPerStep
  int apSoftCap = -1;                    // units.json "ap": { "softCap" }: movement points per turn at the plain price; -1 = the game's softCap
  int maxLevel = -1;                     // units.json "maxLevel": this type's top level (0 = never levels); -1 = rules.progression.maxLevel
  int jumpUp = -1, jumpDown = -1;        // units.json "jump" (height rules only): the most it climbs / drops in one step; -1 = the game's default
};
// Does a unit type match a "units" / "versus" / "repairs" key: its id, or one of its tags.
inline bool unitTypeMatchesKey(const UnitTypeDef& t, const std::string& key) {
  if (t.id == key) return true;
  for (const std::string& tag : t.tags)
    if (tag == key) return true;
  return false;
}
// The sight bonus a square gives a unit of this type: `amount`, or 0 when the bonus is limited to other units.
inline int visionBonusFor(int amount, const std::vector<std::string>& units, const UnitTypeDef& t) {
  if (units.empty()) return amount;
  for (const std::string& key : units)
    if (unitTypeMatchesKey(t, key)) return amount;
  return 0;
}
const int kMaxVision = 20;
const int kMaxVisionBonus = 10;
const int kMaxFogTeams = 32;  // an event's "seen by" is a 32-bit mask with one bit per team

// A skill a side's units get as if they had it themselves (side-wide modifier, docs/SRPG.md "Side-wide modifiers").
// The filter is the unit filter format of the story ("type" / "tag" / {}), reduced to what exists without a map.
struct SkillGrant {
  int skill = -1;           // index into Database::skills
  int filterType = -1;      // only units of this unit type; -1 = any
  std::string filterTag;    // only units whose type has this tag; "" = any
};

// One technology (data/research.json, docs/ECONOMY.md "Research").
struct TechnologyDef {
  std::string id, name, description;
  std::vector<Amount> cost;    // paid when research starts
  int turns = 1;               // 0..99 of the side's own turns; 0 = finished at once
  std::vector<int> needs;   // technologies that must be finished first (all of them)
  std::vector<int> unlocksUnits;  // unit types: with any technology listing a type, a side needs one of them to build it
  std::vector<int> bonusTypes;    // unit types the bonus is for; empty = every unit of the side
  std::vector<int> bonus;         // per stat index, added to those units; empty = no bonus
  // Branching research (docs/ECONOMY.md "Branching research"); every key optional, all empty = a plain technology.
  std::string tree;               // display grouping only
  int tier = 0;                   // display order inside a tree only
  std::string exclusiveGroup;     // once one entry of a group is started or finished by a side, the others are closed to it
  std::vector<std::string> requiresStructureIds;  // as written; resolved into requiresStructures once structures are loaded
  std::vector<int> requiresStructures;            // structure kinds the side must own now to start it
  std::vector<Amount> bonusIncome;                // added to the side's per-turn income once finished (needs rules.economy)
  std::vector<SkillGrant> grants;                 // skills every matching unit of the side has once finished (Battle::skillsOf)
};
const int kMaxResearchTurns = 99;

// One faction (data/factions.json, docs/ECONOMY.md "Factions"): what a side that takes it may produce, build and research, what it
// starts with and what its units get side-wide. A list that was left out is no restriction (limitX false); one that was written
// is the whole set, even when empty.
struct FactionDef {
  std::string id, name, description;
  bool limitUnits = false, limitStructures = false, limitTechs = false;
  std::vector<int> units;       // unit types it may produce
  std::vector<int> structures;  // structure kinds it may build (an upgrade into one)
  std::vector<int> techs;       // technologies it may research
  std::vector<Amount> start;    // ADDED to what the map gives the side
  std::vector<SkillGrant> modifiers;  // side-wide skills from the start (same mechanism as a technology's "grants")
  bool hasWeights = false;      // "ai": { "weights": ... }: the attack weights of its computer units that have none of their own
  AiWeightSet weights;
  std::vector<std::string> tags;
  static bool allows(bool limited, const std::vector<int>& list, int index) {
    if (!limited) return true;
    for (int i : list)
      if (i == index) return true;
    return false;
  }
};

enum class TurnOrder { Sides, Speed };  // Speed: docs/TURN_ORDER.md

// game.json "rules.supply" (docs/ECONOMY.md "Repair and supply"); everything off when the block is missing.
struct SupplyRules {
  bool repair = false;        // own structures repair units at the start of the side's turn
  int repairAmount = 0;       // percent of max HP per turn (1..100)
  int repairCostShare = 100;  // percent of the proportional unit cost paid (0..100)
  bool structures = false;    // own structures also refill fuel and ammunition
};

// What happens to a unit that walks into a hidden enemy (game.json "rules.fog.ambush"; docs/ECONOMY.md "Fog of war").
enum class AmbushRule { EndsTurn, CanAct };

// Rule switches. Each genre is a different combination of these.
struct RulesConfig {
  TurnOrder turnOrder = TurnOrder::Sides;
  int statHp = -1;    // index of the stat used as hit points (required)
  int statMove = -1;  // index of the stat used as movement points (required)
  bool counterAttacks = true;

  // Defeat conditions: a side is out when any enabled one triggers.
  bool defeatNoUnits = true;     // lost every unit (after having had one)
  bool defeatLeaderDead = false; // a unit flagged "leader" died
  bool defeatLostHq = false;     // owns no "hq"-tagged structure (and started with one)

  bool economy = false;      // income from owned structures
  bool production = false;   // build units on producing structures
  bool builtUnitsCanAct = false;
  bool capture = false;      // units may capture capturable structures
  ItemRules items;           // docs/ITEMS.md
  SupplyRules supply;        // docs/ECONOMY.md "Repair and supply"
  bool hitRollAverage = false;  // "rules.hitRoll": "average2" = a strike's hit rolls two numbers and uses their average (docs/SRPG.md "The hit roll")
  int researchCancelRefundPercent = 0;  // "rules.research.cancelRefundPercent": share of the cost paid back when research is cancelled
  bool researchNeedsStructure = false;  // "rules.research.needsStructure": research needs an owned structure that allows it
  AmbushRule ambush = AmbushRule::EndsTurn;  // "rules.fog.ambush"
  bool progression = false;  // XP + level ups
  int xpPerLevel = 100;
  int xpNoDamageExpr = -1;     // "rules.progression.xpNoDamage" (an index into exprs; -1 = a fight that did no damage gives nothing)
  bool computerGains = true;   // "rules.progression.computerGains": false = only units of human sides gain experience
  int maxLevel = 20;
  RescueRules rescue;        // "rules.rescue" (docs/SRPG.md "Rescue and pair up"); mode Off when the block is missing
  PromotionRules promotion;  // "rules.promotion" (docs/SRPG.md "Promotion")
  PlaceRules places;         // "rules.places" (docs/SRPG.md section 10); the default changes nothing for a game without such structures
  AiRules ai;                // "rules.ai" (docs/AI.md "Scripted-style computer players")
  ActionPointRules ap;       // "rules.actionPoints" (docs/SRPG.md "Action points"); `on` is false when the block is missing
  HeightRules height;        // "rules.height" (docs/SRPG.md "Height rules"); `on` is false when the block is missing
  UnitRankRules unitRanks;   // "rules.progression.levels" (docs/SRPG.md "Unit ranks"); empty when the game levels up the usual way
  JobRules jobs;             // "rules.jobs" (docs/JOBS.md); only means something when `Database::jobs` is not empty
  SpeedRules speed;          // "rules.turnOrder": "speed" (docs/TURN_ORDER.md); `on` is false in the usual side-by-side game
  FacingRules facing;        // "rules.facing" (docs/SRPG.md "Facing"); `on` is false when the block is missing

  // Lua expressions; empty = feature off / engine default. See
  // docs/GAME_FORMAT.md "Formulas" for the variables available.
  std::string fDamage, fHit, fCrit, fStrikes;
  std::string fXpHit, fXpKill, fCaptureAmount;
  double critMultiplier = 3;
};

struct ViewConfig {
  int width = 480, height = 270;  // logical screen size in pixels
  int tileSize = 24;
};

// Which font the game's text uses (docs/GAME_FORMAT.md "font"). The core
// only carries this; the player resolves and loads it.
struct FontConfig {
  std::string name = "pixel";  // a built-in font id (assets/fonts/fonts.json)
  std::string file;            // font file inside the game folder; overrides name
  int size = 0;                // height in logical pixels; 0 = the font's default
  int smooth = -1;             // 1 = anti-aliased, 0 = crisp pixels, -1 = font's default
};

// A named, switchable look for the game (docs/GAME_FORMAT.md "styles").
// Today a style can recolor the placeholder graphics; tilesets/sprites
// will hang off the same ids later. Index 0 is always the default style.
// The kind of view a visual style draws the battle with (docs/VIEWS.md).
enum class StyleView { TopDown, Isometric, Isometric3D, Full3D };
// The kinds of view a hexagon game may use (docs/HEX.md "The other three views"): the loader refuses the others and the
// editor's helper reports these. Every kind is built (the 3D kinds: docs/HEX.md "Implementation notes (3D)"); a kind that
// is added to StyleView later stays refused here until its drawing exists.
inline bool hexGameAllowsView(StyleView v) {
  return v == StyleView::TopDown || v == StyleView::Isometric || v == StyleView::Isometric3D || v == StyleView::Full3D;
}

// Sizes of the isometric kinds of view, in logical pixels.
struct IsoConfig {
  int tileWidth = 0, tileHeight = 0;  // the diamond of one square; 0 = from the game's tileSize
                                      // (hexagon games: tileWidth is one top-down tileSize drawn; no tileHeight)
  int heightStep = -1;                // pixels per height level; -1 = half the diamond's height (hexagons: tileWidth / 4)
  int unitWidth = 0;                  // width unit pictures are drawn at; 0 = half the diamond's width
};

struct StyleDef {
  std::string id, name;
  StyleView view = StyleView::TopDown;
  IsoConfig iso;
  bool hasBackground = false;
  uint32_t background = 0;
  int showGlyphs = -1;  // 1/0 = force terrain letters on/off, -1 = default
  bool largeArt = false;  // "largeArt": needs more memory than a small console has; not offered there (views.h styleUsable)
  // Per terrain index: replacement color, or -1 to keep the terrain's own.
  std::vector<int32_t> terrainColors;
  // Same for structures' placeholder blocks (per structure index).
  std::vector<int32_t> structureColors;
};

struct MapRef { std::string id, name, file; };

struct CampaignDef;  // data/campaign.json (core/data/campaign.h)

// How much saving a game allows (docs/SAVES.md).
enum class SavingMode { Any, Continue, Off };
// "any" / "continue" / "off" -> mode; false for anything else.
bool parseSavingMode(const std::string& text, SavingMode& out);

struct Database {
  std::string name;
  std::string description, author;  // optional, shown in the launcher
  std::string rootPath;  // game folder, no trailing slash
  ViewConfig view;
  FontConfig font;
  RulesConfig rules;
  SavingMode saving = SavingMode::Any;  // game.json "saving"
  // game.json "battleScene": "scene" = fights are shown as the side-view battle scene unless the player chose otherwise;
  // "map" (default) = quick on-map fights (docs/BATTLE_SCENE.md). Presentation only.
  bool battleSceneDefault = false;
  bool anyWeatherCost = false;  // some terrain has a "weatherCost": Battle::enterCost looks at the weather only then
  // game.json "battleBackgroundFrom": whose square decides the battle scene's background and platform: "defender" (false, the default) or "attacker" (true).
  bool battleBackgroundFromAttacker = false;
  // game.json "battlePlatformFrom": "same" (false, the default: both fighters stand on the platform of the square that decides the background) or
  // "own" (true: each fighter's platform comes from the terrain it stands on; the background still follows battleBackgroundFrom).
  bool battlePlatformOwn = false;
  // game.json "battleSceneSides": which side of the battle scene each fighter stands on: 0 "map" (the default), 1 "playerRight", 2 "playerLeft".
  int battleSceneSides = 0;
  // game.json "battleSceneStaves": a staff used on one other unit plays in the battle scene (docs/BATTLE_SCENE.md "Staves in the scene").
  bool battleSceneStaves = false;
  // game.json "menuUnavailable": "hide" = the action menu leaves out entries that have nothing to act on (default "grey": today's menu,
  // where a few stay greyed); "attackWeaponChoice": "pick" = Attack asks which weapon first. Presentation only (docs/SRPG.md "Menu options").
  bool menuHideUnavailable = false;
  bool attackWeaponPick = false;
  // game.json "rules": { "display": { "enemyRanges": false } }: the game maker switches the player's enemy-range display off (default on;
  // docs/SRPG.md "Enemy ranges"). Presentation only: no rule and no save depends on it.
  bool enemyRanges = true;
  GridType gridType = GridType::Square;       // game.json "grid": one grid for the whole game (docs/HEX.md)
  HexLayout hexLayout = HexLayout::Pointy;    // only meaningful when gridType is Hex
  std::vector<StatDef> stats;
  std::vector<ResourceDef> resources;
  std::vector<MoveTypeDef> moveTypes;
  std::vector<SwitchDef> switches;       // game.json "switches" (optional)
  std::vector<VariableDef> variables;    // game.json "variables" (optional)
  std::vector<TerrainDef> terrains;
  std::vector<StructureDef> structures;  // data/structures.json (optional)
  std::vector<WeaponDef> weapons;
  std::vector<ItemDef> items;            // data/items.json (optional)
  std::vector<TechnologyDef> research;   // data/research.json (optional)
  std::vector<FactionDef> factions;      // data/factions.json (optional)
  std::vector<UnitTypeDef> unitTypes;
  std::vector<std::string> scripts;  // Lua files, relative to rootPath
  std::vector<MapRef> maps;
  std::vector<StyleDef> styles;  // never empty after loading; [0] = default
  Graphics graphics;             // art (data/graphics.json); empty = placeholders
  std::shared_ptr<const CampaignDef> campaign;  // data/campaign.json; null = no campaign (docs/EVENTS.md)
  // Strategy-RPG features (docs/SRPG.md); each empty when the game has no such file.
  std::vector<CharacterDef> characters;  // data/characters.json
  std::vector<SkillDef> skills;          // data/skills.json
  std::vector<AbilityDef> abilities;     // data/abilities.json
  SupportData supports;                  // data/supports.json (rules, pairs, texts)
  QuoteData quotes;                      // the wording of characters' quotes (data/characters.json "quotes")
  RankRules ranks;                       // data/ranks.json (weapon rank ladder); empty ladder = no ranks
  std::vector<int> computerCaps;         // data/caps.json "computerCaps": per stat index the cap for computer-controlled units, -1 = the game-wide one; empty = none
  std::vector<int> statCaps;             // data/caps.json: per stat index the game-wide cap, -1 = none; empty = no caps
  std::vector<StaffDef> staves;          // the staves of data/items.json (an item of kind "staff" points here)
  std::vector<ConditionDef> conditions;  // data/conditions.json (docs/SRPG.md section 9)
  std::vector<JobDef> jobs;              // data/jobs.json (docs/JOBS.md); empty = the game has no jobs
  std::vector<JobLearnDef> jobEntries;   // every job's learn entries in file order (JobLearnDef::flat is the index); built by the loader
  // Formula expressions of the new data (skill chances and conditions, rules.rescue.canCarry), in the order the
  // loaders registered them; compiled by Formulas::init and evaluated with Formulas::evalExpr(index, ...).
  std::vector<ExprDef> exprs;
  // Some skill has the effect moveAgain or actAgain: only then the turn flags moveSpent / acted / refreshed are
  // part of a save (set by resolveSrpgRefs).
  bool turnFlagSkills = false;
  // Some skill uses these effects (so the shared code can skip their work in a game that does not).
  bool nullifySkills = false, staffRangeSkills = false, carryBonusSkills = false, cancelEffectiveSkills = false;
  // Fog options in use (set by the loaders; they keep sight.cpp from looking when a game has none): a "vision" skill effect, a terrain
  // that blocks sight, a structure kind with a fixed "light".
  bool visionSkills = false, terrainBlocksSight = false, structureLights = false;
  bool straightWeapons = false;  // some weapon has "fire": "line" or a line-shaped "area" (Battle checks the line only then)
  bool scaledConditions = false;  // some condition has "scale": "turnsLeft" (docs/SRPG.md "Effects that fade")
  bool strikeNoUsed = false;  // some skill block tests or reads the strike number (docs/SRPG.md "One strike of a fight"): forecasts then work out each strike
  bool marksUsed = false;     // some skill block sets or tests a mark (docs/SRPG.md "Linking two strike blocks")
  bool refreshAbilities = false;  // some ability has the "refresh" effect (saves may then carry Unit::refreshedBy)
  DamageTypes damageTypes;        // game.json "damageTypes" (docs/SRPG.md "Damage types"); an empty list = the game has none
  DifficultyRules difficulties;   // game.json "difficulties" (docs/DIFFICULTY.md); an empty list = the game has none

  // Index lookups by id; -1 when not found.
  int findStat(const std::string& id) const;
  int findResource(const std::string& id) const;
  int findMoveType(const std::string& id) const;
  int findSwitch(const std::string& id) const;
  int findVariable(const std::string& id) const;
  int findTerrain(const std::string& id) const;
  int findStructure(const std::string& id) const;
  int findFaction(const std::string& id) const;
  int findWeapon(const std::string& id) const;
  int findItem(const std::string& id) const;
  int findTechnology(const std::string& id) const;
  // Carried weapons: may a unit of this type use this weapon?
  // The class side of "can this unit wield it": the class's `weapons` / `wields`, plus (when `character` >= 0) the groups of the
  // character's own `wields`, which only add (docs/SRPG.md "Weapon ranks"). Locks and ranks are Battle::canWieldNow's.
  bool canWield(int unitType, int weapon, int character = -1) const;
  bool canWieldGroup(int unitType, const std::string& group, int character = -1) const;  // also a staff's group
  int findUnitType(const std::string& id) const;
  int findMap(const std::string& id) const;
  int findStyle(const std::string& id) const;
  int findCharacter(const std::string& id) const;
  int findSkill(const std::string& id) const;
  int findCondition(const std::string& id) const;
  int findAbility(const std::string& id) const;
  int findJob(const std::string& id) const;
  bool jobsOn() const { return !jobs.empty(); }
  // The support pair of two characters (indices into `characters`, either order); -1 = they are not a pair.
  int findSupportPair(int characterA, int characterB) const;
  // Registers a formula expression; returns its index for Formulas::evalExpr.
  int addExpr(const std::string& text, const std::string& label);
};

// Does the formula text read the variable `word` (a whole name, not part of another name and not a field such as `a.word`)?
bool formulaReads(const std::string& text, const char* word);

// ---- A square = terrain + (optionally) the structure standing on it ----
// The ONLY place where terrain and structure rules are combined. Battle's
// queries and the map loader both go through these (structure -1 = none).

// Cost to enter a square for a move type: the structure's cost if it sets
// one for that movement type, else the terrain's. <= 0 = cannot enter.
int enterCostOf(const Database& db, int terrain, int structure, int moveType);

// The numbers formulas see for a square (`ta` / `tb`): the terrain's numbers
// with the structure's numbers of the same name replacing them and its other
// numbers added. Returns the terrain's own list (no copy) when there is no
// structure or it sets no numbers; otherwise fills `scratch` and returns it.
const Props& squareNumbers(const Database& db, int terrain, int structure, Props& scratch);

// ---- Map (one battle setup) ----

struct StoryData;  // events and objectives of a map (core/data/events.h)

struct MapSideDef {
  std::string id, name;
  uint32_t color = 0xffffff;
  std::string team;  // sides sharing a team are allies; defaults to id
  bool ai = false;
  std::vector<Amount> resources;  // starting amounts
  std::vector<ItemGrant> storage; // starting contents of the side's shared storage
  int aiSeesAll = -1;             // fog: -1 = follow the map's "aiSeesAll", 0 = this computer side is bound by fog, 1 = it sees everything
  bool difficultyShift = true;    // docs/DIFFICULTY.md: false = the enemy numbers of the difficulty never touch this computer side
  int faction = -1;               // "faction": index into Database::factions, -1 = none (docs/ECONOMY.md "Factions")
  bool factionChoose = false;     // "faction": "choose": the person picks before the battle (the player), a computer side by the seed (Battle::init)
};

struct MapUnitDef {
  std::string id;    // optional story id, unique in the map (docs/EVENTS.md "Named units"); empty = none
  int type = 0;
  int side = 0;
  int x = 0, y = 0;
  std::string name;  // empty = use the unit type name
  int level = 1;
  int xp = 0;        // "xp" (rank-table games only): the experience it starts with
  bool leader = false;
  std::vector<int> stats;  // final per-stat values (type base + overrides)
  bool hasItems = false;         // the map gives this unit its own items (else the type's)
  std::vector<ItemGrant> items;
  // Weapon ranks: per RankRules::groups index the weapon experience this unit REPLACES its class's with; -1 = not
  // said. Empty = the map says nothing (docs/SRPG.md section 7).
  std::vector<int> weaponXp;
  // Computer opponent (docs/SRPG.md section 10): what this unit goes for; bits of kAiChests | kAiPlaces | kAiDoors. 0 = none.
  int aiGoals = 0;
  // Computer opponent (docs/AI.md): how it moves and when it starts. The default is the plain "advance and attack".
  AiBehaviour behaviour;
  // Skills of its own (docs/SRPG.md "A unit with skills of its own"): indices into Database::skills, learned for good when it is made.
  std::vector<int> skills;
  // Jobs (docs/JOBS.md): its job and progress (the character's, unless the map says otherwise); `jobs.job` < 0 = none.
  JobState jobs;
  // docs/DIFFICULTY.md: appears only on these difficulties (indices into DifficultyRules::list; empty = on all); false = the
  // enemy shift never touches it (a boss); a stat the placement wrote itself is exact and never shifted.
  std::vector<int> onlyOn;
  bool difficultyShift = true;
  std::vector<char> statExact;  // per stat index: the placement gave the stat itself (empty = none)
  int facing = -1;              // "facing" (rules.facing only): the direction it starts looking in (Grid::neighbor numbering); -1 = the default (docs/SRPG.md "Facing")
};

// A structure placed on a map: `x`, `y` is its top-left square (hexagon games: its middle hexagon).
struct MapStructureDef {
  int type = 0;      // index into Database::structures
  int x = 0, y = 0;
  int side = -1;     // owning side index, -1 neutral
  // Doors, chests and visitable places (docs/SRPG.md section 10); all at these defaults for any other structure.
  std::string id;                  // story id of the placed object ("" = none), unique in the map
  std::vector<PlaceLoot> loot;     // a chest's contents
  int locked = -1;                 // -1 = the kind says, 0 / 1 = this placement is unlocked / locked
  std::vector<int> visitSides;     // visit: only these sides may visit (indices into MapDef::sides); empty = the kind's rule
  std::vector<std::string> visitUnits;  // visit: units (story ids) that may visit, in addition
  int weaponUses = -1;             // a kind with a map weapon: this placement's own uses (-1 = the kind's); loader_map_weapon.cpp
};

struct PrepDef;  // a map's preparation screen (core/data/prep.h, docs/EVENTS.md "Preparation")

struct MapDef {
  std::string id, name;
  GridType gridType = GridType::Square;  // always the game's grid (the loader checks)
  HexLayout hexLayout = HexLayout::Pointy;
  int width = 0, height = 0;
  std::vector<int> terrain;  // per cell, row-major, terrain index
  std::vector<int> heights;  // per cell elevation (0 when unused)
  std::vector<MapSideDef> sides;
  std::vector<MapStructureDef> structures;
  std::vector<MapUnitDef> units;
  bool fog = false;  // fog of war (docs/ECONOMY.md "Fog of war")
  int fogVision = 0;         // fog: added to every unit's vision (negative = darkness), -10..10 (docs/ECONOMY.md "Fog of war")
  bool aiSeesAll = false;    // fog: computer sides see everything (a side's own "aiSeesAll" overrides it)
  int tileset = -1;                // the tile layer's tileset (index into Graphics::tilesets), -1 = no tile layer (docs/GRAPHICS.md "Tile layer")
  std::vector<int> tileCells;      // per cell, row-major: tile number or -1 (empty when there is no tile layer)
  std::vector<MapChangeDef> changes;  // named map changes (docs/EVENTS.md "Changing the map"); empty = none
  int battleBackground = -1;  // the map's own background for the battle scene (index into Graphics::battle.backgrounds), -1 = by terrain
  // "battleBackgrounds" / "battlePlatforms" (docs/BATTLE_SCENE.md): the map's own table per terrain (size = terrain count when written, else empty)
  // and its "default"; -1 = none. They win over the game-wide tables of data/graphics.json for this map.
  std::vector<int> bgByTerrain, platformByTerrain;
  std::string weather;  // "weather": "rain" | "snow": the weather the battle starts with ("" = clear); a story `weather` command changes it
  int bgDefault = -1, platformDefault = -1;
  std::shared_ptr<const StoryData> story;  // null = no events and no objectives; shared by copies
  std::shared_ptr<const PrepDef> prep;     // null = the map starts as it is placed (no preparation screen)
  std::vector<MapDifficulty> difficulty;   // "difficulty": the map's own numbers per difficulty (docs/DIFFICULTY.md)
};

}  // namespace ote
