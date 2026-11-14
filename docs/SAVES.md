# Saving

Contract for saving and resuming battles. Owner decisions (2026-11-14):

- The player keeps one automatic **Continue** save per game, so closing the
  window or a crash never loses a battle, AND offers manual **Save / Load**
  with a handful of slots.
- Game makers control it per game: saving allowed **any time**, **only the
  automatic Continue save**, or **off**.

## Game setting (game.json)

```jsonc
"saving": "any"        // "any" (default) | "continue" | "off"
```
- `any`: Continue save + manual slots.
- `continue`: Continue save only (no Save/Load in menus). Stops players
  from undoing a bad dice roll by reloading an older position.
- `off`: nothing is saved; leaving a battle abandons it.

`Database::saving` (enum). Any other value is a loader error.

## What a save is

A save holds one battle in progress: which game and map, and the battle's
changing state. It does NOT copy the game's data; on load the game and map
are loaded normally, a fresh `Battle` is initialised, and the saved state
is put on top.

```jsonc
{ "saveVersion": 1,
  "game": "demo-srpg",            // game folder name
  "gameName": "Emblem Demo", "map": "bridge", "mapName": "Skirmish at the Ford",
  "dataHash": "9f3c…",            // see below
  "savedAt": 1794651000,          // seconds since 1970 (UTC), for display
  "round": 3, "activeSide": 0,    // also inside "battle"; repeated here so lists need not parse it all
  "battle": { … } }               // everything in Battle that changes during play
```

- `battle` contains everything needed to continue exactly: round, active
  side, over/winner, the random generator's state, every side (resources,
  defeated and the other flags), every unit ever created (alive or not, in
  id order, with all fields), every structure's owner and capture state.
  Things fixed by the map (terrain, heights, structure positions and kinds)
  are not stored.
- Because the random generator's state is saved, reloading and repeating
  the same action gives the same dice.
- `dataHash`: a hash of the **rules** of this battle, read from
  `game.json`, every `data/*.json` the loader reads except `graphics.json`,
  the scripts listed in `game.json`, and the map file. Things that only
  change how the game is called or drawn do not count (owner decision
  2026-11-14): names, descriptions, the author, colours, letters, fonts,
  visual styles, screen size, the saving setting, art, and the layout and
  comments of the files. A maker can rename and restyle a game without
  locking players out of their saves.
- **A save whose `dataHash` does not match the game as it is now is not
  loaded.** It stays on disk and is listed as "made with a different
  version of this game". (Indices inside the save are only meaningful
  against identical data.) Unknown `saveVersion`: same treatment.
- Restoring double-checks counts and ranges (sides, units' type and side
  and cell, structures, occupancy) and fails with a clear message instead
  of crashing on a damaged file.

Core API (`engine/core/battle/save.{h,cpp}`; no file access in these two):
```cpp
// Everything that changes during play, as JSON.
void saveBattleState(const Battle& battle, Json& out);
// `battle` must be freshly initialised from the same game and map.
bool restoreBattleState(Battle& battle, const Json& saved, std::string& err);
// Hash for "dataHash" (reads the files named above).
bool computeDataHash(const Database& db, const std::string& mapId, std::string& out, std::string& err);
```
Round trip is exact: save → restore into a fresh battle → the two battles
behave identically from then on (same events for the same actions).

## Where saves live

`<user data folder>/saves/<game folder name>/continue.json` and
`slot1.json` … `slot5.json`. Written atomically (temp file + rename), then
`commitUserData()` (the web build stores them in the browser the same way
as settings). `--saves DIR` uses another folder; headless runs and
`--no-settings` runs save nothing unless `--saves` is given. The editor's
Play panel never saves (it passes a flag that turns saving off).

## Player behaviour

- **Continue save** (settings `any` and `continue`): written at the start
  of every turn and after every completed action of a human player, once
  the action's animation has finished. Deleted when the battle ends. If
  writing fails, play continues and one notice says saving is not working.
- **Launcher, game page**: when a Continue save exists, **Continue** is the
  first row (map name, turn number). With `any`, a **Load** row opens the
  slot list. Starting a new battle while a Continue save exists asks first
  ("This replaces your Continue save").
- **Battle menu**: with `any`: **Save** and **Load** open the slot list
  (5 slots: map name, turn, date and time, or "Empty"). Saving over a used
  slot and loading (which leaves the current battle) ask first. Saving is
  only offered when it is a human player's turn and nothing is animating.
- **Leaving a battle**: the battle menu has **Leave battle**, which writes
  the Continue save and returns to the game page, so the battle can be
  picked up later. When nothing is being saved (setting `off`, or saving
  unavailable) the entry reads **Abandon battle** instead.
- Saves that cannot be loaded are shown greyed out with the reason.
- After loading, the battle view is rebuilt from the state (no replay of
  old animations); if it is a computer player's turn, it carries on.
- Command line: `--continue` starts the Continue save of `--game` directly;
  `--load-slot N` loads a slot (both for tests and shortcuts).

## Editor

Rules screen, a small **Saving** card: "Players can save: Any time / Only
the automatic Continue save / Never", with one line of help each.

## Implementation notes (engine side, 2026-11-14)

Built as specified; the editor's Saving card is not part of this package.
Decisions where the contract was silent, and the few deviations:

