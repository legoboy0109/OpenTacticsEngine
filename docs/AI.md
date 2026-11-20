# The computer opponent

How the computer plays its side, and what a maker can say about **one unit**. The rules core calls `aiTakeStep` (engine/core/ai/) once per
decision; it is stateless on purpose (the same battle always gives the same decision), except for the one flag a waiting unit keeps (`awake`, below).

## The default: how every computer unit plays

Every computer unit that has no `behaviour` block plays like this, and every shipped game, test and `ote-sim` result depends on it staying exactly so:

- Each step it scores the best order of every unit that has not acted (move somewhere, then attack, capture, use a staff or item, supply, promote,
  use an ability, open / visit a place) and does the single best one; ties go to the lowest unit id. After all have acted it researches (the lowest price: cost less what an income bonus and granted skills are worth, docs/ECONOMY.md "Branching research"; it never picks a technology the rules close to it and never cancels), builds and upgrades.
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
- **Map weapons** (docs/SRPG.md "Map weapons"): a computer unit standing on a structure with a weapon it may use counts that weapon among its attacks (same scoring). A unit marked
  `"goesFor": ["weapons"]` (default off) also walks to a **free** one, but only when from there it can hit a foe this turn and it has no attack of its own that is better
  (the ballista attack is the fallback; an unmarked unit never walks to one). Code: `ai/ai_order.cpp`.

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
| `mode` | `"advance"` (default), `"hold"`, `"guard"`, `"guardArea"`, `"seek"`, `"patrol"` |
| `wake` | `guard` only: `"once"` (default) or `"react"` |
| `group` | `guard` / `guardArea`: a name (letters, digits, `_`, `-`); the members of a group (same side, same name) wake together |
| `area` | `guardArea`: a rectangle `{ "x", "y", "w", "h" }` (`w`, `h` default 1; squares of the map as the `reach` trigger counts them) |
| `within` | `guardArea`: a whole number 1 to 99, squares from the unit (or a movement cost, with `measure`) |
| `measure` | `guardArea` with `within`: `"steps"` (default) or `"moveCost"` (section "Scripted-style computer players", 8) |
| `route` | `patrol`: 2 to 32 squares `{ "x", "y" }` (section "Scripted-style computer players", 6) |
| `unit` / `place` / `x` + `y` | `seek`: exactly one target: a unit's story id, a placed structure's `id`, or a square |
| `fromTurn` | any mode: whole number 1 to 99; the unit does nothing before that round (1 = from the start, the same as leaving it out) |
| `prefer` | any mode: `"weakest"`, `"nearest"`, `{ "unit": id }`, `{ "type": id }`, `{ "tag": text }` |
| `order`, `weights`, `caution`, `retreat`, `idleTurns`, `ignores`, `heals` | any mode: the scripted-style options (section "Scripted-style computer players" below); all opt-in |

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
- **Fog**: every check (waking, `seek` of a hostile unit, `prefer`) reads only what the unit's team sees. A computer side is bound by fog by default; the map's
  or the side's `aiSeesAll` (docs/ECONOMY.md "Fog options") makes it see everything.
- **Carrying / rescue** (docs/SRPG.md section 5): the AI never rescues, and a carried unit is off the board: nothing changes.
- **Summons and reinforcements**: a unit made by a summon or by production (economy games) has the default behaviour. A unit added by `addUnit` takes the same
  `behaviour` key as a map unit.
- **Groups across sides**: a group is per side.

### Walls

A unit marked `"goesFor": ["walls"]` (docs/SRPG.md section 10, "Walls that break"; default off, so every unmarked unit plays exactly as before) attacks a breakable
door (hit points, `by` allows breaking) **only when no enemy can be reached at all** by its move type, i.e. a closed door is in the way: from a square it can stop on
that is in range it hits with the weapon that does the most damage (score 150 minus half the travel, like opening a door); otherwise it walks toward the
nearest square from which it could (score about 120). It never breaks a door while an enemy can be reached, and an unmarked unit never touches one. A door opening
runs the map's `place` events like any other opening (so a wall that turns into rubble does it for the computer's attack too). Code: `ai/ai_walls.cpp`.

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
has woken); the scripted-style options add `"retreating": true`, `"idleSeen": n` and `"patrolAt": n` (each only when not at its start value). Nothing else is state (a seeker's goal is in its block; the AI keeps no memory). A map that uses none of this saves byte for byte as before; old
saves load. The data hash covers the map files whole, so a behaviour counts only in a game that has one.

### Scripted-style computer players

Opt-in options that let a game reproduce the computer player of a classic handheld tactics game: one unit at a time in a fixed order, a simple weighted attack
score, straight-line pursuit, retreat to heal. **Everything here is off unless a game or a unit asks for it**: a game without the keys plays move for move as
before (the demo sims are identical), and a save without them is byte for byte what it was.

Game-wide settings are `rules.ai` in `game.json`; the per-unit keys are in the unit's `behaviour` block (a map unit, an `addUnit` reinforcement, the story command
`setBehaviour`; the same loader checks for all three).

