# Economy options

Contract for PLAN item 3g. Owner decisions (2026-11-14): all four in the
first version:

- **Fog of war**: the maker switches it on per map. Advance Wars kind: the
  map itself is known; enemy units and who owns far-away structures are
  hidden outside sight range; some terrain hides units until adjacent.
- **Repair and supply**: repair on own structures is the base; fuel and
  ammunition only for unit types / weapons the maker gives an amount.
- **Research**: pay and wait. Cost, turns (0 = instant), prerequisites, one
  at a time, at a structure that allows it; unlocks unit types, structure
  upgrades or bonuses.
- **Upgradable structures**: a structure becomes a better kind for a cost.

Everything below is engine design and may change; wording shown to players
and makers is placeholder until the owner approves it.

Status: design only; see "Build order".

A game that uses none of this behaves exactly as today and keeps its save
hash. Each part is switched on by its own data, not by one big switch.

## Upgradable structures

structures.json, per structure kind:
```jsonc
"upgrades": [ { "to": "town", "cost": { "funds": 3000 },
                "requires": "masonry" } ]        // optional technology (Research)
```
- Action `UpgradeStructure(structure, to)`: by the owning side, on its turn,
  when it can pay, no enemy unit stands on the structure, and `requires` is
  researched. Free of any unit (like building). The structure becomes kind
  `to` at once: same position, owner kept, capture progress reset. `to` must
  have the same `width` x `height` (loader error otherwise).
- Event `StructureUpgraded` (structure, from kind, to kind).
- Saves: each structure's current kind is saved (today only owner and
  capture state are; the kind was fixed by the map).
- AI: upgrades when it has more than twice the cost left after its builds.

## Research

`data/research.json` (optional; missing = no research):
```jsonc
{ "technologies": [
  { "id": "masonry", "name": "Masonry", "description": "Towns can be built up.",
    "cost": { "funds": 4000 }, "turns": 2,          // 0 = finished at once
    "requires": ["tools"],                          // optional, all needed
    "unlocks": { "units": ["tank"] },               // optional: these unit types need it to be built
    "bonus": { "types": ["infantry"], "stats": { "def": 1 } } }   // optional: units of the side, new and old
] }
```
- structures.json: `"research": true` on a structure kind = a side owning
  one may research.
- A unit type or structure upgrade that some technology unlocks cannot be
  built / done by a side until that side has it. `unlocks.units` is the
  only place that says so (one fact, one place).
- Per side: finished technologies, and at most one in progress (id + turns
  left). Action `StartResearch(side, technology)`: pays at once; needs a
  research structure, the prerequisites and nothing in progress. Turns count
  down at the start of the side's turn; at 0 it is finished (`turns: 0`
  finishes immediately). Losing the last research structure pauses the
  countdown, it does not lose the progress. No cancelling in this version.
- Bonuses apply through `Battle::stat` (as accessories do), to every unit
  of the side whose type is listed (`types` missing = all), HP and move
  included.
- Events `ResearchStarted`, `ResearchFinished` (side, technology).
- Story events get: condition `{ "researched": "masonry", "side": "blue" }`
  and command `{ "grantResearch": "masonry", "side": "blue" }`.
- Campaign: finished research carries over with the playthrough.
- AI: researches the cheapest available technology when it can afford it
  and still build.

## Repair and supply

game.json `rules.supply` (optional):
```jsonc
"supply": { "repair": { "amount": 20,          // percent of max HP per turn, on a structure of its own side
                        "costShare": 100 },    // percent of the proportional unit cost paid (0 = free)
            "structures": true }               // own structures also refill fuel and ammunition
```
- **Repair**: at the start of a side's turn every unit of it standing on a
  structure it owns heals `amount`% of max HP (rounded up, capped), paying
  `costShare`% of `unit cost x healed share`; if the side cannot pay, it
  heals what it can afford. structures.json `"repairs": ["tag", "unitType"]`
  limits which units a structure kind repairs (missing = all).
- **Fuel** (optional per unit type): units.json `"fuel": 60`, optional
  `"fuelPerTurn": 5` (aircraft, ships). Moving costs fuel equal to the
  movement points spent. A unit cannot move further than its fuel allows. At
  the start of its side's turn a unit with `fuelPerTurn` loses that much;
  at 0 fuel a unit with `fuelPerTurn` is destroyed, others just cannot move.
