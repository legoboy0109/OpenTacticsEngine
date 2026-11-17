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
changing state (a game with a campaign saves a whole playthrough instead: see
"Campaign saves" at the end). It does NOT copy the game's data; on load the game and map
are loaded normally, a fresh `Battle` is initialised, and the saved state
is put on top.

```jsonc
{ "saveVersion": 1,
  "game": "demo-srpg",            // game folder name
  "gameName": "Story Tactics Demo", "map": "bridge", "mapName": "Skirmish at the Ford",
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
- Games with story (docs/EVENTS.md): `battle` also holds a `story` object
  with the switches and variables, the ids of the events that have run, and
  which objectives are current; units keep their story `id`. See the
  implementation notes below.
- `dataHash`: a hash of the **rules** of this battle, read from
  `game.json`, every `data/*.json` the loader reads except `graphics.json`,
  the scripts listed in `game.json`, and the map file. Things that only
  change how the game is called or drawn do not count (owner decision
  2026-11-14): names, descriptions, the author, colours, letters, fonts,
  visual styles, screen size, the saving setting, art, sound (`"sounds"` blocks, `"music"`,
  `"playerTurnMusic"` and `"enemyTurnMusic"` keys, data/audio.json, which is not read at all, a scene's
  `{ "music", "commands" }` shape; docs/SOUND.md; the story commands `playMusic` / `playSound` keep their
  place in the hash but not which track or sound), and the layout and
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

**Story music (docs/SOUND.md).** The `story` object gets an optional `"music"` string (a track id, `""` = the story stopped the
music) only once a `playMusic` / `stopMusic` command has run in the battle, so every other save is byte for byte what it was. A
resumed battle plays that track (or stays silent); the key must be absent or a string of letters, digits, `_`, `-` (or `""`),
anything else refuses the save (`battle.story.music`). The player checks nothing else about it: an id the game no longer has is a
warning and silence.

## Where saves live

`<user data folder>/saves/<game folder name>/continue.json` and
`slot1.json` … `slot5.json`. Written atomically (temp file + rename), then
`commitUserData()` (the web build stores them in the browser the same way
as settings). `--saves DIR` uses another folder; headless runs and
`--no-settings` runs save nothing unless `--saves` is given. The editor's
Play panel never saves (it passes a flag that turns saving off).

## Player behaviour

(A game with a campaign behaves differently in the places marked by "Campaign saves in the player" at the end of this file; everything below is a game without one, or a free battle.)

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

## Strategy-RPG state

Skills learned for good, promotions, carried units (rescue and pair up) and support points and levels are saved too, each only in a game
that uses the feature and only when not at its default, so older saves stay valid and `saveVersion` stays 1. Characters add nothing to a
save. What is stored, how the data hash and the campaign save treat these files, and the rules for adding keys are in docs/SRPG.md
(the "Saves" part of each section and "Save format summary and versioning").

Weapon ranks add one optional key, `ranks` (`{ "sword": 120 }`, weapon experience per weapon group, only groups above 0), to a battle
save's units and to a campaign roster entry, written only in a game with `data/ranks.json`; stat caps add no state. The data hash covers
`data/ranks.json` and `data/caps.json` only when they hold something (an empty ladder / empty `caps` is the same game as no file). See
docs/SRPG.md section 7 "Saves and hash".

Status conditions (docs/SRPG.md section 9) add one optional key, `conditions` (`[ { "condition": index, "left": turns } ]`, oldest first, `left` only
for a counted condition), to a battle save's units, written only for a unit that has some and refused in a game without `data/conditions.json`. The
data hash covers `data/conditions.json` only when it holds something (name, description and tag are cosmetic). Conditions are not part of a campaign
save: they end with the map.

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
  `sides[]` (resources, defeated, everHadUnits, hadHq, leaderLost, and in a
  game with a shared storage `storage[]`), `units[]`
  (type, side, cell, name, level, xp, stats, hp, moved, done, alive, leader,
  and in a game with items `items[]`: item, uses left, drop; `"fuel": n` for a unit whose type has fuel, and
  `"ammo": { "<weapon id>": n }` for a unit with a weapon that has ammunition, unlimited weapons not listed:
  both required for such units, refused for the others, `fuel` from 0 to the tank, each count from 0 to the
  weapon's `ammo`, an unknown or unlimited weapon refused; games without these features write neither key)
  and `structures[]` (owner, capture, capturer, and `"type"` when an upgrade changed its kind: the index of
  the current kind, written only when it differs from the map's). A unit that has a story id
  also has `"id"`. A side that has finished or started research also has
  `"research": { "done": [technology indices, ascending], "current": n, "left": n }` (`current` and `left`
  only while one is under way, and then both). Restoring checks: `type` a kind with the map kind's width and
  height that the map kind can reach through `upgrades`; `done` ascending without repeats, within the game's
  technologies, every finished or running technology having its prerequisites finished; `current` not
  finished; `left` from 1 to the technology's turns; a `research` key in a game without research is refused;
  a living unit's `hp` is checked against its maximum with the side's research bonus.
  A battle on a map with fog of war (docs/ECONOMY.md "Fog of war") also has
  `"seenOwners": [[side index or -1, ...], ...]`: one list per team (in team order), each with one entry per
  structure, what that team last saw of its owner (-1 = nobody's). Required for fog maps, refused otherwise
  (`battle.seenOwners: this battle has no fog of war`); a list per team, one number per structure, whole, from -1 to
  the last side. What each team currently sees is not saved: it is recomputed from the units when the save is
  loaded. A fog battle where some team last saw a structure as a kind other than the one the map placed (an upgrade
  it did not see) also has `"seenKinds": [[kind index, ...], ...]`, shaped like `seenOwners` but with an index into the
  game's structure kinds. It is written only then (so every save of a game that never upgraded anything out of sight is
  byte for byte what it was) and optional when reading (absent = every team believes the map's kinds); refused without
  fog (`battle.seenKinds: this battle has no fog of war`), and each entry must be a kind with the footprint of the
  structure's map kind and reachable from it through upgrades (the same checks as a structure's own `type`).
  A fog battle where a staff's `light` still lasts (docs/SRPG.md section 8, stage 2) also has
  `"lights": [ { "side": 0, "cell": 53, "radius": 2, "turns": 2 } ]`: the side that made it (an index), the centre square, the
  radius (1 to 10) and the turns of that side's turns left (1 to 10). Written only when there is a light (so every other save is byte
  for byte what it was), optional when reading (absent = none; an old save loads), refused without fog (`battle.lights: this battle
  has no fog of war`); each entry's side, square, radius and turns are checked with a path in the message.
  Games and maps without fog write and read exactly what they did before (the pinned demo hash and save
  bytes are unchanged); the map's `fog` key and the new unit, terrain and structure keys are part of the data
  hash like any other rule data (those files are hashed whole).
  A game with events, objectives, switches or variables also gets
  `"story": { "switches": [true, ...], "variables": [3, ...], "fired": ["event ids"],
  "objectives": 0 }` (all four required then; `switches` and `variables` have
  exactly the game's lengths, variables are whole numbers within plus or minus
  1,000,000,000, `fired` lists known events once each, `objectives` is the
  index of the current set: 0 = the map's own, then one per `objective`
  command in file order). Other games write no `story` key and no unit
  `"id"`, so their saves are byte for byte what they were, and a `story` key in
  a save of a game without story is refused. Restoring checks all of it and
  that story ids are valid and unique among the units, like the rest. A side's id, name, colour,
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
  `resources`, `moveTypes`, `roles`, `rules`, `formulas`, `scripts`, and
  (when present, so a game without them hashes as before) `switches` and
  `variables` count.
  Then `data/weapons.json`, `units.json`, `terrain.json`, `structures.json`
  `items.json` and `research.json` (each when present; a game without `items.json` or `research.json` hashes
  exactly as it did before they existed; the supply keys (`rules.supply`, `ammo`, `fuel`, `fuelPerTurn`, `supplies`, `repairs`) sit in files
  that are hashed whole, so they change the hash exactly when a game uses them; a technology's `name` and `description` do not count), the scripts in `game.json` order (byte for byte) and the map
  file, each preceded by its path and length. Key order counts. In the map
  file the **wording** of story events is cosmetic like names: the text of
  `say`, `speakerName`, a Talk `label` and an objective's `text` are blanked
  before hashing (the key stays, so adding or removing a line still changes
  the hash); everything else in `events` and `objectives` (triggers,
  conditions, speakers, portraits, every command) counts. Unit `id`s count.
  A game without events, objectives, switches or variables hashes exactly as
  it did before they existed (tests/test_story_save.cpp holds the hashes of
  both demos). A new
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

## Campaign saves (engine side)

A game with `data/campaign.json` (docs/EVENTS.md "Campaign") saves **a playthrough**,
not a single battle. Games without a campaign are unaffected: their files and
hashes are byte for byte as before (pinned by tests).

```jsonc
{ "saveVersion": 1, "game": "demo-srpg", "gameName": "…",
  "map": "bridge", "mapName": "…",          // the current chapter's map (for lists)
  "chapter": "ch1", "chapterName": "…",     // the current chapter (for lists)
  "dataHash": "…",                          // the campaign hash, below
  "savedAt": 1794651000,
  "round": 3, "activeSide": 0,              // only with "battle", as in battle saves
  "playthrough": {
    "chapter": 0,                           // index into the campaign's chapters
    "phase": "before" | "battle" | "after" | "finished" | "over",
    "roster": [ { "id": "eirik", "type": 0, "name": "Eirik", "level": 3, "xp": 40,
                  "stats": [24, 7, …],      // one per stat, the unit's own values
                  "items": [ { "item": 1, "uses": 2 } ],   // as in battle saves; omitted when empty
                  "side": "blue", "gone": false } ],
    "sides": [ { "id": "blue", "resources": [155], "storage": [ { "item": 0, "uses": 3 } ],
                 "research": [0, 2] } ],     // finished technologies (indices, ascending); omitted when none
    "switches": [true, false], "variables": [3, 0] },
  "battle": { … } }                         // exactly a battle save's "battle"; present only while a battle is under way
```

- The top-level fields are the existing ones (a list needs no more than `game`,
  `map`, `mapName`, `dataHash`, `savedAt`, `saveVersion`); a campaign save is told
  apart by the `playthrough` key (`isCampaignSave`). `round` / `activeSide` and
  `battle` are written only with a battle; phase `battle` without a `battle` key
  means the before scene is done and the battle has not started (the player starts
  it on load). `battle` in any other phase is refused.
- **`dataHash` of a campaign save** (`computeCampaignHash`): the same FNV-1a, format and
  cosmetic rules as `computeDataHash`, over game.json's rule keys, the data files
  (weapons, units, terrain, structures, items), the scripts, `data/campaign.json`
  and then **the map file of every chapter in chapter order**. A playthrough is
  therefore refused ("Made with a different version of this game") when the
  rules of any chapter change; renaming things and rewording dialogue does not.
  In `data/campaign.json` the **wording** does not count: the campaign's and the
  chapters' `name`, the text of `say`, `speakerName` (the keys stay, so adding or
  removing a line counts); everything else counts (`rules`, `freeBattles`, chapter
  order, maps, `next`, `goto`, every scene command, `portrait`, `speaker`).
- **`computeDataHash` (single battle) also mixes `data/campaign.json`**, after the
  map, **only when the file exists** (a game without it hashes exactly as before;
  the pinned-hash test is unchanged). That makes single-battle saves of a campaign
  game depend on the campaign file too; free battles of a campaign game are saved
  like any battle, with this hash.
- Restoring checks every field's type and range (`chapter` within the chapters; `phase`
  one of the five; roster ids well-formed and unique, `type` a unit type, `name` text up to
  200, `level` 1..1,000,000, `xp` and stats whole numbers up to 1e9, items as in battle saves
  with uses fitting the item, `side` an id, `gone` true/false; carried side ids unique,
  one resource per game resource, storage only if the game has one, `research` (optional) ascending indices of
  the game's technologies and only if the game has research (research under way is not carried: it is part of a
  battle save only); `switches` /
  `variables` exactly the game's lists, variables within plus or minus 1e9), then the
  battle part by the existing rules. Nothing is applied unless the whole file is
  valid; messages name the field (`playthrough.roster[2].level: value 0 is outside 1..1000000`).
  Fields in a save that the game does not know are ignored, as in battle saves.

Core API (`engine/core/campaign/campaign_save.h`, `playthrough.h`; no file access):
```cpp
bool buildCampaignSave(db, gameId, playthrough, const Battle* battleOrNull, savedAt, Json& file, err);
bool campaignSaveUsable(db, gameId, const Json& file, std::string& reasonForPlayer); // version, game, hash
bool restoreCampaignSave(db, formulas, gameId, const Json& file, Playthrough& pt, Battle& battle, bool& hasBattle, err);
```

**What the player calls** (the flow built on it is "Campaign saves in the player" below):
- *Save*: `buildCampaignSave(db, gameId, pt, inBattle ? &battle : nullptr, nowSeconds(), file, err)`, then
  the usual atomic write. Battles are only saved in phase `Battle` (`pt.phase`), with the
  `Battle` that `startChapterBattle` made. Free battles (`freeBattleAllowed`) are saved as before
  (`buildSaveFile`) and never with a playthrough.
- *Listing*: read the file; if `isCampaignSave(root)`, check `campaignSaveUsable` instead of the
  battle checks (`SaveStore::parseFile` requires `round` and `battle`; it must accept a campaign save
  without them). `map`, `mapName`, `chapterName`, `savedAt` are for display; `round` exists only mid-battle.
- *Load*: `restoreCampaignSave(...)`. It fills `pt` and, when the file has a battle, `battle` (it ran
  `prepareChapterBattle` and put the saved state on top, so the roster is back); `hasBattle` says which.
  Then by `pt.phase`: `Before` -> run the before scene; `Battle` -> `hasBattle ? continue that battle :
  startChapterBattle(...)`; `After` -> run the after scene; `Finished` / `Over` -> show the end.
- Continue-save: the same file, written at the same moments as today while a chapter's battle is on;
  also write it when a scene ends and when a battle is won (phase changes), so a closed window never
  loses a chapter.

## Campaign saves in the player (built)

Code: `engine/player/app_campaign.cpp` (flow, loading), `campaign/` (`campaign_logic.*` the testable decisions,
`story_scene.*`, `end_scenes.*`, `campaign_run.*`), `battle/battle_campaign.cpp`, `saves.*` (reading), `scene_game.*`,
`ui/slot_panel.*` (lists). Tests: `tests/test_campaign_player.cpp`, `tests/campaign_player_test.py` (the player run
windowless on a three-chapter game), `tools/webtest/suites/campaign.mjs`.

- **One file kind per game.** In a game with a campaign the Continue save and the five slots hold **campaign saves**
  (`buildCampaignSave` / `restoreCampaignSave`) whenever they are written from the story. `SaveStore::parseFile` accepts
  them: `playthrough` with a `phase` replaces the required `round` and `battle` (those are needed only when the file has a
  `battle`). `SaveInfo` gained `campaign`, `inBattle`, `chapterName`, `phase`; `inspect` judges a campaign save with
  `campaignSaveUsable` (version, game, hash over every chapter; no map lookup), `read` hands the whole file back
  (`SaveFile::root`) for `restoreCampaignSave`.
- **Continue is shared like this (owner may change it).** The Continue file belongs to the story. A **free battle**
  of a campaign game (a map started from the page's "Free battles" list, or `--map`) **never writes or deletes the Continue
  file** (`continueIsWritten`): it is "not saved automatically" (the rows say so), the battle menu says **Abandon battle**
  instead of Leave battle, and a free battle in progress cannot destroy a playthrough. Free battles can still be
  saved by hand into a slot (a plain battle save, as before, with the plain `dataHash`, which also mixes
  `data/campaign.json`), and such a slot is loaded as a free battle (no playthrough involved). Games without a campaign
  are untouched, byte for byte (the launcher and game pages of both demos were compared pixel for pixel).
- **When the Continue file is written** (campaign game, `saving` is `any` or `continue`, not `--autoplay`): at the start of
  every turn and after every completed action of a human player, as in any battle; **after each scene ends** (the state the
  scene left behind: after a before scene phase `battle` without a battle, after an after scene the next chapter's phase
  `before`); **when a chapter battle is won** (phase `after`, written when the result banner appears, before the
  player confirms it); when a battle is **lost with `retry`** (phase `battle` without a battle, so a closed window does not
  bring the lost position back); and on **Leave battle**. It is **deleted** when a battle is lost with `onDefeat: "end"` (at
  the result banner) and when the last chapter's after scene ends (the story is finished). A scene that is not finished
  when the window closes is not saved: Continue shows it again from its title card.
- **Loading** (Continue, a slot, `--continue`, `--load-slot N`) continues by phase exactly as the contract says: `before` ->
  title card + scene; `battle` with a battle -> that battle, as a saved battle is resumed (no replay; a computer turn
  carries on); `battle` without one -> a new chapter battle (new seed, no scene); `after` -> the after scene;
  `finished` / `over` -> the end screen. A campaign save that cannot be loaded is greyed out with the reason
  ("Made with a different version of this game", ...), like any save; a damaged one fails with "The save is damaged (field and
  problem)" and changes nothing.
- **Lists.** Continue row / slot rows show the **chapter's name** (the map's name for a plain save) and a detail line:
  **Turn N** in the middle of a battle, else where the story stands ("Chapter begins", "Ready to fight", "Battle won",
  "Story complete", "Story ended"), then the date and time in slots. The "This replaces your Continue save." question
  before New game and before loading a slot from the page applies to campaign saves only (a free battle's plain slot
  does not touch the Continue file, so loading it asks nothing).
- **Battle menu.** Save / Load as before (the slot gets a campaign save including the battle); Leave battle writes the
  Continue save and returns to the game page.
- **Settings.** `saving: "continue"` = the Continue file only, no Load row and no slots; `"off"` = nothing is written
  (a story still plays, Leave battle reads Abandon battle, no Continue row).
- **Command line.** `--continue` and `--load-slot N` resume a campaign save of `--game` directly. `--campaign` (with
  `--game`) starts a New game. `--map ID` on a campaign game starts that map as a free battle whatever its `freeBattles`
  say and never touches campaign saves (the editor's Play panel and the tests rely on it). `--campaign` cannot be
  combined with `--map`, `--continue` or `--load-slot`. `--autoplay --campaign` plays the whole story with the computer
  on the human side (title cards, scene lines, results and the retry choice pass by themselves; nothing is saved); with
  `--frames N` it stops as before.

## Dialogue choices (docs/EVENTS.md "Dialogue choices")

Nothing is added to any save: the picked answer's number is the `choice` command's `store` variable, which lives in `story.variables` of a
battle save and `playthrough.variables` of a campaign save. A Battle with a question waiting cannot be saved (`buildSaveFile` and
`buildCampaignSave` refuse it); the player never offers a save then. Games and maps without a choice keep their save bytes and hashes.
