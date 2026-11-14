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
- **Dialogue choices come later**, not in the first version.
- **Fallen units: the maker chooses per game** (gone for good, or back next
  chapter).
- **Single maps next to a campaign: the maker chooses** (campaign only, or
  campaign plus free battles).

Everything below is engine design and may change; all wording shown to
players and makers is placeholder until the owner approves it.

Status: steps 1 and 2 of the "Build order" (the rules core: data, loader,
battle, saves, `ote-sim`; and the player's part inside one battle: dialogue
box, Talk, objectives) are built and tested; the sections on the campaign and
the editor are still design only. Where building step 1
refined or corrected the first contract, the text below is the corrected
version and says so ("refined").

## Principles

- Events are **data**, edited as lists of commands like RPG Maker's event
  commands. No scripting is needed; a Lua hook can come later.
- Events run in the rules core, so they are deterministic, saved, and
  played by `ote-sim`. A command changes the battle at once and leaves
  entries in the event stream; the player shows them in order (a line of
  dialogue is one such entry). Nothing waits for input inside the core, so
  dialogue choices are not in this version (owner decision).
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
| `{ "talk": "eirik", "to": "lyra", "label": "Talk" }` | adds **Talk** to the first unit's action menu while the two units (named by id) are alive and next to each other; fires when chosen (does not end the turn unless the event says so). `label` defaults to "Talk". Only the first unit can start it; the computer's side never does |

### Conditions (`if`, and inside the `if` command)

`{ "switch": id, "is": true }` (`is` default true),
`{ "variable": id, "op": "=", "value": 3 }` (`= != < <= > >=`, `==` also
accepted), `{ "alive": <filter> }` / `{ "dead": <filter> }`,
`{ "round": { "op": ">=", "value": 5 } }`,
`{ "carries": "vulnerary", "unit": <filter> }` (`unit` default any).
Refined: `alive` = some living unit matches; `dead` = a unit that matches
exists (or existed, also when it was removed) and none is alive, so a unit
that an event has not added yet is neither. `carries` = a living matching
unit has the item in its inventory (not the storage).

Conditions are judged when the event is **about to run**, so an event sees
what earlier ones did. An event whose condition fails is not used up: a
non-repeating event can still run at a later occurrence of its trigger.

### Commands (`do`)

