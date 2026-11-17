# The computer opponent

How the computer plays its side, and what a maker can say about **one unit**. The rules core calls `aiTakeStep` (engine/core/ai/) once per
decision; it is stateless on purpose (the same battle always gives the same decision), except for the one flag a waiting unit keeps (`awake`, below).

## The default: how every computer unit plays

Every computer unit that has no `behaviour` block plays like this, and every shipped game, test and `ote-sim` result depends on it staying exactly so:

- Each step it scores the best order of every unit that has not acted (move somewhere, then attack, capture, use a staff or item, supply, promote,
  use an ability, open / visit a place) and does the single best one; ties go to the lowest unit id. After all have acted it researches, builds and upgrades.
- **Attacks.** Every square the unit can stop on x every weapon x every enemy it knows (fog: only what its team sees) is scored from the forecast
  (`ai_score.cpp`): expected damage as a share of the target's worth, plus a bonus for a likely kill (a leader that ends the game is worth far more), minus the
  counterattack it would take, plus a little for defensive terrain, minus a cost for being outnumbered there (`ai_order.cpp`). Any attack that deals damage
  outranks plain advancing.
- **Advancing.** With nothing to hit, it walks toward the nearest enemy it knows (fog: the structures it last saw in enemy hands, else the unseen square
  farthest from its own), stopping where it is not alone outnumbered. A capturer walks to the structure it was assigned. A leader whose death loses the game
  stays home while it has company.
- **Staves, items, abilities, supply** (docs/SRPG.md section 8 "AI", docs/ITEMS.md, docs/ECONOMY.md): a hurt unit may drink a healing item; a healer heals; a unit may
  summon or heal with an ability; a badly hurt or empty unit may fall back to a repairing structure.
- **`goesFor`** (docs/SRPG.md section 10 "Computer opponent"): a unit marked `"goesFor": ["chests", "villages", "doors", "steal"]` also opens chests, visits or
  destroys places, opens a door that is in its way or steals. Unmarked units never do.

## Behaviour: what a maker can change per unit

A map unit (`maps/*.json` `units`), an `addUnit` command and the story command `setBehaviour` take the same block, for a unit of a **computer side** only (a unit on
a side a person plays has none: the loader says so).

```json
{ "type": "soldier", "side": "red", "x": 8, "y": 2, "id": "warden", "behaviour": "hold" }
{ "type": "soldier", "side": "red", "x": 9, "y": 4, "behaviour": { "mode": "guard", "group": "ambush", "fromTurn": 2 } }
```

`"behaviour"` is a mode name (`"advance"`, `"hold"`, `"guard"`) or an object. The block with nothing in it, `"advance"` and no key at all are the same: the
default above. Unknown keys, bad values and keys that do not belong to the mode are refused in plain words.

| key | meaning |
|---|---|
| `mode` | `"advance"` (default), `"hold"`, `"guard"`, `"guardArea"`, `"seek"` |
| `wake` | `guard` only: `"once"` (default) or `"react"` |
| `group` | `guard` / `guardArea`: a name (letters, digits, `_`, `-`); the members of a group (same side, same name) wake together |
| `area` | `guardArea`: a rectangle `{ "x", "y", "w", "h" }` (`w`, `h` default 1; squares of the map as the `reach` trigger counts them) |
| `within` | `guardArea`: a whole number 1 to 99, squares from the unit |
| `unit` / `place` / `x` + `y` | `seek`: exactly one target: a unit's story id, a placed structure's `id`, or a square |
| `fromTurn` | any mode: whole number 1 to 99; the unit does nothing before that round (1 = from the start, the same as leaving it out) |
| `prefer` | any mode: `"weakest"`, `"nearest"`, `{ "unit": id }`, `{ "type": id }`, `{ "tag": text }` |

### The modes

**`advance`** (default). Today's behaviour, above.

**`hold`.** The unit never changes square. From where it stands it still attacks what it can reach (its weapons, `afterMove` rules do not apply because it
does not move), uses its staves, items and abilities, opens a chest or door next to it if it `goesFor` them, and counterattacks like any unit (counters are
rules, not decisions, so no behaviour touches them). It never retreats to repair. Example: a boss on his throne: `{ "mode": "hold" }`. A unit that holds
with no weapon, staff or ability can never do anything (the editor says so).

**`guard`.** Waits ("asleep", `awake` false) until a hostile unit is within its **threat range**: the unit could attack something it knows of from a
square it can reach this turn (its move and its weapon's range, with the same reach the AI plays with: blocked squares, terrain costs, `afterMove`,
fog). The check runs at the start of every AI step of its side. From then on it plays as `advance` for good (`wake: "once"`, default). A unit that is
hit wakes at once (a blow it survives), and so does its whole group. A unit with no weapon never wakes by range (only by group, a hit or the story).
- `"wake": "react"`: it never wakes for good. Each turn it attacks whatever it can reach from some square it can get to, and otherwise stays where it is:
  it never walks toward anyone. (A reactive guard that is hit does not "wake"; there is nothing to wake.) Example: a sentry that only meets anyone who walks up to it.

**`guardArea`.** Waits until a hostile unit it knows of stands **inside `area`**, and / or is **within `within` squares** of the unit (at least one
is needed; either wakes it). Reach is not considered: an enemy that could attack it but is outside both does not wake it. Wakes for good like `guard`;
group, a hit and the story wake it as well. Example: an ambush: `{ "mode": "guardArea", "area": { "x": 4, "y": 1, "w": 3, "h": 3 }, "group": "ambush" }`.

