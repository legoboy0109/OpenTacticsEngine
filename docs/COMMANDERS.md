# Commanders

An optional system in the spirit of modern-war turn-based strategy games: each side is led by a **commander** who gives the whole army passive strengths
and weaknesses, fills a **power meter** as the battle goes on, and can spend it on a temporary army-wide **power**. The engine's neutral word is
*commander*; a game names its own (a CO, a general, a hero) in the `name` of each entry. Without `data/commanders.json` everything plays, saves and
simulates exactly as before (the five demo simulators and their event streams are byte for byte the same).

It is built on two things that already exist: the **side-wide skills** (docs/SRPG.md "Side-wide modifiers": `SideState::modifiers`, `Battle::skillsOf`) carry both the
passive skills and a power's timed skills, so the whole rules engine and the computer opponent see them with no special case; and **factions** (docs/ECONOMY.md
"Factions"): a side picks a faction and then a commander, and a faction may limit the choice. Tests: `tests/test_commanders.cpp` (the rules),
`tests/test_commanders_more.cpp` (saves, factions, action points, speed order, the computer), `tests/commanders_player_test.py` (the player),
`tests/editor_api_commanders_checks.py`; the fixture game is `tests/fixtures/commanders`.

## data/commanders.json

```json
{ "commanders": [
  { "id": "ada", "name": "Ada", "description": "A steady captain.", "portrait": "ada",
    "passive": [ { "skill": "bulwark", "filter": { "tag": "infantry" } } ],
    "meter": { "max": 100, "perDamageDealt": 2, "perDamageTaken": 1, "perTurn": 0 },
    "powers": [
      { "id": "rally", "name": "Rally", "description": "Heals every unit 5 and gives +3 Strength.", "cost": 40, "turns": 1,
        "modifiers": ["frenzy"], "effects": [ { "heal": 5 } ] },
      { "id": "surge", "name": "Surge", "cost": 80, "turns": 2, "costGrowthPercent": 50,
        "modifiers": ["fleet"], "effects": [ { "refresh": true } ] } ] },
  { "id": "bram", "name": "Bram",
    "passive": [ "fleet", { "skill": "brittle", "filter": { "tag": "heavy" } } ],
    "meter": { "max": 60, "perDamageDealt": 1, "perTurn": 10 },
    "powers": [ { "id": "storm", "name": "Storm", "cost": 30,
                  "effects": [ { "damage": 4, "to": "enemies" }, { "giveResource": { "gold": 25 } } ] } ] } ] }
```

A commander (`id` is required, unknown keys are refused):

