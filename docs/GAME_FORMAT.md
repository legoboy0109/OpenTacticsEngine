# Game format (version 1)

A game is a folder of plain-text files. The same folder runs on every
platform. JSON files may contain `//` comments and trailing commas.

```
my-game/
  game.json            rules, stats, resources, map list
  data/terrain.json    terrain types
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
  "view": { "width": 480, "height": 270, "tileSize": 24 },   // optional

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

  "rules": {
    "turnOrder": "sides",              // only "sides" for now
    "counterAttacks": true,
    "defeat": ["noUnits"],             // any of: noUnits, leaderDead, lostHq
    "economy":     { "enabled": false },
    "production":  { "enabled": false, "builtUnitsCanAct": false },
    "capture":     { "enabled": false, "amount": "a.hp" },
    "progression": { "enabled": false, "xpPerLevel": 100, "maxLevel": 20,
                     "xpHit": "10", "xpKill": "30" }
  },

  "formulas": {
    "damage": "a.str + w.might - b.def",   // required
    "hit": "90",                            // optional, percent; absent = always hit
    "crit": "0",                            // optional, percent
    "critMultiplier": 3,
    "strikes": "1"                          // optional, attacks per combat
  },

  "scripts": ["scripts/formulas.lua"],      // optional, loaded in order
  "maps": [ { "id": "m1", "name": "First Battle", "file": "maps/m1.json" } ]
}
```

Every `rules` block and key is optional; defaults are shown above
(`defeat` defaults to `["noUnits"]`).

## data/terrain.json

```jsonc
{ "terrain": [
  { "id": "city", "name": "City",
    "color": "#b7b7b7", "glyph": "c",          // placeholder look
    "moveCost": { "foot": 1, "tread": 1 },     // missing type = 1; 0 or less = impassable
    "props": { "defense": 2 },                 // free-form numbers for formulas
    "capture": 20,                             // capture points; omit/0 = not capturable
    "income": { "funds": 1000 },               // per turn to owner
    "produces": ["infantry", "tank"],          // unit type ids buildable here
    "tags": ["hq"] }
] }
```

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
    "damage": "a.str + w.might"   // optional override of formulas.damage
  }
] }
```

`versus` lookup order: target's unit type id, then its tags in listed order,
then `versusDefault`. A weapon without a `versus` table can target anything
and gets `vs = 1`.

## data/units.json

```jsonc
{ "units": [
  { "id": "knight", "name": "Knight", "glyph": "K",
    "moveType": "foot",
    "stats":   { "hp": 22, "str": 8, "def": 9, "mov": 4 },   // missing = 0
    "growths": { "hp": 80, "str": 50 },                      // % per level, missing = 0
    "weapons": ["lance"],
    "cost": { "funds": 3000 },
    "tags": ["armored"],
    "canCapture": false }
] }
```

## maps/*.json

```jsonc
{
  "name": "First Battle",
  "grid": { "type": "square", "width": 4, "height": 3 },
  "legend": { ".": "plain", "f": "forest", "H": "hq" },   // char -> terrain id
  "tiles": [ "H..f",
             "....",
             "f..H" ],                   // exactly height rows of width chars
  "heights": [ [0,0,1,1], [0,0,0,1], [0,0,0,0] ],          // optional
  "sides": [
    { "id": "blue", "name": "Blue", "color": "#3d85c6", "team": "blue",
      "controller": "human",             // or "ai"
      "resources": { "funds": 3000 } },
    { "id": "red", "name": "Red", "color": "#cc0000", "controller": "ai" }
  ],
  "owners": [ { "x": 0, "y": 0, "side": "blue" }, { "x": 3, "y": 2, "side": "red" } ],
  "units": [
    { "type": "knight", "side": "blue", "x": 1, "y": 0,
      "name": "Gilda", "level": 3, "leader": true,
      "stats": { "hp": 26 } }            // optional per-unit overrides
  ]
}
```

Sides take turns in listed order. `x` counts columns from the left, `y`
rows from the top, both starting at 0. Two units may not share a cell, and
a unit may not start on terrain impassable to it.

## Formulas

Formulas are Lua expressions. Variables:

| name   | meaning |
|--------|---------|
| `a`    | acting unit: every stat by id, plus `hp` (current), `maxhp`, `level`, `xp`, `type`, `name`, `tags.<tag>` |
| `b`    | the other unit (same fields); nil for `capture.amount` |
| `w`    | the weapon's `props` |
| `ta`, `tb` | `props` of the terrain under `a` / `b` |
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
