// The Database is everything a game defines in its data files: stats,
// terrain, weapons, unit types and the rule switches. It is read-only once
// loaded. Cross references are stored as indices into these vectors.
//
// File format: docs/GAME_FORMAT.md. Loader: core/data/loader.h.
#pragma once
#include "core/data/graphics.h"
#include "core/base/types.h"
#include "core/grid/grid.h"

namespace ote {

struct StatDef { std::string id, name; };
struct ResourceDef { std::string id, name; };
struct MoveTypeDef { std::string id, name; };

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
  int capturePoints = 0;       // > 0 makes the terrain capturable
  std::vector<Amount> income;  // per turn to the owner (economy module)
  std::vector<int> produces;   // unit type indices buildable here
  std::vector<std::string> tags;  // e.g. "hq"
};

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
};

struct UnitTypeDef {
  std::string id, name;
  char glyph = '?';
  int moveType = 0;
  std::vector<int> stats;    // base value per stat index
  std::vector<int> growths;  // % chance per stat to gain +1 on level up
  std::vector<int> weapons;  // weapon indices
  std::vector<Amount> cost;  // production cost
  std::vector<std::string> tags;
  bool canCapture = false;
};

enum class TurnOrder { Sides /*, Speed (planned: per-unit, FFT style) */ };

// Rule switches. Each genre is a different combination of these.
struct RulesConfig {
  TurnOrder turnOrder = TurnOrder::Sides;
  int statHp = -1;    // index of the stat used as hit points (required)
  int statMove = -1;  // index of the stat used as movement points (required)
  bool counterAttacks = true;

  // Defeat conditions: a side is out when any enabled one triggers.
  bool defeatNoUnits = true;     // lost every unit (after having had one)
  bool defeatLeaderDead = false; // a unit flagged "leader" died
  bool defeatLostHq = false;     // owns no "hq"-tagged cell (and started with one)

  bool economy = false;      // income from owned terrain
  bool production = false;   // build units on producing terrain
  bool builtUnitsCanAct = false;
  bool capture = false;      // units may capture capturable terrain
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
struct StyleDef {
  std::string id, name;
  bool hasBackground = false;
  uint32_t background = 0;
  int showGlyphs = -1;  // 1/0 = force terrain letters on/off, -1 = default
  // Per terrain index: replacement color, or -1 to keep the terrain's own.
  std::vector<int32_t> terrainColors;
};

struct MapRef { std::string id, name, file; };

struct Database {
  std::string name;
  std::string description, author;  // optional, shown in the launcher
  std::string rootPath;  // game folder, no trailing slash
  ViewConfig view;
  FontConfig font;
  RulesConfig rules;
  std::vector<StatDef> stats;
  std::vector<ResourceDef> resources;
  std::vector<MoveTypeDef> moveTypes;
  std::vector<TerrainDef> terrains;
  std::vector<WeaponDef> weapons;
  std::vector<UnitTypeDef> unitTypes;
  std::vector<std::string> scripts;  // Lua files, relative to rootPath
  std::vector<MapRef> maps;
  std::vector<StyleDef> styles;  // never empty after loading; [0] = default
  Graphics graphics;             // art (data/graphics.json); empty = placeholders

  // Index lookups by id; -1 when not found.
  int findStat(const std::string& id) const;
  int findResource(const std::string& id) const;
  int findMoveType(const std::string& id) const;
  int findTerrain(const std::string& id) const;
  int findWeapon(const std::string& id) const;
  int findUnitType(const std::string& id) const;
  int findMap(const std::string& id) const;
  int findStyle(const std::string& id) const;
};

// ---- Map (one battle setup) ----

struct MapSideDef {
  std::string id, name;
  uint32_t color = 0xffffff;
  std::string team;  // sides sharing a team are allies; defaults to id
  bool ai = false;
  std::vector<Amount> resources;  // starting amounts
};

struct MapUnitDef {
  int type = 0;
  int side = 0;
  int x = 0, y = 0;
  std::string name;  // empty = use the unit type name
  int level = 1;
  bool leader = false;
  std::vector<int> stats;  // final per-stat values (type base + overrides)
};

struct MapDef {
  std::string id, name;
  GridType gridType = GridType::Square;
  int width = 0, height = 0;
  std::vector<int> terrain;  // per cell, row-major, terrain index
  std::vector<int> heights;  // per cell elevation (0 when unused)
  std::vector<int> owner;    // per cell side index, -1 = neutral
  std::vector<MapSideDef> sides;
  std::vector<MapUnitDef> units;
};

}  // namespace ote
