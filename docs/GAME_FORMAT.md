# Game format (version 1)

A game is a folder of plain-text files. The same folder runs on every
platform. JSON files may contain `//` comments and trailing commas.

```
my-game/
  game.json            rules, stats, resources, map list
  data/terrain.json    terrain types (the ground)
  data/structures.json optional: buildings standing on terrain (see docs/STRUCTURES.md)
  data/weapons.json    weapons / attacks
  data/units.json      unit types (classes)
  maps/<name>.json     one battle each
  scripts/*.lua        optional Lua (functions used by formulas)
```

Ids are lowercase strings unique within their list. Every reference to an
id that does not exist is a load error.

## game.json

```jsonc
{
  "formatVersion": 1,
  "name": "My Game",
  "description": "A short line shown in the launcher.",      // optional
  "author": "Your Name",                                     // optional
  "view": { "width": 480, "height": 270, "tileSize": 24 },   // optional
  "saving": "any",                                           // optional: "any" | "continue" | "off"
  "grid": { "type": "hex", "layout": "pointy" },             // optional: hexagon game, "pointy" | "flat" (docs/HEX.md);
                                                             // missing (or { "type": "square" }) = a square game

  // Text font (optional; default is the built-in 8x8 pixel font).
  "font": {
    "name": "sans",              // a built-in font, see "Fonts" below
    // "file": "fonts/My.ttf",   // OR a .ttf/.otf inside the game folder
    "size": 10,                  // text height in logical pixels (optional)
    "smooth": true               // optional, see "Fonts"
  },

  // Stats are whatever the game wants. The engine itself only needs to know
  // which one is hit points and which one is movement ("roles").
  "stats": [ { "id": "hp", "name": "HP" }, { "id": "mov", "name": "Move" } ],
  "roles": { "hp": "hp", "move": "mov" },                     // required

  "resources": [ { "id": "funds", "name": "Funds" } ],        // optional
  "moveTypes": [ { "id": "foot", "name": "Foot" } ],          // at least one

  // Story switches (off/on) and variables (whole numbers, start 0) for map events; optional.
  "switches":  [ { "id": "village_saved", "name": "Village saved" } ],
  "variables": [ { "id": "gold_found", "name": "Gold found" } ],

  "rules": {
    "turnOrder": "sides",              // only "sides" for now
    "counterAttacks": true,
    "defeat": ["noUnits"],             // any of: noUnits, leaderDead, lostHq (no "hq" structure left)
    "economy":     { "enabled": false },
    "production":  { "enabled": false, "builtUnitsCanAct": false },
    "capture":     { "enabled": false, "amount": "a.hp" },
    "progression": { "enabled": false, "xpPerLevel": 100, "maxLevel": 20,
                     "xpHit": "10", "xpKill": "30" },
    // Optional (docs/ECONOMY.md "Repair and supply"); unknown keys are refused. No block = nothing happens.
    "supply": { "repair": { "amount": 20, "costShare": 100 },   // % of max HP per turn on an own structure, 1..100;
                                                                  // % of the proportional unit cost, 0..100, default 100
                "structures": true },                             // own structures also refill fuel and ammunition
    // Optional (docs/ECONOMY.md "Research", "Fog of war"); unknown keys and other values are refused.
    "research": { "needsStructure": false },   // true: research needs an owned structure kind with "research": true. Default
                                               // false: a side can research whenever the game has technologies
    "fog": { "ambush": "endsTurn" }            // a unit that walks into a hidden enemy: "endsTurn" (default) or "canAct"
                                               // (stopped and moved, but it may still attack, wait, capture...)
  },

  "formulas": {
    "damage": "a.str + w.might - b.def",   // required
    "hit": "90",                            // optional, percent; absent = always hit
    "crit": "0",                            // optional, percent
    "critMultiplier": 3,
    "strikes": "1"                          // optional, attacks per combat
  },

  // Switchable looks, see "Visual styles" below (optional).
  "styles": [ { "id": "classic", "name": "Classic" } ],

  "scripts": ["scripts/formulas.lua"],      // optional, loaded in order
  "maps": [ { "id": "m1", "name": "First Battle", "file": "maps/m1.json" } ]
}
```