```json
"rules": { "ai": {
  "turnOrder": "order",
  "weights": { "classic": { "dmg": 2, "left": 3, "friends": 4, "class": 1, "turn": 1, "counter": 2, "danger": 6, "wounds": 1,
                            "priority": { "archer": 10, "soldier": 0 } } } } }
```

| key | where | meaning |
|---|---|---|
| `turnOrder` | `rules.ai` | `"best"` (default: today's way) or `"order"` |
| `weights` | `rules.ai` | named sets of attack weights: `{ "<name>": { ... } }` |
| `order` | behaviour | whole number -100000 to 100000, default 0 |
| `weights` | behaviour | the name of a set of `rules.ai.weights` |
| `caution` | behaviour | `"normal"` (default) or `"none"` |
| `retreat` | behaviour | `{ "below": 30, "until": 80, "to": { "terrain": ["fort"] }, "useItems": true }` |
| `idleTurns` | behaviour | whole number 1 to 99 |
| `ignores` | behaviour | a list (up to 32) of unit story ids or character ids |
| `mode: "patrol"`, `route` | behaviour | 2 to 32 squares |
| `measure` | behaviour, `guardArea` | `"steps"` (default) or `"moveCost"` |
| `heals` | behaviour | `"anyone"` (default) or `"retreating"` |

#### 1. Acting order (`turnOrder` and `order`)

With `"turnOrder": "best"` (the default) the computer does the single best action of any unit first, as the top of this document says. With `"order"` the
side's computer units act **one at a time**: each step the unit with the **lowest `order`** among those that have not acted goes next; a tie goes to the
unit that comes first in **placement order**. That is the unit's number in the battle (its id): the map's `units` list in the order it is written, then units that arrive
later in the order they arrive (an `addUnit` reinforcement, a summon, a unit built at a structure: each is appended when it appears; several in one command: the order of the
command). That unit chooses **its own best order** (attack, staff, item, capture, a move toward the enemy, wait: the usual choice for one unit, scored as ever) and is then done,
whatever the others could have done better. When every unit has acted the side researches, builds and upgrades as usual. The `order` of a unit that has none is 0, and the key does
nothing while `turnOrder` is `"best"` (the editor says so in a note). It can be set on a placed unit, on a reinforcement and by `setBehaviour` (which replaces the block, so it replaces the number).

*Groups* (`guard` / `guardArea` `group`) coordinate only by waking together; that is unchanged. Waking is checked at the start of every step, so a sleeping unit that is woken
by a hit or by its group acts when its place in the sequence comes; a sleeping unit whose place has passed has already used it (it waited). A sleeping unit still takes its place:
it simply does nothing there.

#### 2. Attack weights (`weights`)

A unit whose behaviour has `"weights": "<name>"` chooses among its possible attacks by this score instead of the default scoring. A **candidate** is
a weapon it can use (its weapons in the order of its inventory; a free map weapon only for a unit marked `goesFor` "weapons") x a target (a hostile unit it knows of, in
placement order) x a square it can attack from (any square it may stop on this turn). The score of a candidate:

```
score = max(0, dmgTerm + leftTerm + friendsTerm + classTerm + turnTerm + noCounterBonus - counterTerm - dangerTerm - woundsTerm)
```

Every number is a whole number; the weights are 0 to 100 (default 0, except `dmg` 1), the bases and caps are optional (0 to 1000, `dangerDiv` 1 to 1000). A set's `attackAtZero` is
`false` by default. **The prediction** is the forecast of the fight (the same the player's forecast shows: skills, terrain, weapon triangle, hit and strike counts): per strike
`damage x hit / 100` (the hit chance is included; critical hits are not), added up over the strikes the side will make (`strikes x multiStrike`, or the strikes' own numbers where they
differ). It is an expected value, not rounded. `predicted` below means this number for the attacker's blow, capped at the target's hit points.

| term | as built |
|---|---|
| `dmgTerm` | `killValue` (default 50) if the (uncapped) prediction is at least the target's hit points, i.e. the blow is expected to bring it to 0; otherwise `min(predicted, dmgCap) x dmg` (`dmgCap` default 40). A kill **replaces** the damage term |
| `leftTerm` | `max(0, leftBase - targetHpAfter) x left`; `leftBase` default 20; `targetHpAfter = targetHp - predicted` (0 for a kill) |
| `friendsTerm` | `min(friendsCap, friends x n)`; `n` = the attacker's allies (its team, not itself) on a square next to the target; `friendsCap` default 10 |
| `classTerm` | `class x priority[target's unit type]`; the table `priority` of the set (unit type id: whole number -1000 to 1000, default 0 for a type not listed) |
| `turnTerm` | `turn x` the battle round number (the first round is 1) |
| `counterTerm` | `min(counterCap, predictedBack) x counter`; `predictedBack` = the same prediction for the target's answer, capped at the attacker's hit points; 0 when the blow is expected to kill (a dead unit does not answer); `counterCap` default 40 |
| `noCounterBonus` | added when the target cannot answer from that square with that weapon (out of its range, no weapon, blocked by a condition); default 10 |
| `dangerTerm` | `min(dangerCap, danger x sum / dangerDiv)`; `sum` = for each hostile unit the side knows that could attack the unit on that square after walking (any weapon, any square it can reach), the most it expects to deal to the unit there, capped at the unit's hit points; added up. Worked out on a copy of the battle with the unit standing on the square, so terrain and blocking are those of the square; `dangerDiv` default 8, `dangerCap` default 20. Only computed when `danger` is above 0 |
| `woundsTerm` | `wounds x missing / 5`; `missing` = the attacker's missing hit points in percent, `100 - floor(hp x 100 / maxHp)` |

There is no randomness in the score. **Choice.** All squares are scored; the score of a (weapon, target) pair is the best score of its squares. The pair with the highest score wins; **a tie goes to
the LATER pair** in the order weapon (the unit's own order) outer, target (placement order) inner. A unit attacks whenever the best pair scores **above 0**, or any pair if the set says
`"attackAtZero": true`; with nothing to attack it does the rest of its turn as it would anyway (capture, staff, advance by its mode). **The square** to attack from is among
the squares that reach that best score, in this fixed order: one where the target cannot answer; better terrain `defense`; better terrain `avoid`; fewer known enemies next to it; lower path cost
(the unit's own move); the lower square number. Within what its mode allows: `hold` attacks without moving; a sleeping `guard` / `guardArea` does nothing; a `guard` with `wake: "react"`, an
`advance`, a `seek` and a `patrol` unit may attack from anything it can reach (a seeker's "free hit only" rule and a `prefer` nudge do not apply to a unit with weights; the unit's `ignores` do).
An attack scoring above 0 outranks a plain advance and is outranked by a capture, as in the default scoring. The default "outnumbered" cost and its fade are not part of the weighted score: use `danger`.
A badly hurt unit that carries a healing item still drinks it instead of an attack that does not kill, as the default does (docs/ITEMS.md).

#### 3. Pursuit without caution (`caution`)

`"caution": "none"`: when the unit **moves toward enemies** (or toward the structure a capturer was assigned) it ignores the outnumbered / danger cost of the squares it may stop on. It
takes the path-cost-nearest hostile unit over the whole map (a distance field from every hostile unit, terrain costs of its move type, units in the way ignored) and stops on the reachable
square with the **least remaining path cost**; equal squares: the better terrain `defense`, then the square found first by the move search (the cheaper walk, then the lower square number).
It does not move when no reachable square is nearer than where it stands. Attacks are scored by whatever scoring the unit uses; `caution` is about the walk only. (`seek` and `patrol` never had the cost.)

#### 4. Retreat to heal (`retreat`)

`"retreat": { "below": 30, "until": 80, "to": { "terrain": ["fort"] }, "useItems": true }`. `below` 1 to 99, `until` 2 to 100 (default 100) and more than `below`; `to` and `useItems`
(default true) are optional. At the **start of its side's turn** (after healing, repair and condition damage) the unit's hit-point percent is compared: below `below` (`hp x 100 < below x maxHp`)
it **enters retreat**; once retreating it stays so until it is at `until` or more (`hp x 100 >= until x maxHp`). The state is saved. While retreating it **does not attack** (nor capture, nor use
a staff or ability; counterattacks are rules and still happen) and, in this order:
1. if `useItems` and it carries a healing item for itself, it uses it where it stands (the unit's turn is then over);
2. else, unless its mode is `hold` (a holding unit never moves), it walks toward the nearest **free** square of the listed terrain(s) it can enter (path cost, units in the way ignored): the reachable square with the least remaining cost (ties: better defense, then the move search's order), staying where it is if nothing is nearer, and it waits there (the game's own healing rules for that square do the rest);
3. with no `to`, or no such square it can reach, it goes to the reachable square (or stays) where the **fewest known enemies** could hit it next turn (their move plus their longest weapon); ties: the farther from the nearest enemy, then the better terrain, then the first found.

Retreat comes after `fromTurn` and an asleep guard (those still keep a unit idle); `setBehaviour` ends it (the state starts afresh).

#### 5. Idle turns (`idleTurns`)

`"idleTurns": n`: for its first `n` own turns **on the map** the unit makes **no approach move** (it neither advances, seeks nor patrols) but attacks whatever it can reach by its normal rules (moving to
attack is allowed, as for a reactive guard); after that it behaves by its mode. The turns are counted from the turn the unit arrived (a placed unit: the first turn of its side; a reinforcement: its side's first turn after it appears, or the turn it appears in when that is its own side's turn), and the count is saved. `setBehaviour` starts it afresh (the turn the command comes in counts as the first when it comes in the unit's own turn). This differs from `fromTurn`, which is an
absolute round and makes the unit fully passive.

#### 6. Patrol route (`mode: "patrol"`)

`{ "mode": "patrol", "route": [ { "x": 12, "y": 2 }, { "x": 8, "y": 2 } ] }` (2 to 32 squares inside the map, never the same square twice in a row, the last square counting as followed by the first).
With no target in reach the unit walks toward its **next route square** (path cost, the reachable square with the least remaining cost; ties as for caution none) and stops there; at the start
of its next turn, having **reached** the square (standing on it, or next to it while another unit stands on it), it takes the following one, and after the last the first. The index is saved. It attacks what it can
reach by the normal rules and never sleeps: `wake` is as a `guard` with `"wake": "react"` (it hits what it can reach this turn, and a patrol that is hit does not wake: there is nothing to wake). It has no
caution about outnumbering (as `seek`). `idleTurns` and `fromTurn` hold a patrol back as for any unit.

#### 7. Never chase a given unit (`ignores`)

`"ignores": ["decoy", "lord"]` (unit story ids or character ids; the loader refuses an id nothing on the map or in the game's characters has). Those units are never chosen as an **approach goal** and
never as an **attack target** (the default scoring and the weights both skip them), **unless they are the only hostile units the side still knows of** (then the list is ignored). A seeker's own target
is not ignored.

#### 8. Guard area measured by movement cost (`measure`)

`guardArea` with `within` and `"measure": "moveCost"`: `within` is a **cost**, not a distance. A hostile unit the side knows of wakes the unit when the cheapest walk from the unit to it (the unit's move type,
terrain costs of every square entered including the foe's own square, units in the way ignored) costs at most `within`. `"steps"` (the default) is the distance as before. An `area` still wakes by position.

#### 9. Healers that serve retreating allies (`heals`)

`"heals": "retreating"` (the other value, `"anyone"`, is the default and means today's healer; a block that says it stores nothing). Only the unit's **staves with a heal effect** are
affected (a staff that only cures or inflicts a condition, and every other staff kind the computer uses, work as before). With `"retreating"` such a staff is used only when **every unit it
would heal** is an **ally in the retreat state** (the saved `retreating` flag, see section 4), **below full hit points**, and **not the user**. A staff that heals the user (`"target": "self"`) or an
area that also reaches a non-retreating ally is therefore never used. Among the uses it could make from the squares it can stop on, the one that heals the **unit placed last** wins: the highest unit
id, that is the latest in the map's unit list (a reinforcement is later still), **not** the most wounded one; it then uses the staff the way the default AI does (the same squares, the staff and
the exposure rule), and when no use qualifies the unit does what it would do without the staff (attack, advance, wait). The key is read each step from the block, so `setBehaviour` changes it.
It needs no state of its own: nothing is saved but the key in the block (written only when `"retreating"`). The retreat state itself still comes from the allies' `retreat` blocks; an ally with
none is never in it, so such a healer then never heals. Refusal: `"heals" must be "anyone" (the default) or "retreating" (...)`.

#### Worked example

A game for a classic handheld style. `game.json`:

```json
"rules": {
  "defeat": ["noUnits"], "counterAttacks": true, "items": { "weapons": "fixed", "inventorySize": 4 },
  "ai": { "turnOrder": "order",
          "weights": { "classic": { "dmg": 2, "left": 3, "friends": 4, "class": 1, "turn": 1, "counter": 2, "danger": 6, "wounds": 1,
                                    "priority": { "archer": 10, "soldier": 0 } } } } }
```

The map's red units (the computer side):

```json
{ "type": "archer",  "id": "scout", "side": "red", "x": 12, "y": 4, "behaviour": { "order": 1, "weights": "classic", "caution": "none" } },
{ "type": "soldier", "id": "boss",  "side": "red", "x": 13, "y": 2, "behaviour": { "mode": "hold", "order": 9, "weights": "classic",
                                                                                    "retreat": { "below": 30, "useItems": true } } },
{ "type": "soldier", "id": "gate",  "side": "red", "x": 11, "y": 1, "behaviour": { "mode": "patrol", "order": 2,
                                                                                    "route": [ { "x": 11, "y": 1 }, { "x": 8, "y": 1 }, { "x": 8, "y": 3 } ] } },
{ "type": "soldier", "id": "late",  "side": "red", "x": 12, "y": 0, "behaviour": { "order": 3, "idleTurns": 2, "ignores": ["decoy"] } }
```

Each red turn: `scout` (order 1) goes first and picks the highest-scoring attack of its candidates; if none scores above 0 it walks straight at the nearest enemy without minding the danger. Then
`gate` (2) attacks what is in reach or walks to its next route square; `late` (3) waits for two turns and then walks, never going for the unit `decoy` while another enemy remains; `boss` (9) holds
its throne, attacks what is next to it by the same weights, and under 30 % of its hit points drinks its potion instead (and goes on doing so until it is at 100 %). The same behaviour is
given in one command: `{ "setBehaviour": { "order": 3, "weights": "classic" }, "units": { "unit": "late" } }`. A complete fixture with every key is `tests/fixtures/ai-scripted`.

### Tests

`tests/test_ai_behaviour.cpp` (rules, square and hex), `tests/test_ai_behaviour_data.cpp` (loader, commands, saves; fixture `tests/ai_behaviour_fixture.h`),
`tests/ai_behaviour_player_test.py` (the headless player over a few turns); the scripted-style options: `tests/test_ai_scripted.cpp` (rules: a hand-computed score table, order, caution, retreat, idle,
patrol, ignores, measure), `tests/test_ai_scripted_data.cpp` (loader, refusals, reinforcements, saves; fixture `tests/ai_scripted_fixture.h`), `tests/test_ai_heals.cpp` (`heals`: default, retreating only, never the user, last placed wins, out of reach, no target, loader and saves), `tests/ai_scripted_sim_test.py` (ote-sim on the game
`tests/fixtures/ai-scripted`: deterministic), `tests/ai_scripted_player_test.py` (the headless player), editor: `tests/unit/aiScripted.test.ts`, `tests/e2e/ai-scripted.spec.ts`, `tests/editor_api_scripted_checks.py`; editor: `tests/unit/behaviour.test.ts`, `tests/e2e/ai-behaviour.spec.ts`,
`tests/editor_api_behaviour_checks.py`. Code: `ai/ai_behaviour.cpp` (stance, waking, seek, favourites), `battle/ai_wake.cpp`, `battle/save_behaviour.cpp`,
`data/behaviour.cpp` and `data/loader_behaviour.cpp`. Scripted-style options: `ai/ai_scripted.cpp` (order, ignores, patrol, measure), `ai/ai_weights.cpp`, `ai/ai_retreat.cpp`, `battle/ai_state.cpp`
(the counters and the retreat state at the start of a turn), `ai/ai_staves.cpp` (`heals`), `data/behaviour_scripted.cpp`, `data/loader_ai_rules.cpp`.

### Not built

- Retreating to a repairing structure as a behaviour option: `retreat` (below) goes to terrain; the supply logic still retreats a badly hurt unit to a repairing structure by itself.
- Remembering a chosen target across turns: a seeker's goal is fixed by its block, and the AI otherwise recomputes everything each step.