**`seek`.** The unit makes for a goal instead of the nearest enemy:
- `"unit": "lord"`: the square of that unit (it follows it). If the unit is gone (defeated, removed), or an enemy the unit's side cannot see (fog), the
  unit **plays as `advance`** until it is there again. Attacking that unit is preferred: an attack on it scores four times the usual plus a flat bonus, so it
  beats any other hit.
- `"place": "throne"`: the structure with that story id (any of its squares; the unit is as happy on one as on another).
- `"x": 3, "y": 4`: that square.
Other targets are ignored **unless they stand in the way or are a free hit**: the unit may attack something that is not its target only from a square that
is among the closest to the goal it could reach this turn (so the hit costs it no ground). When nothing it can reach gets it closer (a unit blocks the road),
any attack from where it stands counts as free. On the goal it stops and stays (it still attacks from there, it still counterattacks, and standing on a
capturable structure it captures it by the usual rules). A seeker has no `danger` caution: it does not wait for company. Example: a courier running for
an exit square: `{ "mode": "seek", "x": 13, "y": 4 }`.

**Escaping the map.** There is no separate "escape": the existing story commands do it. Make the exit square the goal and remove the unit when it reaches it,
counting it if you like:
```json
{ "id": "away", "when": { "reach": { "x": 13, "y": 4 }, "by": { "unit": "runner" } },
  "do": [ { "removeUnit": { "unit": "runner" } }, { "addVariable": "escaped", "by": 1 } ] }
```
(`removeUnit` is not a defeat: no drops, no defeat events.) Tested in `tests/test_ai_behaviour_data.cpp`.

### Start turn

`"fromTurn": 3` makes the unit do nothing (it just waits) in rounds 1 and 2, whatever its mode; the guard checks also start at that round. It still answers
blows (counters are rules). `fromTurn` is per unit: to start a whole group late give each member the same number (or use a `turn` event with `wakeGroup`).

### Favourite target (`prefer`)

A small nudge, not an order. When the unit chooses between attacks, the score of an attack on a favourite is multiplied by up to 1.25 (`"weakest"`: by
how hurt the target is, 0 to 25 %; `"nearest"`: 25 % / distance; a named unit, type or tag: 25 %). It decides close calls only; a clearly better attack elsewhere
still wins, and an attack the AI would not make anyway is not made. Works with every mode (a seeker's own target outranks it).

### How it combines with the rest

- **`goesFor`**: independent. A `hold` unit with `goesFor` opens or visits only what it can reach from its square. A `seek` unit that also `goesFor` chests
  still opens one it passes (the marker is the maker's explicit wish and outranks the walk).
- **Conditions** (docs/SRPG.md section 9): a unit that cannot act or move is stopped by the rules as ever. A **berserk** unit takes its own action at the start of its
  turn and ignores its behaviour completely (a `hold` unit that goes berserk walks to the nearest unit and strikes it). Sleeping guards and sleeping
  conditions are different things: a unit can be both.
- **Fog**: every check (waking, `seek` of a hostile unit, `prefer`) reads only what the unit's team sees.
- **Carrying / rescue** (docs/SRPG.md section 5): the AI never rescues, and a carried unit is off the board: nothing changes.
- **Summons and reinforcements**: a unit made by a summon or by production (economy games) has the default behaviour. A unit added by `addUnit` takes the same
  `behaviour` key as a map unit.
- **Groups across sides**: a group is per side.

### Story commands

```json
{ "setBehaviour": <behaviour block>, "units": <unit filter> }
{ "wakeGroup": "ambush" }
```
- `setBehaviour` gives the block to every **living computer unit** the filter matches (a unit on a side a person plays is skipped; a filter naming such a side is
  refused). The block replaces the old one **completely** (a bare `"advance"` puts a unit back to the default) and a guard starts asleep again.
- `wakeGroup` wakes every waiting unit whose group has that name, on any side. The loader refuses a name no unit of the map gives (map units, `addUnit` units and
  `setBehaviour` blocks count).
- Map events only: a campaign scene refuses both (they are not scene commands).
- The triggers already exist: a unit of any side that **ends a move inside a rectangle** fires `reach` (`by` filters who), so "when the hero steps on the
  bridge, wake the ambush" is `{ "when": { "reach": { "x": 6, "y": 2, "w": 2, "h": 3 }, "by": { "side": "blue" } }, "do": [ { "wakeGroup": "ambush" } ] }`.

### Saves

A battle save writes, per unit, `"behaviour"` (the block as the map writes it, only when it is not the default) and `"awake": true` (only for a guard that
has woken). Nothing else is state (a seeker's goal is in its block; the AI keeps no memory). A map that uses none of this saves byte for byte as before; old
saves load. The data hash covers the map files whole, so a behaviour counts only in a game that has one.

### Tests

`tests/test_ai_behaviour.cpp` (rules, square and hex), `tests/test_ai_behaviour_data.cpp` (loader, commands, saves; fixture `tests/ai_behaviour_fixture.h`),
`tests/ai_behaviour_player_test.py` (the headless player over a few turns), editor: `tests/unit/behaviour.test.ts`, `tests/e2e/ai-behaviour.spec.ts`,
`tests/editor_api_behaviour_checks.py`. Code: `ai/ai_behaviour.cpp` (stance, waking, seek, favourites), `battle/ai_wake.cpp`, `battle/save_behaviour.cpp`,
`data/behaviour.cpp` and `data/loader_behaviour.cpp`.

### Not built

- Retreating to heal (`flee`) as a behaviour (the supply logic already retreats a badly hurt unit to a repairing structure).
- Remembering a chosen target across turns: a seeker's goal is fixed by its block, and the AI otherwise recomputes everything each step.