- **Ammunition** (optional per weapon): weapons.json `"ammo": 6`. Each
  attack (not each strike) with it spends one; at 0 it cannot be used
  (another weapon of the unit is used, or none). With carried weapons
  (docs/ITEMS.md) item `uses` already do this job: `ammo` on a weapon that
  is carried as an item is a loader error.
- **Resupply**: own structures (if `structures: true`) at the start of the
  turn, for free; and unit types with `"supplies": true`: at the start of
  their side's turn, and by the action `Supply(unit)` (ends its turn), they
  refill fuel and ammunition of adjacent units of their side.
- Events `UnitRepaired` (unit, amount, cost), `UnitResupplied` (unit),
  `FuelSpent` is not an event (the view reads the unit).
- Saves: per unit fuel and per fixed weapon ammunition.
- AI: a unit below 40% HP that can reach an own structure this turn and has
  no kill goes there; a unit below a quarter of its fuel or out of
  ammunition goes toward the nearest own structure or supplier.

## Fog of war

Map file: `"fog": true` (default false).
- units.json `"vision": 3` (default 2); terrain.json / structures.json
  optional `"visionBonus": 2` (for units standing on it, e.g. mountains)
  and `"hides": true` (forest: a unit standing on it is seen only from an
  adjacent square). Structures owned by a side see 1 around themselves.
- A **team** shares sight. A square is visible to a team when within
  vision range (grid distance, no line-of-sight blocking in this version)
  of one of its units, subject to `hides`.
- Hidden from a team: enemy units on squares it does not see, and the
  **current owner / capture state** of structures it does not see (it keeps
  the last owner it saw). Terrain, heights and structure positions are
  always known.
- Rules: a unit cannot target what its team does not see. A move is planned
  on what is seen; if the path runs into a hidden enemy the unit stops on
  the last free square before it and its turn ends (`MoveBlocked` event:
  the ambush). Sight is recalculated after every move, death, capture and
  build. Sight is always "now": squares passed during a move do not stay
  revealed.
- `Battle` keeps the truth; a view asks `visibleTo(team, cell)`,
  `unitVisibleTo(team, unit)`, `seenOwner(team, structure)`. The event
  stream carries, per event, which teams saw it, so the player shows a human
  only what its team saw (an unseen enemy move is not animated; a unit
  walking into view appears at the first seen square).
- AI plays by the same rules: it only knows what its team sees (plus last
  seen structure owners).
- Hot-seat (two humans on one device): the screen is covered between turns
  ("Pass the device to <side>") when fog is on.
- Saves: last seen owner per team and structure. Sight itself is
  recomputed.
- Story events: unaffected (they act on the truth); dialogue always shows.
  New condition `{ "sees": <filter>, "team": "blue" }`.

## Editor

- Structures: upgrades list, "research" checkbox, repairs list, vision
  bonus, hides.
- New **Research** screen (like Items): technologies, costs, turns,
  prerequisites, unlocks, bonus.
- Rules: Supply group (repair amount / cost share, structures resupply).
- Units: vision, fuel, fuel per turn, supplies. Weapons: ammunition.
  Terrain: vision bonus, hides.
- Map settings: Fog of war checkbox.
- Play panel: with fog, a "show everything" toggle for the maker.

## Player

- Structure menu (on an own structure with nothing to build, or beside
  Build): **Upgrade**, **Research** (list with cost, turns, what it gives;
  the one in progress with turns left).
- Info panel: fuel and ammunition when the unit has them; research in
  progress under the side's resources.
- Fog: unseen squares are dimmed; hidden units are not drawn; structures
  show the last seen owner. Works in all four kinds of view.
- Action menu: **Supply** for supplier units.

## Not in this version

Line-of-sight blocking by terrain; cancelling research; selling or
demolishing structures; upkeep costs; trade between sides; per-unit radar /
stealth abilities beyond `hides`; several research queues.

## Build order

1. Core: upgradable structures + research (data, rules, saves, AI, tests).
2. Core: repair and supply.
3. Core: fog of war (the biggest: sight, per-team event visibility, AI).
4. Player: menus and panel for 1 and 2; fog drawing in every kind of view.
5. Editor: all screens; `demo-economy` content (a town upgrade, two
   technologies, repair, one fog map).
