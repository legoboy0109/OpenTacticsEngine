# Story events, dialogue and campaigns

Contract for PLAN item 3f. Owner decisions (2026-11-14):

- **Triggers**: battle start / end and turn number; a unit reaching a
  square or area; a unit defeated or fighting; a Talk command between two
  units.
- **Actions** besides dialogue: add / remove / move units; give items or
  resources; set win / lose conditions; switches and variables.
- The first version **includes a campaign**: chapters in order, surviving
  units with levels and items carried over, scenes between maps.

Everything below is engine design and may change; all wording shown to
players and makers is placeholder until the owner approves it. Points
marked **(owner)** are choices still to be put to the owner.

Status: design only; see "Build order".

## Principles

- Events are **data**, edited as lists of commands like RPG Maker's event
  commands. No scripting is needed; a Lua hook can come later.
- Events run in the rules core, so they are deterministic, saved, and
  played by `ote-sim`. A command changes the battle at once and leaves
  entries in the event stream; the player shows them in order (a line of
  dialogue is one such entry). Nothing waits for input inside the core, so
  **dialogue choices are not in this version** **(owner)**.
- A game without events, objectives or a campaign behaves exactly as today
  and keeps its save hash.

## Named units

A map unit may have `"id": "eirik"` (unique in the map). Events,
objectives and the campaign point at units by id. A unit filter, used
wherever "which unit" is asked, is one of: `{ "unit": "eirik" }`,
`{ "side": "blue" }`, `{ "team": "blue" }`, `{ "type": "knight" }`,
`{ "tag": "flier" }`, or `{}` = any unit.

## Switches and variables

`game.json`: `"switches": [ { "id": "village_saved", "name": "Village saved" } ]`,
`"variables": [ { "id": "gold_found", "name": "Gold found" } ]`. Switches
are off / on, variables whole numbers (start 0). They belong to the
playthrough: in a campaign they carry from chapter to chapter; in a single
battle they start fresh.

## Events (map file)

```jsonc
"events": [
  { "id": "ambush",                         // unique in the map
    "when": { "turn": 4, "side": "red" },   // the trigger, see below
    "if": [ { "switch": "village_saved", "is": false } ],   // optional, all must hold
    "repeat": false,                        // default: runs once
    "do": [ { "say": "Now! Strike from the forest!", "speaker": "gorm" },
            { "addUnit": { "type": "soldier", "side": "red", "x": 9, "y": 1 } } ] }
]
```

### Triggers (`when`)

| Trigger | Fires |
|---|---|
| `{ "battle": "start" }` | before the first turn |
| `{ "battle": "won" }` / `"lost"` / `"end"` | when the battle is decided (for the human team) |
| `{ "turn": 4, "side": "red" }` | at the start of that side's turn in round 4; `side` optional = first side; `"turn": "every"` = each round |
| `{ "reach": { "x": 5, "y": 2, "w": 2, "h": 1 }, "by": <filter> }` | when a matching unit ends a move inside the rectangle (`w`, `h` default 1) |
| `{ "defeated": <filter> }` | when a matching unit is defeated; `"by": <filter>` optional |
| `{ "fight": <filter>, "with": <filter> }` | just before a fight involving a matching unit begins (`with` optional) |
| `{ "talk": "eirik", "to": "lyra", "label": "Talk" }` | adds **Talk** to the first unit's action menu while the two stand next to each other; fires when chosen (does not end the turn unless the event says so) |

### Conditions (`if`, and inside the `if` command)

`{ "switch": id, "is": true }`, `{ "variable": id, "op": "=", "value": 3 }`
(`= != < <= > >=`), `{ "alive": <filter> }` / `{ "dead": "eirik" }`,
`{ "round": { "op": ">=", "value": 5 } }`, `{ "carries": "vulnerary", "unit": <filter> }`.

### Commands (`do`)

| Command | What |
|---|---|
| `{ "say": "text", "speaker": "eirik" }` | a line of dialogue; `speaker` is a unit id or a free name (`"speakerName": "Villager"`); optional `"portrait"` id |
| `{ "addUnit": { …a map unit… } }` | places a unit (as in the map's `units`); if the square is taken, the nearest free square it can stand on |
| `{ "removeUnit": <filter> }` | leaves the map (not a defeat: no drops, no defeat events) |
| `{ "moveUnit": "eirik", "x": 3, "y": 4 }` | walks or is put there |
| `{ "changeSide": <filter>, "side": "blue" }` | joins another side (recruiting) |
| `{ "giveItem": "vulnerary", "to": <filter> }` | follows the item rules (no room → storage → lost) |
| `{ "giveResource": { "gold": 500 }, "side": "blue" }` | may be negative |
| `{ "setSwitch": id, "to": true }`, `{ "setVariable": id, "to": 3 }`, `{ "addVariable": id, "by": 1 }` | |
| `{ "objective": { … } }` | replaces the map's objectives (below) |
| `{ "win": "blue" }` / `{ "lose": "blue" }` | ends the battle for that team |
| `{ "if": [conditions], "then": [commands], "else": [commands] }` | |
| `{ "endTurn": true }` | (Talk only) the talking unit's turn ends |
| `{ "focus": { "x": 3, "y": 4 } }` or `{ "focus": "eirik" }` | the view moves there before the next line |

An event that fires while another runs waits its turn; a chain of more
than 100 events in one go is cut with a logged error.

## Objectives (map file)

```jsonc
"objectives": {
  "text": "Defeat Gorm",                         // shown to the player
  "win":  [ { "defeated": { "unit": "gorm" } } ],        // any one wins
  "lose": [ { "defeated": { "unit": "eirik" } }, { "round": 15 } ]   // any one loses
}
```
Kinds: `defeated` (filter; a side or team = all of its units), `reach`
(rectangle + `by`), `round` (reaching that round), `survive` (rounds; the
same as `round` under `win`). Objectives are judged for the human team;
missing `objectives` = the game's `rules.defeat`, as today, which also
keeps applying to the other sides.

## Dialogue (player)

A box across the bottom of the screen: the speaker's name, the text
appearing letter by letter (confirm = finish the line, then next; a hold
or a "skip" button runs through the rest), and the speaker's portrait if
one exists **(owner: look and position)**. Portraits are pictures in
`data/graphics.json` under `"portraits": { "eirik": { "sheet": …, "cell": … } }`,
per style like other art. In 3D styles the box is the same 2D box. The
objective text is shown at battle start and in the battle menu.

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
Whether single maps stay startable on their own is the maker's setting
**(owner)**.

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
   tests; `ote-sim` runs them.
2. Player: dialogue box, Talk, objective display.
3. Editor: events, objectives, switches / variables, portraits.
4. Campaign: core roster and progress, saves, launcher.
5. Editor: Campaign screen; demo content (`demo-srpg`: an opening scene, a
   boss line, a Talk, reinforcements, a second chapter).
