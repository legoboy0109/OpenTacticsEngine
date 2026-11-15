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

Status: upgradable structures and research (build order step 1), repair and supply (step 2) and fog of war (step 3)
are built in the rules core (done 2026-11-14, the sections below describe what the core does). The player has the
menus, the research panel, the info-panel lines and the event banners for upgrades, research, repair and supply
(step 4, see "Player") and draws fog of war in all four kinds of view; `games/demo-economy` uses all of it (step 5's demo
content, see "Build order"); the editor screens are the other half of step 5.

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
- **Where research is done** (game.json `"rules": { "research": { "needsStructure": true } }`, optional, default **false**;
  unknown keys and non-boolean values are refused): by default a side can research whenever the game has technologies and
  nothing ever pauses (the player offers it in the battle menu). With `needsStructure: true` a side must own a structure
  whose kind has `"research": true` in structures.json (structures.json `"research": true` stays valid data either way; it
  matters only while the rule is on, and the loader says nothing about it when the rule is off). Rule on: losing the last
  such structure pauses the countdown (below). A game that has no `needsStructure` key keeps its save fingerprint (game.json
  rules are hashed whole, so the key counts only when present), but its behaviour changed on 2026-11-14: research no longer
  needs a building (the owner's decision).
- **A unit type unlocked by several technologies needs any one of them** (a unit type no technology lists is always
  free). `unlocks.units` is the only place that says so for unit types; structure upgrades say it themselves with
  `requires` (a technology cannot "unlock" an upgrade from the research file).
- Per side (`SideState`): `researched[]` (per technology, 1 = finished), `researching` (technology or -1) and
  `researchLeft`. Action `StartResearch(technology)` (`Action::research`; always for the active side): pays at once;
  needs (with `needsStructure` on) a structure with `research: true` owned by the side, the prerequisites finished, the money, nothing in
  progress, and the technology not finished. `turns` counts the **side's own turns**: the countdown runs when
  the side's turn starts (after `TurnStarted` and income, before story events of that turn); a technology with
  `turns: 1` started now finishes at the start of the side's next turn, `turns: 0` finishes inside the action
  (events `ResearchStarted` with c = 0, then `ResearchFinished`). With `needsStructure` on, owning no research structure at
  that moment pauses the countdown without losing the progress. No cancelling in this version.
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
  turn"), `researching(side)`, `researchTurnsLeft(side)`, `hasTechnology(side, tech)`, `canResearch(side)` (rule off: the game has technologies;
  rule on: the side owns a research structure), `unitUnlocked(side, unitType)`, `unlockedBy(unitType, out)` (the technologies of which any
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
- AI: it follows the same rule (`canResearch`). Before its builds it starts the **cheapest** technology (by summed cost; ties go to the lowest index) it can
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

Map file: `"fog": true` (default false; a boolean, anything else is refused). At most 32 sides on a fog map (an
event's "seen by" is a 32-bit mask with one bit per team). Built in the rules core and the player (2026-11-14); the
editor has no switch for it yet (step 5).

**Data.**
- units.json `"vision": 3` (whole number 0 to 20, default 2): how many squares the unit's team sees from where it
  stands, counted in grid distance (the same measure as weapon ranges), no line-of-sight blocking in this version.
  It is not a stat: research and accessories do not change it.
- terrain.json and structures.json `"visionBonus": 2` (0 to 10, default 0): added to the vision of a unit standing on
  the square (a mountain). A unit on a structure square gets the terrain's and the structure's bonus **added**.
- terrain.json and structures.json `"hides": true` (default false): see "Hiding" below. Either one hiding is enough.
- Every refusal names the file, the entry and the key (`units.json: unit "scout": "vision" must be from 0 to 20`).

**Sight** (`Battle::visibleTo`), per **team** (sides of a team share everything):
- A square is in sight when it is within `vision + bonus` of a living unit of the team, or within 1 of any square of
  the footprint of a structure owned by a side of the team (so the whole footprint and one square around it).
  A neutral structure sees nothing.
- **Hiding**: a unit standing on a square that hides is seen only by a unit of the team standing next to it (grid
  distance 1; even a unit with vision 0 notices a neighbour in a hideout). The hiding square's terrain itself is
  always known, as all terrain, heights and structure positions are. A unit of the team itself is always seen
  by it (`unitVisibleTo` is true for the team's own units, hideout or not); dead units are never seen.
- Sight is recomputed after every change made through `apply` (move, death, capture, build, unit added, removed or
  relocated, side change, structure upgrade) and at battle start and load; the per-team bit grids are rebuilt lazily
  the first time a query asks after a change (deterministic, no randomness). Code that edits the public state by hand
  (tests, tools) calls `Battle::refreshSight()` afterwards. It is cheap: about units x area per team, rebuilt only
  when dirty.

**Rules with fog on.**
- **Targeting**: attack options, `forecast` and `apply(Attack)` leave out units the acting team does not see (the same
  refusal as any impossible attack, "that attack is not possible"); Talk is not offered towards an unseen unit. Item
  and heal targets, trading, supply and the shop only ever involve units of the same team (or none), so they need no
  rule. Planning "as if the unit stood on another square" still judges the targets on the sight of right now. A
  defender still **counterattacks** an attacker its team cannot see (the exchange is the rules' business, not a
  choice of the team).
- **Movement is planned on what is seen**: `reachable` treats an enemy unit the unit's team does not see as absent (the
  square costs its terrain, can be passed and can even be a stop). Seen enemies block as before.
- **Ambush**: an order is carried out along the planned path square by square. If the next square holds an enemy
  unit (seen or not: the plan is fixed, it is not replanned when something comes into view on the way), the unit stops on
  the last square before it that it may legally end on (an ally's square can be walked through, never stopped on, so it
  falls back further; at worst its own starting square), spends fuel only for the squares it walked, has `moved` set, and the
  event `MoveBlocked` follows the `UnitMoved` of the squares walked (no `UnitMoved` when it never left). Story "reach"
  triggers fire for the square it really ended on, only if it moved. What happens to the unit's turn is the maker's choice,
  game.json `"rules": { "fog": { "ambush": "endsTurn" | "canAct" } }` (optional, default `"endsTurn"`; unknown keys and other
  values are refused; a game without the key is unchanged, also in its save fingerprint):
  - `endsTurn`: `done` is set too, the turn is over for the unit (the behaviour before the rule existed).
  - `canAct`: the unit is **not** done. It has "moved" (also when it walked zero squares) so it cannot move again, but it may
    do anything else it could do after a normal move to that square: attack (the revealed enemy included, once the team
    sees it, which a neighbour on open ground does if the unit's vision is at least 1), Capture, Wait, items, Talk, trading,
    Supply. There is no undo. A unit with vision 0 does not notice the neighbour that stopped it, so it cannot attack it.
    Saves need nothing new (`moved` and `done` are saved per unit; a mid-turn save with a moved, not done unit resumes as is).
  - AI with `canAct`: after a blocked move it chooses again from where it stopped through the same code as a unit that
    already moved (attack if worth it, capture, use an item, else wait); deterministic.
  The mover's team notices structures passed on the way (their last seen owner is updated at every square walked, not
  only at the end).
- **Last seen owner per team and structure** (`seenOwner(team, structure)`): the true owner while the team sees any
  square of the structure, otherwise the last one it saw; the map's starting owners are known to everybody at the start.
  A change of owner is learned by every team that saw the `StructureCaptured` event (rules below) including the
  old and the new owner's team; a defeated side's structures turn neutral for **every** team (`SideDefeated` is public).
  Capture progress is not remembered: a view shows a structure's capture points only for a structure it currently sees
  (`structureVisibleTo`); `CaptureProgress` events are seen only by teams that see the capturer.
- **The truth decides**: defeat and victory rules, objectives and story events (including triggers by unseen units and
  `Talk` conditions) act on the real situation. Dialogue and objective changes are public.
- **Story condition** `{ "sees": <unit filter>, "team": "<team>" }` (docs/EVENTS.md): true when the team sees at least one
  living unit that matches. Without fog it sees every living unit.

**Queries** (`Battle`, with fog off they answer "everything visible" and the true owner, so callers need no special
case): `fogOn()`, `visibleTo(team, cell)`, `unitVisibleTo(team, unit)` (a unit id or a `Unit`), `structureVisibleTo(team,
structure)`, `seenOwner(team, structure)`, the side forms `visibleToSide`, `unitVisibleToSide`, `seenOwnerForSide`,
`refreshSight()`.

**Event visibility.** Every `Event` has `uint32_t seen` (bit `t` = team `t` saw it; all bits without fog and for the
public events, so nothing changes for existing consumers) and, for `UnitMoved` with fog, `seenSteps`: per team (index =
team) two numbers, the first and last index into `data` at which that team saw the mover (-1, -1 = never; the mover's
own team: 0 and the last). "Saw" is decided with the sight of the moment the event happened:
 - public (all teams): `TurnStarted`, `SideDefeated`, `BattleEnded`, `Dialogue`, `Focus`, `ObjectiveChanged`;
 - `UnitMoved`: the mover's team and every team that saw the mover on at least one square of the path; a team that saw
   only part of it gets the whole event and uses `seenSteps` to draw the part it saw (a unit walking into view appears
   at its first seen square);
 - `MoveBlocked`, `Attack`: the teams of both units and every team that sees either unit (a combat is shown in
   full, counters included);
 - `UnitDied`, `UnitRemoved`: the unit's team and the teams that saw it on its square; `UnitRelocated`: its team and
   the teams that see either square; `UnitBuilt`, `UnitAdded`, `UnitHealed`, `CaptureProgress`: the unit's team and the
   teams that see the unit; `ItemUsed`: teams that see the user or the target; `SideChanged`: both sides' teams and
   the teams that see the unit;
 - `StructureCaptured`: the new owner's team, the previous owner's team and every team that sees any square of it;
   `StructureUpgraded`: the owner's team and every team that sees any square of it;
 - one team's bookkeeping (`XpGained`, `LevelUp`, `StatsRaised`, `ItemBroke`, `ItemEquipped`, `ItemLost`,
   `UnitRepaired`, `UnitResupplied`): only the unit's own team; resources, items and research of a side
   (`ResourceChanged`, `ItemGained`, `ItemMoved`, `ResearchStarted`, `ResearchFinished`): only that side's team.
The state of a battle is always the truth and is already updated when the events are animated; what a human sees of
the stream is decided by the player from these masks.

**AI** (`ai/ai_knowledge.cpp`): it reads enemy units, structure owners and capture state only through one small set of
helpers (`knownEnemy`, `knownOwner`, `knownCapturable`, `enemyGoals`) that answer from its team's sight and memory; without fog
they answer from the truth, so decisions are bit-for-bit what they were (checked with the sim's verbose output).
Under fog: danger and exposure only count enemies it sees; capture targets are judged on the owner it last saw
(a structure it believes its own is not a target); what to build counts only the unit types of enemies in sight (with
none in sight every type counts as a possible target, which is data a human also has); advancing goes to the
enemy units in sight, else to the structures it last saw in enemy hands (the enemy HQ is one), else to the unseen square
farthest from everything it has. It never plans a move the rules refuse; a move that is ambushed ends like any
other (the unit's turn is over and the AI simply continues with the next unit). Tests move a hidden enemy
around out of sight and check that no decision changes.

**Saves** (docs/SAVES.md): `"seenOwners"`, the last seen owner per team and structure, only for fog maps. Sight is
recomputed on load.

**Tools**: `ote-sim <game> <map> --force-fog` plays a map with fog on whatever the file says (single maps only), and
prints `MoveBlocked` and, for events a team did not see, `[seen by team ...]`.

**Player** (step 4, built 2026-11-14; code `battle/battle_fog.cpp`, `battle_view_fog.cpp`, `ui/pass_device.*`, words in
`battle/fog_text.h`; test `tests/fog_player_test.py`).
- **The viewer** (`BattleView::setViewer`): the team of the human side whose turn it is; during AI turns the team of the
  human side that played last (at the start: the first human side). With fog off, with `--show-all` (the embed page's
  `showall=1`), with no human side (autoplay, all AI): no viewer, every picture is exactly what it was before fog.
  The log says `fog: viewer <side> (team N)`.
- **Drawing**, all four kinds of view and both 3D backends: squares the viewer does not see are dimmed to 55 % (top-down:
  a black veil of alpha 115 per square, under the capture numbers and the cursor; 2D isometric: the square's picture, its
  side faces and its letter are tinted; 3D: one blended black quad per unseen square plus the lower side faces, in the same
  batch as the move highlights, so no mesh is rebuilt and the shader / material interface is unchanged; structure pictures,
  boxes and models are tinted / `ModelTint.dim`). Units the viewer cannot see are not drawn (the view's `UnitView::visible`),
  so they are not hoverable and the info panel shows only the terrain and structure. Structures are drawn with
  `seenOwner`; the panel shows a structure's capture points only while the structure is in sight. A tall sprite of a unit
  can overhang a dimmed square above it in the top-down view (the veil is per square).
- **Sight follows the animation** for the viewer's own units: while events play, the dimmed squares are worked out from where
  its units are drawn (re-flooded when one changes square), when the view is idle from the Battle.
- **Events**: played only when `seen` has the viewer's bit; an unseen one changes the view's own copy at once, silently (no
  camera, banner, float, wait), so an AI turn nobody sees costs only the banners of the turn change (`TurnStarted` is
  public) and runs in a single frame (`BattleScene::aiStep` loops over unseen steps). A partly seen walk appears at the
  first seen square, walks to the last and vanishes when it goes on unseen (`seenSteps`; logged as `fog: <unit> seen on
  squares a..b of its n-square walk`). A seen fight shows both fighters, also a hidden attacker, until the run is over. A
  `MoveBlocked` shows an "Ambush!" float on the stopped unit, brings in the revealed enemy and the camera; with the default rule it ends the
  unit's turn (no action menu, no undo), with `"ambush": "canAct"` the action menu opens (see "Player" below). A `Focus` event on a square the viewer does not see is ignored. Nobody gets a
  banner about research, upgrades or resources of another team: those events are masked to their own team.
- **Hot-seat** (two or more human sides of different teams, fog on): before each human turn of another team than the
  last one shown, and when a battle starts or resumes on a human turn, the whole screen is covered with "Pass the device to
  <side>" (nothing of the map is drawn, input goes to the cover); Confirm or a tap lowers it and the map is drawn for that
  side. The Continue save made at a turn start resumes into the cover. Logged as `fog: cover up: ...` / `fog: cover down: ...`.
- Not hidden yet: the kind a structure was upgraded to (it is drawn as it is); there are no minimap, unit lists or objective
  markers in the player, so nothing else needed filtering.
Story events: unaffected (they act on the truth); dialogue always shows.

## Editor

- Structures: upgrades list, "research" checkbox (only with the rule), repairs list, vision
  bonus, hides.
- New **Research** screen (like Items): technologies, costs, turns,
  prerequisites, unlocks, bonus.
- Rules: Supply group (repair amount / cost share, structures resupply); **Research** group (shown when the game has
  technologies or the rule is set): "Research needs a structure"; **Fog of war** group (always): "When a unit walks into a
  hidden enemy: its turn ends / it can still act". Defaults are not written; an emptied block is removed.
- Structures: "Research can be done here" is shown only while the rule is on; otherwise one hint line with a link to the
  Rules screen (an existing `research: true` stays in the file). The Research screen's "no structure kind allows research
  yet" hint shows only while the rule is on. Checks (Rules screen) mirror the loader for both rules.
- Units: vision, fuel, fuel per turn, supplies. Weapons: ammunition.
  Terrain: vision bonus, hides.
- Map settings: Fog of war checkbox.
- Play panel: with fog, a "show everything" toggle for the maker.

## Player

- Structure menu (on an own structure, beside Build): **Upgrade**. Research is **not** in it any more (2026-11-14): a
  structure with only one thing opens it directly, a structure with nothing (an HQ) behaves like any structure without a
  menu (Confirm on it opens the battle menu).
- Battle menu (End Turn, Objective, **Research**, Save, Load, Leave battle, Cancel): **Research** is shown after Objective when
  the game has technologies and the side is human. It opens the research panel (list with cost, turns, what it gives; the one
  in progress with turns left); Cancel goes back to the battle menu. With `needsStructure` and no research structure it is
  still shown: the rows are greyed with "needs a structure that can research" and a countdown shows "paused: no research
  structure". Seven rows (with an objective and manual saves) fit the 480 x 270 screen; a game without technologies has
  exactly the menu it had. Log: `battle menu: ...`, `research reason: ...` (only when greyed for lack of a structure).
- Ambush: with `"ambush": "canAct"` and a human unit, the action menu opens on the stopped unit after the "Ambush!" cue, the
  hint line says `Stopped: an enemy was in the way. The unit can still act` and the menu hint `X:close (the move stays)`;
  Cancel only closes the menu (the move is not undone; the unit stays selectable and opens its action menu at once). With
  `endsTurn` nothing changes (`Stopped: an enemy was in the way`, no menu). Log: `ambushed: <unit> may still act from x,y`.
- Info panel: fuel and ammunition when the unit has them; research in
  progress under the side's resources.
- Fog: see "Fog of war > Player" above (dimmed unseen squares, hidden units not drawn, last seen owners, the event
  stream filtered by `Event::seen`, hot-seat cover). Built (2026-11-14).
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
3. Core: fog of war (the biggest: sight, per-team event visibility, AI). **Done.**
4. Player: menus and panel for 1 and 2; fog drawing in every kind of view. **Done.**
5. Editor: all screens (see docs/EDITOR_UI.md "Economy options"). `demo-economy` content: **done** (2026-11-14), placeholder
   numbers for the owner to judge:
   - City -> Town (2000 Funds, income 1000 -> 2000) -> Metropolis (4000 Funds, income 3000, needs the technology Urbanism).
   - The HQ researches (`"research": true`): Armor Doctrine (4000, 2 turns; unlocks the Tank), Reinforced Plating (5000, 2 turns,
     needs Armor; Tanks and APCs +20 HP), Urbanism (6000, 2 turns).
   - `rules.supply`: repair 20 % of max HP per turn at the full proportional price, own structures also refill fuel and
     ammunition. Fuel on Recon 80, Tank 70, Artillery 50, APC 70 (no `fuelPerTurn`: the demo has no aircraft or ships); ammunition
     on the Tank Gun (9) and the Howitzer (6). New unit type APC (5000 Funds, `supplies`, no weapon, so the AI never buys one).
   - Second map `misty_ford` (11 x 8, `"fog": true`, mirror-symmetric, river with two fords): forests hide, the two mountains
     by the river see 2 further, vision Recon 5, Tank 3, the rest 2. `twin_rivers` has no fog.
   - Art: starter pictures for Town, Metropolis and APC in `tools/art` (generated into the game, never hand-edited).
   - Tests that need "a game without the optional features" use the frozen copy `tests/fixtures/plain-economy`
     (save bytes and data hash pinned there); `tests/demo_economy_player_test.py` checks the demo in the player.
   ote-sim (deterministic: every seed plays the same): twin_rivers Red 30 of 30 (10 rounds; before the content 15), misty_ford
   Blue 30 of 30 (11 rounds); the AI researches all three, upgrades, repairs, resupplies, and walks into ambushes on the fog map.
