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
- `dataHash`: a hash of the bytes of the files that define the rules of
  this battle: `game.json`, every `data/*.json` the loader reads except
  `graphics.json`, the scripts listed in `game.json`, and the map file. Art
  and fonts are not included.
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
- **Leaving a battle** to the launcher keeps the Continue save, so the
  battle can be picked up later.
- Saves that cannot be loaded are shown greyed out with the reason.
- After loading, the battle view is rebuilt from the state (no replay of
  old animations); if it is a computer player's turn, it carries on.
- Command line: `--continue` starts the Continue save of `--game` directly;
  `--load-slot N` loads a slot (both for tests and shortcuts).

## Editor

Rules screen, a small **Saving** card: "Players can save: Any time / Only
the automatic Continue save / Never", with one line of help each.
