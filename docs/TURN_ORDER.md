# Speed turn order and charged actions

An optional alternative to "each side moves all its units, then the next side": **units act one at a time, in an order set by their speed**, and some actions
**take time to go off** (as in job-based tactics RPGs). Without the block everything plays, saves and simulates exactly as before (the five demo simulators and
their event streams are byte for byte the same). Tests: `tests/test_speed_clock.cpp`, `test_speed_charged.cpp`, `test_speed_save_ai.cpp`, `speed_order_player_test.py`,
`editor_api_speed_checks.py`; the fixture game is `tests/fixtures/speed-order`.

## Format

```json
"rules": { "turnOrder": { "mode": "speed", "stat": "spd", "threshold": 100, "preview": 8, "roundTicks": 10,
                          "ctCost": { "both": 100, "one": 80, "none": 60 } } }
```

`"turnOrder"` is `"sides"` (or missing: the usual game), `"speed"`, or an object with `"mode": "speed"` and:

| key | default | meaning |
|---|---|---|
| `stat` | the stat called `speed` | the stat every unit gains per tick of the clock (a stat id from `game.json` `stats`) |
| `threshold` | 100 | the charge at which a unit (or a charged action) acts (1 to 100000) |
| `preview` | 8 | how many entries of the order the player shows (1 to 30) |
| `roundTicks` | a tenth of `threshold`, at least 1 | a round passes every this many ticks |
| `ctCost` | `both` 100, `one` 80, `none` 60 | what a turn costs the unit's charge: it moved **and** acted / did one of the two / did neither (each at least 1) |

```json
// weapons.json                                   // abilities.json
{ "id": "fire", "range": [2, 5], "props": {...},  { "id": "hush", "target": "enemy", "range": [1, 4],
  "area": { "shape": "radius", "radius": 1 },       "inflict": { "condition": "sleep", "turns": 2 },
  "chargeSpeed": 25, "interruptedBy": ["sleep"] }   "chargeSpeed": 25, "chargeTargets": "cell" }
```

`"chargeSpeed"` (1 to 1000) makes a weapon or an ability a **charged action**; `"chargeTargets"` (`"unit"` or `"cell"`) says what it is aimed at; `"interruptedBy"`
is a list of condition ids. All three are **refused in a game without the speed rule** (`... need "rules": { "turnOrder": "speed" } in game.json`), and
`"chargeTargets"` / `"interruptedBy"` without `"chargeSpeed"`.

**Refused at load** (each names the file, the entry and the key): an unknown stat or key, a number out of range, a cost of 0, `actionPoints` together with
`speed` (`rules: "turnOrder": "speed" cannot be combined with "actionPoints"`), `production` together with `speed`, an unknown condition in `interruptedBy`,
a free charged ability (`needs "cost": "action"`), a charged `unlock` ability, `"chargeTargets": "cell"` on a `"self"` ability.

## The clock

Every living unit on the board has a **charge** (`Unit::ct`, 0 at the start). The battle advances in **ticks**: each tick every unit gains its speed stat
(`Battle::speedOf`: the stat as the rules see it, so items and skills count; 0 for a unit off the board and for a unit a condition **stops**: one that blocks
acting, like `cannotAct`). The clock stops when something is ready:

1. A **charged action** whose counter has reached the threshold goes off first, before any unit (several: the fuller counter, then the faster, then the one declared first).
2. Else the unit whose charge has reached the threshold has the turn. Several at once: **more charge, then more speed, then the lower unit id.** No randomness anywhere.

The unit that has the turn is `Battle::activeUnit` and `activeSide` is its side, so everything that asks "whose turn is it" keeps working. The one helper for "may this
unit act now" is `Battle::ownsTurn(unit)` (rules) and `Battle::mayActNow(unit)` (alive, its turn, not finished); `canAct` and the player use them. In a game that plays side by side
`ownsTurn` is `side == activeSide`, as ever.