| key | meaning |
|---|---|
| `name`, `description` | shown on the pick screen and in the panel (`name` defaults to the id) |
| `portrait` | a portrait id of `graphics.json` (docs/GRAPHICS.md "Portraits"); stored and checked by nothing, **not drawn by the player in this version** |
| `passive` | side-wide skills that are always on: a skill id, or `{ "skill": id, "filter": { "type": id } \| { "tag": text } \| {} }` (the filter of a technology's `grants`). A weakness is a passive too (a skill with a negative bonus) |
| `meter` | **required**: how the power meter fills (below) |
| `powers` | up to **4** powers (none is fine: a passive-only commander) |

### The meter

`"meter": { "max": n, "perDamageDealt": a, "perDamageTaken": b, "perTurn": c }`. `max` is 1 to 100000; the three rates are whole numbers from 0 to 1000 (a key left out is 0) and
at least one must be above 0. All integer maths, no randomness.

- **`perDamageDealt`**: for every **hit point of strike damage** a unit of the side takes off a foe (the damage the `Attack` event reports, capped at the foe's remaining hit points; a
  counter-attack and each strike of an area attack count). **`perDamageTaken`**: the same for damage its units take. One rule, whatever the game: it counts **hit points of
  strike damage**, never the value or cost of the units. Damage from skills, conditions (poison), recoil, staves, and a story fight does not fill the meter.
- **`perTurn`**: added when the side's turn starts (in a game with the speed turn order: when a round starts).
- The meter is **capped at `max`**. It does **not fill while the side's power is active** (nothing at all: not from damage, not from `perTurn`); the other side's meter is not affected.
- It starts at 0 and carries over between turns. Using a power pays its cost out of it (it does not empty it).

### Powers

| key | meaning |
|---|---|
| `cost` | meter points, 1 to the meter's `max` (a dearer power could never be used: refused) |
| `turns` | how long its timed skills last, in the side's own turns, default **1** = until that side's next turn starts. Only with `modifiers` (refused otherwise) |
| `costGrowthPercent` | each earlier use makes the next one dearer by this percent of the base cost (whole points, 0 to 1000, default 0). A power never costs more than the meter holds |
| `modifiers` | side-wide skills added while the power is active (same format as `passive`) |
| `effects` | instant effects applied once when it is used (below) |

A power needs at least one of `modifiers` and `effects`. A power with **timed skills** becomes the side's **active power**: no other power can be used until it ends, and the meter rests.
A power of **instant effects only** is over as soon as it is used (nothing is active, the meter keeps filling, and the next one may follow at once if the meter allows).

**Effects** (a small closed set, one per entry; anything else is refused at load):

| effect | does |
|---|---|
| `{ "heal": n, "filter"? }` | every living unit of the side on the map that matches the filter regains up to `n` hit points |
| `{ "damage": n, "to": "enemies", "filter"? }` | every hostile unit on the map (seen or not) that matches loses `n` hit points, **but never below 1**: a power does not defeat units. Guards that are hurt wake up |
| `{ "giveResource": { "gold": n } }` | the resources are added to the side (scaled for a human side by the difficulty as the story's `giveResource` is) |
| `{ "refresh": filter \| true }` | matching units of the side that moved or acted may act again (the Refresh ability's effect; in a speed game their charge is set to the threshold, except the unit that has the turn) |

`filter` is `{ "type": id }`, `{ "tag": text }` or `{}` (every unit). Healing and hurting are the `UnitHealed` / `UnitHurt` events a skill block's effects already use.

## On a map

A side names its commander with `"commander": "<id>"` or `"commander": "choose"`. No key = no commander. `"choose"`: a person picks on a screen before the battle (after the
side's faction screen, if the faction is also `"choose"`); a **computer side takes one from the seed** (the same mixing as a faction, with other constants: it never moves the
battle's own random numbers). A faction may list the commanders a `"choose"` side of that faction may pick: `"commanders": ["ada", "cato"]` in `factions.json` (an empty list is refused;
no key = any). **A commander the map names itself is taken as written, whatever the faction lists.** A person's side that nobody picks for (a campaign chapter, `--autoplay`) takes the
first allowed commander.

**Campaigns:** the commander belongs to the map's side; nothing is carried from one chapter to the next in this version (each chapter's map says which commander its sides have, and
a `"choose"` side of a chapter takes the first allowed one: the campaign has no pick screen).

## In the battle

`Action::usePower(power)` (`ActionType::UsePower`): the **active side** spends its meter on power `power` (an index into its commander's `powers`). It is free of any unit and costs no
action points. Allowed when: the battle is on, the side has a commander and it is the side's turn (a speed game: the unit that has the turn is the side's own), no power of the side is active, and the
meter holds the cost (`Battle::canUsePower(side, power, &why)`; why: `needs 12 more`, `a power is active already`, `it is not that side's turn`). Then, in this order: the meter is paid, `PowerUsed` is
appended (a = side, b = commander, c = power, d = the price, data = {meter left}), the timed skills go in, the instant effects follow as their usual events (`UnitHealed`, `UnitHurt`, `ResourceChanged`,
`UnitRefreshed`). The use is counted (`Battle::powerUses`) for the cost growth.

An active power ends when the side's `turns`-th own turn start arrives after the one it was used in (`turns` 1: the next one): the timed skills go and `PowerEnded` (a = side, b = commander, c = power)
is appended. A side's turn start is also where `perTurn` is added, after the end. Both events come **before** `EventType::Choice`, which stays the last entry.

Order of a side's modifiers (so skills are judged in the same order live and after a save): the faction's, the commander's passive skills, finished technologies', and last the active power's.

**Interactions**

| | |
|---|---|
| action points | works: a power costs no AP; strikes fill the meter as usual |
| speed turn order | works: *the side's turn* is "the active unit belongs to the side"; `turns` and `perTurn` count **rounds** (the per-side effects of a round are where a side's turn start is); a refresh sets the charge to the threshold |
| factions | a faction's skills come first, then the commander's; the faction may limit the pick |
| fog | the events are public: everybody sees that a power was used |
| research | a technology finished while a power is active goes in front of the power's skills |
| difficulty | a gain of gold is scaled for a human side; the meter is not |

## Player

- **Pick screen** (a free battle; docs/ECONOMY.md "Factions" pick comes first): one row per commander the side may take; under the list the description, the passive skills, each power with its
  price and what it does, and the meter's rule. Arrows + Confirm, a tap, Cancel goes back.
- **Side panel**: the commander's name with `meter/top`, a bar with a tick at the price of each power (green while a power can be used, amber while one is active, then "Power: name  turns left").
  It shows the side the screen is for. **Under it, every other side's commander gets one slim block**: the name in that side's colour, `meter/top` (dropped when the row is too narrow, as at UI
  scale 150), a thin bar with a tick per power price, and while its power runs an amber bar with `P<turns left>`. Rule for three or more sides: the sides are taken in turn
  order starting with the one after the viewed side (the next to act first), at most **two** blocks, and only while they fit above the cell and unit rows (the unit rows are never pushed off).
  **When the whole unit summary (eight rows) would not fit under a full block** (UI scale 150 on the 480x270 canvas), the block is the bar alone: three pixel rows in the side's colour with the
  price ticks, tucked right under the own bar, so the unit's weapon line still fits. The player logs `other meter: <name> <n>/<top> (bar only)` then.
  **Fog hides nothing here**: a meter is not information about the map. The player test logs `other meter: <name> <meter>/<top>` whenever it changes.
- **Map menu** (the menu of an empty square, with End Turn): one **`Power: <name>`** entry per power with its price; greyed with `needs 12 more` while the meter is short, or `active`.
  Using one shows a banner with the commander's name and the power's name. The idle hint line adds "(power ready)".
- An enemy's (or any other side's) power shows the same banner (commander and power name, in that side's colour), also under fog: `PowerUsed` / `PowerEnded` are public events.

## The computer opponent

See docs/AI.md "Commanders".

## Refused at load

Each message names the file, the entry and the key: an unknown skill, resource or unit type / tag; a duplicate commander or power id; a `cost` over the meter's `max` or below 1; more than 4 powers; a bad
effect (`has "explode" but no effect`, two effects in one entry, a key the effect does not take, `damage` to anything but `enemies`); a meter that fills by nothing; `turns` without `modifiers`; a power that
does nothing; a faction listing an unknown commander (or none: `[]`) or in a game without commanders; a map side naming an unknown commander, or `commander` / `"choose"` in a game without `data/commanders.json`; an empty `commanders` list.

## Not in this version

A commander that carries its meter or use counts into the next chapter; a commander portrait on screen; a power with a cost in something other than meter points; an editor form (the editor server only passes
`data/commanders.json` and the keys through, docs/EDITOR_API.md); damage that can defeat; targeted powers (aimed at a square or a unit).