| Command | What |
|---|---|
| `{ "say": "text", "speaker": "eirik" }` | a line of dialogue (entry `Dialogue`); `speaker` is a unit id or a free name (`"speakerName": "Villager"`, not both); optional `"portrait"` id (just text for the player to look up) |
| `{ "addUnit": { …a map unit… } }` | places a unit (keys as in the map's `units`, including `id`); if the square is taken or the unit cannot stand there, the nearest free square it can stand on (grid distance, the lowest cell number on a tie). A repeated `addUnit` with an id that a unit already has creates the unit without the id (logged). No free square at all: nothing is added (logged) |
| `{ "removeUnit": <filter> }` | leaves the map (not a defeat: no drops, no defeat events; the defeat rules still apply, so removing a side's last unit ends it like any other loss) |
| `{ "moveUnit": "eirik", "x": 3, "y": 4 }` | refined: the unit is put there (no path is walked; the entry gives the squares it left and reached, the player may animate it); if the square is taken, the nearest free one as for `addUnit`. Does not spend its move. Fires `reach` events |
| `{ "changeSide": <filter>, "side": "blue" }` | matching living units join another side (recruiting); an unfinished capture is dropped |
| `{ "giveItem": "vulnerary", "to": <filter> }` | every matching living unit gets one, following the item rules (room: inventory, else the shared storage if the game has one, else lost) |
| `{ "giveResource": { "gold": 500 }, "side": "blue" }` | may be negative; a side's funds never go below zero |
| `{ "setSwitch": id, "to": true }`, `{ "setVariable": id, "to": 3 }`, `{ "addVariable": id, "by": 1 }` | |
| `{ "objective": { … } }` | replaces the current objectives (below) |
| `{ "win": "blue" }` / `{ "lose": "blue" }` | ends the battle for that team (refined: a team name or a side id). `lose` leaves the win to the only other team still in the battle, nobody if there are several. The first decision stands; the rest of the event still runs |
| `{ "if": [conditions], "then": [commands], "else": [commands] }` | `then` required, `else` optional |
| `{ "endTurn": true }` | only in an event whose trigger is `talk` (a loader error anywhere else): the talking unit's turn ends |
| `{ "focus": { "x": 3, "y": 4 } }` or `{ "focus": "eirik" }` | the view moves there before the next entry (a unit that is not on the map: nothing) |

**Running.** Events run in file order for one occurrence, and an event that
fires while another runs (a `reach` caused by `moveUnit`, a `won` event caused
by `win`) waits in a queue, so every event sees what the previous one left.
At most 100 events run in one go; the next one is cut with a logged error.

### What the player gets (event stream)

New `EventType`s, in the stream in order with everything else (texts are
indices for `Battle::storyText(i)`):

| Entry | a, b, c, d |
|---|---|
| `Dialogue` | a = speaker unit (-1 none), b = text, c = free speaker name (-1 none), d = portrait id (-1 none) |
| `Focus` | a = cell, b = the unit followed (-1 just the square) |
| `UnitAdded` | a = unit, b = cell |
| `UnitRemoved` | a = unit, b = the cell it stood on |
| `UnitRelocated` | a = unit, b = cell it left, c = cell it reached |
| `SideChanged` | a = unit, b = old side, c = new side |
| `ObjectiveChanged` | a = text of the new objective (-1 none), b = objective set |

Gifts use the existing item and resource entries. The speaker's name for a
unit is `units[a].name`. Queries: `talkOptions(unit, fromCell)` returns
(target, event, label) for the Talk menu; `Action::talk(unit, target[, event])`
performs it (free like the item actions, cannot be undone);
`objectiveText()` is the text to show (at battle start and in the menu; no
entry is made at the start); `humanTeam()`; `unitByStoryId(id)`.

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

`data/campaign.json` (optional; missing = single battles, as today):

```jsonc
{ "name": "The Ford War",
  "rules": { "fallen": "gone",          // "gone": a defeated unit never returns; "return": it is back next chapter
             "onDefeat": "retry" },     // "retry": the chapter can be started again; "end": the campaign is over
  "chapters": [
    { "id": "ch1", "name": "The Ford", "map": "bridge",
      "before": [ { "say": "…", "speaker": "eirik" } ],   // a scene: say / focus-less commands, switches, variables
      "after":  [ … ],
      "next": "ch2" }                    // optional; default = the next in the list. A chapter's `after`
                                         // may also hold { "goto": "ch3b" } inside an `if` (branching)
  ] }
```

**Carry-over.** The campaign keeps a **roster**: every named unit (`id`)
of a human side, with its type, name, level, experience, stats and items,
plus the side's shared storage, resources, switches and variables. When a
chapter's map places a named unit that is in the roster, the roster's
version is used (the map gives only the square); one that is "gone" is not
placed. After a won chapter the survivors are written back and new named
units join. Units without an id are extras and are never carried.
Whether a maker can let the player choose who to bring is a preparations
screen, not in this version.

**Saves.** A save slot becomes one playthrough: campaign progress
(chapter, roster, storage, resources, switches, variables) and, if a
battle is under way, that battle as today. Saves of games without a
campaign keep their format. The save hash rule stays: rules changes
refuse old saves, wording and art do not; dialogue text is wording.

**Launcher.** A game with a campaign offers New game / Continue / Load.
`"freeBattles"` in campaign.json (`"none"` default, `"all"`, or a list of map
ids) says which maps can also be started on their own.

## Editor

- Map editor: unit **id** field; an **Events** panel (list; per event the
  trigger, conditions and a command list with add / reorder / delete, each
  command a small form); **Objectives**.
- Game screen: switches and variables (names).
- Art: portraits.
- **Campaign** screen: chapter list in order, each with its map and the
  scenes before and after.
- Problems: unknown ids, events that can never fire, a chapter without a
  map.

## Not in this version

Dialogue choices; events outside battles other than chapter scenes (no
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
3. Editor: events, objectives, switches / variables, portraits.
4. Campaign: core roster and progress, saves, launcher.
5. Editor: Campaign screen; demo content (`demo-srpg`: an opening scene, a
   boss line, a Talk, reinforcements, a second chapter).
