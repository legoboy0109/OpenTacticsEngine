# Story events, dialogue and campaigns

Contract for PLAN item 3f. Owner decisions (2026-11-14):

- **Triggers**: battle start / end and turn number; a unit reaching a
  square or area; a unit defeated or fighting; a Talk command between two
  units.
- **Actions** besides dialogue: add / remove / move units; give items or
  resources; set win / lose conditions; switches and variables.
- The first version **includes a campaign**: chapters in order, surviving
  units with levels and items carried over, scenes between maps.
- **Dialogue** is a box across the bottom with the speaker's name and, when
  the game has one, the speaker's portrait beside it.
- **Dialogue choices**: built after the first version (PLAN k item 6); see "Dialogue choices" below.
- **Fallen units: the maker chooses per game** (gone for good, or back next
  chapter).
- **Single maps next to a campaign: the maker chooses** (campaign only, or
  campaign plus free battles).

Everything below is engine design and may change; all wording shown to
players and makers is placeholder until the owner approves it.

Status: steps 1 to 4 of the "Build order" are built and tested, and
`demo-srpg` has a story and a two-chapter campaign; still to do: the
editor's Campaign screen (the rest of step 5). Where building step 1
refined or corrected the first contract, the text below is the corrected
version and says so ("refined").

## Principles

- Events are **data**, edited as lists of commands like RPG Maker's event
  commands. No scripting is needed; a Lua hook can come later.
- Events run in the rules core, so they are deterministic, saved, and
  played by `ote-sim`. A command changes the battle at once and leaves
  entries in the event stream; the player shows them in order (a line of
  dialogue is one such entry). Nothing waits for input inside the core: a question that a person must answer is done by replaying
  the action with the answer (see "Dialogue choices").
- A game without events, objectives, switches or variables behaves exactly
  as today and keeps its save hash and its saves byte for byte.
- Wording (what is said, names, labels, objective text) is kept apart from
  rules: it lives in one text table that the Battle owns, events carry only
  indices into it, and it does not count for the save hash.

## Named units

A map unit may have `"id": "eirik"`: letters, digits, `_` and `-` (up to 64),
unique among the map's units and the units its `addUnit` commands create.
Events, objectives and the campaign point at units by id; ids that nothing
knows are loader errors. A **unit filter**, used wherever "which unit" is
asked, is one of: `{ "unit": "eirik" }`, `{ "side": "blue" }`,
`{ "team": "blue" }`, `{ "type": "knight" }`, `{ "tag": "flier" }`, or `{}` =
any unit. Exactly one key (refined: no combinations). A plain string where a
filter is expected means `{ "unit": that }`.

## Switches and variables

`game.json`: `"switches": [ { "id": "village_saved", "name": "Village saved" } ]`,
`"variables": [ { "id": "gold_found", "name": "Gold found" } ]` (ids as for
units, unique). Switches are off / on, variables whole numbers (start 0;
refined: they stay within plus or minus 1,000,000,000, going past it stops
there, so a save can check them). They belong to the playthrough: in a
campaign they carry from chapter to chapter; in a single battle they start
fresh. A caller that carries them (the campaign, later) calls
`Battle::setStartingStory(switches, variables)` after `init()` and before
`start()` and reads `battle.storyState` afterwards. Switch and variable
lists in `game.json` count as rules for the save hash; their names do not.

## Events (map file)

```jsonc
"events": [
  { "id": "ambush",                         // unique in the map (letters, digits, _ and -)
    "when": { "turn": 4, "side": "red" },   // the trigger, see below
    "if": [ { "switch": "village_saved", "is": false } ],   // optional, all must hold
    "repeat": false,                        // default: runs once
    "do": [ { "say": "Now! Strike from the forest!", "speaker": "gorm" },
            { "addUnit": { "type": "soldier", "side": "red", "x": 9, "y": 1 } } ] }
]
```

An unknown key in an event, a trigger, a condition, a command or an
objective is a loader error (a typo must not pass silently), and so is every
unknown id (unit, side, team, type, item, resource, switch,
variable). Messages name the map, the event and the command:
`maps/bridge.json: map "bridge": event "ambush": "do": command 2 (addUnit): unknown unit type "dragon"`.

### Triggers (`when`)