`saving` says how much saving the player offers (docs/SAVES.md): `"any"` (the
default) = an automatic Continue save plus five manual slots; `"continue"` =
only the automatic Continue save (no Save/Load in the menus, so a bad dice
roll cannot be undone by reloading); `"off"` = nothing is saved. Any other
value is a loader error (`game.json: "saving" must be "any", "continue" or
"off"`). A save is only loadable while the rules files it was made with
(game.json, `data/*.json` except graphics.json and audio.json, the scripts, the map) are
unchanged; art and fonts may change freely.

`switches` and `variables` are what the events of the maps read and set
(docs/EVENTS.md); ids are letters, digits, `_` and `-`, unique within their
list. They count as rules for saves; their names do not. A variable's number may come from a unit instead of being written:
`{ "setVariable": "bar", "level": "eirik" }` (also `hp`, `maxHp`, `"stat": "str", "of": "eirik"`; docs/EVENTS.md "Reading a unit's
numbers"), and `{ "takeItem": "salve", "from": "eirik", "all": true }` takes an item away from units (docs/EVENTS.md "Taking an item away");
both work in map events and in campaign scenes.

Every `rules` block and key is optional; defaults are shown above
(`defeat` defaults to `["noUnits"]`).

## data/terrain.json

```jsonc
{ "terrain": [
  { "id": "city", "name": "City",
    "color": "#b7b7b7", "glyph": "c",          // placeholder look
    "moveCost": { "foot": 1, "tread": 1 },     // missing type = 1; 0 or less = impassable
    "props": { "defense": 2 },                 // free-form numbers for formulas
    "tags": ["open"],                          // free-form labels
    "visionBonus": 2,                          // optional (fog of war, docs/ECONOMY.md): 0..10, added to the
                                               // vision of a unit standing here; or for some unit types only:
                                               // { "amount": 2, "units": ["infantry", "<tag>"] } (ids or tags)
    "hides": true }                            // optional: a unit standing here is seen only from next to it
] }
```

Terrain is only the ground. Buildings are not terrain any more: `capture`,
`capturePoints`, `income`, `produces` and the tag `hq` are **refused** on
terrain with a message that says they belong to `data/structures.json`.

## data/structures.json (optional)

Structures stand on top of terrain squares, may cover several squares, and
can be captured, pay income, build units and count as headquarters. A game
without this file simply has no structures. Full rules: docs/STRUCTURES.md.

```jsonc
{ "structures": [
  { "id": "city", "name": "City",                 // name defaults to the id
    "color": "#b7b7b7", "glyph": "c",             // placeholder look
    "width": 1, "height": 1,                       // squares, 1..8 each, default 1
    // "rings": 1,                                 // hexagon games instead of width/height: 0 = one hexagon, 1 = it and its six
                                                   // neighbours (docs/HEX.md)
    "moveCost": { "tread": 2, "ship": 0 },        // optional: replaces the terrain's cost for
                                                   // the listed move types; 0 = cannot enter
    "props": { "defense": 3 },                    // optional: replaces / adds to the terrain's numbers
    "capturePoints": 20,                           // > 0 = can be captured
    "income": { "funds": 1000 },                  // per turn to the owner, once per structure
    "produces": ["infantry", "tank"],             // unit types the owner can build on its squares
    "sells": ["salve"],                       // optional: a shop; each item needs a price (docs/ITEMS.md)
    "tags": ["hq"],                               // "hq" = headquarters (defeat: lostHq)
    "research": true,                             // optional: only counts while rules.research.needsStructure is on (docs/ECONOMY.md)
    "upgrades": [ { "to": "town", "cost": { "funds": 3000 },   // optional: kinds it can be built up into (same
                    "requires": "masonry" } ],                 // width x height); "requires" = a technology
    "repairs": ["air", "tank"] }                   // optional: unit type ids / tags it repairs (rules.supply.repair);
                                                   // missing = all, [] = none
] }
```

Everything a structure does not set comes from the terrain underneath each
of its squares, so one structure can stand on grass, road or sand and keep
the ground's movement costs. Ids are unique among structures.

A structure kind may also be a **door**, a **chest** or a **visitable place** (a village), with a `"door"`, `"chest"` or `"visit"` block, and may say
`"blocksSight": true` (fog: units do not see past it). Opening, visiting or destroying one turns it into another kind of the same size.
Keys, the thief's ability, the unlock staff, the map keys (`id`, `contents`, `locked`, `visitSides`, `visitUnits`) and `"goesFor"` on units are
specified in docs/SRPG.md section 10; the editor has no forms for them (docs/EDITOR_UI.md).

## data/weapons.json

```jsonc
{ "weapons": [
  { "id": "bow", "name": "Bow",
    "range": [2, 2],              // [min, max]; default [1, 1]
    "counter": true,              // can counterattack (default true)
    "afterMove": true,            // false = cannot fire after moving
    "props": { "might": 6, "hit": 85 },
    "versus": { "flier": 3 },     // key = target unit type id or tag
    "versusDefault": 1,           // omit => targets not in "versus" can't be attacked
    "group": "bow",               // optional: with carried weapons, who can wield it (units.json "wields")
    "wieldedBy": ["kell"],        // optional: only these characters (data/characters.json) may wield it; docs/SRPG.md
    "wieldedByTypes": ["knight"], // optional: or units of these types (units.json ids); a unit on either list may
    "ammo": 6,                    // optional, 1..99: attacks per supply (fixed weapons only; docs/ECONOMY.md)
    "damage": "a.str + w.might",  // optional override of formulas.damage
    "sounds": { "hit": "sword_hit", "miss": "whiff", "critical": "crit" }   // optional (docs/SOUND.md): sound ids of data/audio.json
  }
] }
```

`versus` lookup order: target's unit type id, then its tags in listed order,
then `versusDefault`. A weapon without a `versus` table can target anything
and gets `vs = 1`.

`wieldedBy` (carried weapons; needs data/characters.json) lists the character ids that may wield the weapon, on top of the class
rules (`group` / `wields`) and any weapon rank: someone else, or a unit without a character, can carry it but not use it
("Only Kell can wield this"). `wieldedByTypes` does the same with unit type ids; with both lists a unit passes by either one. An absent
or empty list means no lock. The rule, the wording and the loader errors are in docs/SRPG.md
"Character-locked weapons".

## data/units.json

```jsonc
{ "units": [
  { "id": "knight", "name": "Knight", "glyph": "K",
    "moveType": "foot",
    "stats":   { "hp": 22, "str": 8, "def": 9, "mov": 4 },   // missing = 0
    "growths": { "hp": 80, "str": 50 },                      // % per level, missing = 0
    "weapons": ["lance"],                                    // fixed weapons: what it fights with.
                                                             // carried weapons: what it can wield
    "wields": ["lance"],                                     // optional: weapon groups it can wield (carried)
    "items": ["iron_lance", "salve"],                    // optional: what it starts with (docs/ITEMS.md)
    "cost": { "funds": 3000 },
    "tags": ["armored"],
    "canCapture": false,
    "fuel": 60, "fuelPerTurn": 5,                            // optional (docs/ECONOMY.md): tank 1..999; burn per turn
                                                             // (needs fuel, less than fuel)
    "supplies": false,                                       // optional: refills adjacent units of its side
    "vision": 3,                                             // optional (fog of war, docs/ECONOMY.md): how many squares
                                                             // its team sees from where it stands, 0..20, default 2
    "sounds": { "step": "hooves", "defeated": "horse_fall", "select": "neigh" } }   // optional (docs/SOUND.md): step, hit, miss,
                                                             // critical, defeated, select, heal
] }
```

## data/items.json (optional)

Things units carry: usable items, carried weapons and accessories, with
optional limited uses and prices. A game without this file has no items.
The switch between fixed and carried weapons, the inventory size and the
shared storage are in `game.json` under `rules.items`. A map can give each
unit its own `"items"` and each side a starting `"storage"`. Everything is
in docs/ITEMS.md. An item may have `"sounds": { "use": "potion" }` (docs/SOUND.md). An item of `"kind": "staff"` (a staff a unit wields by
group and rank and uses on another unit at range: healing, warping, pulling, swapping, repairing an item, lighting a square in fog)
has its own keys (`group`, `range`, `target`, `area`, `effects`, `hit`, `xp`, `rank`, `wieldedBy` ...), all specified in docs/SRPG.md
section 8 "Staves" (the editor's form covers healing only so far; the other effects are written by hand and kept by the editor). Any item may
say `"repairable": false` (only with `uses`) so that a staff's `repair` never offers it. A **key** is a usable item with `"opens": ["door"]`, `["chest"]`,
both and/or structure kind ids (valid without `heal`); its `uses` are the openings it has (docs/SRPG.md section 10).

## data/research.json (optional)

Technologies a side can research: cost, turns, prerequisites, unit types they unlock and stat bonuses. A game
without this file has no research. `{ "technologies": [ { "id", "name", "description", "cost": {…}, "turns": 0..99
(default 1), "requires": ["id"], "unlocks": { "units": ["id"] }, "bonus": { "types": ["unit id"], "stats":
{ "def": 1 } } } ] }`; unknown keys are refused. With `rules.research.needsStructure` on, a structure kind needs `"research": true` to research at (by default no building is needed).
Everything is in docs/ECONOMY.md.

## maps/*.json

```jsonc
{
  "name": "First Battle",
  "grid": { "type": "square", "width": 4, "height": 3 },   // "hex" in a hexagon game: must match game.json "grid"
  "legend": { ".": "plain", "f": "forest", "r": "road" },   // char -> terrain id
  "tiles": [ "...f",
             "rrrr",
             "f..." ],                   // exactly height rows of width chars
  "heights": [ [0,0,1,1], [0,0,0,1], [0,0,0,0] ],          // optional
  "fog": true,                                             // optional: fog of war (docs/ECONOMY.md), default false;
                                                           // at most 32 sides then
  "music": "boss",                                         // optional (docs/SOUND.md): music ids of data/audio.json for this map
  "playerTurnMusic": "calm", "enemyTurnMusic": "tense",    // (each optional; "" = none on this map)
  "sides": [
    { "id": "blue", "name": "Blue", "color": "#3d85c6", "team": "blue",
      "controller": "human",             // or "ai"
      "resources": { "funds": 3000 } },
    { "id": "red", "name": "Red", "color": "#cc0000", "controller": "ai" }
  ],
  "structures": [                        // optional; needs data/structures.json
    { "type": "hq", "x": 0, "y": 0, "side": "blue" },   // x, y = top-left square; side optional = neutral
    { "type": "city", "x": 3, "y": 2 },
    { "type": "chest", "x": 5, "y": 1, "id": "c1", "contents": [ "potion" ] } ],   // doors, chests, places: id, contents, locked, visitSides, visitUnits (docs/SRPG.md section 10)
  "units": [
    { "type": "knight", "side": "blue", "x": 1, "y": 0,
      "name": "Gilda", "level": 3, "leader": true,
      "id": "gilda",                     // optional story id, unique in the map (docs/EVENTS.md)
      "goesFor": ["chests"],             // optional, computer sides only: chests / villages / doors (docs/SRPG.md section 10)
      "stats": { "hp": 26 } }            // optional per-unit overrides
  ],
  "events": [ … ],                       // optional: story events (docs/EVENTS.md)
  "objectives": { … }                    // optional: win / lose conditions and the text shown (docs/EVENTS.md)
}
```

`events` and `objectives` are specified in docs/EVENTS.md: triggers,
conditions, the list of commands (dialogue, adding / removing / moving
units, items, resources, switches and variables, win / lose, and questions with two to four answers: `choice`, docs/EVENTS.md "Dialogue choices") and the
objective kinds. Every id they name (units, sides, teams, types, items,
resources, switches, variables) must exist and unknown keys are refused, with
a message naming the map, the event and the command. A map without them
loads and plays exactly as before.

Sides take turns in listed order. `x` counts columns from the left, `y`
rows from the top, both starting at 0. Two units may not share a cell, and
a unit may not start on a square impassable to it (judged with the
structure's movement costs applied, if one stands there).

Each structure's whole rectangle (`width` x `height` from its top-left
square) must lie inside the map, and structures may not overlap. (Hexagon
games: `x`, `y` is the middle hexagon and the ring around it must fit too;
docs/HEX.md.) The type
and side must exist. The old `owners` list is refused: ownership now
belongs to structures (their `side`). Only the owner of a structure changes
during a battle.

## data/audio.json (optional)

Sound and music (docs/SOUND.md, built): tracks and sound effects by id and the game's standard set. A game without the
file is silent. Presentation only: not read by the rules core, not part of the save hash. Unknown keys and unknown ids are loader
errors (the player's loader, like graphics.json); a missing audio file is a warning and silence.

```jsonc
{ "music":  { "title":  { "file": "audio/title.ogg", "loopStart": 2.5 },        // loopStart seconds, optional
              "battle": { "file": "audio/battle.ogg", "volume": 80 },           // percent of the music volume, optional
              "win":    { "file": "audio/win.ogg", "loop": false } },           // a jingle
  "sounds": { "hit":  { "file": "audio/hit.wav" },
              "step": { "files": ["audio/step1.wav", "audio/step2.wav"] } },    // played in turn
  "use": { "titleMusic": "title", "mapMusic": "battle", "playerTurnMusic": "", "enemyTurnMusic": "",
           "sceneMusic": "", "victory": "win", "defeat": "lose", "levelUp": "levelup",
           "sounds": { "cursor": "tick", "hit": "hit", "step": "step" } } }     // event -> sound id, every key optional
```

Files are `.ogg` (Vorbis) or `.wav` inside the game folder. The event keys, the overrides (`"sounds"` on weapons, units and items,
`"music"` / `"playerTurnMusic"` / `"enemyTurnMusic"` on a map file, `"music"` on a campaign scene), the story commands
`playMusic` / `stopMusic` / `playSound` and what each message looks like are in docs/SOUND.md "Step 2 as built".

## data/campaign.json (optional)

A campaign strings maps together as chapters with scenes between them and carries
surviving named units (levels, items), storage, resources, switches and variables
from chapter to chapter. A game without the file plays single maps as before.
Specified in docs/EVENTS.md "Campaign": `name`, `rules` (`fallen`, `onDefeat`),
`freeBattles` and `chapters` (`id`, `name`, `map`, `before` / `after` scenes, `next`). Scenes may ask questions (`choice`) and
branch with `goto` (also from a before scene, which skips that chapter's battle).
Unknown keys, unknown maps / chapters and bad rule values are loader errors. The
file counts for the save hash except its wording (docs/SAVES.md).

## Strategy-RPG data (optional)

Games can add named characters (`data/characters.json`), skills (`data/skills.json`), menu abilities (`data/abilities.json`) and
supports (`data/supports.json`), a `rules.promotion` and a `rules.rescue` block, extra keys on unit types, weapons and items (skills,
learned skills, promotions, abilities), and map units placed as a character (`"character"` in place of `"type"`). Every one is optional,
a game that uses none of them loads and plays exactly as before, and the files, keys, limits and loader refusals are specified in
docs/SRPG.md ("Files at a glance" lists them all). Nothing is repeated here.

**Status conditions** (docs/SRPG.md section 9, optional): `data/conditions.json` (poison, sleep, silence, berserk and the like, built from skill blocks
plus `cannotAct`, `cannotMove`, `forbid` and `berserk`), `"inflict"` on a weapon, `"cure"` / `"inflict"` on a usable item, `inflict` and `cure` effects of a
staff, and the skill effect `immune`. A game without the file loads, plays, saves and hashes exactly as before.

**Weapon ranks and stat caps** (docs/SRPG.md section 7, both optional): `data/ranks.json` (the maker's rank ladder, e.g. E to S, and
whether ranks are fixed or grow by use; needs carried weapons), `data/caps.json` (game-wide stat caps), `"rank"` / `"rankXp"` on a
weapon (needs a `group`), `"ranks"` and `"caps"` on a unit type, `"ranks"`, `"capAdjust"` and `"wields"` (extra weapon groups) on a character, `"ranks"` on a map unit.
A unit whose rank is too low carries a weapon but cannot equip or attack with it. A file with an empty ladder / empty `caps` is the
same game as no file.

## Formulas

Formulas are Lua expressions. Variables:

| name   | meaning |
|--------|---------|
| `a`    | acting unit: every stat by id, plus `hp` (current), `maxhp`, `level`, `xp`, `type`, `name`, `tags.<tag>` |
| `b`    | the other unit (same fields); nil for `capture.amount` |
| `w`    | the weapon: its `props` as numbers, plus `id` and `group` as text (`w.group == "sword"`; group `""` when it has none) |
| `fw`   | the weapon the other fighter fights with, shaped like `w`: its `props` as numbers plus `id` and `group` as text (strike formulas and skill formulas; empty with `none = 1` and numbers reading 0 without one). In the strike formulas `fw.answers` is 1 when the foe can really counter in this fight, else nil. Weapon triangle example: docs/SRPG.md "Formulas in this document" |
| `ta`, `tb` | the numbers of the square under `a` / `b`: the terrain's `props`, with the `props` of a structure standing there replacing numbers of the same name and adding the others |
| `dist` | grid distance between them |
| `vs`   | the weapon's `versus` value for `b` |

If the hit-point stat is called `hp`, `a.hp` is the *current* value and
`a.maxhp` the maximum. Results are rounded down; damage below 0 becomes 0.
Formulas must not be random (no `math.random`): the engine rolls hit and
crit chances itself so battles replay identically.

Functions defined in `scripts/*.lua` can be called from formulas, e.g.
`"damage": "my_damage(a, b, w, tb, vs)"`.

## Fonts

`font.name` picks one of the fonts shipped with the engine
(`assets/fonts/fonts.json` lists them; add entries there to make more
built-ins):

| name | look |
|------|------|
| `pixel` | built-in 8x8 pixel font (default; needs no font file, works on every platform) |
| `pixel-ttf` | Pixelify Sans, a proportional pixel-style font |
| `arcade` | Press Start 2P, chunky arcade lettering |
| `sans` | Atkinson Hyperlegible, a clean, highly readable sans-serif |
| `serif` | Alegreya, a bookish serif suited to fantasy |
| `mono` | JetBrains Mono, fixed-width |

`font.file` uses your own TrueType/OpenType font from the game folder
instead. `size` is the text height in logical pixels (the game's `view`
resolution); each built-in has a sensible default.

`smooth: true` draws anti-aliased text at the display's full resolution
(sharp on any screen size). `smooth: false` draws at the logical resolution
with hard pixel edges, which suits pixel fonts. Built-ins choose a default.

The player can override a game's font: `ote-player --font sans
--font-size 11` (or `--font path/to/file.ttf`). If a font cannot be loaded
the player falls back to `pixel` and logs a warning.

## Visual styles

A game can offer several named looks; the player's "Visual style" option
switches between them (globally or per game). The first style in the list
is the default. A game without a `styles` list has one style, `default`.

```jsonc
"styles": [
  { "id": "classic", "name": "Classic" },
  { "id": "night", "name": "Night",
    "background": "#05060f",                       // screen background
    "showGlyphs": false,                           // hide terrain letters
    "terrainColors": { "plain": "#2f4f3a", "sea": "#101a40" },   // recolor terrain
    "structureColors": { "city": "#6a6a78" } }                    // recolor structure blocks
]
```

A style also has a kind of view (`"view"`: top-down by default, or
`"isometric"`; `"isometric-3d"` and `"3d"` are reserved for the 3D kinds), with
optional sizes under `"isometric"`: see docs/VIEWS.md.

Every key except `id` is optional. These keys recolor the placeholder
graphics (`structureColors` does the same for structure blocks, by structure
id, like `terrainColors` does for terrain). Real art (sheets, tiles, sprites) lives in the optional
`data/graphics.json`, which can also override art per style id: see
docs/GRAPHICS.md.

## Player settings (not part of a game)

The player stores the user's options in `settings.json` in the user data
folder (`--settings FILE` picks another file). Options are global and apply
to every game; any single option can be overridden for one game. Lookup
order for an option, highest first: command-line flag, per-game override,
global value, the game's own default (e.g. `font`, the first style), then the
engine default. The options today are `font`, `fontSize`, `uiScale`
(percent; multiplies the text size, rounded) and `style` (a style id above;
a game that lacks the chosen id uses its first style). An explicit
`fontSize` wins; otherwise the size of the font in use applies (the game's
`font.size` only while the game's own font is in use). The built-in `pixel`
font only grows in whole multiples of 8.