**A turn** is the usual one: one move and one action, either order as today (`move again`, `act again` skills work). It ends when the player (or the computer) chooses
Wait / End turn, or **by itself** when the unit has done what ends its turn (an attack, an ability, a capture..., or a move after its action). Then the unit pays
`ctCost` by how much it used: it **moved and acted** (`both`), only one of the two (`one`), neither (`none`). A turn that a condition took away costs `one`. The
unit's "moved / done" marks are cleared when its turn ends, so a unit that waits looks like any other.

**The first turns:** all charges start at 0, so the first turn comes when the fastest unit has gained the threshold. A unit that joins later (a reinforcement, a summon)
starts at 0 as well.

**A battle nobody can continue** (every unit has speed 0 or is stopped for good) ends as a draw after 20000 idle ticks.

## Rounds

A **round** passes every `roundTicks` ticks of the clock (`Battle::round`, `clockTick` counts within it). Round 1 begins when the battle starts. What the usual game does when a
**side's** turn starts happens at the start of every round, once for every side still in the battle, in side order: torches burn down (`lights`), resource refills, income,
the research countdown, the story's `turn` events (`side`, `round`: a trigger for "round 3 of Blue" fires at the start of round 3). So objectives and story triggers that count rounds
work unchanged, and income / research are *per round*, not per unit turn. A `RoundStarted` event (`a` = the round) announces it.

What belongs to a **unit** happens in that unit's own turn: its conditions count down and tick (poison...), healing terrain, repair, supply and fuel, ability waits and summon
lifetimes, promotions, "turn start" skills, a berserk unit's own action, the computer's idle / retreat counters (when its turn begins); supports' "end of turn" and conditions that
run out (when it ends). A **stopped** unit never gets a turn, so its conditions would never count: they count once per round instead (a stop of 2 turns lasts 2 rounds).

## Charged actions

An ability or a weapon with a `chargeSpeed` is not carried out when it is chosen. The unit **declares** it (an `ActionCharging` event) and its turn ends (the clock charges
it as a turn that acted). The action sits in the order with its own counter, which **gains `chargeSpeed` per tick** and goes off when it reaches the threshold: `ChargeResolved`, then
the action's own events as usual. (`"chargeSpeed": 25` with a threshold of 100 = 4 ticks.) One pending action per unit.

- **What it is aimed at.** `"chargeTargets": "unit"`: the action **follows the unit** that was aimed at (it goes off at that unit's square then). `"cell"`: it hits **the square**, and
  whoever stands there then. The default: an area weapon, and an ability whose target is a `"cell"`, are aimed at a square; a single-target weapon or an ability on a unit follows the unit;
  a `"self"` ability follows its user. A unit "following" works on units only: an area weapon aimed at a square nobody stands on is a square.
- **When it goes off** it is judged again from where the caster stands then: out of range, or nobody there (area: no enemy in the blast), or the followed unit is gone, and it
  **fizzles** (`ChargeCancelled`, reason `kChargeFizzled`). An ability's use, cooldown and item are paid when it is declared (so a cancelled one is still spent); a weapon's ammunition when it goes off.
- **A charged weapon is never an instant answer:** it does not counter-attack, and the computer's normal scoring does not see it. It cannot be fired from a structure's map weapon.
- **Cancelled** (`ChargeCancelled`, `d` = reason): the caster is **defeated** or taken off the map (`kChargeDefeated`, `kChargeGone`), it gets one of the conditions listed in
  `interruptedBy` (`kChargeInterrupted`), it **gives it up** (`kChargeByCaster`), or it fizzles (`kChargeFizzled`).
- **A pending action can be attacked**: its caster can be hurt or killed before it goes off, and killing it cancels the action.
- **The caster's next turn before the action goes off:** it may **move** (and use its items, `Wait`) but **not act**: every other order is refused with
  `that unit is charging an action: it may only move, or give the charge up`. Or it **gives the charge up** (`ActionType::CancelCharge`, free) and acts normally.
- **Refresh** (a "dance"): the ability `{"refresh": true}` sets the target's charge to the threshold, so it acts as soon as the order reaches it (after the units that are ahead of it by
  the tie rules). It cannot target the unit that has the turn or one that is ready already.

