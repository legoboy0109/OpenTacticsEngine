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

Status: upgradable structures and research (build order step 1) and repair and supply (step 2) are built in the
rules core (done 2026-11-14, the sections below describe what the core does); fog of war is still
design only. The player has the menus, the research panel, the info-panel lines and the event banners for upgrades,
research, repair and supply (step 4, see "Player"; fog drawing is not done); no editor screens and no demo content yet
(step 5).

A game that uses none of this behaves exactly as today and keeps its save
hash. Each part is switched on by its own data, not by one big switch.

## Upgradable structures

structures.json, per structure kind:
```jsonc
"upgrades": [ { "to": "town", "cost": { "funds": 3000 },
                "requires": "masonry" } ]        // optional technology (Research); cost may be left out (free)
```
- Loader refusals (each names the structure and the entry): `to` unknown, `to` the structure itself, `to`
  listed twice, a different `width` x `height` than the kind, `cost` naming an unknown resource or negative,
  `requires` naming an unknown technology (also when the game has no `data/research.json`), unknown keys.
  `to` may name a kind that comes later in the file; chains (a -> b -> c) and loops (b -> a again) are allowed.
- Action `UpgradeStructure(structure, to)` (`Action::upgrade`): by the owning side, on its turn, when `to` is one of
  the kind's upgrades, it can pay, no unit **of an opposing team** stands on any square of the structure (own and
  allied units may), and `requires` is researched by the side. Free of any unit (like building), does not end
  anything. The structure becomes kind `to` at once: same position, owner kept, capture progress reset to the new
  kind's points. Events: `ResourceChanged` for the payment, then `StructureUpgraded`.
- The kind is `StructureState::type` (it already was per structure, and every rule asks `defOf` / `structureTypeAt`),
  so income, `produces`, `sells`, capture points, move costs, formula numbers, the `hq` tag, the AI and the views
  follow by themselves. `StructureState::mapType` remembers the kind the map placed (for saves).
  Caution for makers: upgrading an `hq` kind into a kind without the `hq` tag removes the headquarters (with
  `defeat: lostHq` the side is then out at the next check); a side that "had an HQ" is decided at the start of the
  battle, so upgrading something into an `hq` kind later does not start the rule for a side that had none.
  The loader does not check either.
- Event `StructureUpgraded` (a = structure index, b = previous kind, c = new kind).
- Saves: the current kind is saved as `"type"` on the structure, only when it differs from the map's.
- Query `upgradeOptions(structure, out)`: every upgrade of the structure's kind for the **active side's own**
  structure (empty otherwise), each with `to`, `def` (cost, requirement), `ok` and `why` ("needs Masonry",
  "an enemy unit stands on it", "not enough Funds").
- AI: after its builds, upgrades when it has **more than twice the cost** of every resource of a paid
  upgrade left (free upgrades are never taken: they could repeat forever); the first such upgrade by structure
  index, then upgrade order.

## Research

`data/research.json` (optional; missing = no research):
```jsonc
{ "technologies": [
  { "id": "masonry", "name": "Masonry", "description": "Towns can be built up.",
    "cost": { "funds": 4000 }, "turns": 2,          // cost may be left out (free); turns 0..99, default 1; 0 = finished at once
    "requires": ["tools"],                          // optional, all needed
    "unlocks": { "units": ["tank"] },               // optional: these unit types need it to be built
    "bonus": { "types": ["infantry"], "stats": { "def": 1 } } }   // optional: units of the side, new and old
] }
```
- Loader refusals: top-level or entry keys other than the listed ones, empty list, empty / duplicate id, unknown
  resource in `cost`, negative cost, `turns` outside 0..99 or not whole, unknown or duplicate `requires`, a
  prerequisite loop (the message shows it: `a -> c -> b -> a`), `unlocks.units` / `bonus.types` naming unknown
  unit types or listing one twice, `bonus` without `stats` or with all zeros or unknown stats. `name` defaults to
  the id.
- structures.json: `"research": true` on a structure kind = a side owning one may research.
- **A unit type unlocked by several technologies needs any one of them** (a unit type no technology lists is always
  free). `unlocks.units` is the only place that says so for unit types; structure upgrades say it themselves with
  `requires` (a technology cannot "unlock" an upgrade from the research file).
- Per side (`SideState`): `researched[]` (per technology, 1 = finished), `researching` (technology or -1) and
  `researchLeft`. Action `StartResearch(technology)` (`Action::research`; always for the active side): pays at once;
  needs a structure with `research: true` owned by the side, the prerequisites finished, the money, nothing in
  progress, and the technology not finished. `turns` counts the **side's own turns**: the countdown runs when
  the side's turn starts (after `TurnStarted` and income, before story events of that turn); a technology with
  `turns: 1` started now finishes at the start of the side's next turn, `turns: 0` finishes inside the action
  (events `ResearchStarted` with c = 0, then `ResearchFinished`). Owning no research structure at that moment pauses the
  countdown without losing the progress. No cancelling in this version.