| Trigger | Fires |
|---|---|
| `{ "battle": "start" }` | before the first turn (before the first `TurnStarted` entry) |
| `{ "battle": "won" }` / `"lost"` / `"end"` | when the battle is decided, right after the `BattleEnded` entry: `won` if the human team won, `lost` if it did not (also a draw), `end` either way |
| `{ "turn": 4, "side": "red" }` | at the start of that side's turn in round 4 (after income); `side` optional = the first side; `"turn": "every"` = every round (refined: it implies `repeat`) |
| `{ "reach": { "x": 5, "y": 2, "w": 2, "h": 1 }, "by": <filter> }` | when a matching unit **ends** a move inside the rectangle (`w`, `h` default 1). Walking through does not count. Refined: units moved by a `moveUnit` command also "end a move"; units of the computer's side count like any other |
| `{ "defeated": <filter> }` | when a matching unit is defeated (including the units that leave with a defeated side); `"by": <filter>` optional = by anything. Refined: a side wiped out by the defeat rules has no killer, so an event with `by` does not fire for those units |
| `{ "fight": <filter>, "with": <filter> }` | just before a fight begins, when one of the two fighters matches `fight` and the other `with` (`with` optional), whichever one attacks. If the event takes a fighter off the map or ends the battle there is no fight (the attacker's action is spent) |
| `{ "place": "d1", "is": "opened" \| "visited" \| "destroyed", "by": <filter> }` | a door or chest of the map (its `id` in the map's `structures`) was opened, a place visited or destroyed (docs/SRPG.md section 10); `by` optional = by anyone, also by a story command |
| `{ "talk": "eirik", "to": "lyra", "label": "Talk" }` | adds **Talk** to the first unit's action menu while the two units (named by id) are alive and next to each other; fires when chosen (does not end the turn unless the event says so). `label` defaults to "Talk". Only the first unit can start it; the computer's side never does |

### Conditions (`if`, and inside the `if` command)

`{ "switch": id, "is": true }` (`is` default true),
`{ "variable": id, "op": "=", "value": 3 }` (`= != < <= > >=`, `==` also
accepted), `{ "alive": <filter> }` / `{ "dead": <filter> }`,
`{ "round": { "op": ">=", "value": 5 } }`,
`{ "carries": "salve", "unit": <filter> }` (`unit` default any),
`{ "sees": <unit filter>, "team": "<team>" }` (fog of war, docs/ECONOMY.md; `team` is required: a team name or a side id
for that side's team), `{ "hasCondition": "poison", "unit": <filter> }` (a status condition, docs/SRPG.md section 9; `unit` default any;
map events only), `{ "actor": <unit filter> }` (the unit that set this event off, see "Branching on who acted" below; map events only), `{ "difficulty": "hard" }` / `{ "difficulty": { "atLeast": "normal" } }` / `{ "atMost": ... }` (the difficulty the battle was built with,
compared by position in the game's list; also in campaign scenes, where it reads the playthrough's; docs/DIFFICULTY.md; the game must have difficulties).
Refined: `alive` = some living unit matches; `dead` = a unit that matches
exists (or existed, also when it was removed) and none is alive, so a unit
that an event has not added yet is neither. `carries` = a living matching
unit has the item in its inventory (not the storage). `sees` = the team sees at least one living unit that
matches the filter: a unit of the team itself counts, a unit on a hiding square counts only with a unit of the team
next to it, and without fog on the map every living matching unit counts. It is judged on the team's sight at the
moment the event is about to run; story events and objectives otherwise act on the truth, not on what a team has seen.

Conditions are judged when the event is **about to run**, so an event sees
what earlier ones did. An event whose condition fails is not used up: a
non-repeating event can still run at a later occurrence of its trigger.

### Commands (`do`)

| Command | What |
|---|---|
| `{ "say": "text", "speaker": "eirik" }` | a line of dialogue (entry `Dialogue`); `speaker` is a unit id or a free name (`"speakerName": "Villager"`, not both); optional `"portrait"` id (just text for the player to look up) |
| `{ "addUnit": { …a map unit… } }` | places a unit (keys as in the map's `units`, including `id` and `ranks`); if the square is taken or the unit cannot stand there, the nearest free square it can stand on (grid distance, the lowest cell number on a tie). A repeated `addUnit` with an id that a unit already has creates the unit without the id (logged). No free square at all: nothing is added (logged) |
| `{ "removeUnit": <filter> }` | leaves the map (not a defeat: no drops, no defeat events; the defeat rules still apply, so removing a side's last unit ends it like any other loss) |
| `{ "moveUnit": "eirik", "x": 3, "y": 4 }` | refined: the unit is put there (no path is walked; the entry gives the squares it left and reached, the player may animate it); if the square is taken, the nearest free one as for `addUnit`. Does not spend its move. Fires `reach` events. `"walk": true` (with optional `"wait": false`) plays it as a walk along the way ("Scene presentation"; the rules result is the same) |
| `{ "changeSide": <filter>, "side": "blue" }` | matching living units join another side (recruiting); an unfinished capture is dropped |
| `{ "giveItem": "salve", "to": <filter> }` | every matching living unit gets one, following the item rules (room: inventory, else the shared storage if the game has one, else lost) |
| `{ "takeItem": "salve", "from": <filter>, "all": true }` | the opposite of `giveItem`: see "Taking an item away" below |
| `{ "fight": { "attacker": "eirik", "defender": "brigand", "strikes": [ … ] } }` | two named map units fight, with every strike written down or with real rolls: see "A staged fight" below (map events only) |
| `{ "storeItems": <filter>, "keep": ["salve"] }` | sends every item a unit carries (except the kept ones) to the side's shared storage: see "Sending a unit's items to storage" below |
| `{ "openDoor": "d1" }`, `{ "closeDoor": "d1" }`, `{ "lockDoor": "d1" }`, `{ "unlockDoor": "d1" }`, `{ "openChest": "c1", "by": "tink" }` | doors and chests by their map `id` (docs/SRPG.md section 10): open a shut door, shut an open one (skipped while a unit stands in it), shut-and-lock / lock a door, take the lock off a door or chest (it stays shut), open a chest (the contents go to `by`, else to the first human side's storage / funds) |
| `{ "setBehaviour": "hold" \| { "mode": "seek", "unit": "lord" }, "units": <filter> }`, `{ "wakeGroup": "ambush" }` | the computer's per-unit behaviour (docs/AI.md "Story commands", map events only): give matching computer units a new behaviour block (a guard starts asleep again); wake every waiting unit of a group. `addUnit` takes the same `"behaviour"` key as a map unit. The block may carry the scripted-style keys (`order`, `weights`, `caution`, `retreat`, `idleTurns`, `ignores`, a `patrol` route; docs/AI.md "Scripted-style computer players"): a reinforcement joins the acting order after the units already on the map, and `setBehaviour` starts the idle, retreat and patrol state afresh. To make a unit escape, send it to a square (`seek`) and remove it with a `reach` event |
| `{ "addCondition": "poison", "to": <filter>, "turns": 2 }` | gives a status condition to every matching living unit: see "Status conditions" below (map events only) |
| `{ "removeCondition": "poison" \| "all", "from": <filter> }` | takes one (or every) status condition off the matching units (map events only) |
| `{ "giveResource": { "gold": 500 }, "side": "blue" }` | may be negative; a side's funds never go below zero |
| `{ "setSwitch": id, "to": true }`, `{ "setVariable": id, "to": 3 }`, `{ "addVariable": id, "by": 1 }` | the number may instead be read from a unit: `{ "setVariable": id, "level": "eirik" }` (see "Reading a unit's numbers" below) |
| `{ "objective": { … } }` | replaces the current objectives (below) |
| `{ "win": "blue" }` / `{ "lose": "blue" }` | ends the battle for that team (refined: a team name or a side id). `lose` leaves the win to the only other team still in the battle, nobody if there are several. The first decision stands; the rest of the event still runs |
| `{ "if": [conditions], "then": [commands], "else": [commands] }` | `then` required, `else` optional |
| `{ "endTurn": true }` | only in an event whose trigger is `talk` (a loader error anywhere else): the talking unit's turn ends |
| `{ "focus": { "x": 3, "y": 4 } }` or `{ "focus": "eirik" }` | the view moves there before the next entry (a unit that is not on the map: nothing); optional `ms` (scroll time) and `wait` ("Scene presentation") |
| `{ "playMusic": "boss", "fade": 1.0 }` | sound (docs/SOUND.md): the track `boss` of `data/audio.json` plays until another command changes it or the battle ends; `fade` seconds (0..60, optional: not given = the player's 0.5 s cross-fade, 0 = a cut). Presentation only: entry `MusicChanged`; the core keeps only "the music the story asked for" (saved with the battle, docs/SAVES.md). The id is opaque to the core (it goes into the text table, so which track it is does not count for the save hash, but the command does); the player refuses a game whose ids are not in `audio.json` |
| `{ "stopMusic": true, "fade": 1.0 }` | silence until a later `playMusic` or the end of the battle (entry `MusicChanged` with no track) |
| `{ "playSound": "thunder" }` | one sound effect (entry `SoundRequested`) |
| `{ "setTerrain": "bridge", "area": { "x": 4, "y": 1, "w": 1, "h": 3 } }` or `{ "setTerrain": "bridge", "cells": [[4, 1], [4, 2]] }` | gives the squares that terrain at once (exactly one of `area` / `cells`): see "Changing the map" below (map events only) |
| `{ "applyChange": "bridge" }`, `{ "revertChange": "bridge" }` | applies one of the map's named `changes`, or puts back what its squares held before it was applied (map events only) |
| `{ "setFog": false }`, `{ "setVision": -1 }` | fog of war on / off, and an adjustment added to every unit's vision (-10..10): docs/ECONOMY.md "Fog options" (map events only) |

**Running.** Events run in file order for one occurrence, and an event that
fires while another runs (a `reach` caused by `moveUnit`, a `won` event caused
by `win`) waits in a queue, so every event sees what the previous one left.
At most 100 events run in one go; the next one is cut with a logged error.

### Reading a unit's numbers

`setVariable` and `addVariable` take their number either written (`"to"` / `"by"`) or **read from a unit** when the command runs. Give
exactly one reading instead of the number:

```json
{ "setVariable": "bar", "level": "eirik" }                      // the unit's level (the one the player sees)
{ "addVariable": "bar", "level": "lyra" }
{ "addVariable": "bar", "hp": "eirik" }                         // hit points now (map events only)
{ "addVariable": "bar", "maxHp": "eirik" }                      // most hit points
{ "addVariable": "bar", "stat": "str", "of": "eirik" }          // a stat of the game (game.json "stats"); "of" is the unit
```

Because the result is an ordinary variable, sums and bars use the existing `variable` condition. Worked example (neutral names): the
guard Edda survives the chapter if her level plus the squire Bram's level reaches 7:

```json
{ "id": "check", "when": { "turn": 5 }, "do": [
  { "setVariable": "bar", "level": "edda" },
  { "addVariable": "bar", "level": "bram" },
  { "if": [ { "variable": "bar", "op": ">=", "value": 7 } ],
    "then": [ { "say": "Edda stands firm.", "speaker": "edda" } ],
    "else": [ { "removeUnit": { "unit": "edda" } }, { "say": "Edda is lost." } ] } ] }
```

- **The unit** is named as other commands name one (`moveUnit`, `say`'s `speaker`): the unit's story `id`, which for a unit placed from
  data/characters.json is its character id. A map event may only name an `id` that the map (or an earlier `addUnit`) gives a unit; a campaign
  scene names a roster id and, as `alive` / `dead` do, only has the shape of the id checked (the character may join in a later chapter).
- **What reads as 0:** a unit that is not on the map yet (an `addUnit` that has not run), one that was removed (`removeUnit`, `changeSide` does
  not remove), one that was defeated, and in a campaign scene a character who is not in the roster or whose entry is `gone` (fell in an
  earlier chapter under `"fallen": "gone"`). A unit that is carried (rescued) still exists and is read. A character that fell under
  `"fallen": "return"` is in the roster and not gone, so it is read normally. A negative stat stays negative; the result is kept
  within the usual +-1,000,000,000.
- **Which number:** `level` is `Unit.level` (the roster's `level` in a scene), i.e. the level shown on the unit: after a promotion that
  resets the level (`rules.promotion.levelAfter`) it is the new, reset level; the engine keeps no separate total level. `hp` is current hit
  points (never negative). `maxHp` and `stat` are the stats as the rules see them (own value plus accessories, research, skills and
  carry bonuses). In a scene they are the roster's own stored values, without accessories or research (a scene has no map and no sides);
  `hp` is refused there because the roster keeps no hit points between battles (a chapter begins at full strength).
- **Loader refusals** (each names the map or file, the event and the command): both a number and a reading (`give "to" (a number) or a
  reading from a unit, not both`); more than one reading; `stat` without `of` (or `of` with anything else); an empty unit id; a unit id the
  map does not know (map events); an unknown stat (`game.json "stats"`); `hp` in a scene.
- Saves need nothing new: the variable is saved like any variable and the reading happens when the command runs.

### Taking an item away

`{ "takeItem": "salve", "from": <unit filter>, "all": true }` removes one copy of the item (the first in the unit's inventory) from every
**existing** unit matching `from` (on the board, or carried by another unit), or every copy with `"all": true` (default false). Filters are the
same as `giveItem`'s `to`. A unit that carries none is left alone, silently. If the item was the weapon a unit fights with (carried weapons,
docs/ITEMS.md), nothing else is needed: the equipped weapon is simply the first wieldable item still carried, so the unit falls back to
its next usable weapon or fights unarmed. The shared storage is not touched (`giveItem` does not fill it except when a unit has no room).
It works in every trigger, including `battle: "start"` and `defeated`. Because the inventory is saved with the battle and carried into the
roster at the end of the chapter, a taken item stays gone through save / load and into later chapters.

In a campaign scene: `{ "takeItem": "salve", "from": "eirik", "all": true }` where `from` is a roster id (only the shape is checked); it
removes the item from that character's carried belongings, so it is gone in every later chapter. A character who is not in the roster (or who
is gone) is left alone. Loader refusals: an unknown item (`"takeItem" names unknown item "x"`), a missing `from`, an unknown unit id (map
events), a bad `all`.

Three uses (neutral names):

```json
{ "id": "lost_blade", "when": { "battle": "won" }, "do": [ { "takeItem": "oak_blade", "from": "edda" } ] }        // a weapon gone after a battle
{ "id": "strip", "when": { "battle": "start" }, "do": [ { "takeItem": "healing_herb", "from": { "side": "blue" }, "all": true } ] }  // all of one item removed at battle start
{ "id": "return", "when": { "turn": 4 }, "do": [ { "giveItem": "healing_herb", "to": { "side": "blue" } } ] }     // ... given back later
```

### Campaign characters in map events

A map event can ask about a campaign character who is **not on this map**: `{ "rosterAlive": "kara" }`, `{ "rosterDead": "kara" }`,
`{ "inRoster": "kara" }`, each with an optional `"party": "any" | "active"` (default `any`; a game without parties has one party, so
the two are the same). They work wherever conditions do: an event's `if`, an `if` command, an answer's `if`, and campaign scenes
(where `alive` / `dead` already read the active party; the new ones add `party: any`).
- The id is a character / unit id (letters, digits, `_`, `-`); it is **not** checked against the map (the character may be elsewhere).
- **`rosterAlive`**: the character is on this map and alive or hidden (see below); or, if it is not on this map, it is in the
  roster (as it was when the chapter began) and not gone. **`rosterDead`**: it was on this map and is defeated or removed; or it is in
  the roster marked gone (and not alive in another party). **`inRoster`**: it is in the roster and not gone - the roster only, whatever the
  map says.
- **The map's own unit wins.** A character that stands on this map is read from the map (alive now, defeated now), not from the
  roster file's start-of-chapter state.
- **`party`**: `active` looks at the active party's roster only; `any` also at every parked party's.
- **A free battle (no campaign playthrough)** has no roster: `rosterAlive` / `rosterDead` read the units on the map, `inRoster` is true
  for a unit that is on the map or hidden, false for any other.
- Saves: nothing new (the roster is the playthrough's). Code: `battle/story_hide.cpp`, `data/loader_story_roster.cpp`.

### Taking a unit off the map and bringing it back

`{ "removeUnit": <filter> }` makes the campaign count the unit as gone and `addUnit` of its character is refused. To take a unit away
**and keep it**, add `"hide": true`:
```jsonc
{ "removeUnit": { "unit": "lyra" }, "hide": true }        // leaves the map (the usual fade); the unit is still in the story
{ "showUnit": "lyra", "x": 4, "y": 2 }                     // the same unit comes back (the nearest free square it can stand on)
{ "addUnit": { "character": "lyra", "side": "blue", "x": 4, "y": 2 } }               // does the same for a hidden character (a plain unit id cannot be added twice: use showUnit)
```
- **State kept** while hidden: hit points, items, level, experience, stats, learned skills, weapon ranks and **status conditions** (they do not
  count down while the unit is away). The `moved` / `done` flags keep whatever they had. A hidden unit's **summons vanish** and a unit it
  **carried is put down**, as for a removal. A carried unit cannot be hidden.
- **What hidden units do not do:** they are not on any square, cannot be targeted, do not act, and `alive` / `dead` read them as
  neither (like a unit not added yet). `removeUnit` / numbers read from them (`setVariable ... readUnit`) see 0. They count for **no defeat rule**
  (a side whose only units are hidden is defeated) and the objective `defeated` skips them.
  `rosterAlive` / `inRoster` say they live.
- **`showUnit`** on a unit that is not hidden does nothing; a side defeated meanwhile cannot get its hidden unit back (it stays hidden, logged).
  `showUnit` fires `reach` events like any arrival. `removeUnit` **without** `hide` on a hidden unit makes it gone for good.
- **Campaign:** at the end of a won battle a hidden roster character is written to the roster **alive** (a removal still counts as lost).
- Saves: a hidden unit has `"hidden": true` in `units[i]` (no key for anyone else, so games that never hide keep their bytes); a save
  claiming a hidden unit on the map or with no hit points is refused. Tests: `tests/test_fidelity_story.cpp`.

### Branching on who acted

`{ "actor": <unit filter> }` is a condition (an event's `if`, the `if` of an `if` command, the `if` of a question's answer) that is true when **the
unit that set the running event off** matches the filter. It uses the same unit filter as the triggers (`{ "unit": "eirik" }`, `{ "side": "blue" }`,
`{ "team": … }`, `{ "type": … }`, `{ "tag": … }`, a bare id, `{}` = any unit). Which unit that is depends on the trigger:

| Trigger | The actor |
|---|---|
| `reach` | the unit that ended its move in the area (a `moveUnit` counts too) |
| `defeated` | the unit that fell (the killer is not an actor; use the trigger's own `by`) |
| `fight` | the attacker (the one that chose to fight, whichever fighter your `fight` / `with` filters matched) |
| `place` | the unit that opened, visited or destroyed it; nobody when a story command did |
| `talk` | the unit that started the talk (the first unit named by the trigger) |
| `promoted` | the unit that promoted |
| `battle`, `turn`, `support` | nobody: `actor` is false, whatever the filter (even `{}`) |

The trigger already filters who may start an event, so `actor` is for **branching inside one event**: one `reach` event for a whole area, an
`if` / `else` command on who arrived. It is judged where the condition sits, at the moment the event runs (a queued event keeps its own actor).
Recipe:

```json
{ "id": "gate", "when": { "reach": { "x": 3, "y": 0, "w": 1, "h": 3 } }, "repeat": true, "do": [
  { "if": [ { "actor": { "unit": "eirik" } } ],
    "then": [ { "say": "The lord arrives.", "speakerName": "Narrator" }, { "setSwitch": "lord_in", "to": true } ],
    "else": [ { "say": "Somebody arrives.", "speakerName": "Narrator" } ] } ] }
```

A campaign scene has no acting unit: the scene loader refuses it (`"actor" is for map events only: a campaign scene has no unit that set it off`).
Loader refusals are those of a unit filter (`unknown unit "x"`, `unknown side`, `unknown key`, a filter that is not an object or an id). Code:
`battle/story_actor.cpp` (and `StoryRun::actor` in `story_state.h`), `data/loader_story_actor.cpp`; tests `tests/test_extras_actor.cpp`,
`tests/srpg_player_extras_test.py`.

### A staged fight

```jsonc
{ "fight": { "attacker": "eirik", "defender": "brigand",
             "strikes": [ { "by": "attacker", "result": "hit", "damage": 4 },
                          { "by": "defender", "result": "miss" },
                          { "by": "attacker", "result": "crit" } ],
             "weapon": { "attacker": "iron_sword", "defender": "old_axe" },
             "lethal": false, "xp": true, "wear": true } }
```

Two units of the map (named by their `id`, so one a `addUnit` adds works too) fight as if the attacker had started it. **Map events only** (a
campaign scene has no map units). Both must be on the map when the command runs; if one is not, nothing happens and the log says
`story: fight 'a' against 'b': a fighter is not on the map; no fight`. The fight is shown by the same battle scene (or on-map blows) as a real
one and writes the same `Attack` entries, so nothing in the player is special.

- **`strikes`** (1 to 24; leave the key out for real rolls). Each is `{ "by": "attacker" | "defender", "result": "hit" | "miss" | "crit", "damage": n }`
  and they are played **in the order written**; the defender's strikes are counters (the scene shows them so). `damage` is optional: left out, it
  is **the normal computed damage** of that strike (the weapon's damage formula, the unit's skill blocks that need no roll, `critMultiplier` for a `crit`),
  `0` is allowed, a number is exact (not changed by a crit or percents; capped at the target's HP), and a `miss` does no damage (a `damage` other than
  0 on a miss is refused). **No random number of the battle is used** (the generator is where it was). A fighter that falls ends the fight: the strikes
  after that are not played. A strike by a fighter with no weapon is skipped (logged). Skill blocks that fire on a strike, a weapon's inflict, drain
  and recoil do **not** happen in a written fight: it is a scene, every number is the maker's.
- **`strikes` left out:** an ordinary fight with real rolls, the attacker striking first: the same strikes, counters, follow-ups and skill blocks as
  one a unit starts (`rules.counterAttacks`, the follow-up rules, hit and crit rolls from the battle's generator), without the checks of an order
  (range, sight, the move rule, teams are not asked: allies may fight; the answering weapon is the one the defender would use at that distance, and it
  answers only if its range reaches).
- **`weapon`**: `{ "attacker": id, "defender": id }`, each optional (a weapon of `data/weapons.json` the unit owns: an item it carries, or its type's
  weapons); left out, the unit's **equipped** weapon. A unit that does not own the named weapon, or an attacker with none, means no fight (logged).
- **`lethal`** (default true): `false` keeps a fighter that would fall at **1 HP** (it is not defeated; the scene shows the 1).
- **`xp`** (default false): `true` pays the experience the rules would pay for the fight (the same `xpHit` / `xpKill` / `xpNoDamage`, level-ups and all).
  No experience by default.
- **`wear`** (default false): `true` spends the weapons' uses (and the first strike's ammunition) as a real fight does; by default nothing wears.
- A defeat is normal: drops go to the victor, the fallen unit's defeat quote runs, the story's `defeated` events and the win checks run.
- **What it does not do:** it does not set off the `fight` trigger or the characters' battle quotes before it (an event cannot start itself), does not
  pay weapon ranks, supports' "fought together" or `afterCombat` skill blocks, and does not end the attacker's turn (it is not an action).
- **Loader refusals:** `missing required "attacker"` / `"defender"`, `unknown unit "x"`, `a unit cannot fight itself`, `unknown key`, `"strikes" must be
  a list of 1 to 24 strikes`, `"by" must be "attacker" or "defender"`, `"result" must be "hit", "miss" or "crit"`, `"damage" must be a whole number from 0 to
  9999`, `a miss does no damage`, `unknown weapon "x"`, `"lethal"` / `"xp"` / `"wear"` that are not true / false.
- **Saves, hash:** nothing new (the result is HP and who is alive, saved as ever). The command is part of the data hash like any other.
- Code: `core/data/loader_story_fight.cpp`, `core/battle/story_fight.cpp` (`Battle::storyFight`, `stagedStrike`; `fightExperience` is applyAttack's
  reward code made reusable). Editor: the command "Stage a fight" (Units group of the command picker). Tests: `tests/test_fidelity2_rules.cpp`,
  `tests/srpg_player_fidelity2_test.py`.

### Sending a unit's items to storage

`{ "storeItems": <unit filter>, "keep": ["oak_blade"] }` moves every item each matching unit carries to the shared storage of the unit's side (the
storage the item menu and the preparation screen show, docs/ITEMS.md), except the items named in `keep` (optional; every copy of a kept item stays).
It works in map events and in campaign scenes. The items arrive in the storage in the order the unit carried them; the event stream gets one
`ItemMoved` entry (unit to storage) per item, like the Store action. Decisions:

- **No storage in the game** (`rules.items.storage` is `off`): the **loader refuses** the command in an event and in a scene
  (`"storeItems" needs a shared storage, and this game has none (game.json rules.items "storage" is "off"): turn the storage on, or leave this
  command out`). Nothing is silently dropped.
- **The storage is full:** it cannot be; the storage has no size limit in this engine, so nothing is ever refused for room.
- **The weapon in hand** goes like every other item. With carried weapons the unit then fights with the next weapon it can wield, or unarmed
  (docs/ITEMS.md "State": the equipped weapon is the first wieldable item still carried). Name the weapon in `keep` to keep it. An accessory that
  raised the unit's maximum HP is gone with the rest: the unit's HP is clamped.
- **Whose items:** a unit that exists on the map (a rescued unit too). Units of a **computer** side are left alone (their storage is not something a
  story or a player ever reaches). A unit that carries nothing is left alone, silently. The drop mark (`drop`) is cleared.
- **In a campaign scene** `"storeItems": "eirik"` names a roster id (as the scene's `takeItem` does; only the shape is checked, the character may join
  later); the items leave the character's carried belongings and go to the campaign's carried storage of the chapter map's preparation side (the
  `side` of its `prep` block, else its first human side), so the next chapter's battle starts with them in the storage. A character who is not in the roster, or
  is gone, is left alone.
- Saved naturally: the units' inventories and the side's storage are already part of a battle save, and the roster and the carried storage part of a
  campaign save (docs/SAVES.md). A game that does not use the command saves, hashes and plays exactly as before.

Loader refusals: no storage (above); an unknown unit or filter; an unknown item in `keep` (`"keep" names unknown item "x"`); `keep` that is not a list;
an unknown key; in a scene a `storeItems` that is not a roster id. Code: `battle/story_store.cpp`, `campaign/scene_extras.cpp`,
`data/loader_story_store.cpp`, `data/loader_campaign_extras.cpp`; tests `tests/test_extras_store.cpp`.

### Status conditions

Three pieces for the conditions of `data/conditions.json` (docs/SRPG.md section 9); they work in **map events only**, because a condition ends
when the map ends and a campaign scene has no map: the campaign loader refuses all three with `"addCondition" is for map events only: status
conditions end when the map ends, so a campaign scene cannot give or take one` (likewise `removeCondition` and the test).

```json
{ "addCondition": "poison", "to": <unit filter>, "turns": 2 }
{ "removeCondition": "poison", "from": <unit filter> }        // or "removeCondition": "all"
{ "hasCondition": "poison", "unit": <unit filter> }          // a condition (`if`); `unit` default: any unit
```

- `addCondition` goes to every **living** unit the filter matches (the filter is required, `{}` = every unit) through the same code as a staff, a
  weapon or an item: an **immune** unit resists (`ConditionResisted`, nothing else), `replaces` is honoured, a unit that has it already has it
  start over (never summed), and the events are the usual `ConditionApplied` / `ConditionResisted` (the giver, `d`, is -1: the story).
  `turns` (1 to 99, optional) overrides the condition's own number of turns and is refused for a condition that does not count turns. A condition
  given by a battle-start event has its first turn counted at once (the first turn of the battle starts after the event), like one applied at
  the start of any turn.
- `removeCondition` takes the named condition (`"all"` = every condition) off every living matching unit: `ConditionEnded` with reason "cured".
  A unit that does not have it is left alone, silently.
- `hasCondition` is true when some living unit matching `unit` has the condition. Like `hasSkill` it is judged on the truth, not on what a team sees.
- Loader refusals: an unknown condition id (`"addCondition" names unknown condition "x" (data/conditions.json)`, or `... but the game has no
  conditions` when there is no file), a missing `to` / `from`, `turns` outside 1 to 99, `turns` on a condition that does not count turns, an
  unknown key, an id that is not text. Tests: `tests/test_story_conditions.cpp`.

### What the player gets (event stream)

New `EventType`s, in the stream in order with everything else (texts are
indices for `Battle::storyText(i)`):

| Entry | a, b, c, d |
|---|---|
| `Dialogue` | a = speaker unit (-1 none), b = text, c = free speaker name (-1 none), d = portrait id (-1 none) |
| `Focus` | a = cell, b = the unit followed (-1 just the square), c = scroll ms (-1 = not given), flags `kFocusNoWait` ("Scene presentation") |
| `UnitAdded` | a = unit, b = cell |
| `UnitRemoved` | a = unit, b = the cell it stood on |
| `UnitRelocated` | a = unit, b = cell it left, c = cell it reached |
| `SideChanged` | a = unit, b = old side, c = new side |
| `ObjectiveChanged` | a = text of the new objective (-1 none), b = objective set |
| `MusicChanged` | a = text index of the track id (-1 = the story stopped the music), b = fade in milliseconds (-1 = not given) |
| `SoundRequested` | a = text index of the sound id |
| `TerrainChanged` | a = number of squares, b = the terrain they all got (-1 = not all the same), data = the squares (cell numbers). Public (every team: terrain is known to all). Comes before `Choice`, which stays the last entry |
| `SceneEffect` | "Scene presentation": a = the command (`SceneFx::Kind`: background, fade, wait, shake, leave, weather, enter, move; enter / move: b = speaker, c = ms, d = place `kPos*`, flags `kSceneFadeTrans` = fade / slide, data = [portrait text index] for an enter that names one), b = text index (picture id, speaker, weather; -1 none), c = ms, d = fade colour / shake strength, flags `kSceneOut` / `kSceneFadeTrans` / `kSceneAuto`. A `Dialogue` entry also carries the line's `position` / narrate in its flags and "auto" in `data`; a walked `UnitRelocated` carries the path in `data` |

With fog of war every event, these included, also carries `Event::seen` (a bit per team: dialogue, focus and
objective changes and the two sound entries are seen by every team; the others by the teams that saw the unit or the square, see
docs/ECONOMY.md "Fog of war") and the stream gains `MoveBlocked`.

Gifts use the existing item and resource entries. The speaker's name for a
unit is `units[a].name`. Queries: `talkOptions(unit, fromCell)` returns
(target, event, label) for the Talk menu; `Action::talk(unit, target[, event])`
performs it (free like the item actions, cannot be undone);
`objectiveText()` is the text to show (at battle start and in the menu; no
entry is made at the start); `humanTeam()`; `unitByStoryId(id)`.

### Difficulty in events (docs/DIFFICULTY.md)

The condition above is the only way events read the difficulty. `addUnit` takes `"difficulty": ["hard"]` (it adds the unit only on the listed
difficulties; computer sides only) and `"difficultyShift": false` (a boss: the enemy numbers never change it); a stat written on the `addUnit` is exact.
`giveResource` gains for a human side are scaled by the difficulty's `resourcePercent` (in a campaign scene too); a negative amount never is.

## Changing the map

A battle's terrain is not fixed: a story command can change squares while it is played. The classic uses: a bridge appears, a wall opens, a
cave-in blocks a road, a gate rises. Everything that is worked out from terrain follows **at once**: movement costs, sight and fog (a wall that
opens opens the view), the terrain numbers formulas read (`ta`, `tb`), the computer opponent's paths, the squares a unit can stand on, capture and
visit squares. Heights are **never** changed (a changed square keeps its height; the 3D views fix their elevations at the start of the battle).

**Direct: `setTerrain`.** `{ "setTerrain": "bridge", "area": { "x": 4, "y": 1, "w": 1, "h": 3 } }` or `"cells": [[4, 1], [4, 2]]`. Squares that already
hold that terrain are left alone; if no square changes there is no entry. The change is permanent (nothing remembers what was there).

**Named changes.** A map may define the changes it will need, prepared in the editor (Changes tab):

```jsonc
"changes": {
  "bridge": { "cells": [ { "x": 4, "y": 1, "terrain": "bridge" } ] },
  "gap":    { "cells": [ { "x": 5, "y": 1, "terrain": "plain" } ],
              "structures": [ { "id": "gate", "kind": "portcullis_up" } ] }
}
```
`{ "applyChange": "bridge" }` gives the squares their new terrain and turns each listed structure (a structure the map placed, named by its `id`) into
the given kind (the same size: a structure's footprint never changes; the kind changes like an upgrade does, so art, movement cost and sight follow
and the structure's capture points, lock and hit points start over for the new kind). `{ "revertChange": "bridge" }` puts back **what those squares
held when the change was applied** (not "the map's original terrain": if something else changed them in between, the recorded values come back). Applying a
change that is applied already, or reverting one that is not, does nothing. Several changes may touch the same square; each remembers its own "before".
Rules the loader enforces (each message names the map, the change and the entry): `changes` is an object, ids are letters, digits, `_` and `-` (at most
64 changes, at most 4096 squares in one), a change changes something, every square is inside the map and named once per change, the terrain and the
structure kind exist, a structure `id` is one a placed structure has, the kind has that structure's size, and `applyChange` / `revertChange` name a
change of this map. **Adding or removing a structure** is not part of a change (indices of structures never change during a battle): turn one into a
different kind instead (a gate into a raised gate, a wall into rubble); a kind that is drawn as nothing works as "removed".

**A unit on a square it can no longer stand on (the stranded-unit rule).** After a change, a living unit standing on a changed square (or on a structure
that changed kind) whose move type cannot enter that square (cost 0 or less) is **moved at once to the nearest free square it can stand on** (grid
distance; the lowest square number, row by row, on a tie), exactly like a story `moveUnit` whose target was taken: a `UnitRelocated` entry follows the
`TerrainChanged` one and `reach` events fire for the new square. Units are handled in unit order. A unit with no free square to go to stays where it is (it can
still move off). Units that can still stand there never move: a flyer over a new river stays, a foot soldier on a new bridge stays. This is the least
surprising rule: nobody is ever left standing inside a wall, and nobody dies of a map change.

**Breakable terrain ("snag", "cracked wall").** Doors with hit points (docs/SRPG.md section 10) already cover it: a door kind with `by: ["break"]`, `hp` and
any footprint (3 squares wide: it breaks as **one piece**) that opens into a kind drawn as rubble or nothing, so the ground beneath shows. To change the map
when it breaks there is **no extra key**: the existing `place` trigger is the hook, and it runs for every way of opening (attack, key, staff, story):

```jsonc
{ "id": "wall_gone", "when": { "place": "w1", "is": "opened" }, "do": [ { "applyChange": "rubble" } ] }
```
(see "Walls that break" in docs/SRPG.md section 10; the computer opponent breaks one in its way when its unit is marked `goesFor: ["walls"]`.)

**Tiles in changes (docs/GRAPHICS.md "Tile layer").** On a map with a `tileLayer` a change and `setTerrain` can also set the squares' **tiles**
(presentation only; the rules never see them). In a change a cell may carry `"tile": 5` (a tile number of the map's tileset; `-1` or `null` = no
tile, the terrain's look shows); in `setTerrain` the command carries one `"tile"` for all its squares. A tile can be given with or without a new
terrain, so a change may only repaint a square (a torch burning out): then the square's terrain must still be named, as always, and may be the one
it has. **A square whose terrain really changes and that is given no tile loses its tile** (it falls back to the terrain's look: a bridge replaces a
painted river instead of staying drawn as water), unless `"keepTile": true` (on a cell, on the whole change, or on a `setTerrain` command), which
leaves its tile alone. A square whose terrain does not change and that is given no tile keeps its tile. `revertChange` puts back **the tiles the
squares held when the change was applied**, together with their terrain. The refusals are the loader's: `"tile"` outside the tileset, a `"tile"` on
a map with no tile layer (`needs a tile layer: the map has no "tileLayer"`), `"keepTile"` that is not true / false. A square whose tile alone changes
is in the `TerrainChanged` entry (it flashes like any changed square) but never strands a unit. Games and maps without a tile layer behave exactly
as before.

**What the player does.** `TerrainChanged` plays a brief white flash on the changed squares (every kind of view) and the camera looks at them; the squares
already show their new terrain. In the **3D views the whole ground mesh is rebuilt** the first frame the terrain differs from what it was built from (the renderer
has no partial mesh update; a map is a few thousand squares at most). Under fog a square the viewer cannot see changes without a flash.

**Saves** (docs/SAVES.md "Map changes"): one optional key, `mapState`, only after something changed; a battle that never changed the map, or switched
fog, saves byte for byte as before. **Tests:** `tests/test_mapchange.cpp`, `tests/mapchange_player_test.py`; fixture `tests/fixtures/mapchange`.

## Objectives (map file)

```jsonc
"objectives": {
  "text": "Defeat Gorm",                         // shown to the player
  "win":  [ { "defeated": { "unit": "gorm" } } ],        // any one wins
  "lose": [ { "defeated": { "unit": "eirik" } }, { "round": 15 } ]   // any one loses
}
```
Kinds: `defeated` (filter; a side or team = all of its units; true when
something matching exists or existed and none is alive), `reach`
(rectangle + `by`; true while a matching living unit stands inside; refined:
without `by`, under `win` any unit of the human team counts and under `lose`
any unit of another team), `round` (true when that round has begun),
`survive` (rounds; the same as `round` but only under `win`: a loader error
under `lose`).

Objectives are judged for the **human team**: the team of the first side
whose controller is human, or the first side's team if there is none. They
are checked after every action and at battle start; if a `lose` and a `win`
hold at the same moment, the team has lost. Missing `objectives` = the
game's `rules.defeat`, as today, which also keeps applying to every side
(a human team that is wiped out has lost whatever the objectives say). An
objective block with only a `text` shows the text and judges nothing.

## Dialogue (player)

Built (step 2): `engine/player/ui/dialogue_{text,box}.*` (text logic and the
box), `engine/player/battle/battle_view_story.cpp` (how the view plays the
story entries) and `battle_story.cpp` (the scene side).

- **The box**: across the bottom of the screen, just above the hint line, in
  every kind of view (it is a plain 2D overlay). The speaker's name on the
  first row (the unit's `name`, or the free name; a line with neither has no
  name row); the text appears at 60 letters a second
  (`kDialogueLettersPerSecond`), word-wrapped to the box, up to three lines
  a page (fewer with a very large font). A line too long continues on a
  second page: Confirm turns it. A blinking `v` shows that the page is
  complete. The map stays visible above; its camera, cursor and orders do not
  react while a line is up.
- **Input**: Confirm / tap = show the whole page, then turn the page, then
  end the line. Holding Cancel or Menu for 0.5 s, tapping the **Skip** tab
  on the box's top-right corner, or the `--input` token `skip` skips every
  remaining line of the **current run of events** (one `Battle::apply` /
  `start` / AI step's entries; also fights and moves still play). A tap of
  Cancel alone does nothing. The hint line shows the keys.
- **Portrait**: `"portraits": { "<id>": { "sheet": …, "cell": [x, y] } }` in
  `data/graphics.json` (docs/GRAPHICS.md "Portraits"). A line uses its own
  `portrait` id, else the speaking unit's story id (`Unit::storyId`), else
  none; an id with no portrait shows the box without one. It is drawn to the
  left of the box at 48 x 48 px with the 8 px font (it grows with the font
  size and the UI scale), keeping the cell's proportions, sitting on the
  box's bottom edge.
- **Pausing**: a `Dialogue` entry becomes the view's current entry with no
  end time; the view's queue does not advance, so everything after it (a
  reinforcement the line announces, the AI's next step) waits until the scene
  calls `BattleView::skip` (Confirm on the last page) or `skipDialogues` (skip
  all: lines then pass at once until the queue drains). In `--autoplay` a
  line ends by itself after 1 s (and nothing else is waited for), so runs and
  the embedded editor's Play cannot hang on dialogue. A line triggered on the
  AI's turn pauses the AI the same way. Loading a save replays nothing (the
  view starts empty; the story state is saved, the stream is not).
- **Other entries**: `Focus` moves the view's focus to the square or unit and
  keeps it there for the rest of the run (so the camera stays on it while the
  next line is up); `UnitAdded` pops in like a built unit; `UnitRemoved`
  fades out (no death effect); `UnitRelocated` slides (snaps when more than 8
  squares away); `SideChanged` keeps the old colour until its entry plays,
  then takes the new side's colour with a short white flash and a "Joins
  <side>" float; `ObjectiveChanged` shows a "New objective" banner with the
  text (nothing when the objective was cleared).
- **Talk**: the action menu has an entry per `talkOptions` result before
  "Items": the event's label ("Talk" when it has none), and with several
  results "<label>: <partner name>". Choosing it submits `Action::talk`; the
  menu is then rebuilt and the move can no longer be undone (like the item
  actions), unless the event ended the turn, removed the unit or ended the
  battle.
- **Objective**: when a new battle starts, its objective text is a second
  line under the first turn banner ("Objective: …", the banner stays up
  2.4 s; Confirm skips it). The battle menu (the one with End Turn) has an
  "Objective" entry, only when there is text, that shows it in a small box.
  A save restores the current objective. A battle ending through objectives
  shows the usual VICTORY / DEFEAT after the `won` / `lost` events' lines.

## Campaign

Built (step 4: the rules core and the player; the editor part is step 5). `data/campaign.json`
is optional; a game without it behaves exactly as before and keeps its save
hashes. Code: `engine/core/data/campaign.h` and `loader_campaign*.cpp` (the
file), `engine/core/campaign/` (playing it and saving it), `Battle::setStartingRoster`
(`battle/carried.*`); tests `tests/test_campaign_*.cpp`; `ote-sim <game> --campaign`.

```jsonc
{ "name": "The Ford War",
  "rules": { "fallen": "gone",          // "gone" (default): a defeated unit never returns; "return": it is back next chapter
             "onDefeat": "retry" },     // "retry" (default): the chapter can be started again; "end": the campaign is over
  "freeBattles": "none",                // "none" (default) | "all" | ["map_id", ...]: maps that can also be started on their own
                                        // or the object form { "maps": "all" | ["map_id", ...], "when": "always" | "storyComplete",
                                        //   "lockedNote": "text" } (see "Free battles" below)
  "chapters": [
    { "id": "ch1", "name": "The Ford", "map": "bridge",
      "before": [ { "say": "…", "speaker": "eirik" } ],   // scenes, see below
      "after":  [ { "if": [ { "switch": "village_saved" } ], "then": [ { "goto": "ch3b" } ] } ],
      "next": "ch2" }                    // optional; default = the following chapter in the list
  ] }
```

Keys are checked like the rest of the game files: an unknown key, a bad
`rules` / `freeBattles` value, an empty `chapters` list, a duplicate chapter
id, an unknown `map` / `next` / `goto` / `freeBattles` map, a `goto` in a
`before` scene to its own chapter, or a scene command that is not one of the list below is a
loader error naming the file, the chapter and the command
(`data/campaign.json: chapter "ch1": "before": command 2 (say): …`). A chapter
nobody can reach is **not** an error. `name` of the campaign and the chapters
is wording (default: "Campaign" / the chapter id).

**Scenes** are lists of a subset of the event commands, same JSON shapes: `playMusic`, `stopMusic` and `playSound`
(as in the map events, docs/SOUND.md; they come back between the `say` lines as `SceneLine`s of kind `PlayMusic` / `StopMusic` / `PlaySound`
with `text` = the id and `fadeMs`, and the player acts on them when it reaches them; nothing in the playthrough changes), `say`
(`speaker` = a roster unit id, only checked for being a well-formed id since the
unit may join later; or `speakerName`; optional `portrait`), `setSwitch`,
`setVariable`, `addVariable` (also reading a roster unit's level, most hit points or a stat: "Reading a unit's numbers"), `takeItem` ("Taking an item away"), `giveResource` (`"side": "<side id>"`: added to what that
side carries; a side id is only checked for being well-formed because scenes
know no map), `if` with the conditions `switch`, `variable`, `alive` / `dead` (a
roster id) and `{ "goto": "<chapter id>" }` (also inside `if`; in a `before` scene it skips the chapter's battle, see "Dialogue choices"), `choice` ("Dialogue choices").
A chapter's `before` / `after` may also be written `{ "music": "scene_theme", "commands": [ ... ] }`: the same
scene with a `playMusic` in front (`commands` optional; the data hash sees the plain list, so adding music to a scene changes no save).
Refined: `alive` = the id is in the roster and not gone, `dead` = it is in the
roster and gone (a unit that never joined is neither). A scene runs at once
with no input: state changes apply immediately and the `say` lines come back in
order as `SceneLine`s (speaker id, the name to show, portrait id, text), resolved like a battle's
`Dialogue` entry: a roster unit's own name, else the free `speakerName`, else
nobody; so the player can use one dialogue box for both.

**Where the player is** (`Playthrough`): the chapter and a **phase**: `Before` (the
chapter's before scene is due), `Battle` (scene done; the battle is to be played
or is under way), `After` (battle won and taken over; the after scene is due),
`Finished` (the last chapter's after scene ran), `Over` (a battle was lost and
`onDefeat` is `end`). Also the roster, what the sides carry, switches and
variables. The next chapter after an `after` scene is the last `goto` that ran,
else the chapter's `next`, else the following one in the list; after the last one
the playthrough is `Finished`.

**Carry-over.** The **roster** holds every named unit (`id`) of a human side: id, type,
name, level, xp, stats, items with uses left, the side id it last fought on, and
whether it is gone. Units without an id are extras and never carried. "Human
side" = a side the map does not give to the computer.
- *Starting a chapter's battle* (`startChapterBattle`): the map is loaded and the
  Battle initialised as usual, then every map unit with an `id` on a human side is
  looked up in the roster: found, it takes the roster's type, name, level, xp, stats
  and items, starts at full HP (maximum from its stats and accessories) on the map's
  square; marked gone, it is not placed (taken off the board before anything
  happens, not "defeated", so no event sees it); not in the roster, it starts as
  the map says. The same goes for a named unit that an `addUnit` event creates later
  on a human side (a gone one is not added; logged). The roster dresses a unit only
  where it is placed or added on a human side: a unit that starts on the computer's side
  and is recruited by `changeSide` stays as the map says until the chapter's end.
  The side's storage = carried storage followed by the map's `storage` entries;
  resources = carried + the map's (the map's amount is that chapter's allowance).
  Switches and variables go in through `Battle::setStartingStory`.
- *After a won battle* (the human team won, `finishChapterBattle`): every living
  named unit on a human side is written to the roster (including ones that joined
  through `addUnit` or `changeSide`); a named human-side unit that was defeated
  becomes gone (`fallen: gone`; a unit that joined and fell in the same chapter
  gets a gone entry too) or stays as the roster had it at the start of the chapter,
  gaining nothing (`fallen: return`; one that was not in the roster stays out of
  it, so it starts as its next map says); a named unit that is on a non-human side at
  the end leaves the roster; storage, resources, switches and variables of the human
  sides are taken from the battle. Refined: a unit removed by a `removeUnit` event is
  counted like a defeated one (the Battle does not tell them apart). Then the `after`
  scene runs, then the next chapter.
- *After a lost battle* (a draw counts): `retry` leaves the playthrough exactly as it
  was before the battle (phase stays `Battle`; the chapter can be started again; the
  before scene is not replayed); `end` makes the phase `Over`.

**Saves.** One save slot is one playthrough: `"playthrough": { … }` and, in phase
`Battle` while a battle is under way, `"battle": { … }`. Format, hash and the
exact calls: docs/SAVES.md "Campaign saves". Saves of games without a campaign are
unchanged byte for byte.

**Free battles.** `freeBattleAllowed(db, mapId)`: true for every map when the game has no
campaign; otherwise per `freeBattles`. A free battle is an ordinary `Battle::init` +
`start` and never touches a playthrough.

**Free battles that wait for the story.** `freeBattles` takes an object next to the plain values:
`{ "maps": "all" | ["map_id", ...], "when": "always" | "storyComplete", "lockedNote": "text" }`. `maps` is
required and means what the plain value means ("none" is accepted too); `when` defaults to `"always"`; the plain
forms (`"none"`, `"all"`, a list) mean `when: "always"`, stay valid and are written back unchanged. With
`"storyComplete"` the player's **game page** lists the free battles only when this player has completed the story
(rule and record: docs/SAVES.md "Progress record and free battles that wait for the story"); until then the list is
simply absent (no heading, no teaser) unless `lockedNote` is set, which is shown as one greyed line in its place.
Nothing special is shown the first time it unlocks. `when` and `lockedNote` only steer the game page:
`freeBattleAllowed`, `--map ID` and the editor's Play panel still start any map. Loader refusals: no `maps`, a
`maps` / `when` / `lockedNote` of the wrong kind, an unknown key, an unknown map. The save hash counts the object form
as its `maps` value alone, so moving a game to it (or changing `when` / the note) keeps every save usable. Tests:
`tests/test_free_battles.cpp`, `tests/freebattles_player_test.py`.

### Parties

Built. A campaign can hold several **parties**: each has its own roster of characters (their items, levels and so on), its own carried
storage (the convoy) and its own funds and research, while the story (switches, variables, the chapter flow) is shared by the whole playthrough.
One party is active at a time. This is the shape of "part 3 starts a whole new game with its own characters, items, convoy and funds, and part 4
combines them with the original party". Code: `engine/core/campaign/parties.cpp`, `engine/core/data/loader_campaign_parties.cpp`; tests
`tests/test_parties.cpp`, `tests/test_parties_prep.cpp`, `tests/parties_player_test.py` (game `tests/fixtures/parties-flow`).

```jsonc
{ "parties": [ { "id": "main",  "name": "The company" },      // the first is the party the campaign starts with
               { "id": "north", "name": "The northern road" } ],
  "chapters": [
    { "id": "ch1", "map": "road" },                                          // part 1: played by "main"
    { "id": "ch2", "map": "pass" },                                          // part 2: "main"
    { "id": "ch3", "map": "forest",                                          // part 3: a whole new game for "north"
      "before": [ { "switchParty": "north" }, { "say": "Meanwhile, in the north." } ] },
    { "id": "ch4", "map": "reunion",                                         // part 4: the roads meet
      "before": [ { "mergeParties": ["north"], "into": "main" } ] } ] }
```

- **`parties`** (optional): a list of `{ "id", "name" }`; ids are letters, digits, `_` and `-`, unique; `name` is wording (default: the id; it is
  not part of the save hash, like a chapter's name). No key (or an empty list) = the campaign has one roster, exactly as before: nothing about
  parties is written to saves, the hash and the sims are unchanged.
- **`{ "switchParty": "north" }`** (a scene command, in `before` / `after`, also inside `if` and answers): makes that party the active one. A party
  used for the first time starts **empty**: no characters, empty storage, zero resources, no research, no remembered deployment - "a whole new
  game". Switching back brings the first party back exactly as it was left. Switching to the party that is already active, or to one that was
  merged away, does nothing.
- **`{ "mergeParties": ["north"], "into": "main" }`**: the listed parties join `into`, which becomes (or stays) the active party. Units move over;
  storages are appended; resources are added (never above one billion); finished research is united; the remembered deployment keeps the target's
  order and adds the others' characters after it. The merged parties **cease to exist**: a later `switchParty` to one of them does nothing, and a
  party that was never used is still retired (it had nothing to give).
- **A character both parties have** (decided; no static check is possible, as it depends on who was recruited): the **target's copy stays as it is**
  (level, stats, items) and the other copy is dropped, except that **its items go to the target's storage** (the side id it fought on), so nothing
  is lost. A fallen (`gone`) copy in the target stays gone.
- **`"party": "north"` on a chapter** is a shorthand for a `switchParty` at the very start of that chapter's `before` scene (it takes effect even
  when the chapter has no scene). Both forms work; a scene command is the only way to switch at another moment (for example in an `after` scene).
- **Loader refusals** (`data/campaign.json: ...`): a duplicate or badly formed party id, an unknown key on a party, `parties` that is not a list; a
  `switchParty`, `mergeParties` or chapter `party` naming a party that is not listed (and a plain "needs the campaign to have parties" when there
  are none); `mergeParties` that is not a non-empty list, lists a party twice or lists `into` itself, a missing `into`.

**What is per party**: the roster (every named unit with items, level, xp, stats, skills, ranks), each carried side's resources, storage and finished
research, `lastDeployed` (the default pick of the preparation screen) and the preparation screen's own state. **Shared**: switches, variables, the
once-per-campaign quotes, the chapter flow and, for **supports**, the pairs' points and levels (they are global: a pair is kept per character pair,
not per party; pairs whose two characters are not both in the active party simply do not meet at the camp, since a conversation needs both in the
roster, and a battle only places the pair's characters that exist). Play counters live with the player's own settings, not the playthrough.

**What uses the active party**: the preparation screen (its candidates, items, shop, selling and storage, and the funds it shows),
`prepareChapterBattle` (map units marked as roster characters are dressed from the active party's roster), `finishChapterBattle` (written back to the
active party; a recruit joins it; resources, storage and research go to its carried sides), the scene commands `takeItem`, `storeItems`,
`giveResource` and the `alive` / `dead` conditions or readings of a unit (they look at the active party only), and the camp's pending supports. A
lost battle (`retry`) leaves the active party as it was. **A map that names a character of another (inactive) party**: that character is created
fresh from the map's own definition, exactly as if never recruited (no roster entry matches); the editor notes it where it can be seen from the
chapter order (Campaign screen checks). The loader cannot know who was recruited, so it says nothing.

**Saves** (docs/SAVES.md "Campaign saves"): a campaign that has parties adds `"party"` (the active one's id), `"parties"` (the other parties that
were used, each with `roster`, `sides` and optional `lastDeployed` / `prep`) and `"mergedParties"`; the active party's own data stays in the usual
`roster` / `sides` / `lastDeployed` / `prep` keys, so a campaign without parties saves byte for byte as before. The `parties` keys are part of
`data/campaign.json`, which counts for the save hash (a save from before the parties were added is therefore refused, like any other data change).

**Player**: the chapter's title card reads "Chapter 3 - The northern road" and the preparation screen's title "Preparation - The northern road" when
the campaign has parties; nothing else changes. The log says `campaign: chapter 3 is played by party 'north' (The northern road)` when a chapter's
battle starts. `ote-sim <game> --campaign` follows the same rules and prints `party: <name>` after each chapter's before scene.

**Editor**: the Campaign screen has a "The parties" card (add, rename, reorder, delete with a list of what goes with it; the first is the starting
party), a "Starts with party" choice on each chapter, the two commands in the scene picker under "Party", a party sign on the timeline's boxes
("What changes"), and checks: the loader's refusals, plus notes for a party switched to but never merged or ended on, a merge of a party nothing
switched to before, a switch to a merged party and a map that puts characters on the player's side that so far only another party had.

### Campaign (player)

Built (step 4, player half). Code: `engine/player/app_campaign.cpp` (the flow), `campaign/` (`campaign_logic.*` the
decisions, `story_scene.*`, `end_scenes.*`), `battle/battle_campaign.cpp`, `scene_game.*` (the page); the saves are in
docs/SAVES.md "Campaign saves in the player". All wording below is placeholder.

- **Game page** of a game with `data/campaign.json`: **Continue** (when a Continue save exists: the chapter's name, and
  "Turn N" in the middle of a battle, else where the story stands), **New game**, **Load** (saving `any`), Game options,
  Back, then under the heading **Free battles** the maps that `freeBattleAllowed` allows (none = no heading). New game
  while a Continue save exists asks first ("This replaces your Continue save."). A free battle ends back on this page
  (there is no separate map list for such a game).
- **Scene screen** (`StoryScene`): the style's background colour; when a chapter begins, a **title card**
  ("Chapter N" small above the chapter's name, N = its position in the list; Confirm / tap leaves it, or it leaves by
  itself after 2.5 s); then the lines in the **same `DialogueBox` as a battle** (portrait from the line's `portrait`, else
  the speaker's roster id; same controls: Confirm / tap, pages, hold Cancel or Menu for half a second, the Skip tab,
  the `skip` script token). An empty before scene is only the card; an empty after scene is not shown at all. Note:
  a roster id names a unit only once it is in the roster, so the first chapter's before scene should use
  `speakerName` (the roster is empty before chapter 1's battle).
- **Flow**: New game -> chapter 1's card + before scene -> `startChapterBattle` -> battle -> `finishChapterBattle` when it is
  over. **Won**: the normal VICTORY banner ("Confirm: continue"), then the after scene, then the next chapter's card +
  before scene; after the last chapter's after scene an end screen ("The End" + the campaign's name) -> the game
  page. **Lost, `retry`**: the normal DEFEAT banner, then a choice **Try again** / **Back to title**; Try again starts the
  chapter's battle again with a new seed (`args.seed` + the number of battles started so far) and without the before scene;
  Back to title (or Cancel) goes to the game page, where Continue starts the chapter again. **Lost, `end`**: DEFEAT, then
  the end screen "The story ends here." -> the game page. A draw counts as lost. "Leave battle" in the battle menu
  saves and returns to the game page as in any battle.
- **`--autoplay`** plays the story through (see docs/SAVES.md "Command line"): the AI plays the human side and every
  screen in between passes by itself (card 1 s, each line 1 s, result 1.5 s, end screens 1 s, retry = Try again).
- **Web**: the same code; `tools/webtest/suites/campaign.mjs` plays New game -> scene -> battle -> reload -> Continue on
  the test game in a temp folder that serves the web build with its own `games/`.

## Preparation

Built (PLAN k item 8; rules `engine/core/campaign/prep*.cpp`, loader `engine/core/data/loader_prep.cpp`, player
`engine/player/battle/battle_prep*.cpp`, editor Preparation tab and Slots tool; tests `tests/test_prep_*.cpp`,
`tests/srpg_player_prep_test.py`, `editor/web/tests/unit/prep.test.ts`, `tests/e2e/prep.spec.ts`; fixture `tests/fixtures/srpg-prep`).
All wording is placeholder (player: `battle/prep_text.h`; editor: `panels/prepText.ts`).

A map may have a **preparation screen** shown before its battle. The maker chooses per map: no `"prep"` key = the map starts exactly
as it is placed (data, saves and hashes unchanged). Order: the chapter's `before` scene, then the screen, then the battle-start events.

```jsonc
"prep": {
  "slots": [ { "x": 0, "y": 1 }, { "x": 0, "y": 2 } ],   // deployment squares, numbered from 1 in this order (required, up to 64)
  "limit": 3,                                              // most characters deployed, 1 .. slots (default: all slots)
  "required": [ "sera", { "character": "tomas", "slot": 2 } ],  // always deployed; "slot" (number from 1) also fixes the square
  "forbidden": [ "wren" ],                                 // cannot take part in this map
  "fixed": [ "guard" ],                                    // placed units that stay where the map puts them, outside the choice
  "shop": { "sells": [ "potion", "tonic" ],                // same item ids as a structure's "sells"; each needs a price
            "buys": true },                                // optional, default true: the shop also buys the party's items back ("Selling" below)
  "side": "blue" }                                         // the human side the deployed characters join (default: the first one)
```

**Who is a candidate.** A *candidate* is any living roster character (the party carried between chapters) plus the named units of the
prep side that the map itself places and the roster does not know yet. A candidate's placed square is ignored: the slots decide where
it starts. Units that stay where they are placed (computer units, unnamed units, other sides, the `fixed` list) are not candidates and
do not count against the limit. The old rule "the roster dresses only placed units" no longer limits a chapter: the roster may hold
more characters than a map deploys. A benched character stays in the roster untouched (level, xp, items, skills) and is available next
chapter; a map character who is benched (or never opened the screen) still joins the roster when the chapter is won. `fallen: gone`
affects only units that were in the battle; `changeSide` to a computer side still removes a unit from the roster; `addUnit` joiners
are written to the roster as before.

**Default pick** (what the simulator, `--autoplay`, the editor's Play-without-interaction and plain `startMap` use, no screen):
required characters first, then those deployed in the previous chapter that had a preparation (`Playthrough::lastDeployed`, in battle
order), then the rest in roster order (then the map's own candidates), up to the limit; slots are filled in order (a required character
with its own slot takes it first; a unit goes on the first free slot its movement type can stand on). The screen opens on this pick.

**Rules** (`core/campaign/prep.h`, free functions over a `Playthrough` and the chapter's `MapDef`, each refusing with a plain reason):
`prepOpen`, `prepDeploy`, `prepBench` (a required character cannot be benched; the limit; forbidden; fallen), `prepMove` (move to a free
slot or swap two; both must be able to stand there; a required character with its own slot cannot move), `prepGive` / `prepStore` /
`prepFetch` (trade between any two roster characters and the shared storage; the storage exists when `rules.items.storage`
is not `off`; any setting but `off` counts as reachable on this screen), `prepBuy` (the shop; price paid from what the prep side
carries; into the character's pack, else the storage, else refused), `prepSell` / `prepSellValue` (selling, below), `prepBegin`. `prepOpen` also folds the map's
allowance for the prep side (resources, storage) into what the campaign carries, so the screen can spend it and the battle starts from
the carried amounts alone. `prepareChapterBattle` builds the battle from the deployment (`prepApplyToMap`).

**Selling** (owner decision 2026-11-17; in-battle shops did not buy items before and still do not: this is the preparation shop only).
A shop that has `"buys"` not false lets the player sell a character's items and items in the storage for resources.

- **The rule** (one place, `prepSellValue`): for every resource in the item's `price`, the shop pays
  `floor(amount * sellPercent / 100 * usesLeft / usesWhenNew)`; an item with unlimited uses is not scaled. So a worn item fetches less, a
  used-up one (0 uses left) fetches 0 and can still be handed over for nothing. `sellPercent` is `rules.items.sellPercent`, a whole number
  0 to 100, default **50** (`docs/GAME_FORMAT.md`). Example: a Sword (price 100 gold, 10 uses) with 3 uses left fetches 15 at the default.
- **Refused, nothing changes, with a plain reason:** an item with no `price` ("Token has no price, so nobody buys it."), an item whose
  `items.json` entry says `"sellable": false` ("Relic cannot be sold."), a battle without a shop ("This battle has no shop."), a shop with
  `"buys": false` ("This shop does not buy items."), a position that holds nothing, a character who is not in the party.
- `prepSell(db, pt, map, id, slot, why)`: `id` = a character, or empty for the storage; `slot` = the item's position in that pack. The
  money goes to what the prep side carries (the same total the shop spends, the battle starts from, and the save holds), the item leaves
  the pack. Works with or without `rules.items.storage`.
- The simulator's default pick does not sell, so `ote-sim` results are unchanged.

**Equip.** `prepEquip(db, pt, map, id, slot, why)` moves the weapon at `slot` to the front of the pack, which is what makes it the one
in hand (docs/ITEMS.md "State": the equipped weapon is the first item that is a weapon the unit can wield). It is refused, with the
reason, in a game with fixed weapons, for an item that is no weapon, for a weapon the character cannot wield now (class, character lock
`wieldedBy` / `wieldedByTypes`, rank: it asks the battle's own `canWieldNow`, so the screen and the battle agree), and for the weapon
already in hand. `prepEquipReason` is the same test without changing anything, `prepWieldReason` asks only "can they wield it",
`prepEquippedAt` says which position is in hand (first wieldable, so a leading weapon that cannot be wielded is not the equipped one).

**Free battles** (no roster): the same calls on a scratch playthrough; the pool is the map's named units of the prep side; positions,
units and the map view only (no items or shop in the player).

**Saves.** In phase `battle`, `"playthrough": { "prep": { "open", "folded", "deployed": [ { "id", "slot" } ] }, "lastDeployed": [ids] }`,
written only when there is something to write. A save made during the preparation has no `"battle"` and restores into the screen; a save
in the battle restores the same battle (the deployment is in the playthrough). Bad ids, slots or a deployment the map cannot take are
refused with the field named.

**Player** (Mode::Prep in the battle scene; the Battle is set up but not started): menu Units / Positions / Items / Shop / View map /
Save / Leave (Abandon in a free battle) / Begin. Units: a list with "Slot N" / "Out", `*` for required, the reason in the detail line,
"Deployed n / limit". Positions: slots numbered on the map, pick up / put down (swap), tap works, a Back button for pointers. Items:
whose items, trade with another character or the storage (the battle's trade panel). In a game with carried weapons the weapon in hand
is marked, a weapon the character cannot wield is greyed with the reason in the detail line, and choosing a weapon of the character
on the left that is not in hand opens a small menu under the row: **Hand over** (**Put in storage** when the other side is the storage)
or **Equip** (greyed, with the reason, when it cannot be wielded; the whole reason also shows in the message line). Choosing the weapon in
hand, or any other item, hands it over at once as before, so a game with fixed weapons sees no change. Shop: tabs **Buy | Sell** in the
panel's title row (Left/Right or a tap on a name; shown only when the shop buys). Buy is the battle's shop list, then who gets it.
Sell asks whose items first (every character, then the Storage; counts shown), then lists that pack with what each item fetches
("10 Gold"; a greyed "-" and the reason where the item cannot be sold); choosing a row sells it at once (buying has no confirmation
either), "Sold Potion for 10 Gold" shows, and the Gold in the panel's head and the unit details change live.
View map: free cursor, unit details, the reach of a visible enemy under the cursor (fog respected). Begin asks "Begin with N of M?"
when fewer than the limit (and than the available characters) are deployed. After a lost battle the screen comes back with the same
choice. Works in all four kinds of view (the map parts reuse the battle's drawing).

**Checks.** The loader refuses, naming the file and map: an unknown key, no slots, a slot outside / duplicated / on a square nobody can
stand on / on the square of a unit that stays, a limit of 0 or above the slots, more required characters than the limit, a required slot
that does not exist or is used twice, a character both required and forbidden / fixed, a fixed id the map does not place, a forbidden
unit the map places on the prep side, an empty or unknown or unpriced shop item, a computer or unknown `side`.

## Dialogue choices

PLAN k item 6. All wording is placeholder until the owner approves it.

**The command** (map events and campaign scenes, same JSON in both):

```jsonc
{ "choice": "An old gate blocks the path. What now?",   // the question (not empty)
  "speaker": "mara",            // or "speakerName": "Guide", and "portrait": "id": as for "say" (all optional)
  "store": "gate",              // optional: a game variable that receives the NUMBER of the answer picked
  "answers": [                  // two to four
    { "text": "Open it",  "do": [ { "setSwitch": "gate_open", "to": true } ] },        // "do": any commands legal in this place
    { "text": "Go around" },                                                          // "do" optional (nothing happens)
    { "text": "Ask Tobin first", "if": [ { "alive": "tobin" } ],                      // "if": optional, hides the answer
      "do": [ { "choice": "Trust him?", "answers": [ { "text": "Yes" }, { "text": "No" } ] } ] } ] }   // a nested question
```

Number = the answer's position **as written** (1 = the first), also when earlier answers are hidden; 0 = never asked. It is
an ordinary variable, so everything that reads variables works: `{ "variable": "gate", "op": "=", "value": 2 }` in event and
answer conditions and in campaign scenes, and it is saved and carried between chapters like any variable (so it needs no
save format of its own). A new choice id was not invented: the maker's id is the variable's id (game.json `variables`).

**Branching the campaign.** Two ways, both on what exists: an answer sets the variable and a later `if` branches (`{ "if":
[ { "variable": "road", "value": 2 } ], "then": [ { "goto": "ch3" } ] }`), or the answer holds `{ "goto": "<chapter id>" }`
itself. `goto` works in every scene command list, answers included. In an **after** scene it picks the next chapter (the last
`goto` that ran wins, as before). In a **before** scene (new) it skips this chapter's battle: the chapter it names becomes
current in phase `Before`, so its own before scene runs next, and from there the order is normal (its `next`, else the
following chapter). That is how a first scene asks "Play the tutorial?" (a chapter whose before scene asks and whose map is
never played). A before scene may not name its own chapter (loader error: it would run for ever); longer cycles are the maker's
to avoid.

**Hidden answers.** `if` uses the engine's conditions (map: all kinds; scene: switch, variable, alive, dead, support). At run
time: two or more offered = asked; exactly one offered = taken at once (stored, its commands run, nothing shown); none offered =
the question is skipped (nothing stored). The loader cannot know which conditions hold, so it only refuses fewer than two or
more than four answers written.

**Loader refusals** (plain English, naming the map / chapter, the event or scene and `command N (choice)`): a question that is
not text or is empty; fewer than two / more than four answers; an answer with empty or missing `text`; an answer with the same
`text` as an earlier one; an unknown key (in the command or an answer); an unknown command or a bad command inside an answer
(the message names `answer N: "do": command M`); an unknown `store` variable, unknown switch / unit in a condition; `goto` to
a chapter that does not exist (also inside answers, nested questions and `if`s); a `goto` in a before scene to its own chapter;
and **two questions of one map (or of one scene) with the same `store`** (almost always a copy that was not edited; reuse across
different scenes / maps is fine, the later answer overwrites). The question wording and the answers' `text` are wording for
the save hash; `store`, the answer count and what the answers do are rules.

**Runtime in a battle** (`Battle`, `core/battle/story_choice.cpp`). `Battle::setAskChoices(false)` (default; `ote-sim`, AI-only
runs, `--autoplay`, the editor's Play): the first offered answer is taken on the spot and a `Dialogue` entry with the question
leaves the stream, so every existing consumer works. With `setAskChoices(true)` (the player, a person at the screen)
`apply` / `start` stop at the first unanswered question: the entries up to and including a `Choice` entry are returned
(a = speaker unit, b = question text, c = free speaker name, d = portrait id, `data` = the offered answers' texts), the Battle
is the state exactly at the question (what the view shows matches the entries), `choiceWaiting()` is true and every
`apply` is refused. `answerChoice(pick, events)` (pick = position among the offered answers) goes back to the state before
the action, runs the same action again with all answers given so far (deterministic, so it reaches the same question) and
returns the entries after the question, up to the next unanswered question if there is one. The rest of the action (a fight, the
turn) therefore happens after the answer in the stream and in the state, exactly as if the answer's commands had been written
in place: a test checks the final state against the plain commands. Cost: one copy of the Battle per action while asking is
on and the map has a choice (maps without one are untouched and byte-identical). `Choice` is the last `EventType`.

**Saves.** The answer is the variable, saved with the battle and the playthrough as always; a save made after the choice
(also in the middle of a battle) restores it, and the question is not asked again. A save made **while a question is on screen
is impossible**: the player is busy then (no menu, no autosave), and the save writers refuse a Battle with a waiting question.
The previous Continue save stays (a battle question: the playthrough before the battle, or the last turn's save; a scene
question: the state before the scene), so Continue asks the question again.

**Scenes** (`runScene`): `runScene(db, pt, which, lines)` takes the first offered answer (a `Say` line with the question).
`runScene(db, pt, which, lines, answers)` stops at the first unanswered question: its `SceneLine` (kind `Choice`, `text`,
`answers`, speaker) is the last line, `waiting` is true, the playthrough is as the scene left it and the phase has not moved.
To go on the caller runs the scene again from a copy of the playthrough as it was before it with the answers plus the new one:
the same lines come back (answered questions as `Choice` lines with `waiting` false; the player skips what it has shown), then
the rest.

**Simulator.** `ote-sim` (single maps and `--campaign`) always takes the first offered answer (documented default; no option, the
numbers of the demo games do not change).

**Player.** The question reads like any line in the dialogue box (same speaker / portrait handling, pages, typing). When its last
page is complete the answers come up in a panel directly above the box: Up / Down move the highlight (they wrap), Confirm
picks; a pointer over an answer highlights it, a tap on the highlighted answer picks it (a first tap on a touch screen only
highlights, so a slip of the finger does not decide the story). Answers wrap to the box's width; when four long answers would not
fit above the box on a small screen each gets fewer lines and the last line is cut with "..". There is no way out: Cancel, Menu,
the Skip tab, holding Cancel / Menu and the `skip` script token only show the whole question at once; skipping dialogue (in
a battle or a scene) stops at the next question. Hint line while the answers are up: "Up/Down + Z: answer   tap: pick, tap again:
confirm". Test scripts: the player logs `choice: ... N answers: 0@x,y 1@x,y` (row centres, logical pixels) and `choice: battle|scene
answer N`; `tests/choice_player_test.py` and the game `tests/fixtures/choice-story` use them.

## Battle and defeat quotes

Built (PLAN k item 7; owner: "go ahead", scope below is the assistant's default for the owner to correct). A **character**
(`data/characters.json`) can say lines when it first fights on a map and when it is struck down. Generic units (no
character) have none. All wording is placeholder.

```jsonc
{ "id": "gorm", "name": "Gorm", "type": "brute",
  "quotes": {
    "battle": [
      { "against": "eirik", "once": "always", "lines": [ { "say": "Eirik. At last." } ] },
      { "once": "campaign", "lines": [ { "say": "Come, little ones." }, { "say": "Do not run.", "speaker": "mira" } ] } ],
    "defeat": { "falls":    [ { "say": "Impossible..." }, { "say": "Brother!", "speaker": "mira" } ],
                "retreats": [ { "say": "I must pull back!" } ] } } }
```

- **Lines** are the ordinary `say` command (`say`, `speaker` = a character id, `speakerName`, `portrait`), read by the loader code
  that reads support conversations. A line with neither `speaker` nor `speakerName` is **said by the character the quote belongs
  to**; a line may be spoken by any other character (a friend answering). A character that is not on the map is shown by name
  with the portrait of its id. Only `say` is allowed.
- **Battle quote**: `against` (a character id, absent = anyone; not the character itself), `once` (`"map"` default, `"campaign"`,
  `"always"`), `lines` (at least one). A character has at most one quote per `against` value and one for anyone (a second would never
  be said; the loader refuses it).
- **When**: a fight is about to begin (every attack, a counter-less fight too; also the computer's and a berserk unit's), before the
  strikes and **before** the `fight` story events. Both fighters may speak, **the attacked unit first, then the attacker**
  (both are chosen before either is said). Each fighter says at most one quote per fight: its quote `against` the other fighter if
  it has one that is still allowed, else its general one. So a quote against a character wins over the general quote, and when the
  pair quote is used up the general one follows.
- **How often**: `"map"` = once per battle (state in the battle, saved); `"campaign"` = once per playthrough (carried between
  chapters; a lost chapter that is retried does not use it up; in a game without a campaign it is the same as `"map"`);
  `"always"` = every time. There is no "per opponent": a quote against one character is one pair already.
- **Defeat quote**: said right after a unit is **struck down in a fight** (its killing strike, a counter included, on any turn), before
  the `defeated` story events (and so before the victory when it was the boss). Which set: `falls` normally; `retreats` only for a
  unit of a human (non-computer) side in a game whose `data/campaign.json` says `"fallen": "return"` (the unit comes back next
  chapter). So in a "gone" game, or a game with no campaign, only `falls` is used (the editor offers one set), and with "return" both
  are offered (`falls` is then for enemies and other sides). A set that is absent is silent.
- **Not defeats**: a poison tick, fuel running out, a unit lost with its carrier, a side wiped out by the defeat rules, and a story
  `removeUnit` never say a defeat quote. Only a fight does.
- **Stream**: the lines are `Dialogue` entries in the action's own event list, in this order: battle quotes, `fight` events' lines,
  the strikes, the killing strike, `UnitDied`, the defeat quote, the `defeated` events' lines, then the rest (experience, victory).
  Nothing waits; no new event type. The player shows them in the normal dialogue box before the combat animation / after the fall
  and skips them like other dialogue (Skip tab, hold Cancel, `--autoplay` ends them by itself). The simulator emits them and ignores
  them; `ote-sim` results are unchanged. A `fight` event that takes a fighter away still leaves the quotes said (they come first).
- **Saves** (docs/SAVES.md): `"quotes": { "said": [n, ...] }` in a battle and in a campaign playthrough, written only when something
  was said. A game without quotes saves and hashes as before.
- **Loader refusals** (`data/characters.json: character "eirik": battle quote 1: ...`): `"against" names unknown character "x"`; `"against"
  names the character itself`; `a second quote against "gorm"` / `against anyone`; `"lines" must be a list with at least one say
  command`; `"once" must be "map", "campaign" or "always"`; `"speaker" names unknown character`; `give "speaker" or "speakerName", not
  both`; `only "say" is allowed in a quote`; an unknown key in `quotes`, a quote or `defeat`.
- Tests: `tests/test_quotes_rules.cpp`, `tests/test_quotes_data.cpp` (fixture `tests/fixtures/srpg-quotes`),
  `tests/srpg_player_quotes_test.py`. The player logs `[info] dialogue: <speaker> | <text>` for each line that reaches the box.

## Scene presentation

Built. Code: `engine/core/data/scene_fx.h`, `loader_scene_fx.*`, `loader_scene_backgrounds.cpp`, `engine/core/battle/story_scene_fx.cpp`;
player `engine/player/ui/scene_stage*.{h,cpp}`, `battle/battle_view_story.cpp`, `campaign/story_scene.cpp`. Tests:
`tests/test_scene_fx.cpp`, `tests/test_scene_stage.cpp`, `tests/test_stage_slots.cpp` (the seven places, enter / move / leave, the layout maths, the focus speed), `tests/stage_player_test.py` (fixture `tests/fixtures/stage-pres`, art by `tools/art/make_stage_test_art.py`), `tests/scene_player_test.py` (fixture `tests/fixtures/scene-pres`, its art drawn by
`tools/art/make_scene_test_art.py`), `tests/editor_api_scene_checks.py`, `editor/web/tests/unit/scene.test.ts`,
`editor/web/tests/e2e/scene.spec.ts`. The classic scene toolkit: a picture behind the dialogue, fades, pauses, a camera that scrolls
and shakes, characters standing left and right, weather, units that walk in. **All of it is presentation only**: the rules never read
it, a battle ends the same with and without it, `ote-sim` ignores it (the simulator's results are the same to the digit), and every
wait and fade is skipped by the player's skip control. The commands work in map events **and** in campaign scenes, except where the
table says "map only".

| Command | What | Where |
|---|---|---|
| `{ "background": "forest" }` | shows the full-screen picture `forest` (data/graphics.json `backgrounds`, docs/GRAPHICS.md "Backgrounds") behind the dialogue; in a map event it hides the map and the panels until taken away | both |
| `{ "background": "hall", "transition": "fade", "ms": 600 }` | the same with a cross-fade (`transition`: `"cut"` (default) or `"fade"`; `ms` 0 to 10000, default 500, used by a fade only) | both |
| `{ "background": null }` | takes the background away: back to the map (map event) or to the scene's plain colour, which is also what a campaign scene shows when it never had a background (today's look); takes `transition` / `ms` too | both |
| `{ "fade": "out", "ms": 500, "color": "#000000" }` | fades the whole screen to the colour (`#rrggbb` or `#rgb`, default black; `ms` 0 to 10000, default 500). It **stays faded until faded in** and the commands in between run behind it: adding, removing and moving units, changing the background or the music (the classic "fade out, rearrange, fade in") | both |
| `{ "fade": "in", "ms": 500 }` | brings it back, from the colour it went out to (a `color` here is ignored) | both |
| `{ "wait": 500 }` | a pause in milliseconds (0 to 60000) | both |
| `{ "shake": { "ms": 400, "strength": 3 } }` | shakes the picture; `ms` 0 to 10000 (default 400), `strength` 1 to 32 logical pixels (default 3) | both |
| `{ "focus": { "x": 3, "y": 4 }, "ms": 600, "wait": false }` | `focus` as before plus `ms` (the scroll takes that long, 0 to 10000; 0 jumps; left out = the usual quick ease with a half-second wait) and `wait` (default true: the next command waits for the scroll; false: it goes on while the camera still scrolls) | map only |
| `{ "focus": "eirik", "speed": 4 }` | `speed` in squares per second (0.1 to 100) **instead of** `ms` (both is a loader error): the scroll takes as long as its distance needs (see "The camera, per kind of view") | map only |
| `{ "setPhaseMusic": { "side": "red", "music": "calm" } }` | changes the music a side's turn plays for the rest of the battle (`"music": ""` = no phase music for that side, `null` = take the change away); docs/SOUND.md "Music by situation". Saved with the battle | map only |
| `{ "say": "…", "speaker": "eirik", "position": "left" }` | `say` with a place on the stage: one of seven slots, `"farLeft"`, `"left"`, `"midLeft"`, `"center"`, `"midRight"`, `"right"`, `"farRight"` (see "Standing portraits"). Without `position` the line looks exactly as it always did | both |
| `{ "enter": "eirik", "position": "midLeft", "portrait": "eirik_old", "transition": "fade", "ms": 400 }` | puts a portrait **on stage without a line**; `portrait` (optional) names another picture than the speaker's own, `transition` is `"cut"` (default) or `"fade"` (fades in over `ms`, default 500; the commands behind it wait). Nobody is lit until a line of theirs | both |
| `{ "move": "eirik", "position": "right", "transition": "cut", "ms": 600 }` | slides a speaker who stands on the stage to another slot (a slide by default, 400 ms or `ms`; `"transition": "cut"` jumps). Whoever stands in the new slot leaves. A speaker who is not on the stage: nothing | both |
| `{ "say": "…", "auto": 800 }` | the line goes on by itself 1 to 60000 ms after its last page is complete (Confirm and Skip still work; a question never does) | both |
| `{ "narrate": "Night falls." }` | a line with no name and no portrait, the text centred in the box (over the background, if there is one); takes `auto` too | both |
| `{ "leave": "eirik", "transition": "fade", "ms": 300 }` | takes that speaker's portrait off the stage (the unit's story id, or the free name the line used); optional `transition` (`"cut"` default, or `"fade"` out over `ms`) | both |
| `{ "clearStage": true, "transition": "fade" }` | everybody leaves the stage (same optional `transition` / `ms`) | both |
| `{ "weather": "rain" }` | `"rain"`, `"snow"` or `"none"`: a particle overlay falling until changed. Saved with the battle | map only |
| `{ "moveUnit": "eirik", "x": 3, "y": 4, "walk": true, "wait": false }` | `moveUnit` as before plus `walk`: the unit **walks** its way at the speed of an ordinary move (with its walking pictures when it has them, docs/GRAPHICS.md "Walking") instead of sliding. `wait` (only with `walk`, default true) says whether the next command waits for the walk | map only |

Everything out of range, an unknown key, a `transition` that is not `cut` / `fade`, a `position` that is not one of the seven places (`"position" must be "left", "right" or "center" (or "farLeft", "midLeft", "midRight", "farRight")`), an `enter` / `move` without a `position`, a `focus` with both `ms` and `speed`, a colour that is not `#rgb` / `#rrggbb`, an unknown background id (`"background" names unknown background "swamp"
(data/graphics.json "backgrounds")`, or `this game has no backgrounds`), `weather` in a campaign scene (`"weather" is for map events
only: a campaign scene has no map to put weather over`) and `focus` / `moveUnit` in a scene (they are not scene commands) is a loader
error naming the file, the event or chapter and the command. `fade` and `wait` are also modifiers of other commands (`playMusic` /
`stopMusic` `fade` seconds, `focus` / `moveUnit` `wait`): next to those commands they keep that meaning, on their own they are the
new commands.

**Example scene** (a map event at the start of a battle; a campaign scene takes the same list without `weather`, `focus` and `walk`):

```json
{ "id": "ambush", "when": { "battle": "start" }, "do": [
  { "fade": "out", "ms": 400, "color": "#000000" },
  { "background": "forest_dusk" },
  { "fade": "in", "ms": 400 },
  { "say": "The forest is quiet.", "speaker": "eirik", "position": "left" },
  { "say": "Too quiet.", "speaker": "lyra", "position": "right" },
  { "wait": 400 },
  { "shake": { "ms": 300, "strength": 3 } },
  { "narrate": "Something stirs in the dark." },
  { "say": "Who goes there?", "speaker": "gorm", "position": "center", "auto": 1200 },
  { "leave": "gorm" },
  { "fade": "out", "ms": 300 },
  { "background": null },
  { "clearStage": true },
  { "fade": "in", "ms": 300 } ] }
```

### Standing portraits

The dialogue box shows **one** portrait, beside the box. A line with `position` puts its speaker on the **stage** instead: the portrait
stands at one of **seven places** across the screen on the top edge of the box, drawn as large as a whole-number scale allows within 55
percent of the screen's height and 45 percent of its width (3 times on the 480 x 270 screen with 48 x 48 faces; smaller only when the
picture cannot fit, or when many portraits share the stage, below). The speaker is drawn bright, everybody else on the stage **dimmed**. A portrait stays until `leave` /
`clearStage`, or the scene ends (a map event's run of events; a campaign scene). One portrait per place: a newcomer takes the place of
whoever stood there, and a speaker given a new `position` moves. A line **without** `position` from someone who stands on the stage
lights that portrait where it stands; from someone who does not, it shows the classic portrait beside the box and the stage stays as it
is. Whose picture it is follows the line's `portrait` id, else the speaker's unit id, as always. Far / near variants are not built.

**The seven places**, left to right: `farLeft`, `left`, `midLeft`, `center`, `midRight`, `right`, `farRight`. `{ "enter": ... }` puts a portrait on
a place with no line, `{ "move": ... }` slides it to another one, `leave` / `clearStage` take it off (with an optional fade). A portrait
that is entered, moved or left is only presentation; nothing about it is saved.

- **Classic layout** (every game before the new places): only `left`, `center` and `right` are on the stage. `left` and `right` stand against the
  screen's edges and `center` in the middle, exactly as they always did, at the largest whole-number scale that fits.
- **Wide layout**: as soon as one of the four new places (`farLeft`, `midLeft`, `midRight`, `farRight`) is occupied, the seven places are spread
  evenly from the left edge (`farLeft`) to the right edge (`farRight`), and the portraits shrink (whole-number scales first, fractions when
  they must) until portraits on neighbouring places on the stage overlap by **at most 40 percent** of the wider one's width. On 480 x 270 that
  means 3 times for three portraits spread out, 2 times when all seven stand. The speaker is drawn last, in front where portraits overlap. When
  the last portrait of the four new places leaves, the classic layout is back.
- **A phone-shaped screen** (taller than wide, such as 270 x 480) in the wide layout shows **at most three** portraits: the speaker and its
  neighbours on the stage (the next one on each side in left-to-right order; with nobody speaking, the three in the middle). They are spread to the
  left edge, the middle and the right edge (two: both edges; one: where its place is); the others stay on the stage, hidden, and come back when
  the speaker changes. The box always stays clear, because portraits stand on its top edge.
- **Facing.** A portrait whose graphics entry says which way it looks (`"faces": "left"` or `"right"`, docs/GRAPHICS.md "Portraits") is turned to
  face the middle of the screen: those on the left half look right, those on the right half look left, `center` is drawn as it is. A portrait that
  does not say is **never turned** (that is today's look; most classic sets look left, so write `"faces": "left"` to get the classic stage).
- **The Skip tab.** The box's Skip tab used to sit on the top edge of the box, over the corner of a portrait that stands at the far right.
  While portraits stand on the stage it now sits **inside** the box, in its top-right corner (the name row; a nameless line keeps that row free).
  A box with nobody on the stage looks exactly as it always did.
- **Talk and blink frames.** A portrait may list `"talk"` frames (cells cycled every 140 ms while its speaker's line is being typed, on the stage and
  beside the box) and `"blink"` frames (shown for 130 ms every 2.8 to 5.4 seconds, the exact times worked out from the clock and the speaker's name,
  never random). The player has no reduced-motion setting, so there is nothing to respect; a portrait without the keys never changes.

### Time, skipping, saving

- **Order.** Commands play in order. `fade`, `wait`, `shake`, a background with `"transition": "fade"`, a `focus` that waits and a
  `walk` that waits hold the commands behind them until they are over. The rules happen at once in the core (the unit added, moved or
  removed behind a fade really is), only what the player sees waits. `wait: false` on a `focus` / `walk` lets the next commands run while
  it goes on; the run does not end before it has finished.
- **A fade never outlives its scene.** In a map event the core ends a run of events that is still faded out with an automatic
  `fade in` (500 ms, logged "(automatic)"), so no save is ever made in the dark; a campaign scene that ends faded out fades in (500 ms)
  before the next screen. A maker who wants the dark to last starts the next scene with the picture they want.
- **Skipping.** The existing skip control (the Skip tab or button, the `skip` script token, or holding Cancel / Menu for half a second,
  also while a fade, wait, shake or walk plays with no box up) fast-forwards the whole scene: fades, waits, shakes, transitions and
  walks take no time, the remaining lines pass, and the end state is exactly what playing reaches (background, fade, who stands on the
  stage, weather, unit squares, music: a skipped campaign scene still makes its `playMusic` / `stopMusic` changes; one-shot sounds stay
  silent). A tap or Confirm during a wait ends just that wait. `--autoplay` never waits on a scene.
- **Weather and movement cost.** Weather is also a rules fact since the terrain's `weatherCost` (docs/ECONOMY.md "Weather and movement cost"): the
  `weather` command sets the weather the cost uses, from then on, and a map may start with one (`"weather": "rain"`).
- **Saved.** Only the **picture** and the **weather** of a map event are state: the battle's `story` object gets `"scene":
  { "background": "hall", "weather": "rain" }` (each key only while set; docs/SAVES.md) and a resumed battle shows them at once, with
  nothing replayed. The fade needs no key (never on at a save), and the stage, a shake and a scroll under way are not saved (a save is
  made when a run of events is over; the picture and weather stay, the standing portraits go). A **campaign scene is not saved while it
  plays**: closing the window in a scene leaves the Continue save as it was and the scene plays again from its title card; a scene that
  has ended leaves its changes (switches, variables, music) as before, but its background, stage and fade end with it.
- **Hash.** The data hash sees that a command is there (and its milliseconds) but not which picture, which weather, who leaves or what a
  narrator says (wording, like a `say`).

### The camera, per kind of view

`focus` takes **`ms`** (how long) **or `speed`** (squares per second, 0.1 to 100; both together is a loader error: `give "ms" ... or "speed" ..., not both`). With
`speed` the scroll takes `distance / speed`, the distance being measured **from the previous focus point** (the middle of the map at the start of a run
of events) to the new one in squares (hexagon and isometric maps measure on the screen's own plane); at most 10 seconds. Everything else (`wait`, a skip jumping)
is as with `ms`. Example: a focus from the middle of an 8 x 4 map to the square 6,1 at 4 squares per second takes 637 ms. The log line says it:
`scene: focus 6,1 637 ms (speed 4)`.

| View | `focus` `ms` | `shake` |
|---|---|---|
| 2D, 2D isometric, 3D isometric (scrolling camera) | a steady scroll of that length, eased at both ends; 0 jumps; `wait` says whether the commands behind it wait | the map moves for its length (the panels do not) |
| Full 3D | 0 puts the camera there at once; more uses the 3D camera's own easing (the length is not exact; the wait still lasts `ms`) | the camera turns a hair left / right and up / down |
| any, while a background shows | the camera still scrolls, under the picture | the **picture** and the standing portraits shake instead |

Hexagon maps follow their kind of view. A `focus` on a square the fog hides, or on a unit that is not on the map, does nothing (as before).
In a campaign scene there is no map: a `shake` moves the background and the standing portraits (a scene with neither shows nothing
move, it only takes its time).

### What the player logs

One line per new command, which the tests read: `scene: fade out 300 ms #000000`, `scene: background forest cut` / `... fade 600 ms`,
`scene: wait 400 ms`, `scene: shake 300 ms strength 3`, `scene: portrait eirik left`, `scene: enter Mira farRight fade`, `scene: move Mira midLeft` (` cut` when it jumped),
`scene: leave gorm` (` fade` when it fades), `scene: narrate`,
`scene: weather snow`, `scene: focus 6,1 400 ms (no wait)`, `scene: walk lyra 3 steps`, with ` (skipped)` after those a skip made
instant. A game that uses none of the keys logs exactly what it always did.

## Strategy-RPG additions

A unit placed as a character is known to events and objectives by the character's id, so conditions and commands that name a unit work
for characters without change. New commands and conditions (promote, teach or forget a skill, "has skill", "is promoted"), a `support`
trigger and support points as a campaign effect, summons and promotion in the event stream the player animates, and campaign roster
behaviour for characters are specified in docs/SRPG.md (the "Story" part of each section, "Events (what the player animates)").

## Editor

- Map editor: unit **id** field; an **Events** panel (list; per event the
  trigger, conditions and a command list with add / reorder / delete, each
  command a small form); **Objectives**.
- Game screen: switches and variables (names).
- Art: portraits.
- **Campaign** screen: chapter list in order, each with its map and the
  scenes before and after.
- Dialogue choices: the command **Ask a question** (docs/EDITOR_UI.md).
- The condition **The unit that set this off** and the command **Send a unit's items to storage** (docs/EDITOR_UI.md "Small extras").
- Problems: unknown ids, events that can never fire, a chapter without a
  map.

## Not in this version

Events outside battles other than chapter scenes (no
walking around towns); common events shared between maps; a preparations
screen; world maps; cut-scene unit animation beyond moving; Lua event
commands; voice.

## Build order

1. Core: named units, switches / variables, events, objectives, saves,
   tests; `ote-sim` runs them. **Done** (engine/core/data/events.h and
   loader_events.cpp / loader_story_*.cpp; engine/core/battle/story*.cpp,
   objectives.cpp, save_story.cpp; tests/test_story_*.cpp).
2. Player: dialogue box, Talk, objective display. **Done** (see "Dialogue
   (player)"; tests `test_dialogue_text.cpp`, `test_dialogue_box.cpp`,
   `test_portraits.cpp`).
3. Editor: events, objectives, switches / variables, portraits. **Done**
   (docs/EDITOR_API.md "Story data"; `editor/web/src/screens/maps/editor/story/`).
4. Campaign: core roster and progress, saves (**done**: engine/core/campaign/, tests/test_campaign_*.cpp,
   `ote-sim --campaign`), and the player (game page, scenes, chapter flow, campaign saves; see "Campaign (player)").
5. Editor: Campaign screen (not built yet); demo content **done**
   (`demo-srpg`: opening scene, reinforcements, a recruit by Talk, a fort
   reward, boss lines, and the campaign "The Ford War" with a second
   chapter, `maps/north_road.json`, `data/campaign.json`).