## The preview

`Battle::turnPreview(count, out, extra)` runs the clock forward on a copy of the numbers, changing nothing, and returns the next `count` entries: the unit that has the turn first,
then units and pending actions (`TurnEntry::charged`). It assumes every unit uses its whole turn (cost `both`) and stays as it is (speed, conditions); with all three costs equal
it is **exactly** what then happens (a test plays both). `extra` adds an action the active unit would declare now: `Battle::chargeForecast(speed, count, &ticks)` says where
it would land. The player shows the first entries in a strip, and a dashed tag while a charged action is aimed.

## Interactions

| | |
|---|---|
| action points | **refused** with `speed` (one shared pool needs the side to act together) |
| production | **refused** with `speed` (a built unit has no place in one unit's turn) |
| economy, research, upgrades | work: income and the research countdown per round; research and upgrades are started with `StartResearch` / `UpgradeStructure` in a unit's turn of that side |
| facing | works (a charged weapon turns the caster to its aim when it is declared) |
| hot seat / several human sides | works: the acting side is the one of the unit that has the turn; the cover (fog) goes up when the turn passes to another team |
| fog | sight updates as today; an event about a unit is seen by the teams that see it; the strip shows `?` for an enemy the viewer cannot see |
| stopped units | their charge does not build; their conditions count in rounds |
| `refresh` | sets the target's charge to the threshold |
| `rules.ai.turnOrder` ("best" / "order") | has no meaning: the computer plays the one unit that has the turn |
| computer opponent | docs/AI.md "Speed turn order" |

## Events (all before `Choice`, which stays last)

`TurnStarted` (a = side, b = round) now comes when the turn **passes to another side** (the banner and the side's music), `UnitTurnBegan` (a = unit, b = round, c = its charge) at every unit's turn,
`RoundStarted`, `ActionCharging` (a = caster, b = 0 ability / 1 weapon, c = which, d = the square aimed at, data = {followed unit or -1, speed}), `ChargeResolved`, `ChargeCancelled`.

## Saves

Only in a speed game (docs/SAVES.md "Speed turn order"): each unit's `"ct"` and `"turnActed"` (when not 0 / false), and the battle's `"clock"`: `activeUnit`, `tick`, `turn`, `serial` and the
`pending` actions. A save in the middle of a charge round-trips. Inconsistent data is refused in the usual words (`battle.clock.pending[0].id: "x" is not a weapon of this game`).

## The player

- **The turn strip** along the top edge of the map: one name tag per entry (its side's colour; the unit that has the turn first, with a white double frame; the unit under the cursor underlined),
  a charged action on its way as an **orange-framed tag** with the action's name; as many entries as fit (`preview`, fewer on a big UI scale, where the names shorten to two letters).
- **Markers on the map** (through the overlay tones, so all four kinds of view paint them): the square of a unit that is charging **teal**, the squares its action will hit **magenta** (the whole blast of an area weapon).
- **The cursor and the camera jump to the unit** when its turn starts; only that unit can be picked (tapping another opens the map menu, whose "Unit" entry inspects it); the hint line says `end turn`.
  A charging unit's action menu is `Info`, `Cancel Charge`, `Wait`. The panel shows `Round N`, and `Charge` / `Casting` rows for a unit.

## Decisions you may want differently

- A turn's cost is by **how much of it was used** (100 / 80 / 60), not by what was done. Make all three equal for the plain "every turn costs the same" feel.
- Round = `threshold / 10` ticks: a unit of speed 10 acts about once per round. Income and research are per round, so a fast unit does not earn more.
- A charging unit may move but not act; it may give the charge up. (Alternative: let it act and lose the charge.)
- A declaration always ends the turn, and an ability's use is spent even if the action is cancelled.
- A stopped unit's conditions count in rounds, not in its (non-existent) turns.
- The computer does not use charged *abilities* at all; for weapons see docs/AI.md.