- Bonuses apply through `Battle::stat` / `unitStat(db, unit, stat, &battle.sides)` (so formulas, forecasts and movement
  see them), to every unit of the side whose type is listed (`types` missing = all), for **new and existing units**.
  They add to the unit's own value and to accessory bonuses, never below 0. A rise of maximum HP does not heal;
  a fall clamps current HP (to at least 1) when the technology finishes, as an accessory does; a unit that
  changes side (story `changeSide`) is clamped to its new side's maximum; new units start at full HP.
  The unit's own stored values (`Unit::stats`, roster, saves) never include the bonus.
- Events (appended after the existing types): `ResearchStarted` (a = side, b = technology, c = turns, 0 = at once),
  `ResearchFinished` (a = side, b = technology; also from the story command).
- Queries: `researchOptions(side, out)` (technologies not finished and not under way, each `ok` / `why`: "needs
  Tools", "not enough Funds", "already researching X", "needs a structure that can research", "it is not that side's
  turn"), `researching(side)`, `researchTurnsLeft(side)`, `hasTechnology(side, tech)`, `canResearch(side)` (owns a
  research structure), `unitUnlocked(side, unitType)`, `unlockedBy(unitType, out)` (the technologies of which any
  one is enough), `canBuild(cell, unitType, why)` (the whole Build check, including unlocks) and
  `buildOptions(cell, out, includeLocked = false)` (locked types are left out unless asked for).
- Story events: condition `{ "researched": "masonry", "side": "blue" }` and command
  `{ "grantResearch": "masonry", "side": "blue" }` (finished at once, no cost, emits `ResearchFinished`; nothing
  when the side has it already; if it is the one under way, that stops being under way). Campaign scenes do not get
  them (scenes have no map and so no sides; they could be added with a side id of the carried sides if wanted).
- Campaign: the finished technologies of **human** sides carry over with the playthrough (`CarriedSide::research`,
  by side id, like resources); **research in progress does not carry** (it is lost with the chapter, and a
  retry of a lost chapter keeps the playthrough as it was). They are handed to the next battle before the roster
  dresses its units, so carried units start at full HP including bonuses.
- Saves: a side with something to say gets `"research": { "done": [technology indices, ascending], "current": n,
  "left": n }` (`current` and `left` only while one is under way); the carried list in a playthrough is
  `"research": [indices]` on the side. See docs/SAVES.md.
- AI: before its builds it starts the **cheapest** technology (by summed cost; ties go to the lowest index) it can
  start now and pay for while still being able to afford a unit it could build today (a side with nothing to build
  is not held back).

## Repair and supply

game.json `rules.supply` (optional):
```jsonc
"supply": { "repair": { "amount": 20,          // percent of max HP per turn, 1..100 (required inside "repair")
                        "costShare": 100 },    // percent of the proportional unit cost paid, 0..100 (default 100; 0 = free)
            "structures": true }               // own structures also refill fuel and ammunition (default false)
```
Unknown keys inside `supply` / `repair` are refused. A game without the block repairs and resupplies nothing.

- **Repair**: at the start of a side's turn every hurt unit of it standing on a structure it owns (any square of a
  bigger one; allied or neutral structures do not count) heals `amount`% of its **maximum HP** (`Battle::stat`, so
  research and accessory bonuses count), rounded up, never more than is missing. structures.json
  `"repairs": ["air", "ship", "tank"]` limits which units a structure kind repairs: each entry is a unit type id or a
  tag some unit type carries (the same rule as weapon `versus` keys, same refusal when it is neither); no key = all;
  `[]` = repairs nothing. A unit is matched when its type id or one of its tags is listed.
- **Price**: for healing h HP, each resource `r` in the unit type's `cost` is paid as
  `ceil(cost[r] x costShare / 100 x h / maxHp)`. Rounding is **up, per resource**, so a repair is never free by
  rounding; a unit type with no `cost`, or `costShare` 0, repairs free. If the side cannot pay the full amount the
  unit heals the **most HP it can afford** with every resource (the price only grows with h; possibly none, then
  nothing happens and no event is sent). Units are served in unit index order, so a short purse pays the first ones.
- **Fuel** (optional per unit type): units.json `"fuel": 60` (1..999), optional `"fuelPerTurn": 5` (needs `fuel`,
  1..999 and less than `fuel`). A unit starts with a full tank. Moving costs fuel equal to the movement points of the
  path; `Battle::reachable` limits the budget to `min(movement, fuel)`, so the movement range, the path planner, the
  Move check and the AI all see the same squares. A unit with no fuel left can still wait, attack, capture. At the start
  of its side's turn a unit with `fuelPerTurn` loses that much (the first turn of the battle included; a unit built or
  added later burns from its side's next turn on); when that leaves 0 it is destroyed: a normal death (`UnitDied`, the
  story's `defeated` events with no killer, defeat rules and objectives are checked right away). A type without
  `fuelPerTurn` at 0 just cannot move.
