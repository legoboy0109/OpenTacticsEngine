// The Database is everything a game defines in its data files: stats,
// terrain, weapons, unit types and the rule switches. It is read-only once
// loaded. Cross references are stored as indices into these vectors.
//
// File format: docs/GAME_FORMAT.md. Loader: core/data/loader.h.
#pragma once
#include <memory>

#include "core/data/graphics.h"
#include "core/base/types.h"
#include "core/grid/grid.h"

namespace ote {

struct StatDef { std::string id, name; };
struct ResourceDef { std::string id, name; };
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
  Props props;                 // formula-visible numbers (defense, avoid...)
  std::vector<std::string> tags;  // free-form labels (the "hq" tag moved to structures)
  // Fog of war (docs/ECONOMY.md "Fog of war"): squares a unit stands on add this to its vision (0..10); a unit on
  // a square that hides is seen only by units next to it.
  int visionBonus = 0;
  bool hides = false;
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
  int width = 1, height = 1;   // footprint in squares (1..kMaxStructureSize)
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
  bool hides = false;
};

const int kMaxStructureSize = 8;

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
  int ammo = 0;               // attacks per supply (1..99); 0 = unlimited (docs/ECONOMY.md "Repair and supply")
};

// ---- Items (docs/ITEMS.md) ----

enum class ItemKind { Usable, Weapon, Accessory };

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
};
const int kHealFull = 1000000000;

// One item as it is handed out by data: a unit's starting items, a side's starting storage.
struct ItemGrant {
  int item = 0;
  int uses = -1;      // what is left; -1 = unlimited (filled in from the item when the entry says nothing)
  bool drop = false;  // goes to whoever defeats the carrier
};

enum class ItemStorage { Off, Anywhere, Structure, Leader };

struct ItemRules {
  bool carriedWeapons = false;  // false: a unit type's weapons, fixed; true: weapons are items
  int inventorySize = 5;
  ItemStorage storage = ItemStorage::Off;
};
const int kMaxInventory = 20;

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
};
const int kMaxVision = 20;
const int kMaxVisionBonus = 10;
const int kMaxFogTeams = 32;  // an event's "seen by" is a 32-bit mask with one bit per team

// One technology (data/research.json, docs/ECONOMY.md "Research").
struct TechnologyDef {
  std::string id, name, description;
  std::vector<Amount> cost;    // paid when research starts
  int turns = 1;               // 0..99 of the side's own turns; 0 = finished at once
  std::vector<int> needs;   // technologies that must be finished first (all of them)
  std::vector<int> unlocksUnits;  // unit types: with any technology listing a type, a side needs one of them to build it
  std::vector<int> bonusTypes;    // unit types the bonus is for; empty = every unit of the side
  std::vector<int> bonus;         // per stat index, added to those units; empty = no bonus
};
const int kMaxResearchTurns = 99;

enum class TurnOrder { Sides /*, Speed (planned: per-unit, FFT style) */ };

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
  bool researchNeedsStructure = false;  // "rules.research.needsStructure": research needs an owned structure that allows it
  AmbushRule ambush = AmbushRule::EndsTurn;  // "rules.fog.ambush"
  bool progression = false;  // XP + level ups
  int xpPerLevel = 100;
  int maxLevel = 20;

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

// Sizes of the isometric kinds of view, in logical pixels.
struct IsoConfig {
  int tileWidth = 0, tileHeight = 0;  // the diamond of one square; 0 = from the game's tileSize
  int heightStep = -1;                // pixels per height level; -1 = half the diamond's height
  int unitWidth = 0;                  // width unit pictures are drawn at; 0 = half the diamond's width
};

struct StyleDef {
  std::string id, name;
  StyleView view = StyleView::TopDown;
  IsoConfig iso;
  bool hasBackground = false;
  uint32_t background = 0;
  int showGlyphs = -1;  // 1/0 = force terrain letters on/off, -1 = default
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
  std::vector<UnitTypeDef> unitTypes;
  std::vector<std::string> scripts;  // Lua files, relative to rootPath
  std::vector<MapRef> maps;
  std::vector<StyleDef> styles;  // never empty after loading; [0] = default
  Graphics graphics;             // art (data/graphics.json); empty = placeholders
  std::shared_ptr<const CampaignDef> campaign;  // data/campaign.json; null = no campaign (docs/EVENTS.md)

  // Index lookups by id; -1 when not found.
  int findStat(const std::string& id) const;
  int findResource(const std::string& id) const;
  int findMoveType(const std::string& id) const;
  int findSwitch(const std::string& id) const;
  int findVariable(const std::string& id) const;
  int findTerrain(const std::string& id) const;
  int findStructure(const std::string& id) const;
  int findWeapon(const std::string& id) const;
  int findItem(const std::string& id) const;
  int findTechnology(const std::string& id) const;
  // Carried weapons: may a unit of this type use this weapon?
  bool canWield(int unitType, int weapon) const;
  int findUnitType(const std::string& id) const;
  int findMap(const std::string& id) const;
  int findStyle(const std::string& id) const;
};

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
};

struct MapUnitDef {
  std::string id;    // optional story id, unique in the map (docs/EVENTS.md "Named units"); empty = none
  int type = 0;
  int side = 0;
  int x = 0, y = 0;
  std::string name;  // empty = use the unit type name
  int level = 1;
  bool leader = false;
  std::vector<int> stats;  // final per-stat values (type base + overrides)
  bool hasItems = false;         // the map gives this unit its own items (else the type's)
  std::vector<ItemGrant> items;
};

// A structure placed on a map: `x`, `y` is its top-left square.
struct MapStructureDef {
  int type = 0;      // index into Database::structures
  int x = 0, y = 0;
  int side = -1;     // owning side index, -1 neutral
};

struct MapDef {
  std::string id, name;
  GridType gridType = GridType::Square;
  int width = 0, height = 0;
  std::vector<int> terrain;  // per cell, row-major, terrain index
  std::vector<int> heights;  // per cell elevation (0 when unused)
  std::vector<MapSideDef> sides;
  std::vector<MapStructureDef> structures;
  std::vector<MapUnitDef> units;
  bool fog = false;  // fog of war (docs/ECONOMY.md "Fog of war")
  std::shared_ptr<const StoryData> story;  // null = no events and no objectives; shared by copies
};

}  // namespace ote