**Save data**
- File: one line of compact JSON. Besides the contract's fields it holds
  `"game"` (the game's folder name; a file whose `game` differs is listed as
  "This save belongs to another game") and `savedAt` in seconds since 1970.
- `battle` holds `round, activeSide, over, winnerTeam, rng {state, inc}`,
  `sides[]` (resources, defeated, everHadUnits, hadHq, leaderLost), `units[]`
  (type, side, cell, name, level, xp, stats, hp, moved, done, alive, leader)
  and `structures[]` (owner, capture, capturer). A side's id, name, colour,
  team and `ai` flag come from the map (the hash covers it), so they are not
  stored. The random generator state is two 16-digit hex strings (a JSON
  number is a double and loses 64-bit values).
- Restoring checks: every field's type and range; the number of sides and
  structures equals the map's; each unit's type, side and stat count; a living
  unit has a cell and 1..max hit points, a dead one has no cell; no two living
  units share a cell (occupancy is rebuilt from the units, so it cannot
  disagree); a structure's owner is a side, its capture progress is within its
  capture points, and its `capturer` is a living unit standing on it (no
  capturer means untouched progress). Nothing is applied unless all of it holds.
- `dataHash` = 64-bit FNV-1a, 16 hex digits (`save_hash.cpp`). JSON files are
  parsed and written out again compactly, so layout and comments do not count,
  with the keys `name`, `description`, `author`, `color` and `glyph` left out at
  every depth, except inside number tables (`props`, `versus`, `stats`,
  `growths`, `moveCost`, `income`, `cost`, `resources`), where a key is a rule
  whatever it is called. Of `game.json` only `formatVersion`, `stats`,
  `resources`, `moveTypes`, `roles`, `rules`, `formulas` and `scripts` count.
  Then `data/weapons.json`, `units.json`, `terrain.json`, `structures.json`
  (when present), the scripts in `game.json` order (byte for byte) and the map
  file, each preceded by its path and length. Key order counts. A new
  top-level rules key in `game.json` must be added to the list in
  `computeDataHash`, and a new cosmetic key to `cosmeticKey`.
- A save is "unusable" (greyed, with a reason, never deleted) when: the file
  cannot be read ("The file cannot be read"), is not valid ("The file is
  damaged"), has another `saveVersion` or a different hash ("Made with a
  different version of this game"), belongs to another game, or names a map the
  game no longer has. A save that passes these but fails the deeper restore
  checks is reported on load as "The save is damaged (<field and problem>)"
  (also logged) and the current battle is left untouched.

**Behaviour**
- Continue save: written when a turn starts (also computer turns, before the
  computer moves) and after each completed action of a human player once its
  animation has finished. A Move alone does not count (it can still be undone
  and is only half an order); Wait, Attack, Capture, Build and End Turn do.
  Battles in `--autoplay` save nothing. It is deleted the moment the result
  banner appears. Failure: the hint line shows "Saving is not working -
  progress may be lost" for 6 seconds, once per battle; the details go to the
  log. A failed manual save shows a message box with the reason instead.
- Game page: rows are Continue (when a Continue file exists), Play, Load (mode
  `any`), Game options, Back, each with a second line (Continue: map and turn;
  Load: how many slots are used). Where saving is off for the game or the run the
  page keeps its old single-line rows. A Continue file that cannot be loaded is
  shown greyed with the reason; choosing it shows that reason on the error
  screen. Load is always offered in mode `any`, even with no saves yet.
- The "This replaces your Continue save." question appears when a map is chosen
  (Play only leads to the map list), and also when a slot is loaded from the
  launcher while a Continue file exists, because the first turn start overwrites
  it. Loading from inside a battle asks "Load slot N? You will leave the current
  battle."; saving over a used slot asks "Replace the save in slot N?". The safe
  answer (Cancel) is selected first.
- Save/Load are entries in the existing End Turn menu (Menu button, or Confirm
  on empty ground), so they are only reachable on a human player's turn with
  nothing animating. Slots holding an unusable save may be saved over (after the
  question); they cannot be loaded. The list opens on the first empty slot when
  saving and on the first loadable one when loading.
- Loading rebuilds the whole battle scene (new view, camera and menus) from the
  state; no events are replayed. It works across maps of the same game. A
  computer turn carries on by itself.
- There is no "leave the battle" command in the player today; leaving means
  closing the window or reloading the page, which keeps the Continue save.
  After the result screen the Continue file is already gone.
- Times are shown as `YYYY-MM-DD HH:MM` in the viewer's local time.

**Command line and platform**
- `--saves DIR`, `--no-saving`, `--continue`, `--load-slot N` (1..5). The last
  two need `--game` and cannot be combined; `--load-slot` on a game that is not
  mode `any`, or any of them on a `saving: "off"` game, fails with an error screen.
  `--no-saving` beats `--saves`. Headless runs and `--no-settings` runs save
  nothing unless `--saves` is given. The editor's embed page passes both
  `--no-settings` and `--no-saving`.
- `platform.h` gained `makeDir` and `removeFile` (SDL3 and SDL2 backends; the
  SDL3 one works on Emscripten's file system too). Every write or delete is
  followed by `commitUserData()`, so the web build stores saves like settings.
- Save files are written to `<name>.tmp` and renamed over the old file; a failed
  write leaves the previous save as it was.
- The player logs one line per battle start saying where saves go ("saves:
  <folder>", "off for this game" or "not available in this run"); the web
  tests rely on it.