- **Ammunition** (optional per weapon): weapons.json `"ammo": 6` (1..99). A unit has one count per entry of its type's
  weapon list. An attack spends one from the weapon used when its first strike is made, whether the unit started the
  fight or counters (several strikes cost one; a counter that never comes, because the first strike killed, costs
  nothing). A weapon at 0 is not offered: `weaponsOf` leaves it out, so attack options, forecasts, the Attack check
  and the AI skip it, and a defender counters with its next usable weapon or not at all. With carried weapons
  (docs/ITEMS.md) item `uses` already do this job: `ammo` on a weapon that is carried as an item is a loader error,
  and a game with carried weapons tracks no ammunition.
- **Resupply** refills fuel and ammunition to full and sends `UnitResupplied` (only when something was missing):
  own structures when `structures: true` (free, a unit on any square), and unit types with units.json
  `"supplies": true`: for the **adjacent units of the same side**, at the start of their side's turn and by the action
  `Supply(unit)` (the supplier must not have acted, needs at least one adjacent unit of its side that lacks
  something; it may move first; it ends the supplier's turn).
- **Order at the start of a side's turn** (one place, `Battle::beginTurn`): `TurnStarted`, income, the research
  countdown, **resupply** (structures, then suppliers), **fuel burn** (and its destructions), **repair**, then the
  story's turn events. Income first so it can pay; research before repair so a finished HP bonus counts; resupply
  before the burn so a unit on a depot is not destroyed by it; burn before repair so a unit that dies does not cost a
  repair. If a unit running dry ends the battle, the rest of the turn start does not happen.
- **Events** (after the old ones): `UnitRepaired` (a = unit, b = HP restored, c = its HP now, data = what it cost
  per resource index, empty when free); the payment comes first as `ResourceChanged` events like every other
  payment. `UnitResupplied` (a = unit). Fuel and ammunition are not events (the view reads the unit).
- **New units** (built, added by a story event, placed by the map, dressed from a campaign roster) start with a full
  tank and full ammunition. A campaign does **not** carry them: every chapter starts full.
- Saves: per unit `"fuel": n` (units whose type has fuel) and `"ammo": { "<weapon id>": n }` (units with a
  weapon that has ammunition; unlimited weapons are not listed); see docs/SAVES.md. Games without these features
  write exactly the old files and keep their hash.
- AI (`ai/ai_supply.cpp`; all of it only when the data exists): a unit **below 40% HP** that can reach this turn a
  structure of its own side that would repair it, and could pay for at least some of it now, goes there (the
  nearest by travel cost, ties to the lowest square), unless its planned attack kills. A unit **below a quarter of its
  fuel, or with no usable weapon left for lack of ammunition**, goes toward the nearest own structure (when structures
  resupply) or own supplier (distance field, so not through impassable ground), waits where it stands when it is
  already on a structure or next to a supplier, and is not held back by a planned kill. A supplier uses `Supply` when
  a square it can reach has a unit of its side next to it that lacks something (it moves there first). Every square is
  taken from `reachable`, so it never plans a move its fuel does not allow.
- Queries for the player (`Battle`): `fuel(unit)`, `maxFuel(unit)` (-1 = none), `ammo(unit, weapon)`,
  `maxAmmo(unit, weapon)` (-1 = unlimited), `needsSupply(unit)`, `canSupply(unit)`, `supplyTargetsFrom(unit, cell,
  out)`, `repairPreview(unit, plan)` / `repairPreviewAt(unit, cell, plan)` (`RepairPlan`: heal, cost per resource).

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
- Action menu: **Supply** for supplier units (the units it would fill are tinted while the entry is selected).
- Built (2026-11-14): a structure that offers one thing opens it at once, several open a menu first; greyed entries show
  their reason (upgrade / build: in place of the price; research: in the panel's detail lines); locked unit types are
  greyed in the Build menu ("needs Armor"). Events show a banner (research), a heal number (repair), a small
  "Resupplied" float and a flash on an upgraded structure. Test: `tests/economy_player_test.py`.

## Not in this version

Line-of-sight blocking by terrain; cancelling research; selling or
demolishing structures; upkeep costs; trade between sides; per-unit radar /
stealth abilities beyond `hides`; several research queues.

## Build order

1. Core: upgradable structures + research (data, rules, saves, AI, tests). **Done.**
2. Core: repair and supply. **Done.**
3. Core: fog of war (the biggest: sight, per-team event visibility, AI).
4. Player: menus and panel for 1 and 2; fog drawing in every kind of view.
5. Editor: all screens; `demo-economy` content (a town upgrade, two
   technologies, repair, one fog map).
