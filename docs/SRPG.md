# Strategy-RPG features

Contract for PLAN item 3k: characters, promotion, skills, menu abilities and summoning, rescue / pair up, and
supports. Generic engine features every game can use or ignore. Owner decisions (2026-11-15):

- **Skills** are passive bonuses and special effects. They come from the class (unit type), from the **character**
  (characters become their own thing: personal stats and growths on top of the class), from weapons and items, and
  are learned at a level. Makers build skills from **blocks** (when it applies / a condition / what it does); new
  games get a **starter set** made of those blocks, which makers can use, change or delete.
- **Menu abilities** (push a neighbour one square, swap places, pull back, summon, and similar): the maker chooses
  **per ability** whether it uses the unit's action or is free, and optionally a number of uses per battle or a wait
  of some turns.
- **Summoning**: per summon ability the maker sets how many can exist at once (default one), whether they vanish
  when the summoner is defeated (default yes) and an optional lifetime in turns. Summoned units are controlled like
  any other unit.
- **Supports**: closeness between two characters grows from ending turns side by side, from fighting together and
  from story events; the maker can use any of these. Reaching a level unlocks a conversation and a bonus when the two
  are near each other. Conversations are played in battle, between chapters, or both: chosen per game.
- **Rescue**: both the classic carry (Rescue / Drop / Give / Take) and **pair up** (two units share a square; the one
  behind gives bonuses and can swap in). Each game picks one, or neither.
- **Promotion**: a unit changes to a stronger class and keeps its identity. Triggers: a promotion item (usable from
  a set level), reaching a level (automatic or offered), a story command, and a **choice** when a class has several
  promotions.
- Close-up battle scenes stay low priority and are **not** part of this contract.
- No content, names or terms distinctive to any commercial game or fan project. Everything below uses generic
  vocabulary and our own neutral names.

Everything below those points is engine design and may change; all wording shown to players and makers is
placeholder until the owner approves it. "Interpretations" and "Open questions for the owner" near the end list
where the decisions had to be extended and what is still the owner's to choose.

Status: nothing is built. The "Build order" at the end splits the work into steps for separate people; each step
appends an "as built" note under its own heading when it lands.

Words used here: a **unit type** is what `data/units.json` defines (the maker may think of it as a class); a
**character** is a named person who is of a unit type at any moment; **foe** is the other fighter of a combat;
**on the board** means a living unit standing on a square.

## Principles

- **Optional and inert.** Every file, key and rule here is optional. A game that uses none of them loads, plays,
  saves and hashes exactly as today: same events, same random numbers, same save bytes, same `dataHash` (the pins in
  `tests/test_campaign_compat.cpp` and the frozen `tests/fixtures/plain-economy` game must not move). A feature a
  game does not use writes no key into its saves.
- **Data, not scripts.** Skills, abilities, supports and promotions are data edited as small forms. Nothing here
  needs Lua. Where a number or a yes/no has to be computed, the existing formula language is reused (the same
  expressions as `formulas.damage`, with the same variables), so functions from `scripts/*.lua` can be called too.
- **Deterministic.** All randomness comes from `Battle::rng`, in an order this document fixes. Queries (forecasts,
  menus) never consume randomness. A battle replays identically from its seed and actions.
- **Small vocabularies that combine.** One list of conditions serves skills and abilities; one bag of combat
  modifiers (`CombatMods`) collects what skills, supports and auras give; one "not on the board" state serves carry
  and pair up.
- **Grid neutral.** Everything positional goes through `Grid` (`neighbors`, `neighbor`, `directionTo`, `distance`,
  `within`), so it works on squares and hexagons alike. "Square" below also means "hexagon".
- **The truth decides, sight limits choices** (as docs/ECONOMY.md "Fog of war"): rules act on the real state; what
  a menu offers is limited to what the acting team sees; an order that runs into something unseen fails the way a
  blocked move does.

## Files at a glance

```
game.json              rules.rescue, rules.promotion                         (optional blocks)
data/characters.json   characters                                            (optional file)
data/skills.json       skills made of blocks                                 (optional file)
data/abilities.json    menu abilities, summons                               (optional file)
data/supports.json     support rules, pairs, levels, conversations           (optional file; needs characters)
data/units.json        per unit type: skills, learns, abilities, promotions, pairBonus
data/weapons.json      per weapon: skills
data/items.json        accessory: skills; usable: promotes
maps/*.json            per unit: character; new story commands, conditions and triggers
data/campaign.json     new scene commands and conditions
```

Ids follow the existing rule (lowercase strings unique within their list). Character ids are also story ids
(letters, digits, `_`, `-`, up to 64). The four new files and every new sub-object **refuse unknown keys**, like the
story and research loaders do; the new keys on unit types, weapons and items are read like their neighbours (those
files do not refuse unknown keys today and that does not change).

**Load order** (`loader.cpp`): `game.json`, then `skills.json` and `abilities.json` (ids and shapes only), then
weapons, items, units as today, then `characters.json`, research, structures, graphics, campaign, `supports.json`,
and last one pass `resolveSrpgRefs` that turns every cross reference between the new data into indices and checks it
(a skill's weapon, terrain and unit keys; a skill's `ability`; an ability's summoned unit type; a promotion's `to`
and `items`; a learn entry's skill). So the files may refer to each other freely.

**Limits** (loader refusals name them): 500 characters, 500 skills, 8 blocks per skill, 8 conditions and 8 effects
per block, 200 abilities, 500 support pairs, 5 support levels, 200 lines per conversation, 8 promotions per unit type.

## Formulas in this document

Wherever a value below is called a **formula** it is a Lua expression in the existing formula language
(docs/GAME_FORMAT.md "Formulas"), compiled once when the game loads, with the usual seven variables. What they hold
is stated where the formula is used; the general rule is `a` = the unit the rule is about, `b` = the other unit
(`nil` when there is none), `w` = the numbers of `a`'s weapon (empty without one), `ta` / `tb` = the numbers of the
squares under them, `dist` = grid distance (0 without `b`), `vs` = the weapon's versus value against `b` (1 without).
A unit table gains one field: `character` (the character id, `nil` for a unit without one). A formula that fails at
run time counts as 0 / false and is logged once, like the others. A syntax error is a load error naming the place:
`data/skills.json: skill "heavy_blow": block 1: "chance": <Lua message>`.

Unit stats inside every formula are the stats **as the rules see them** (`Battle::stat`): own value, accessories,
research, standing skill bonuses, carry penalty and pair bonus; inside a combat also the combat bonuses to stats
(see "Combat pipeline").

Today's code cannot do this: `Formulas` compiles a fixed set of slots (`formulas.h`, `enum Slot`) and builds unit
tables from `unitStat()` without a battle (`formulas.cpp`, `pushUnit`). Step F1 adds a general list of expressions
(`Database::exprs`, `Formulas::evalExpr`) and gives `FormulaCtx` a `battle` pointer and two per-stat bonus lists; see
"Build order".

---

## 1. Characters

A character is a named person: personal stats and growths on top of the unit type's, personal skills, and an
identity that events, the campaign roster and supports use.

### Data: data/characters.json

```jsonc
{ "characters": [
  { "id": "eirik", "name": "Eirik",
    "description": "Captain of the Blue Company.",   // optional, one line (wording)
    "type": "lord",                       // required: the unit type the character starts as
    "level": 3,                           // optional, default 1: the level it joins at
    "stats":   { "hp": 4, "str": 1, "def": 1 },   // optional: ADDED to the unit type's base stats
    "growths": { "hp": 10, "str": 5 },            // optional: ADDED to the unit type's growths (% points)
    "skills": ["quick_study"],                    // optional: personal skills (section 3)
    "learns": [ { "level": 8, "skill": "heavy_blow" } ],   // optional: personal skills learned at a level
    "abilities": ["shove"],                       // optional: personal menu abilities (section 4)
    "items": ["iron_sword", "vulnerary"] }        // optional: replaces the unit type's starting items
] }
```

- `stats` and `growths` may be negative (a frail but gifted character). A unit's stat never goes below 0 and a
  growth below 0 counts as 0.
- `items` uses the entries of docs/ITEMS.md (id, or `{ "item", "uses", "drop" }`), at most `inventorySize`.
- Refusals (all start `data/characters.json: character "<id>": `): `duplicate id`; `"id" must be letters, digits, "_"
  and "-"`; `unknown unit type "<x>"`; `"level" must be from 1 to <rules.progression.maxLevel>`; `"stats" names unknown
  stat "<x>"`; `"skills" names unknown skill "<x>" (data/skills.json)`; `"learns" entry 2: "level" must be from 2 to
  <maxLevel>`; `"abilities" names unknown ability "<x>" (data/abilities.json)`; unknown key: `unknown key "<x>"`.
- Core: `CharacterDef`, `Database::characters`, `Database::findCharacter(id)`.

### Referring to a character

A map unit (and the `addUnit` story command) may say `"character": "<id>"` instead of describing the person:

```jsonc
{ "character": "eirik", "side": "blue", "x": 2, "y": 4, "leader": true }
```

- `type` becomes optional: missing = the character's `type`; given = the unit type it has on **this** map (an enemy
  met again in another class).
- `name` is optional (default: the character's name); `level` is optional (default: the character's `level`).
- `id` must be left out: the unit's story id **is** the character id. Refused: `units[0] (…): a unit with a
  "character" is known to events by the character's id; remove "id"`.
- A plain unit may not use a character's id as its `id`: `units[3] (…): "id" "eirik" is a character
  (data/characters.json); write "character": "eirik"`.
- A character can be on a map once (the existing story id uniqueness check, with the message `"character" "eirik" is
  already placed by units[0]`). An `addUnit` of a character that is already in the battle adds nothing (logged).
- Unknown character: `units[2]: unknown character "x" (data/characters.json)`; without the file: `"character" is
  given but the game has no characters (data/characters.json)`.

### Rules

- **Starting stats.** `stat = unit type base + character's "stats"`, then the map unit's own `"stats"` replace single
  values as today. Levels are not simulated: a character placed at level 5 has exactly the stats the data says (as
  for every unit today).
- **Items.** The map unit's `items`, else the character's `items`, else the unit type's.
- **Growths.** On a level up the chance for stat `s` is `unit type growth + character growth + growth bonuses of
  standing skills` (section 3), below 0 = 0. `Battle::growthOf(unit, stat)` is the one place this lives;
  `progression.cpp` reads it instead of `type.growths`. The random rolls are unchanged in number and order (one roll
  per stat whose growth has a remainder), so a game without characters levels exactly as before.
- **Identity.** `Unit::character` (index, -1 = none) is derived from the story id when the unit is created, dressed
  from a roster or restored from a save (`db.findCharacter(storyId)`); it is never saved. The campaign roster already
  identifies a unit by its story id (`RosterUnit::id`), so a character is carried between chapters as today: unit
  type, name, level, xp, stats, items. A roster entry dresses the unit on a human side exactly as today (the roster
  wins over the character file for everything it stores); a character that is not in the roster yet starts as the
  file and the map say.
- **Portraits and art.** Dialogue portraits are looked up by story id (docs/EVENTS.md), which is the character id:
  nothing new. Unit pictures stay per unit type (docs/GRAPHICS.md).
- A unit built by production or summoned never has a character.

### Story

- Unit filters need nothing new: `{ "unit": "eirik" }` (or the plain string) names a character.
- Scenes' `speaker` (a roster id) is a character id for characters.

### Saves and hash

Nothing is added to a battle save or a campaign save for characters. `data/characters.json` is mixed into the data
hash only when the file exists (after `research.json`; `name` and `description` are already cosmetic keys).

---

## 2. Promotion

A unit changes to another unit type and stays the same unit: same id, name, character, side, square, items,
learned skills, supports.

### Data

```jsonc
// game.json
"rules": { "promotion": { "levelAfter": "reset" } }   // "reset" (default): level 1, xp 0 after promoting; "keep"

// data/units.json, per unit type
"promotions": [
  { "to": "captain",                  // required: the unit type it becomes
    "fromLevel": 10,                  // optional, default 1: the lowest level at which it can happen
    "items": ["officers_writ"],       // optional: promotion items that trigger it
    "atLevel": "offer",               // optional: "off" (default) | "offer" | "auto", see below
    "levelAfter": "keep",             // optional: overrides rules.promotion.levelAfter for this promotion
    "gains": { "hp": 3, "str": 2 } }  // optional: stat changes; missing = the difference of the two types' base stats
]

// data/items.json: a promotion item is a usable item
{ "id": "officers_writ", "name": "Officer's Writ", "kind": "usable", "uses": 1, "promotes": true }
```

- `promotes: true` makes a usable item valid without `heal` / `raise` (the items loader's rule becomes "a usable item
  needs `heal`, `raise` and/or `promotes`"). It may have `heal` / `raise` as well; they apply first.
- Refusals: `data/units.json: unit "lord": "promotions" entry 1: unknown unit type "x"`; `… "to" is the unit type
  itself`; `… "fromLevel" must be from 1 to <maxLevel>`; `… "atLevel" must be "off", "offer" or "auto"`; `… "atLevel"
  needs experience and levels (rules.progression)`; `… "items" names "x", which is not a promotion item (it needs
  "promotes": true in data/items.json)`; `… unknown key "x"`; `game.json: rules.promotion: "levelAfter" must be
  "reset" or "keep"`; `data/items.json: item "x": "promotes" belongs to items of kind "usable"`.
- Core: `PromotionDef`, `UnitTypeDef::promotions`, `ItemDef::promotes`, `RulesConfig::promotion`.

### What promoting does (`Battle::promote`, one function for every trigger)

1. `type` becomes `to`.
2. Each stat changes by `gains` (or by `to.stats[s] - from.stats[s]`); a stat stops at 0. Current HP rises by a
   positive HP gain and is then kept within 1..maximum.
3. Level: `reset` = level 1 and xp 0; `keep` = both stay.
4. Fuel and ammunition are set as for a new unit of the new type (`stockUnit`).
5. Skills: the old type's own `skills` go, the new type's apply; **learned skills stay**; the new type's `learns`
   entries at or below the unit's new level are learned at once (section 3 "Learning").
6. Items stay. A carried weapon the new type cannot wield stays in the inventory and simply cannot be equipped (the
   existing "first wieldable weapon is in hand" rule; a unit with none is unarmed).
7. An unfinished capture is kept (the unit did not move). Turn flags are not touched by the promotion itself.
8. Event `UnitPromoted`, then story events waiting for `promoted` (below).

**Where it can happen.** The unit must be able to stand where it is as the new type (`enterCost(cell, new move type)
> 0`); otherwise the promotion is refused (`a <Captain> could not stand on this square`), or, for the automatic
kinds, postponed.

### Triggers

| Trigger | How | Cost |
|---|---|---|
| Item | `UseItem(unit, slot, unit)` with a `promotes` item, on itself. Valid when the unit's type has a promotion that lists the item and the unit's level is at least its `fromLevel`. `Action::unitType` picks the promotion's `to`; -1 = the first that fits, in listed order. Spends one use | ends the unit's turn (as every used item) |
| Offered at a level | `atLevel: "offer"`: from the moment the unit's level is at least `fromLevel` the action **Promote** is available: `Action::promote(unit, to)` (`ActionType::Promote`, `unitType` = `to` or -1 = the first offered) | ends the unit's turn |
| Automatic at a level | `atLevel: "auto"`: the unit promotes by itself right after the `LevelUp` event that brings it to `fromLevel` or above. If it cannot stand there as the new type, it is tried again at the start of each of its side's turns | nothing |
| Story | command `{ "promote": <unit id>, "to": "captain" }` | nothing |

- With several promotions that fit, the acting player chooses (`Action::unitType`); automatic promotion takes the
  first `auto` entry that fits, in listed order.
- Query: `Battle::promotionOptions(unit, slot, out)`: the unit types the unit could become now, through the item in
  `slot`, or (`slot` = -1) through an offer. Empty = no Promote entry.
- A unit without `rules.progression` can still promote by item and by story.

### Story

- Command `{ "promote": "eirik", "to": "captain" }`: `to` may be **any** unit type (a story may change a class
  freely); without `to` the unit's type must have exactly one promotion (else a load error: `"promote" needs "to":
  "lord" has 2 promotions`). When the pair is not in the type's `promotions`, the gains are the base difference and
  the level follows `rules.promotion.levelAfter`. A unit that is gone, not on the board, or cannot stand there as
  the new type: nothing happens (logged).
- Trigger `{ "promoted": <filter> }`: fires after a matching unit promoted, by any trigger.
- Not in scenes (scenes know no units on a map); a campaign promotes at a battle's start with a `battle: start`
  event.

### Saves, events

Nothing new is saved (type, stats, level and xp are saved already; the roster carries the new type). Event
`UnitPromoted`: a = unit, b = old type, c = new type, d = new level, data = change per stat index.

---

## 3. Skills

A skill is a named list of **blocks**. A block says **when** it applies, an optional list of **conditions** (all
must hold), an optional **chance**, and what it **does** (a list of effects).

### Data: data/skills.json

```jsonc
{ "skills": [
  { "id": "back_to_the_wall", "name": "Back to the Wall",
    "description": "Critical +25 while at half HP or less.",      // optional, one line (wording)
    "blocks": [
      { "when": "combat",
        "if": [ { "hp": { "op": "<=", "percent": 50 } } ],
        "do": [ { "crit": 25 } ] } ] },
  { "id": "heavy_blow", "name": "Heavy Blow",
    "blocks": [
      { "when": "strike", "chance": "a.skl", "do": [ { "damagePercent": 200 } ] } ] }
] }
```

Block keys: `when` (required), `if` (optional list), `chance` (optional), `aura` (optional, combat blocks only),
`do` (required, at least one effect). Each condition and each effect is an object with **exactly one** kind key
plus that kind's own parameters.

### Where skills come from

| Source | Key | Applies |
|---|---|---|
| Unit type | units.json `"skills": ["id"]` | while the unit is of that type |
| Unit type, at a level | units.json `"learns": [ { "level": 5, "skill": "id" } ]` | learned for good (below) |
| Character | characters.json `"skills"`, `"learns"` | always / learned for good |
| Accessory | items.json `"skills": ["id"]` on an item of kind `accessory` (the loader's rule becomes "an accessory needs a `bonus` and/or `skills`"; on another kind: `"skills" belongs to items of kind "accessory"`) | while carried |
| Weapon | weapons.json `"skills": ["id"]` | while it is the weapon in use: in a combat the weapon the unit fights with; outside a combat the equipped weapon (carried weapons) or the type's first weapon (fixed weapons) |
| Story | command `teachSkill` | learned for good |

**A unit's skill list** (`Battle::skillsOf(unit, weapon, out)`), in this order: the type's `skills`; the skills it
has learned, oldest first; the character's `skills`; accessories in inventory order; the weapon in use. A skill
that appears twice counts **once**, at its first place. This order is the order in which blocks are judged, modifiers
are added up and chances are rolled.

**Learning.** `Unit::learned` is the list of skills a unit has learned for good (state, saved). When a unit is
created it has every `learns` entry of its type and its character at or below its level. On each `LevelUp` it learns
the entries of its type and its character for exactly the new level (event `SkillLearned`). A promotion keeps the
list and adds the new type's entries at or below the new level. A skill already in the list is not added twice. The
campaign roster carries the list.

### WHEN: the seven points

| `when` | The block is judged | Has a foe |
|---|---|---|
| `always` | whenever the unit's stats or movement are asked for (a standing bonus) | no |
| `turnStart` | at the start of each turn of the unit's side, after repair and supply, before the story's turn events | no |
| `combat` | for a combat the unit takes part in, before the first strike and again before every strike; shown in the forecast | yes |
| `strike` | each time the unit is about to strike | yes |
| `struck` | each time the unit is about to be struck | yes |
| `defeat` | after a combat in which the unit defeated its foe | yes (fallen) |
| `afterCombat` | after a combat the unit took part in and survived | yes (may be fallen) |

### CONDITIONS (`if`)

Every condition is about the skill's owner unless it says `"who": "foe"`, which is allowed only where there is a
foe. `op` is one of `= != < <= > >=` (`==` accepted), default `>=`.

| Condition | True when |
|---|---|
| `{ "hp": { "op": "<=", "percent": 50 } }` | the unit's HP compared with that share of its maximum (0..100), computed as `hp * 100 <op> percent * maxhp` |
| `{ "near": "ally", "within": 1, "op": ">=", "count": 1, "match": "knight" }` | counts other units on the board within `within` steps (1..10, default 1): `"ally"` = same team, `"enemy"` = another team **and seen by the unit's team**; `match` (optional) = a unit type id or tag. `count` default 1. "Nobody next to me" = `"op": "=", "count": 0`. Owner only |
| `{ "weapon": "sword" }` | the unit's weapon in use is that weapon id or has that `group` |
| `{ "terrain": "forest" }` | the square the unit stands on (in a forecast: would stand on) is that terrain id or has that terrain tag |
| `{ "structure": true }` / `{ "structure": "fort" }` | the unit stands on a structure / on one with that id or tag |
| `{ "unit": "armored" }` | the unit's type has that id or tag (`unitTypeMatchesKey`) |
| `{ "carrying": true }` | the unit carries someone / has a unit behind it (section 5); `false` = it does not |
| `{ "round": { "op": ">=", "value": 5 } }` | the battle's round number |
| `{ "initiating": true }` | the unit started this combat (`false` = it is defending). Foe points only |
| `{ "distance": { "op": "=", "value": 1 } }` | grid distance between the two fighters. Foe points only |
| `{ "formula": "a.level >= 10 and b.tags.flier" }` | the formula is true or non-zero: `a` = the owner, `b` = the foe. Not in `always` blocks |

Refused in `always` blocks: `formula`, `initiating`, `distance`, `"who": "foe"` (there is no combat). A block that
changes the **HP stat** in `always` may have no condition at all (a maximum that flickers would cut current HP):
`skill "x": block 1: a bonus to "hp" cannot have a condition`.

### CHANCE (activation)

`"chance": 30` or `"chance": "a.skl * 2"` (a formula: `a` = owner, `b` = foe): a percent, rounded down, kept within
0..100. Allowed on `strike`, `struck`, `defeat`, `afterCombat` and `turnStart` blocks; refused on `always` and
`combat` (those must be the same every time they are asked). A block with a chance is an **activation**: it is
rolled with `Battle::rng` at the moment it is judged, only if its conditions hold, and only when the chance is from
1 to 99 (0 or less never applies and 100 or more always applies, **neither consumes a random number**, like the hit
roll today).

### EFFECTS (`do`)

`to` chooses who receives a numeric effect where the table allows it; default the owner.

| Effect | Meaning | Allowed in |
|---|---|---|
| `{ "stats": { "str": 2, "def": -1 } }` | added to stats | `always`; `combat` (also `"to": "foe"`; not the HP or move stat) |
| `{ "growths": { "str": 10 } }` | added to growths (% points) | `always`, no conditions |
| `{ "maxMoveCost": 1 }` | every square the unit can enter costs it at most this many movement points | `always` |
| `{ "pass": "enemies" }` | may move through enemy units (never stop on them) | `always` |
| `{ "moveAgain": true }` | after an action that ends its turn (anything but Wait) the unit may still move with the movement points it has not used | `always` |
| `{ "xpPercent": 150 }` | XP the unit gains is multiplied (1..1000) | `always` |
| `{ "ability": "shove" }` | the unit has that menu ability (section 4) | `always`, no conditions |
| `{ "hit": 15 }`, `{ "crit": 10 }`, `{ "damage": 3 }` | added to the unit's own hit %, crit %, damage per strike | `combat` (also `"to": "foe"`), `strike` |
| `{ "avoid": 10 }`, `{ "critAvoid": 10 }`, `{ "damageTaken": -2 }` | taken from the foe's hit % and crit %; added to the damage of each strike the unit takes | `combat` (also `"to": "foe"`), `struck` |
| `{ "damagePercent": 150 }` | the unit's strike damage is multiplied (0..1000) | `combat`, `strike` |
| `{ "damageTakenPercent": 50 }` | damage of strikes the unit takes is multiplied (0 = none) | `combat`, `struck` |
| `{ "strikes": 1 }` | added to the unit's number of strikes (-7..7) | `combat` (also `"to": "foe"`) |
| `{ "multiStrike": 2 }` | each of the unit's strikes becomes this many in a row (2..4) | `combat` |
| `{ "strikeFirst": true }` | when defending, the unit strikes before the attacker in each exchange | `combat` |
| `{ "counterAnyRange": true }` | the unit's counterattack ignores its weapon's range | `combat` |
| `{ "noCounter": true }` | the foe does not counterattack | `combat` |
| `{ "drain": 50 }` | the unit heals that percent of the damage this strike dealt | `strike` |
| `{ "strikeAgain": true }` | one more strike straight after this one (the extra strike cannot give another) | `strike` |
| `{ "survive": true }` | a strike that would defeat the unit leaves it at 1 HP, if it had more than 1 HP | `struck` |
| `{ "heal": 5 }` / `{ "heal": { "percent": 20 } }` | restores HP (percent of the receiver's maximum, at least 1). `"to"`: `"self"` (default) or `"allies"` with `"within": 1..10` (other units of its team on the board that close) | `turnStart`, `defeat`, `afterCombat` |
| `{ "hurt": 5 }` | the receiver loses HP but never falls below 1. `"to"`: `"foe"` (`afterCombat`, a foe still alive) or `"enemies"` with `"within"` (`turnStart`; enemies the unit's team sees) | `afterCombat`, `turnStart` |
| `{ "actAgain": true }` | the unit may move and act again this turn. At most once per unit per turn | `defeat`, `afterCombat` (only on its own side's turn) |

An effect in a block whose `when` the table does not list is refused: `skill "x": block 2: "drain" cannot be used in
a "combat" block (it belongs to: strike)`.

**Aura.** A `combat` block may have `"aura": { "to": "allies", "within": 2 }` (`"allies"` or `"enemies"`, 1..10):
its effects then go to every unit of that kind within that distance of the owner **that is fighting**, not to the
owner. Its conditions are judged for the owner (no `"who": "foe"`, `initiating` or `distance`); its effects are
limited to `stats`, `hit`, `avoid`, `crit`, `critAvoid`, `damage`, `damageTaken`, and may not use `to`. The owner must
be on the board; `"enemies"` reaches a fighter only if the owner's team sees it.

### Evaluation and stacking

- **Standing bonuses** (`always`): `Battle::stat(unit, s)` = own stat + accessories + research (as today) + the sum of
  `stats` effects of the unit's `always` blocks whose conditions hold + carry penalty / pair bonus (section 5), not
  below 0. For the HP stat only blocks without conditions are looked at (so judging an `hp` condition never asks for
  itself). Everything reads stats through `Battle::stat`: formulas, maximum HP, movement, the info panel, saves'
  HP check.
- **Movement**: `reachable` uses `min(cost, maxMoveCost)` for squares that can be entered at all (the lowest
  `maxMoveCost` wins), and with `pass` does not treat seen enemies as walls (a square with an enemy is still never
  a stop).
- **Move again**: see "Turn flags" below.
- **Adding up**: numbers add; percents multiply one after another in skill-list order with whole-number division
  (`d = d * p / 100` each); yes/no effects are "any"; `multiStrike` takes the highest; `xpPercent` multiplies.
- **Combat**: see "Combat pipeline" (shared with supports).
- **Timed blocks**:
  - `turnStart`: units of the active side in id order, each one's blocks in list order; effects apply at once
    (`UnitHealed` for heals, `UnitHurt` for `hurt`).
  - After a combat (`applyAttack`, after the existing XP awards): first the attacker, then the defender, each if
    still on the board: its `defeat` blocks if its foe fell in this combat, then its `afterCombat` blocks.
  - `actAgain`: the unit's `moved`, `done`, `acted` and `moveSpent` are cleared and `Unit::refreshed` is set (it
    blocks a second `actAgain` until the side's next turn).
- **Events**: `SkillActivated` (a = unit, b = skill, c = the foe or -1) is emitted when an activation (a block with a
  chance) applies, and when a `turnStart`, `defeat` or `afterCombat` block without a chance changes something. For a
  `strike` activation it comes just before the strike's `Attack` event (also when the strike then misses); for a
  `struck` activation just before the `Attack` event, and only when the strike hits. `always` and `combat` blocks
  are passive and make no event.

### Turn flags for "move again" and "act again"

New unit state, all cleared at the start of the side's turn with `moved` and `done`:

- `Unit::moveSpent`: movement points spent on moves this turn (the `reach.cost` of each move made).
- `Unit::acted`: the unit has used its action and only a move is left.
- `Unit::refreshed`: `actAgain` has been used this turn.

Every action that today sets `done` (Attack, Capture, UseItem, Supply, a Talk that ends the turn, and the new
turn-ending actions of this document) goes through one function, `Battle::endOrder(unit)`: if the unit has
`moveAgain`, is on the board and `moveSpent < movePoints`, it sets `acted` instead of `done`; otherwise `done`. Wait
sets `done` directly. A unit with `acted` may only **Move** (budget `movePoints - moveSpent`, any other rule of Move
unchanged; `reachable` applies the budget) or **Wait**; after that move it is `done`. `validate` refuses anything
else: `that unit can only move or wait now`. An ambush with `rules.fog.ambush: endsTurn` sets `done` as today.

### Forecast

`Forecast` (the query `Battle::forecast`) already carries damage, hit, crit and strikes per side. It now includes
every `combat` block (own, foe's `to: foe`, auras, supports) and every `strike` / `struck` block **without** a
chance, judged on the state before the fight. It gains: `StrikeForecast::multi` (strikes in a row), `Forecast::
defenderFirst`, and `Forecast::notes`: one entry per activation that could fire (`unit`, `skill`, `chance` in
percent, judged now), in roll order. Activations are **not** folded into the numbers. The AI scores attacks from the
same forecast, so it values passive skills and ignores activations.

### Story

- Commands `{ "teachSkill": "heavy_blow", "to": <filter> }` (every matching unit on the board learns it; event
  `SkillLearned`) and `{ "forgetSkill": "heavy_blow", "from": <filter> }` (removed from the learned lists; a skill
  the unit has from another source stays).
- Condition `{ "hasSkill": "heavy_blow", "unit": <filter> }` (`unit` default any): a matching living unit has the
  skill in its skill list.

### Saves and hash

- Battle save, per unit, only when not empty / not zero: `"skills": [skill indices]` (the learned list, in order;
  each a valid index, no repeats), and, **only in a game where some skill has `moveAgain` or `actAgain`**:
  `"moveSpent": n` (when > 0), `"acted": true`, `"refreshed": true` (when true). A key in a game without skills (or
  without those effects) is refused: `units[3].skills: this game has no skills`.
- Roster entry: `"skills": [skill indices]` only when not empty.
- `data/skills.json` is mixed into the data hash only when present (`name` and `description` are cosmetic keys
  already). New keys on units, weapons and items are part of files that are hashed whole.

### The starter set

Thirty skills that new strategy-RPG games get (`editor/templates/strategy-rpg/data/skills.json`) and that the editor
can install into any game (section "Editor"). They are plain data in the blocks above; makers may change or delete
them. Names are our own descriptive placeholders. `ATK`, `SKL`, `SPD`, `DEF` stand for the stats the maker points
them at when installing (the template uses `str`, `skl`, `spd`, `def`); a skill whose stat the maker leaves unset is
not installed.

| # | id / name | Blocks |
|---|---|---|
| 1 | `tough` Tough | always: stats HP +5 |
| 2 | `light_step` Light Step | always: stats Move +1 |
| 3 | `sure_footing` Sure Footing | always: maxMoveCost 1 |
| 4 | `slip_through` Slip Through | always: pass enemies |
| 5 | `strike_and_move` Strike and Move | always: moveAgain |
| 6 | `fast_learner` Fast Learner | always: xpPercent 150 |
| 7 | `gifted` Gifted | always: growths ATK +10, SKL +10, SPD +10, DEF +10 |
| 8 | `opening_blow` Opening Blow | combat, if initiating: damage +3 |
| 9 | `hold_the_line` Hold the Line | combat, if not initiating: damageTaken -3 |
| 10 | `back_to_the_wall` Back to the Wall | combat, if hp <= 50%: crit +25 |
| 11 | `on_my_own` On My Own | combat, if near ally within 2 count = 0: damage +3, hit +10 |
| 12 | `close_fighter` Close Fighter | combat, if distance = 1: avoid +15 |
| 13 | `steady_aim` Steady Aim | combat, if distance >= 2: hit +20 |
| 14 | `beat_them_to_it` Beat Them to It | combat, if not initiating and hp <= 50%: strikeFirst |
| 15 | `double_strike` Double Strike | combat, if initiating: multiStrike 2 |
| 16 | `always_ready` Always Ready | combat: counterAnyRange |
| 17 | `clean_hit` Clean Hit | combat, if initiating and hp = 100%: noCounter |
| 18 | `braced` Braced | combat, if not initiating: strikes -1 to foe |
| 19 | `rallying_presence` Rallying Presence | combat, aura allies within 2: hit +10, avoid +10 |
| 20 | `unnerving_presence` Unnerving Presence | combat, aura enemies within 2: avoid -10 |
| 21 | `heavy_blow` Heavy Blow | strike, chance `a.SKL`: damagePercent 200 |
| 22 | `draining_blow` Draining Blow | strike, chance `a.SKL`: drain 50 |
| 23 | `extra_swing` Extra Swing | strike, chance `a.SPD`: strikeAgain |
| 24 | `glancing_hit` Glancing Hit | struck, chance `a.DEF * 2`: damageTakenPercent 50 |
| 25 | `hang_on` Hang On | struck, if hp >= 50%: survive |
| 26 | `second_charge` Second Charge | defeat, if initiating: actAgain |
| 27 | `catch_breath` Catch Breath | defeat: heal 25% |
| 28 | `parting_jab` Parting Jab | afterCombat, if initiating: hurt 5 to foe |
| 29 | `steady_recovery` Steady Recovery | turnStart: heal 10% |
| 30 | `caring_presence` Caring Presence | turnStart: heal 5 to allies within 1 |

Between them they use every `when`, every condition except `weapon`, `terrain`, `structure`, `unit`, `carrying`,
`round` and `formula` (those need ids of the game; `demo-srpg` adds skills that use them), and every effect except
`ability`.

---

## Combat pipeline (shared by skills, supports and auras)

Today `combat.cpp` computes a strike from the formulas only (`strikeForecast`) and runs a fixed exchange
(`applyAttack`: attacker, defender, attacker, ...). This section is the whole new pipeline; without any source of
modifiers it computes exactly what today's code computes and rolls exactly the same random numbers.

**`CombatMods`** is what one fighter gets for one combat: `stats` (per stat index, added for the formulas), `hit`,
`avoid`, `crit`, `critAvoid`, `damage`, `damageTaken`, `damagePercent[]`, `damageTakenPercent[]`, `strikes`,
`multiStrike` (1 = none), `strikeFirst`, `counterAnyRange`, `noCounter`. `Battle::combatMods(setup, modsA, modsD)`
fills both fighters' bags by asking the sources in this order: the attacker's own skill blocks, the defender's own
skill blocks, auras of other units (unit id order), supports (pair order). A source that a game does not use adds
nothing.

**Setting up a combat** (attacker `A` with weapon `wA` from square `from`, defender `D`):
1. `D`'s answering weapon `wD`: the weapon that would answer today at this distance; if there is none in range, the
   first of its weapons with `counter` that may target `A` at all (the equipped one with carried weapons). -1 = none.
2. The modifiers are collected with `wA` and `wD` as the weapons in use.
3. `D` counterattacks when `rules.counterAttacks`, `wD` >= 0, `A` has no `noCounter`, and `wD` is in range or `D` has
   `counterAnyRange`.

**One side's strike numbers** (`strikeForecast`), with `m` = its own mods and `f` = the foe's:
- the formulas run with `m.stats` added to its stats and `f.stats` to the foe's (`FormulaCtx::bonusA`, `bonusB`);
- `damage = max(0, formula damage + m.damage + f.damageTaken)`, then each percent of `m.damagePercent` and
  `f.damageTakenPercent` in turn;
- `hit = clamp(formula hit (100 without a formula) + m.hit - f.avoid, 0, 100)`;
- `crit = clamp(formula crit (0 without) + m.crit - f.critAvoid, 0, 100)`;
- `strikes = clamp(formula strikes (1 without) + m.strikes, 1, 8)`; `multi = m.multiStrike`.

**Order of strikes.** `rounds = max(strikes of A, strikes of D if it counters)`. In each round the first fighter
makes `multi` strikes in a row if it has a strike left in this round, then the other does the same. The first
fighter is `A`, unless `D` counters and has `strikeFirst` and `A` has not. This order, the counts and whether `D`
counters are fixed when the fight starts; the numbers of each strike are recomputed before that strike (as today:
HP changes during the fight, so `combat` blocks are judged again). `Battle::strikeOrder(forecast, out)` returns the
order for the player's forecast and for `applyAttack`. Ammunition is spent as today (once per fighter, with its
first strike).

**One strike** (`doStrike`), in this exact order:
1. The numbers as above. A carried weapon that broke: no strike (as today).
2. The striker's `strike` activations, in skill-list order, then the struck unit's `struck` activations: each one
   whose conditions hold is rolled (rule under "Chance"). Blocks without a chance simply apply.
3. Hit: `hit >= 100` or `rng.chance(hit)` (as today), with the strike's `hit` effects added first.
4. On a hit: crit as today (`crit > 0 && rng.chance(crit)`, with the strike's `crit` effects); `damage` = the number
   from step 1 plus the strike's `damage` / `damageTaken`, not below 0; a critical multiplies by `critMultiplier`
   (rounded down); then the strike's `damagePercent` / `damageTakenPercent` in turn; `survive`: if the damage would
   defeat a target with more than 1 HP it is cut to `hp - 1` (flag `kSurvived`); the damage is capped to the target's
   HP and applied.
5. `Attack` event (flags as today plus `kSurvived`), preceded by the `SkillActivated` events of step 2.
6. `drain`: the striker heals `damage dealt * percent / 100` (at least 1 when damage was dealt), `UnitHealed`.
7. Weapon wear, drops, defeat and story events as today.
8. `strikeAgain` (if both are still on the board): one extra strike through the same steps, which cannot grant
   another.

A game without skills, supports or auras makes no roll in step 2 and adds nothing anywhere, so its fights are bit
for bit today's.

**Experience.** The existing awards run after the last strike, each amount passed through `xpPercent` of the
receiver. Then the `defeat` / `afterCombat` blocks, then supports' "fought together" (section 6), then
`endOrder(attacker)`.

---

## 4. Menu abilities and summoning

An ability is a command in a unit's action menu that does one thing to a target next to it (or to itself, or to a
free square).

### Data: data/abilities.json

```jsonc
{ "abilities": [
  { "id": "shove", "name": "Shove",
    "description": "Pushes a neighbour one square away.",   // optional, one line (wording)
    "target": "unit",              // "self" | "ally" | "enemy" | "unit" (ally or enemy) | "cell" (a free square)
    "range": [1, 1],               // [min, max] grid distance; default [1, 1]; not with "self"
    "effect": { "push": 1 },       // exactly one effect, see below
    "cost": "action",              // "action" (default): ends the unit's turn; "free": it may still act
    "uses": 2,                     // optional, 1..99: times per battle, per unit
    "cooldown": 3,                 // optional, 1..99: turns to wait after a use
    "afterMove": true,             // optional, default true; false = only before moving
    "if": [ { "hp": { "op": ">", "percent": 50 } } ] },   // optional: conditions of section 3; "who": "foe" = the target unit
  { "id": "call_hound", "name": "Call Hound", "target": "cell", "range": [1, 2],
    "effect": { "summon": { "type": "hound", "level": "summoner", "limit": 1, "vanish": true,
                            "lifetime": 0, "canAct": false, "givesXp": true } },
    "cost": "action", "cooldown": 2 }
] }
```

**Who has an ability**: units.json `"abilities": ["shove"]` (the unit type), characters.json `"abilities"`, or a
skill with the effect `{ "ability": "shove" }` in an `always` block without conditions (so an accessory, a weapon or
a level can grant one through a skill). `Battle::abilitiesOf(unit, out)`: type, character, then skills, each ability
once.

| Effect | Target | What happens |
|---|---|---|
| `{ "push": 1 }` (1..5) | `ally`, `enemy`, `unit`; range must be [1, 1] | the target moves away from the user in a straight line, up to that many squares, stopping before the first square it cannot take |
| `{ "pull": true }` | same | the user steps one square straight back and the target takes the square the user left |
| `{ "swap": true }` | same | user and target exchange squares |
| `{ "moveBehind": true }` | same | the target is put on the square on the user's opposite side |
| `{ "heal": 10 }` / `{ "heal": "full" }` | `self`, `ally`, `unit` | the target regains HP (refused at full HP) |
| `{ "summon": { … } }` | `cell` | a new unit appears on the square (below) |

Refusals (all start `data/abilities.json: ability "<id>": `): `"target" must be "self", "ally", "enemy", "unit" or
"cell"`; `"effect" needs exactly one of push, pull, swap, moveBehind, heal, summon`; `"push" moves a neighbour:
"range" must be [1, 1]`; `"summon" needs "target": "cell"`; `"heal" cannot target "enemy" or "cell"`; `"cost" must be
"action" or "free"`; `"uses" must be from 1 to 99`; `"cooldown" must be from 1 to 99`; `"summon": unknown unit type
"x"`; `"summon": "limit" must be from 1 to 20`; `"summon": "lifetime" must be from 0 to 99`; `"summon": "level" must
be "summoner" or a whole number from 1 to <maxLevel>`; unknown keys.

### Directions (square and hexagon)

All four moves use directions, never coordinates: `dir = grid.directionTo(userCell, targetCell)` (they are
neighbours), "one square further" is `grid.neighbor(cell, dir)` repeated, and "straight back" / "the opposite side"
is `grid.neighbor(userCell, grid.opposite(dir))`. `Grid::opposite(dir)` is new: `(dir + directionCount() / 2) %
directionCount()` (square: N-S, E-W; hexagon: the facing side; docs/HEX.md lists the directions in geometric order
for both layouts, so the same direction index is a straight line across rows and columns).

### Legality

A unit may **take a square** when the square is on the map, `enterCost(square, its move type) > 0` (structures'
costs included, as for walking) and no unit stands on it. Heights do not matter.

- `push`: the first square must be takable by the target. `pull`: the square behind the user by the user (the target
  can always take the user's square if `enterCost` allows; both are checked). `swap`: each can stand on the other's
  square (`enterCost` only). `moveBehind`: the opposite square is takable by the target. `summon`: the square is
  takable by the summoned type and the user has fewer than `limit` living summons of this ability.
- **Fog.** Targets are units the user's team sees; a `cell` target is any square in range. What a menu offers
  (`validate`, `abilityTargets`) treats a square holding an **unseen** enemy as free, like `reachable` does. When
  the ability is carried out and the needed square is in truth taken, it is **blocked**: a push stops in front of
  the unit (possibly moving zero squares), the others do nothing; the cost, the use and the cooldown are still
  spent; `AbilityUsed` carries the flag `kBlocked` and names the unit in the way.
- Other checks, each with its reason: the unit must be able to act and not be `done`; `afterMove: false` and the
  unit has moved; no uses left (`no uses left this battle`); cooling down (`ready in 2 turns`); a `free` ability
  already used this turn (`already used this turn`); `if` conditions (`cannot be used now`); nothing to target.
- A unit that is moved by an ability: abandons a capture it leaves (`releaseCapture`), keeps its turn flags, and
  story `reach` events fire for the square it ends on (target first, then user), as for `moveUnit`.

### Cost, uses, cooldown

- `cost: "action"`: `endOrder(unit)` after the effect. `cost: "free"`: the unit is not done; the player's "undo move"
  is dropped (as after an item action). **A free ability can be used once per turn per unit** (so nothing can loop).
- `uses`: per unit, per battle; not carried between chapters.
- `cooldown: n`: after a use the ability is ready again when `n` of the unit's side's turns have started (1 = next
  turn).
- State: `Unit::abilityUses`, one entry per ability the unit has used: `{ ability, used, wait, turn }` (`used` =
  times this battle, `wait` = turns still to wait, `turn` = used this turn). At the start of its side's turn `wait`
  drops by one (not below 0) and `turn` is cleared.

### Summoning

`{ "summon": { "type", "level", "limit", "vanish", "lifetime", "canAct", "givesXp" } }`; only `type` is required.

- The new unit is of unit type `type`, on the summoner's **side**, named after its type, with the type's base stats
  and items, at `level`: `"summoner"` (default) = the summoner's level, or a fixed number. It has no story id and no
  character. Its items never drop (their `drop` marks are cleared). It is an ordinary unit from then on: the player
  or the AI controls it, it fights, gains XP and levels like any unit.
- `canAct` (default false): false = it arrives with its turn finished (like a built unit with
  `builtUnitsCanAct: false`); true = it can move and act at once.
- `limit` (default 1): how many living summons of **this ability by this summoner** may exist. At the limit the
  ability is refused: `already has 1 summoned Hound`.
- `vanish` (default true): when the summoner is defeated, removed by a story event, or (carry) lost, its summons of
  this ability leave the board at once (`UnitRemoved`: not a defeat, no drops, no XP, no `defeated` events). Summons
  of a summon vanish with it in turn. `false`: they stay.
- `lifetime` (default 0 = no limit): it stays for that many of its side's turns after the turn it was summoned in,
  and leaves when the next one begins (`UnitRemoved`).
- `givesXp` (default true): false = defeating or hitting it gives no XP.
- **Defeat and objectives.** Summoned units do **not** keep a side in the battle: `rules.defeat` `noUnits` counts
  only units that were not summoned (when a side falls, its summons leave with it as every unit does). In
  objectives and story conditions, filters by side, team or "any" ignore summoned units for `defeated` / `dead` /
  `alive`; filters by type or tag see them. A summoned unit is never carried by a campaign (it has no id).
- State on the summoned unit: `Unit::summoner` (unit id), `Unit::summonAbility`, `Unit::summonLeft` (turns of
  lifetime left; -1 = no limit).

### Core API

```cpp
struct AbilityOption { int ability; bool ok; std::string why; };            // one menu entry
void abilityOptions(int unit, Cell from, std::vector<AbilityOption>& out) const;   // every ability it has
// Targets if the unit stood on `from`: units for unit targets, squares for "cell"; a "self" ability returns the unit.
void abilityTargets(int unit, int ability, Cell from, std::vector<int>& units, std::vector<Cell>& cells) const;
static Action ability(int unit, int ability, int target, Cell cell = kNoCell);     // ActionType::Ability, Action::ability
```

### Saves, events

- Per unit, only when not empty: `"abilities": [ { "ability": n, "used": n, "wait": n, "turn": true } ]` (`turn` only
  when true; `ability` a valid index, each once; `used` 1..999; `wait` 0..99).
- Per summoned unit: `"summon": { "by": unit id, "ability": n, "left": n }` (`left` only with a lifetime). `by` must
  be an existing unit with a lower id; `ability` a summon ability.
- Refused in a game without `data/abilities.json`: `units[2].abilities: this game has no abilities`.
- Events: `AbilityUsed` (a = unit, b = ability, c = target unit or -1, d = target square or -1, flags `kBlocked`;
  with `kBlocked`, `data[0]` = the unit in the way), then what the effect did with the existing entries:
  `UnitRelocated` for every unit moved (target first), `UnitHealed`, `UnitAdded` for a summon, `UnitRemoved` when
  one vanishes.
- `data/abilities.json` is mixed into the data hash only when present.

---

## 5. Rescue and pair up

Both put one unit **off the board, attached to another**. A game picks one mode or neither.

### Data: game.json `rules.rescue` (optional; missing = neither)

```jsonc
"rescue": {
  "mode": "carry",                       // required: "carry" or "pairUp"
  "canCarry": "a.str >= b.str and not b.tags.mounted",   // optional formula: a = the unit that stays on the board,
                                                         // b = the one that leaves it; missing = always allowed
  "carrierStats": { "spd": { "percent": 50 }, "skl": { "percent": 50 }, "mov": -1 },
                                         // optional: how the unit on the board is changed while it has someone:
                                         // a number is added, { "percent": n } multiplies (0..1000); not the HP stat
  "pairBonus": { "str": 2, "def": 2 }    // pairUp only, optional: what the unit behind adds to the one in front
}
// data/units.json, per unit type (pairUp only): what a unit of this type gives when it is behind;
// it replaces rules.rescue.pairBonus as a whole
"pairBonus": { "str": 3, "spd": 1 }
```

Refusals: `game.json: rules.rescue: "mode" must be "carry" or "pairUp"`; `… "carrierStats" names unknown stat "x"`;
`… "carrierStats" cannot change "hp" (the health stat)`; `… "pairBonus" belongs to "mode": "pairUp"`; `… unknown key
"x"`; `data/units.json: unit "knight": "pairBonus" needs "rules": { "rescue": { "mode": "pairUp" } } in game.json`; a
formula syntax error names `rules.rescue.canCarry`. Core: `RescueRules`, `RulesConfig::rescue`,
`UnitTypeDef::pairBonus`.

### The shared state: "off the board"

`Unit::carrying` (unit id, -1 = nobody) on the unit that stays, `Unit::carriedBy` on the unit that left. The unit
that left has **`alive == false`**, no square, and **keeps its HP** (a defeated unit has 0 HP). This is deliberate:
about ninety places in the core, the AI and the player loop over units and skip `!alive` before touching `cell`
(movement, sight, targeting, drawing); with this representation all of them are right without change, and only the
handful that ask "does this unit still exist" need the new query:

```cpp
bool Battle::exists(const Unit& u) const { return u.alive || u.carriedBy >= 0; }   // not defeated, on the board or carried
```

Used by (and only by): `checkDefeat` (`noUnits`), the story conditions `alive` / `dead` and the commands that take a
filter, the objective kind `defeated`, `finishChapterBattle` (a carried unit is written to the roster as living),
the save checks, and the player's unit lists. `canAct` stays false for it, so it cannot be ordered; nothing can
target, see, heal, hurt or move it; its skills, auras and abilities are off; it gains nothing at turn start.

At most one unit is attached to another; a unit that carries cannot be carried.

### Carry (`mode: "carry"`)

| Action | Valid when | Effect | Cost |
|---|---|---|---|
| `Rescue(unit, target)` | `target` is next to `unit`, of the **same side**, on the board; neither carries or is carried; `canCarry(a = unit, b = target)` holds | `target` leaves the board (a capture it was making is abandoned) | ends `unit`'s turn |
| `Drop(unit, cell)` | `unit` carries someone; `cell` is next to it and the carried unit may take it | the carried unit stands on `cell` with its turn finished | ends `unit`'s turn |
| `GiveUnit(unit, target)` | `unit` carries someone; `target` is next to it, same side, carries nobody, and `canCarry(a = target, b = the carried unit)` holds | `target` now carries it | free; `unit` counts as having moved |
| `TakeUnit(unit, target)` | `target` is next to `unit`, same side and carries someone; `unit` carries nobody and can carry that unit | `unit` now carries it | free; `unit` counts as having moved |

- Free actions cannot be undone (the player's "undo move" is dropped), like item actions. They can be chained
  across several units, which is the classic way to pass someone along a line.
- **Penalty.** While `carrying >= 0`, `carrierStats` changes the carrier's stats inside `Battle::stat` (percent
  first, then the added number; not below 0).
- **The carrier falls** (defeated, or removed by a story event, or about to change side): the carried unit is put
  back on the board: on the carrier's square if it may take it once the carrier is gone, else on the nearest free
  square it may take (`nearestFreeCell`: grid distance, lowest cell number on a tie), with its turn finished; event
  `UnitDropped` with `kForced`. If there is no such square it is **lost**: it counts as defeated by whatever
  defeated the carrier (`UnitDied`, drops, story `defeated` events, the leader rule).
- **Fog.** A carried unit sees nothing and is seen by nobody. `Drop` offers squares that look free to the team; a
  square that in truth holds an unseen enemy blocks the drop: nothing happens, the action is spent (`kBlocked`).
- **Items and trading.** A carried unit keeps its items; nobody can trade with it while it is carried. The carrier
  uses items, trades, shops and captures as usual.
- **Story.** A filter matches a carried unit (it exists). `removeUnit` on it takes it out of its carrier; `moveUnit`
  on it drops it on the given square; `changeSide` on a carrier or on a carried unit first puts the carried unit down as when the carrier
  falls (if there is no square for it, the command leaves both units as they are; logged); `giveItem` skips a
  carried unit. `reach` triggers and objectives look at squares,
  so they never see it.
- **Turn end, battle end.** Nothing special: a unit stays carried as long as nobody drops it. When a chapter is won
  while it is carried it goes to the roster like any living unit.

### Pair up (`mode: "pairUp"`)

| Action | Valid when | Effect | Cost |
|---|---|---|---|
| `PairUp(unit, target)` | `target` is next to `unit`, same side, on the board; neither is in a pair; `canCarry(a = target, b = unit)` holds | `unit` leaves the board and stands **behind** `target` (`target.carrying = unit`) | ends `unit`'s turn |
| `SwitchPair(unit)` | `unit` is in front of a pair and not done | the two exchange places: the one behind comes to the square with the front unit's `moved`, `acted` and `moveSpent`; the other goes behind | free |
| `Drop(unit, cell)` | as in carry | the unit behind stands on `cell` with its turn finished | ends `unit`'s turn |

- **A pair is one actor.** Only the unit in front can be ordered, targeted, seen and hurt. Switching hands the
  front unit's turn flags to the unit that comes forward, so a pair moves once and acts once a turn whoever is in
  front.
- **Bonus.** The unit in front gets the `pairBonus` of the type of the unit behind it (else
  `rules.rescue.pairBonus`) added to its stats inside `Battle::stat` (not the HP stat), and `carrierStats` if the
  maker set any.
- **What the unit behind can and cannot do.** It cannot act, be targeted or gain XP; its skills, auras and abilities
  are off. For **supports** (section 6) the two units of a pair always count as near each other, and the unit in
  front gets that support's bonus. For **skills**, the condition `{ "carrying": true }` is how a skill reacts to
  having a partner; the unit behind is not counted by `near`.
- **The front unit falls**: the unit behind steps onto the square by the rule of "The carrier falls" above.
- Fog, items, story, turn end and battle end: as in carry.

### Core API

```cpp
void rescueTargets(int unit, Cell from, std::vector<int>& out) const;   // carry: Rescue; pairUp: PairUp partners
void dropCells(int unit, Cell from, std::vector<Cell>& out) const;
void giveTargets(int unit, Cell from, std::vector<int>& out) const;     // carry only
void takeTargets(int unit, Cell from, std::vector<int>& out) const;     // carry only
bool canSwitchPair(int unit) const;                                      // pairUp only
// ActionType::Rescue, Drop, GiveUnit, TakeUnit, PairUp, SwitchPair; target in Action::target, square in Action::cell
```

### Saves, events

- Per unit, only when it has someone: `"carrying": unit id`. The carried unit is saved as it is (`alive: false`,
  `cell: -1`, its HP above 0). Checks: the id names another unit of the same side that is not alive, has HP of 1 or
  more and is named by exactly one `carrying`; a unit that is not alive and has HP must be carried by someone; the
  key is refused in a game without `rules.rescue` (`units[1].carrying: this game has no rescue or pair up`).
- Events: `UnitRescued` (a = the unit that stays, b = the unit that leaves, c = the square it left; flag `kPaired` in
  pair up), `UnitDropped` (a = carrier, b = the unit, c = its square; flags `kForced`, `kBlocked`), `UnitHanded`
  (a = from, b = to, c = the unit), `PairSwitched` (a = the unit that went behind, b = the unit now in front,
  c = the square).

---

## 6. Supports

Two characters grow closer; at set amounts of closeness a conversation unlocks and, once it has been seen, the two
fight better near each other.

### Data: data/supports.json (needs data/characters.json)

```jsonc
{ "rules": {
    "levelNames": ["1", "2", "3"],        // optional wording: what levels are called, lowest first
    "points": { "endTurnAdjacent": 1,     // each optional; missing or 0 = that source is off
                "fightTogether": 2,
                "heal": 2 },
    "range": 3,                           // optional, 1..10, default 3: "near" for fightTogether and for the bonus
    "maxLevels": 5,                       // optional, 1..99: support levels one character may hold over all its
                                          // partners; missing = no limit
    "conversations": "both" },            // "battle" | "camp" | "both" (default "both"): where they are played
  "pairs": [
    { "a": "eirik", "b": "lyra",          // two different characters; a pair may be listed once
      "start": 0,                         // optional: points at the start of a playthrough
      "levels": [                         // 1..5 levels, "points" rising
        { "points": 20,
          "bonus": { "hit": 5, "avoid": 5 },                       // optional
          "talk": [ { "say": "You never miss.", "speaker": "eirik" },
                    { "say": "I miss plenty. You just look away.", "speaker": "lyra" } ] },
        { "points": 60,
          "bonus": { "hit": 10, "avoid": 10, "damage": 1, "stats": { "def": 1 } },
          "talk": [ … ] } ] }
  ] }
```

- `bonus` keys: `hit`, `avoid`, `crit`, `critAvoid`, `damage`, `damageTaken` (whole numbers) and `stats` (not the HP
  or move stat): the combat numbers of section 3. A level states its **whole** bonus (levels do not add up).
- `talk` is a list of `say` commands exactly as in a campaign scene (`speaker` = a character id, or `speakerName`;
  optional `portrait`); nothing else is allowed in it. A level may have no `talk`.
- Refusals (start `data/supports.json: `): `the game has no characters (data/characters.json)`; `pair 2: unknown
  character "x"`; `pair 2: "a" and "b" are the same character`; `pair 3: "eirik" and "lyra" are already a pair (pair
  1)`; `pair 1 ("eirik", "lyra"): "levels" must have from 1 to 5 entries`; `… level 2: "points" must be more than
  level 1's (20)`; `… level 1: "bonus": unknown key "x"`; `… level 1: "talk": command 2 (setSwitch): only "say" is
  allowed in a support conversation`; `rules: "conversations" must be "battle", "camp" or "both"`; `rules: "levelNames"
  has 2 names but pair 1 has 3 levels`; unknown keys.
- Core: `SupportRules`, `SupportPairDef`, `SupportLevelDef`, `Database::supports` (`rules`, `pairs`, `texts`).

### State

```cpp
struct SupportState { std::vector<int> points; std::vector<uint8_t> level; };   // one entry per pair, in file order
Battle::supports          // during a battle
Playthrough::supports     // carried by a campaign
```

A pair has **points** (0..1,000,000) and a **level** (0 = none .. the number of its levels). A single battle starts
every pair at its `start` points and level 0. A campaign hands its state in with `Battle::setStartingSupports`
(after `init`, before `start`, like the roster) and takes it back in `finishChapterBattle` after a **won** battle; a
lost battle changes nothing (so a retry starts from the same closeness).

### Earning points

In a battle, for a pair whose two characters are both on the board and on the same **team**:

- `endTurnAdjacent`: when a side ends its turn (the `EndTurn` action, before the next turn begins), every pair
  whose character `a` belongs to that side and whose two units are neighbours (grid distance 1; the two units of a
  pair-up pair count) gains that many points. Once per round per pair.
- `fightTogether`: after each combat, for each fighter that is still on the board, every pair of that fighter whose
  other character is within `range` of it gains the points (once per pair per combat).
- `heal`: when one of the two heals the other with an item used on an ally or a `heal` ability.
- Story: the command `addSupport` (below), in battles and in scenes.

Each gain makes one `SupportPoints` event (flag `kReady` when this gain made a conversation ready). A fallen
character (`fallen: gone`) earns nothing ever again; its pairs keep their numbers.

### Levels and conversations

The **next level** of a pair is `level + 1`; it is **ready** when the pair's points have reached its `points` and
raising it would not take either character past `rules.maxLevels` (the sum of its levels over all its pairs).

- A ready level **without** `talk` is gained at once (event `SupportReached`).
- A ready level **with** `talk` is gained when its conversation is played:
  - **In battle** (`conversations` `battle` or `both`): the action `Support(unit, target)` (`ActionType::Support`),
    offered to either of the two while they are neighbours on the board and on the same team (in a pair-up pair:
    to the unit in front), when that unit can act and is not done. It is free (does not end the turn, cannot be
    undone). It emits the lines as ordinary `Dialogue` events (speaker = the unit when the line's
    `speaker` is on the board, else the name), then `SupportReached`. Query: `supportOptions(unit, from, out)` (the
    partners with a ready conversation).
  - **Between chapters** (`camp` or `both`): see "Campaign" below.
- With `conversations: "camp"` a free battle (no campaign) never raises a level that has a `talk`.
- Only one level is gained at a time; the next becomes ready when its points are reached (points keep growing
  meanwhile).

Wording of conversations lives in `Database::supports.texts`. `Battle::storyText(index)` already resolves the text
of a `Dialogue` event; indices at or above `kSupportTextBase` (1 << 24) are looked up in that table instead of the
map's, so the player needs no change to show a line.

### The bonus

In a combat, a fighter gets the `bonus` of the **current level** (not the next) of each of its pairs whose other
character is on the board, on its team and within `rules.range` of it (or is its pair-up partner). The bonuses of
several partners add up. They enter `CombatMods` like a skill's `combat` effects (see "Combat pipeline") and so
show in the forecast.

### Story and campaign

- Command (map events and scenes): `{ "addSupport": ["eirik", "lyra"], "by": 10 }` (`by` may be negative; points stay
  within 0..1,000,000). A level without `talk` that becomes ready is gained at once.
- Condition (map events and scenes): `{ "support": ["eirik", "lyra"], "op": ">=", "value": 1 }`: the pair's **level**
  compared with `value` (`op` default `>=`).
- Trigger (map events): `{ "support": ["eirik", "lyra"], "level": 2 }`: fires when the pair gains that level during
  the battle (`level` optional = any level).
- Refused: characters that are not a declared pair (`"eirik" and "bram" are not a support pair
  (data/supports.json)`).
- **Camp** (`engine/core/campaign/`), for the between-chapters menu:

```cpp
// Pairs with a ready conversation whose two characters are in the roster and not gone, in pair order.
void pendingSupports(const Database& db, const Playthrough& pt, std::vector<int>& pairs);
// Plays it: the level is gained and the lines come back like a scene's. False if it is not pending.
bool playSupport(const Database& db, Playthrough& pt, int pair, std::vector<SceneLine>& lines);
```

  The player offers the camp whenever the playthrough is in phase `Before` of a chapter other than the first and
  `pendingSupports` is not empty, before running the chapter's before scene. No new phase exists.

### Saves and hash

- Battle save, only in a game with support pairs: `"supports": { "points": [n, ...], "levels": [n, ...] }` (both
  exactly one entry per pair; points 0..1,000,000; a level within the pair's levels and not above what its points
  allow). Refused in a game without pairs.
- Campaign save: the same object under `"playthrough"`, same rule.
- `data/supports.json` is mixed into the data hash only when present, with the story-wording treatment (`say`,
  `speakerName` blanked, so rewriting a conversation keeps saves valid); `levelNames` becomes a cosmetic key.

---

## Events (what the player animates)

Appended to `EventType` in this order (existing values keep their numbers):

| Entry | a, b, c, d, flags, data | Seen by (fog) |
|---|---|---|
| `SkillLearned` | a = unit, b = skill | the unit's own team |
| `SkillActivated` | a = unit, b = skill, c = the foe or -1 | the unit's team and the teams that see it |
| `UnitHurt` | a = unit, b = HP lost, c = the unit whose skill did it | the unit's team and the teams that see it |
| `UnitPromoted` | a = unit, b = old type, c = new type, d = new level, data = change per stat | the unit's team and the teams that see it |
| `AbilityUsed` | a = unit, b = ability, c = target unit / -1, d = target square / -1, flags `kBlocked` | the user's team and the teams that see the user or the target |
| `UnitRescued` | a = the unit that stays, b = the unit that leaves, c = the square it left, flags `kPaired` | both teams' own and the teams that see either before it happens |
| `UnitDropped` | a = carrier, b = the unit, c = its square, flags `kForced`, `kBlocked` | the unit's team and the teams that see the square |
| `UnitHanded` | a = from, b = to, c = the unit | the units' team and the teams that see either carrier |
| `PairSwitched` | a = now behind, b = now in front, c = the square | the units' team and the teams that see the square |
| `SupportPoints` | a = unit of `a`, b = unit of `b`, c = pair, d = points now, flags `kReady` | the units' own team |
| `SupportReached` | a = unit of `a` (-1 off the board), b = unit of `b`, c = pair, d = new level | the units' own team |

New flag bits (after `kBought = 32`): `kSurvived = 64`, `kBlocked = 128`, `kPaired = 256`, `kForced = 512`,
`kReady = 1024`. `Attack` events may carry `kSurvived`. Existing entries reused: `UnitRelocated` (pushes, pulls,
swaps), `UnitAdded` (a summon), `UnitRemoved` (a summon vanishing), `UnitHealed`, `Dialogue` (support
conversations), `LevelUp`, `XpGained`. Every change of who is on the board calls `markSight()` first, like the
existing code, so events are stamped with the sight of the moment.

## Save format summary and versioning

`saveVersion` stays 1 and `formatVersion` stays 1: every addition is an optional key that is written only when the
game uses the feature **and** the value is not the default, and a save made before a game adopted a feature is
refused by the data hash anyway (the rules changed).

| Where | Key | Written when |
|---|---|---|
| battle `units[i]` | `skills` | the unit has learned skills |
| | `moveSpent`, `acted`, `refreshed` | the game has `moveAgain` / `actAgain` skills and the value is not 0 / false |
| | `abilities` | the unit has used an ability this battle |
| | `summon` | the unit was summoned |
| | `carrying` | the unit carries / has a unit behind it |
| battle | `supports` | the game has support pairs |
| playthrough `roster[i]` | `skills` | the unit has learned skills |
| playthrough | `supports` | the game has support pairs |

Restoring checks every new field's type and range and the cross-unit rules above (carry links, summoner ids), fails
with a path and a plain reason (`units[4].summon.by: unit 9 does not exist`), and applies nothing unless everything
holds. Round trip stays exact, including the middle of a turn with `acted` units, cooldowns and carried units.

Data hash (`save_hash.cpp`, `mixRules`): after `data/research.json`, each only when the file exists and in this
order: `data/characters.json`, `data/skills.json`, `data/abilities.json`, `data/supports.json` (the last with the
map-file wording treatment). `rules.rescue` and `rules.promotion` are part of `game.json`'s `rules`, which is hashed
whole. `cosmeticKey` gains `levelNames`. The campaign hash is built on `mixRules`, so it follows.

## AI (minimum behaviour)

The AI stays stateless and works through the public queries and `apply`. In every area the first rule is: **the AI
never breaks and every turn ends** (it only submits actions that `validate` accepts, and the existing "never leave
the unit ready" fallback covers the new states: a unit with `acted` is given `Wait`).

- **Characters, skills.** Nothing to decide. Attack scores come from `forecast`, which includes passive skills,
  auras and support bonuses. Activations are ignored. A unit with `moveAgain` that has `acted` moves away from
  danger with the existing advance / retreat scoring if it has movement left, else waits.
- **Promotion.** Automatic promotions happen by themselves. A unit of an AI side with an offered promotion takes
  the first one, as its order for the turn, when its best order would otherwise be a plain Wait or advance with no
  attack and no capture. The AI does not use promotion items.
- **Abilities.** `summon`: used when ready and a legal square exists, as the unit's order when it has no attack worth
  more; the square is the legal one nearest to the nearest known enemy (lowest cell number on a tie). `heal`: used
  like the healing item rule (docs/ITEMS.md "AI"): on itself, or on a neighbouring ally below half HP, when no kill
  is available. `push`, `pull`, `swap`, `moveBehind`: never used. A free ability is used at most once per unit per
  turn by rule, so no loop is possible.
- **Rescue / pair up.** The AI never rescues, gives, takes, pairs up, switches or drops. Since nothing else can put
  a unit of an AI side into a carried state, it never has to. A side played by `--autoplay` behaves the same. Units
  that carry are attacked like any others.
- **Supports.** Points are earned by AI-played units like any others. The AI never starts a `Support`
  conversation.
- **Summoned units** are ordinary units for the AI.

---

## Player UI

All wording is placeholder. Everything works with keyboard, gamepad and touch through the existing menu, list and
dialogue components (`ui/menu.*`, `ui/row_list.*`, `ui/dialogue_box.*`); nothing here needs a close-up battle scene.

**Action menu** (`battle_menus.cpp`, `openActionMenu`), entries in this order, each only when it applies:
Attack, Capture, Talk…, **Support** (one entry per partner: "Support: Lyra"), **abilities** (one entry each, by the
ability's name; one that cannot be used now is shown greyed with its reason: "Shove (ready in 2 turns)", "Call
Hound (0 left)"), **Rescue / Drop / Give / Take** or **Pair up / Switch / Drop**, **Promote**, Items, Trade, Storage,
Shop, Supply, **Info**, Wait. A unit with `acted` gets only Info and Wait, and its remaining move range is shown as
when a unit is first selected.

**Targeting.** Abilities, Rescue, Give, Take, Pair up and Support pick a unit the way Attack does (the list of legal
targets, cycled with the direction keys or tapped). Drop and `cell` abilities pick a square from the highlighted
legal squares. For a push, pull, swap or move-behind the squares the two units would end on are marked before
confirming. Cancel goes back to the menu.

**Forecast** (`forecast.cpp`): the numbers already include passive effects. New rows under each column: "x2"
strikes in a row are written "Dmg 7 x2 x2" (strikes, then in a row); a line "Strikes first" for the defender when
`defenderFirst`; then one line per note of `Forecast::notes` for that unit: the skill's name and its chance ("Heavy
Blow 14%"). At most three note lines per column, then "+2 more".

**During a fight** (map animations as today): a `SkillActivated` event shows the skill's name as a floating label
over the unit before the strike it belongs to; `kSurvived` shows "Survived!"; a drain heal shows the usual heal
number.

**Info panel** (`panel.cpp`, `drawUnitInfo`): under the unit's name, the unit type's name (as today when it
differs); a row "Carrying: Bram" / "With: Bram" for a unit that has someone; up to three skill names, then "+N"; a
support mark next to the name when a conversation is ready. Rows that do not fit are dropped as today.

**Unit details** (new overlay, `Mode::UnitInfo`): opened by **Info** in the action menu, and by a new entry
**Unit** in the battle menu (the one with End Turn) when the cursor is on a visible unit (Confirm on a unit that
cannot be ordered opens that menu today). Pages turned with Left / Right or the tabs at the top: **Stats** (stats
with what skills, items and partners add, in colour; growths are not shown), **Skills** (name and description, the
source in small print: class, learned, personal, item, weapon), **Abilities** (name, description, uses left,
turns to wait), **Supports** (partner, level name, a bar towards the next level, "Conversation ready"), **Items**.
Cancel closes it. Only units the viewing team sees can be opened.

**Promotion.** With one possible promotion, Promote (or using the item) asks "Promote Kell to Captain?" with the
stat changes. With several it opens the **promotion screen**: a list of the unit types on the left, and for the
highlighted one its picture (the unit sprite), the stat changes, the skills it brings and what the unit could no
longer wield; Confirm picks it. `UnitPromoted` plays a flash on the unit, the sprite changes to the new type, a
banner "Kell is now a Captain" and the stat changes like a level-up box. An automatic promotion shows the same
banner after the level-up box.

**Abilities.** `AbilityUsed` shows the ability's name over the user; moved units slide (`UnitRelocated`); a summon
pops in (`UnitAdded`) and fades out when it vanishes (`UnitRemoved`); a blocked ability shows "Blocked!" and reveals
the unit in the way like `MoveBlocked` does.

**Rescue / pair up.** `UnitRescued`: the unit slides onto the other and disappears; the unit that stays gets a
small marker (a second, smaller side-coloured tab at its corner) for as long as it has someone. `UnitDropped`: the
unit slides out onto its square. `PairSwitched`: the two sprites cross-fade.

**Supports.** `SupportPoints` with `kReady` shows a small "Support!" label over both units; other gains show
nothing. The Support entry plays the conversation in the dialogue box (the same controls and Skip as story
dialogue); `SupportReached` shows a banner "Eirik and Lyra: support level 1".

**Camp (between chapters).** A new screen in the campaign flow (`engine/player/campaign/camp_scene.*`), shown before
a chapter's title card when the core says there are pending conversations: title "Camp", a list of the pending
pairs ("Eirik and Lyra"), and **Continue the story** at the bottom. Choosing a pair plays its lines in the
`StoryScene` dialogue box, then returns to the list (the pair is gone from it, or shows its next level if that is
ready too). The playthrough is saved after each conversation, like after a scene. `--autoplay` leaves the camp at
once (plays nothing).

**Script tokens.** The `--input` script needs no new tokens (menus are driven with the direction keys and
`confirm`); the player logs each opened menu's entries (`OTE_INFO`), which the script tests read.

---

## Editor

Plain language throughout; "id" and formulas only under Advanced, as in docs/EDITOR_UI.md. Every list screen is the
usual "list on the left, form on the right" with New, Duplicate, Delete (with the "used by" check that removes
references in the same undo step) and reordering. New sidebar entries, after Units: **Characters**, **Skills**,
**Abilities**, **Supports** (each appears once its feature is turned on in Rules, or the file exists).

**Helper API** (docs/EDITOR_API.md): four new document sections, each exactly like `items`: `characters` (list;
file `{ "characters": [...] }`), `skills` (`{ "skills": [...] }`), `abilities` (`{ "abilities": [...] }`), and
`supports` (the file's one object, like `campaign`; `false` removes the file). An empty list removes the list file
(a game without the file and a game with an empty list are the same game, and the hash must not change by opening a
screen). Unknown keys and key order are kept. `GET .../problems` runs the real loader, so every refusal in this
document reaches the Problems panel in the loader's words.

**Rules screen**, card **Strategy-RPG features**: switches "This game has characters", "…skills", "…abilities",
"…supports" (each creates or, after asking, removes its file); **Carrying** (Off / Carry / Pair up, with one line of
help each; then "Who can carry whom" as presets: Anyone; "Carrier's <stat> is at least the other's <stat>"; "Not
units tagged <tag>"; Custom formula under Advanced; "While carrying" as rows of stat + halved / minus N; for pair
up "The partner behind gives" as rows of stat + number); **Promotion**: "After promoting the level: starts again at
1 / stays".

**Characters** screen: name, description, portrait (the existing portrait picker, keyed by the character's id),
unit type, starting level, a stats table with three columns (the unit type's base, greyed; the personal bonus,
editable; the total) and the same for growths, personal skills and skills learned at levels, personal abilities,
starting items. Checks: a character no map places and no event adds ("Not used on any map").

**Units** screen gains cards: **Skills** (skills it always has; "learned at level" rows), **Abilities**,
**Promotion** (rows: becomes <unit type>, from level N, by item <item>, on reaching the level: nothing / offered /
automatic, level afterwards: game setting / starts again / stays, stat changes: "difference between the two types"
or a table), and for pair-up games **Gives when behind**. **Weapons** and **Items**: a Skills picker (accessories
and weapons); usable items get the switch "Promotion item".

**Skills** screen: name, description, and the **block builder**: a skill is a column of block cards. Each card reads
as a sentence with three parts, each a row of pickers:

- **When**: "Always", "At the start of its turn", "In combat", "When it strikes", "When it is struck", "After
  defeating a foe", "After combat".
- **Only if** (optional, add / remove rows): each row picks a condition from the list offered for that When and
  fills its small form ("HP is at most 50%", "at least 1 ally within 2 squares", "it started the fight", "the foe is
  tagged Armored"...). **Chance** (optional): "always" / "N %" / "a stat as a percent" (stat picker, with "times N")
  / Custom under Advanced. **Reaches** (combat only): "itself" / "allies within N" / "enemies within N".
- **Then**: one or more effect rows from the list offered for that When.

Choices that the loader would refuse are not offered (the pickers are filtered by When, exactly following the tables
in section 3), and a block the file already has in a refused shape is shown with the loader's message. Under the
card a one-line plain summary is generated ("In combat, if it started the fight: damage +3"). **Add starter
skills…** opens the installer: the thirty skills with checkboxes (all on), four pickers "Which stat is attack /
skill / speed / defense?" (pre-filled by matching the game's stat ids `str` / `skl` / `spd` / `def` and the stats
the combat presets use; "none" skips the skills that need it), and **Add**: skills whose id already exists are
skipped and listed. The set's data lives in one file of the editor (`editor/web/src/model/starterSkills.ts`) and is
the same list as the template's `data/skills.json` (a unit test compares them).

**Abilities** screen: name, description, target (pickers in words: "itself", "an ally next to it", "an enemy next to
it", "any unit next to it", "a free square within N"), what it does (Push N squares / Pull back / Swap places / Move
to its other side / Heal N or fully / Summon, with the summon's fields: unit type, level "same as the summoner" or
N, "how many at once", "vanish when the summoner is defeated", "lasts N turns / until defeated", "can act at once",
"gives experience"), cost ("uses the unit's action" / "free"), "uses per battle" (unlimited or N), "wait between
uses" (none or N turns), "only before moving", and optional conditions (the same rows as a skill's Only if).

**Supports** screen: a settings card (level names, the three point sources as "off / N points", "how close counts
as near", "most levels one character can hold", "conversations are played: in battle / between chapters / both"),
then the list of pairs (two character pickers, starting points) and for the selected pair its levels: points
needed, bonus rows (the combat numbers, in words), and the conversation, edited with the campaign's `SceneEditor`
limited to lines of dialogue. Checks: a pair whose characters never share a map ("These two never meet"), a level
that can never be reached because no point source is on.

**Map editor**, Units tool: the picker gains a **Characters** tab next to the unit types; placing a character
writes `{ "character", "side", "x", "y" }`; the selected unit's form shows the character (with "Open character") and
hides the id field for it; a character already on the map is greyed in the picker. The events editor gains the new
commands, conditions and triggers as small forms: Promote, Teach skill, Forget skill, Add support points; "has
skill", "support level"; "a unit is promoted", "a support level is reached". The campaign's scene editor gains Add
support points and the support-level condition.

**Checks / Problems** (editor-side, beside the loader's): a skill no unit type, character, item or weapon uses; an
ability nobody has; a promotion item no promotion lists and a promotion that needs an item no map, shop or event
hands out; a unit type that promotes into itself through a chain (a loop is allowed by the engine, flagged as a
note); carried weapons a promoted type cannot wield while the base type starts with them.

**Wording guidelines.** Keep the engine's maker-facing vocabulary: "unit type", "skill", "ability", "character",
"support", "promotion", "carry", "pair up", "square". Never "block" for a skill part in the UI (say "rule" or just
show the sentence), never "aura", "proc", "buff", "debuff", "passive", "cooldown" (say "wait between uses"),
"activation" (say "chance"). Open question 1 below proposes when "unit type" should read "class".

**Tutorial** (docs/TUTORIAL.md): one new lesson, "Characters and skills" (template: strategy-rpg): make a
character, place it on the map, give its unit type a starter skill, build one skill with a condition, Play.

---

## Demo and template content

All names are original to this project; existing demo characters are extended, nothing is taken from any other
game.

**`games/demo-srpg`** (the showcase; its campaign has two chapters today):

- `data/characters.json`: Eirik, Kell, Lyra, Bram, Sera (the company) and Gorm, Hald (the two bosses), with the
  personal stats that are map overrides today moved into the characters, personal growths and one personal skill
  each. The maps place them with `"character"`.
- `data/skills.json`: the starter set, plus three that use the game's own ids: **Armor Breaker**
  (`combat`, if the foe is `armored`: damagePercent 150), **Woodcraft** (`combat`, if terrain `forest`: avoid +20),
  **Sword Dancer** (`combat`, if weapon `sword`: crit +10). Unit types get one or two skills each and `learns`
  entries at levels 3 and 5 so a learned skill shows up within the campaign; one weapon (a new "Keen Blade") and one
  accessory (a new "Runner's Charm": Light Step) carry a skill.
- Promotion: new unit types Captain (from Lord, by story at the start of chapter 2), Lancer and Outrider (two
  choices from Cavalier, by the item **Officer's Writ**, which Gorm drops in chapter 1), Marksman (from Archer,
  offered at level 4), Brute (from Fighter, automatic at level 5). `rules.promotion.levelAfter: "reset"`.
- `data/abilities.json`: **Shove** (Fighter and Knight; uses the action), **Swap** (Cavalier; free, wait 2 turns),
  **Pull Back** (Sera, personal; free, 2 uses per battle), **Rally** (Eirik, personal; heal 5 to an ally next to
  him, uses the action), and for the enemy a new unit type Houndmaster with **Call Hound** (summons a Hound; limit
  2, vanish, lifetime 3), so a computer side summons in chapter 2.
- `rules.rescue`: `carry`, `canCarry`: not units tagged `mounted` (Cavalier and the pegasus rider get the tag) and
  the carrier's Strength at least the other's Defense; `carrierStats`: Skill and Speed halved.
- `data/supports.json`: pairs Eirik–Kell, Eirik–Lyra, Lyra–Bram, Eirik–Sera; two levels each with short
  conversations; all three point sources on; `conversations: "both"`; `maxLevels: 3`. Chapter 1's after scene gives
  Eirik–Sera points when she joined (`addSupport`), chapter 2 has an event on the `support` trigger.
- Its pinned numbers in `tests/test_demos.cpp` and the player tests that count its events are updated by the step
  that changes it.

**`editor/templates/strategy-rpg`** (what a new game starts from): the starter skills file; its named heroes become
characters; two unit types get a skill; one promotion by item; **pair up** as its rescue mode (so both modes ship in
something playable; open question 6) with a small `pairBonus`; one support pair with one level. No abilities file
(the Abilities screen starts empty with a "New ability" hint) except Shove on its Fighter.

**`tests/fixtures/`**: small frozen games for the unit tests: `srpg-skills` (one skill per `when`, condition and
effect, on a 6 x 4 map; also a hexagon copy `srpg-hex` for directions), `srpg-carry`, `srpg-pair`, `srpg-supports`
(a two-chapter campaign). `plain-economy` stays untouched.

---

## What does not change

- Games without the new files and keys: loading, rules, AI decisions, events, random numbers, saves and hashes,
  byte for byte. The loaders for units, weapons, items and maps accept everything they accept today.
- `Battle` stays a copyable value changed only through `apply`; queries stay const and free of randomness; the AI
  stays stateless.
- Unit ids, `alive` meaning "on the board", `occupant`, sight, fog rules, structures, capture, economy, research,
  supply, items, story events, objectives and the campaign flow (no new phase).
- The formula variables and the Lua sandbox. Formulas for damage, hit, crit, strikes and XP are untouched; skills
  add to their results.
- The dialogue box, portraits, the scene format, the event stream's existing entries and their fields.
- The editor's document model (one in-memory document, undo by snapshot), autosave and the helper's "nothing is
  validated on write" rule.

## Not in this version

Close-up battle scenes. Character tags, per-character art or sprites. Simulated levels for units placed above
level 1. A cap on how many skills a unit may hold, or choosing which are active. `formula` conditions and chances
in `always` blocks. Skills that change vision, weapon range or weapon wear; status effects that last several turns;
reflected damage. Abilities with an area, with a formula effect, or that attack. Per-class maximum levels and
demotion. Trading with a carried unit; maps that start with a carried or paired unit; pair-up extras (the partner
striking or shielding in combat, handing a partner over). Support conversations with choices or with commands other
than lines; support points for units of different teams; endings by support. The AI pushing, pulling, carrying or
pairing; the AI using promotion items. Dialogue choices (docs/EVENTS.md).

---

## Tests

Every step adds its tests with its code; a step is not done until the whole existing suite (unit tests, sims, the
player script tests, the editor tests) still passes with **unchanged pins**.

**Unit tests** (`tests/test_srpg_*.cpp`, fixtures above; one file per topic, under 300 lines each):
- Loader: every refusal message of this document, one test each, and a happy file per format; unknown keys.
- Compatibility: the `plain-economy` save pin and hash pin unchanged; adding each new file
  changes the hash and removing it restores the old one (the helper removes a list file instead of writing an empty
  list); `rules.rescue` / `rules.promotion` change it; rewording a
  support conversation or renaming a skill does not.
- Characters: stats and growths add up; map overrides; item precedence; identity through roster and save round trip;
  duplicate character refused; level-up rolls identical in count for a unit with zero personal growths.
- Skills: one test per condition (true and false, self and foe), per effect, per `when`; stacking and order; the
  duplicate rule; learning at creation, at level-up, through promotion and by story; standing HP bonus and current
  HP; movement effects on square and hexagon maps; `moveAgain` and `actAgain` state machines incl. save round trip
  mid-turn.
- Combat pipeline: with no modifiers, a recorded fight sequence (events and final `rng.state`) equals a golden taken
  before the change, on three seeds; each `CombatMods` field alone; strike order with `strikeFirst` and
  `multiStrike`; counter rules; activation roll order (a scripted seed where swapping two skills changes the
  outcome); no roll at chance 0 and 100; forecast equals outcome when nothing is random; `survive`, `drain`,
  `strikeAgain`.
- Promotion: each trigger; choice; refusals (level, square, wrong item); stat gains both ways; level reset / keep;
  wield after promotion; automatic retry at turn start; story trigger and command; roster carries the new type.
- Abilities: each effect on square, pointy and flat hexagon maps (all directions, map edge, impassable, occupied,
  structure costs); fog blocked cases; cost, uses, cooldown, once-per-turn for free ones; conditions; summons:
  limit, vanish chain, lifetime, `canAct`, `givesXp`, no drops, defeat rule and objectives ignoring them; save
  round trip.
- Rescue / pair up: every action's validity; penalty and bonus in `Battle::stat`; carrier falls (same square,
  nearest square, lost); story commands on carried units; fog; `exists` in defeat rules, objectives, conditions and
  the roster; switch hands over turn flags; save round trip and damaged-save refusals.
- Supports: each point source and its once-per rules; ready / gained with and without `talk`; `maxLevels`; bonus
  range and adding up; pair-up partner counts as near; Support action; story command, condition and trigger; camp
  functions; campaign carry-over, retry leaves it unchanged; save round trips.
- AI: summons and heals when it should; promotes when offered; a unit with `acted` always ends; 200-turn sims never
  hang.

**Sims** (`ote-sim`): `demo-srpg` and the template play AI against AI and the campaign (`--campaign`) to the end on
20 seeds without error lines; the event printer (`tools/sim/event_print.cpp`) names every new entry; the same seed
twice gives the same event text (determinism).

**Player script tests** (`tests/srpg_player_test.py`, headless `--input` scripts, reading the menu logs): open the
action menu and find each new entry; use Shove and see the unit moved; Rescue, move, Drop; Pair up and Switch;
promote by item through the choice screen; a Support conversation in battle; the camp between two chapters and
Continue after it; Unit details pages; forecast lines for a skill; phone-sized window and touch for the camp and
the promotion screen.

**Editor tests**: `tests/editor_api_srpg_checks.py` (the four sections round trip with unknown keys and key order
kept; empty list removes the file; problems come back in the loader's words); vitest unit tests for the model
(starter set equals the template's file; installer mapping and skipping; block picker filtering equals the tables
of section 3; reference clean-up on delete); Playwright end to end for each screen (make a character and place it;
build a two-block skill and read its summary; install the starter set; an ability; a promotion; a support pair
with a conversation; Play starts), plus the tutorial lesson's script.

---

## Build order

Steps are sized for one person in one sitting. "Owns" = files the step creates or is the only one to edit; "Must
not touch" protects the work running beside it. Every step ends by appending an "as built" note to this file under
the step's name (only that section), and never edits another contract.

The first step exists to make the rest parallel: it puts every shared declaration and every call site into the
shared files once, with inert stubs, so later steps fill in their own `.cpp` files and do not edit `battle.h`,
`database.h`, `actions.cpp`, `save.cpp`, `turn.cpp`, `loader.cpp` or `battle_menus.cpp` again. A later step that
finds it needs a new shared declaration adds it in `core/data/srpg.h` / `core/battle/srpg_state.h` (append only).

### F1. Skeleton and expressions (core) — depends on nothing
- **Scope.** (a) Data structs in a new `core/data/srpg.h` (`CharacterDef`, `SkillDef` and its block types,
  `AbilityDef`, `PromotionDef`, `RescueRules`, `PromotionRules`, `Support*`), included by `database.h`, which gains
  only the member lines and `find…` lookups. (b) Unit state in `battle.h`: `character`, `learned`, `abilityUses`,
  `summoner`, `summonAbility`, `summonLeft`, `carrying`, `carriedBy`, `moveSpent`, `acted`, `refreshed`;
  `Battle::supports`; all new `EventType`, `ActionType`, flag and `Action` members (`ability`; the factories); every
  query and private function of this document declared. (c) Stub definitions, one new file per area
  (`battle/characters.cpp`, `skills.cpp`, `promotion.cpp`, `abilities.cpp`, `rescue.cpp`, `supports.cpp`,
  `save_srpg.cpp`; `data/loader_characters.cpp`, `loader_skills.cpp`, `loader_abilities.cpp`, `loader_supports.cpp`,
  `loader_srpg_rules.cpp`, `loader_srpg_refs.cpp`; `campaign/supports_campaign.cpp`; `ai/ai_srpg.cpp`): queries
  return empty, actions are refused (`this game has no abilities`), loaders accept a missing file and refuse an
  existing one with `… is not supported by this build yet` until their step lands. (d) Call sites: `validate` /
  `apply` dispatch for the new action types; `Battle::stat` = `srpgStat(u, stat, unitStat(…))` (the hook adds skill bonuses and applies carry changes); `growthOf`;
  `endOrder` replacing the direct `done = true` of turn-ending actions; `beginTurn` hooks (`srpgTurnStart` after
  `repairTurn`, flags cleared with `moved` / `done`); `srpgTurnEnd` before `advanceTurn` on `EndTurn`;
  `srpgUnitGone` in `killUnit` and the `removeUnit` command; `srpgLevelUp` and the XP multiplier in `awardXp`;
  `dressUnit` in `makeUnit`, `applyBuild`, `applyRoster`, `storyAddUnit`; save hooks per unit, per battle, per roster
  entry and per playthrough plus the cross-unit check; the hash's four optional files; `event_visibility.cpp` cases;
  the story loader's key tables with dispatch to per-area readers; `tools/sim/event_print.cpp` names. (e)
  Expressions: `Database::exprs` (text + label, registered by loaders), `Formulas::evalExpr(index, ctx, def)`
  compiled in `init`; `FormulaCtx::battle`, `bonusA`, `bonusB`, used by `pushUnit` (`battle->stat` when set, else
  `unitStat` as today) and set by every existing caller in `combat.cpp`, `actions.cpp`; the unit table's `character`
  field. (f) `Grid::opposite(dir)`. (g) `Battle::exists`.
- **Owns.** The new files above; this one round of edits to the shared files.
- **Must not touch.** `engine/player`, `editor`, `games`, the meaning of any existing rule.
- **Accept.** The whole existing suite passes with every pin unchanged; new tests: expressions (compile error
  message, evaluation, `battle` stats equal `unitStat` when nothing is added), `Grid::opposite` on square and both
  hexagon layouts, a save of `plain-economy` and of `demo-srpg` byte-identical to the build before, every new
  action refused with its message.

#### F1 as built

Everything in the scope is in place and inert: the existing games load, play and save exactly as before
(`tests/test_srpg_compat.cpp` pins save text, event streams, random state and data hashes taken from the build
before this step; all `ote-sim` summaries and verbose logs are identical).

**Where things are (read these, not `battle.h`).**
- `core/data/srpg.h`: all data structs, the limits, `CmpOp` (moved here from `events.h`), `kSupportTextBase`.
  References are kept twice: the id as written (`toId`, `typeId`, `skillId`, `abilityId`, `itemIds`) and the index
  that `resolveSrpgRefs` fills.
- `core/battle/srpg_state.h`: `AbilityUse`, `SupportState`, `AbilityOption`, `ForecastNote`, `CombatSetup`,
  `StrikeStep`, `SkillCtx`; `CombatMods` / `StrikeMods` are only forward declared (F2 defines them in
  `combat_mods.h`).
- `core/battle/srpg_api.h` (public) and `core/battle/srpg_internal.h` (private) are fragments included inside
  `class Battle`. **A later step that needs one more member function appends it there** (append only) and never
  edits `battle.h`. This is a reading of "declared in `battle.h`": the declarations are part of the class, the text
  lives in two files that many steps can append to without conflicts.
- `core/battle/srpg_hooks.cpp`: the shared hooks and the order in which they ask the areas. `srpgStat` = base +
  `skillStatBonus`, then `rescueStat`, never below 0 (returns the base untouched when the game has no skills and
  rescue is off); `dressUnit` = `characterDress`, `learnUpTo`; `srpgTurnStart` = abilities, promotion, skills;
  `srpgUnitGone` = rescue, summons; `srpgLevelUp` = skills, promotion; `srpgAfterCombat` = skills, supports.
- `core/battle/save_srpg.h/.cpp`: the save hooks and their fixed order (skills, abilities, rescue per unit; supports
  per battle and per playthrough; skills per roster entry). Stubs per area: `save_skills.cpp`, `save_abilities.cpp`,
  `save_rescue.cpp`, `save_supports.cpp` (not named in the scope; one file per owner instead of one shared file).
- `core/data/loader_srpg.h`: every loader entry point. `loader_promotion.cpp` was added beside the files of the
  scope (P1's `promotions` / `promotes` keys, the `promote` command and the `promoted` trigger).
  `loader_srpg_rules.cpp` has two owners: R1 (`loadRescueRules`, `readUnitPairBonus`) and P1 (`loadPromotionRules`).
- Story loader: the per-area readers are `StoryLoader` methods declared in `loader_events.h` (`promoteCommand`,
  `promotedTrigger`, `skillCommand`, `hasSkillCondition`, `supportCommand`, `supportCondition`, `supportTrigger`);
  each gets the whole command / condition / "when" object and reads all its keys. The "is not a command" message
  still lists only the old commands (the new ones are in a list of their own, `kSrpgCommandKeys`).
- `tools/sim/event_print_srpg.cpp`: wording of the new events. `core/ai/ai_srpg.cpp`: `ai::considerSrpg`, declared
  in `ai_internal.h`, called by nothing yet (AI1 adds the call).

**Readings of the contract (keep to these).**
- `Action::ability` is the member; the factory is `Action::useAbility(unit, ability, target = -1, cell = kNoCell)`
  (a member and a static function cannot share a name). The other factories: `unitAction`, `promote(unit, type =
  -1)`, `rescue`, `drop(unit, cell)`, `giveUnit`, `takeUnit`, `pairUp`, `switchPair`, `support(unit, target)`.
- Refusals of the stubs: `this game has no promotions`, `this game has no abilities`, `this game has no rescue or
  pair up`, `this game has no supports`. The usual unit checks come first (`that unit cannot act now`, ...).
- `endOrder(u)`: `acted` when the unit is alive, `hasMoveAgain(u)` and `moveSpent < its movement points`, else
  `done`. In `applyAttack` it is only called when the attacker is still alive: a fallen attacker was never marked
  done, and marking it would change saves. `endOrder` is used by capture, attack, use item, supply and the story's
  `endTurn` (talk). Still direct, on purpose: wait, an ambush that ends the turn, a built unit, `beginTurn`.
- A unit with `acted` may only move or wait (`that unit can only move or wait now`); its move ends it. The movement
  budget of that second move (movement points minus `moveSpent`) is not enforced in `movement.cpp` yet: K1.
- `killUnit(unit, events, killer = -1)` gained the killer; `srpgUnitGone` runs at its end and in `removeUnit`.
- `Unit::character` is never saved: `Battle::characterOf(u)` derives it from the story id (added to `srpg_api.h`),
  and `readUnit` sets it before the save hooks. Maximum HP in `readUnit` is now `b.stat`, read after the hooks.
- `growthOf` returns the type's growth as it is (no clamping); C1 / K1 add theirs.
- `skillsOf`, `abilitiesOf`, `growthOf`, `hasSkill` take `const Unit&`; the options queries take a unit id and the
  cell it would act from.
- The signatures of `strikeActivations`, `strikePassives`, `forecastNotes`, `CombatSetup`, `StrikeStep` and
  `SkillCtx` are F1's own (the contract gives their meaning, not their shape). F2 and K2 may change them; nothing
  else depends on them.
- `evalExpr(index, ctx, def)`: a result of nil or false is 0, true is 1; a bad index or a run-time error gives
  `def`. An expression with empty text is a load error ("the formula is empty"); errors start with the label given
  to `Database::addExpr(text, label)`.
- Hash: `characters.json`, `skills.json`, `abilities.json`, `supports.json` are mixed in after `research.json`,
  each only when the file exists; `supports.json` gets the map-file treatment (wording blanked) and `levelNames`
  is a cosmetic key.
- New in loading, and a change for hand-made games: the keys `skills`, `learns`, `abilities`, `promotions`,
  `pairBonus` on a unit type, `skills` on a weapon and `skills` / `promotes` on an item were ignored before and are
  refused now (`"skills" is not supported by this build yet`). No game, fixture or template in the repository
  uses them.

**Formula call sites (all set `ctx.battle`).** `actions.cpp` `applyCapture` (capture amount); `combat.cpp`
`strikeForecast` (damage, hit, crit, strikes) and the XP awards of `applyAttack`. The player, the computer opponent
and the editor server never evaluate a formula themselves (the editor only calls `Formulas::init`).

**`Battle::exists`** is defined and tested; nothing calls it yet. R1 switches the "is it still there" tests over
(`checkDefeat`, story conditions and commands, objectives, `takeUnits` in `chapter.cpp`, saves).

**Left for others.** `engine/player/battle/battle_view.cpp:112` now warns that the new `EventType` values are not
handled in its switch (F1 may not touch the player; U0 adds the cases). Nothing else is open.

### F2. Combat pipeline (core) — depends on F1; parallel with C1, K1
- **Scope.** The section "Combat pipeline": `core/battle/combat_mods.{h,cpp}` (`CombatMods`, `combatMods` asking the
  three stubbed sources `skillCombatMods`, `auraCombatMods`, `supportCombatMods`; `strikeOrder`), and `combat.cpp`
  rewritten onto it (`strikeForecast` with mods, counter rules, order, `doStrike` steps 1 to 8 with the stubbed
  activation hook `strikeActivations`, `afterCombatHooks`). `Forecast` gains `multi`, `defenderFirst`, `notes`.
  A test-only way to inject mods (`Battle::testMods`, compiled in tests).
- **Owns.** `combat.cpp`, `combat_mods.*`. **Must not touch.** `skills*.cpp`, loaders, saves.
- **Accept.** Golden fights (events and final rng state, three seeds, `demo-srpg` and `plain-economy`) identical to
  before; each mod field, order and counter rule tested through injection; `ote-sim` numbers unchanged.

#### F2 as built

The pipeline is in place and inert: with no source of modifiers a fight computes the same numbers, rolls the same
random numbers and makes the same events as before. `tests/test_srpg_golden_fights.cpp` pins that (21 battles on
`demo-srpg`, `demo-economy`, `demo-hex`, `plain-economy`, `hex-pointy` and `hex-flat`, several seeds: every attack
the side to move could make is forecast and fought on a copy; taken from the build before this step), next to the
pins of `test_srpg_compat.cpp`; all `ote-sim` summaries and verbose logs are identical.

**Where things are.**
- `core/battle/combat_mods.h`: `CombatMods` (the fields of the contract; `addStat(stat, by)`,
  `raiseMultiStrike(n)`), `StrikeMods`, `applyPercents`, `rollActivation`, `kMaxStrikeDamage`, `CombatTestMods`.
  Sources include this header (it includes `battle.h`).
- `core/battle/combat_mods.cpp`: `combatMods`, `counterWeapon` (now setting up step 1), `setUpCombat` (steps 1 to
  3), `strikeForecast(setup, counter, own, foe, out)` (one side's numbers), `foldStrikeMods`, `strikeOrder`.
- `core/battle/combat.cpp`: `forecast` / `forecastFight`, `doStrike(setup, counter, extra, events)` (steps 1 to 8),
  `applyAttack`.
- New private declarations are at the end of `srpg_internal.h`; `Battle::testMods` at the end of `srpg_api.h`;
  `struct CombatTestMods;` at the end of `srpg_state.h`.
- **Two declarations in `battle.h` are now dead** (F2 may not edit that file): `strikeForecast(atk, atkCell, def,
  defCell, weapon, out)` and `doStrike(atk, def, weapon, baseFlags, events)`. Nothing defines or calls them; whoever
  next has `battle.h` open should delete the two lines.

**What K2 and S1 build against.**
- `skillCombatMods`, `auraCombatMods`, `supportCombatMods(setup, attacker, defender)`: add to the two bags;
  `combatMods` clears them first and calls the three in that order. They run for every forecast **and again before
  every strike**, so they must be cheap and must not roll or change anything. `setup.counters` is not yet known
  when they run for the set-up itself (it depends on what they return): do not read it there.
- `strikeActivations(setup, counter, out, strikeEvents, struckEvents)`: one `StrikeMods` bag for the strike, holding
  the striker's "strike" effects and the struck unit's "struck" effects together. Roll each activation with
  `rollActivation(rng, chance)` (the no-roll rule at 0 and 100 lives there). `strikePassives` fills the same bag
  from the blocks without a chance, `forecastNotes` lists the others. F1's signatures are unchanged.
- The same bag fields are asked for on the extra strike of a `strikeAgain`; its `strikeAgain` is ignored.
- `setup.weaponD` is the defender's weapon in use even when it does not answer (out of range): -1 only when it
  has no weapon that may ever answer this attacker.
- `afterCombatHooks` of the scope is `srpgAfterCombat` (F1's name), called where the contract says.

**`Battle::testMods`** is a pointer to a `CombatTestMods` (four optional `std::function`s: `combat`, `strike`,
`passive`, `notes`), called right after the real sources. It is always compiled (the core library is built once
for the programs and the tests, so a member that exists only in tests would give the class two layouts); nothing
but the unit tests sets it, it is never saved, and a copied battle shares it. `tests/srpg_combat_fixture.h` wraps
it (`Duel`).

**Readings of the contract (keep to these).**
- **Limits apply to formula plus modifiers.** The formula's value is rounded down but not cut to its range before
  the modifiers are added: a hit formula giving 130 with 20 avoid against it is 100; a damage formula giving -1
  with +3 damage is 2. Without modifiers this equals the old clamping. A formula that returns "not a number" now
  counts as 0 (it used to be undefined).
- **A strike's own effects are added to the numbers of step 1 after their limits**: `hit` / `avoid` and `crit` /
  `critAvoid` are added to the 0..100 value and limited again; `damage` / `damageTaken` to the damage after the
  combat's percents (not below 0), then a critical multiplies, then the strike's `damagePercent` and after them
  its `damageTakenPercent`. The forecast does the same without the critical.
- Percents: the striker's `damagePercent` list first, then the struck unit's `damageTakenPercent`; a percent
  below 0 counts as 0; a strike never does more than 1,000,000. `multi` is kept within 1..4.
- `drain` of several blocks adds up; the heal is limited to the missing HP and makes no event when that is 0; it
  comes after the `Attack` event and before the weapon's wear. It needs damage dealt (a miss or 0 damage: none).
- `strikeAgain` gives its extra strike after a miss too, and not when either fighter has fallen. The extra strike
  is not counted against the fighter's strikes, wears the weapon, spends no ammunition.
- `survive` sets `kSurvived` only when it really cut the damage.
- `strikeFirst` on the attacker, or on both, or on a defender that does not counter, changes nothing
  (`defenderFirst` false). `noCounter` on the defender and `counterAnyRange` on the attacker mean nothing in that
  fight.
- `counterAnyRange` still needs `rules.counterAttacks`, a weapon with `counter`, ammunition for it and a weapon
  that may hurt the attacker.
- Whether the defender counters, the order and the counts come from the forecast made after the story's "before a
  fight" events; modifiers that appear during the fight only change the numbers of later strikes.
- Ammunition: one per fighter, at its first strike that is really made (a defender that strikes first spends its
  own first; an attacker struck down before its first strike spends none).
- `StrikeStep::count` is the number of strikes in a row for that turn; fewer are made when a fighter falls.

**Tests.** `test_srpg_golden_fights.cpp`, `test_srpg_combat_mods.cpp` (each field), `test_srpg_combat_order.cpp`
(order, counter rules, ammunition, carried weapons), `test_srpg_combat_strike.cpp` (one strike: survive, drain,
strikeAgain, events), `test_srpg_combat_rolls.cpp` (no roll at 0 and 100, roll order, activations before the hit
roll). The activation tests use stand-in skills through `testMods`; K2 repeats them with real skills.

**Left for others.** Nothing in the pipeline is open. The player's forecast panel does not show `multi`,
`defenderFirst` or `notes` yet (U1).

### C1. Characters (core) — depends on F1; parallel with F2
- **Scope.** Section 1: `loader_characters.cpp`, the map / `addUnit` `character` key (`map_loader.cpp`
  `readUnitHead` / `readUnitBody`, `loader_story_commands.cpp` key list), `characters.cpp` (`dressUnit`, `growthOf`),
  roster identity. Skill and ability lists on characters are parsed here as id lists and resolved in
  `loader_srpg_refs.cpp` (unknown ids are refused there once K1 / A1 have real lists; until then a non-empty list is
  refused as "not supported yet").
- **Owns.** `loader_characters.cpp`, `characters.cpp`, `tests/test_srpg_characters*.cpp`, fixture `srpg-chars`.
- **Must not touch.** `combat.cpp`, skills, saves' format.
- **Accept.** The Characters tests of "Tests"; the campaign suite unchanged.

#### C1 as built

Characters load, dress units and level up as section 1 says; games without `data/characters.json` are unchanged
(pins in `tests/test_srpg_compat.cpp` untouched, sims identical).

- `loader_characters.cpp`: `loadCharacters` (all refusals of the contract, worded as written; `unknown key` has no
  "expected ..." tail) and `resolveCharacterRefs` (called from `resolveSrpgRefs` as before). Skill and ability ids
  are resolved for real with `findSkill` / `findAbility`: until K1 / A1 land those lists are empty, so any id is
  refused as `"skills" names unknown skill "<x>" (data/skills.json)` (not "not supported yet"); K1 / A1 need to
  change nothing here. `"learns"` entries are numbered from 1 in messages.
- Map units and `addUnit` are resolved at load time in `readUnitHead` / `readUnitBody` into the plain
  `MapUnitDef` they stand for (story id = character id; type / name / level default to the character's; stats =
  this map's unit type + the character's `stats`, floored at 0, then the map's `stats`; items = map, else the
  character's, else the type's). `MapUnitDef` and `database.h` gained nothing. The `addUnit` key list has
  `character`. An `addUnit` of a character skips the "id already used" load check (a repeated add is legal) and
  `Battle::storyAddUnit` adds nothing, with an info log, when that character is already in the battle (dead or alive).
- `characters.cpp`: `characterOf` (story id lookup, -1 without characters), `characterDress`, `growthOf` = type
  growth + character growth, 0 if negative. K1 adds the skills' growth bonus at the marked line. A game without
  characters returns the raw type growth as before.
- Roster identity needed no code: `applyRoster` keys on the story id, `dressUnit` runs after it and re-derives the
  character. Nothing about characters is saved; the hash already mixed `characters.json` in (F1).
- Tests: `tests/test_srpg_characters_loader.cpp`, `test_srpg_characters.cpp`, helper `srpg_chars_fixture.h` (a
  writable copy of `tests/fixtures/srpg-chars`). The stub test for `characters.json` was removed (it now loads).

### K1. Skills: data, sources, standing effects, learning (core) — depends on C1; parallel with F2, P1
- **Scope.** Section 3 except the combat and timed blocks: `loader_skills.cpp` + `loader_skill_blocks.cpp` (the whole
  format, every refusal, all seven `when`s parsed), references from units, characters, items, weapons;
  `skills.cpp` (`skillsOf`, learning, `teachSkill` / `forgetSkill` / `hasSkill`), `skill_cond.cpp` (the condition
  evaluator, used later by K2 and A1: `bool Battle::skillCondHolds(const SkillCond&, const SkillCtx&) const`),
  `skill_standing.cpp` (`always` effects: stats, growths, movement in `movement.cpp`, `xpPercent`, `moveAgain` with
  `endOrder` and the `acted` rules in `validate`), `save_skills.cpp`.
- **Owns.** Those files; the `movement.cpp` change. **Must not touch.** `combat.cpp`, `combat_mods.*`.
- **Accept.** Loader, standing, learning, movement and move-again tests; saves of skill-less games unchanged.

#### K1 as built

Skills load, give their standing effects and are learned; games without `data/skills.json` are unchanged (every pin in
`tests/test_srpg_compat.cpp` untouched, the four sims identical to the build before).

**Where things are.**
- Loader: `loader_skills.cpp` (the file, `skills` / `learns` on unit types, `skills` on weapons and items, the story's
  `teachSkill` / `forgetSkill` / `hasSkill`, `resolveSkillRefs`) and `loader_skill_blocks.cpp` (a block: `when`, `if`,
  `chance`, `aura`, `do`; every refusal for all seven `when`s). A menu ability's `if` (A1) reuses
  `readSkillConds(c, db, list, where, SkillCondScope, labelPrefix, out)` and `resolveSkillConds`, both declared in
  `loader_srpg.h`; the scope says whether `"who": "foe"`, `initiating` / `distance` and `formula` are allowed.
- Battle: `skills.cpp` (`skillsOf`, `hasSkill`, learning, the story commands), `skill_cond.cpp` (`skillCondHolds`, all
  eleven conditions), `skill_standing.cpp` (`standingEffects` and what is built on it: stat and growth bonuses, move
  cost cap, `pass`, `moveAgain`, `xpPercent`), `save_skills.cpp`. `skill_combat.cpp` and `skill_timed.cpp` hold the
  stubs K2 replaces (K2 owns both; `skills.cpp` has nothing of K2 left in it).
- Fixture `tests/fixtures/srpg-skills`, helper `tests/srpg_skills_fixture.h` (`Copy`, `World`), tests
  `test_srpg_skills*.cpp` (27 tests; 968 in the whole suite, and the registrar of `tests/test.h` holds at most 1024).

**For the steps after.** K2: `Battle::skillCondHolds(cond, SkillCtx)` judges one condition (`SkillCtx::combat` and
`initiating` give the foe points a meaning; `cell` / `foeCell` / `weapon` / `foeWeapon` override where and with what
a fighter is judged); `skillsOf(u, weapon, out)` takes a weapon index, `-1` = the weapon in use outside a combat
(`restingWeapon(u)`, public) or `-2` = no weapon at all (a defender with no answering weapon); the parsed blocks are in
`SkillBlock` as F1 declared them (`aura`, `hasChance` / `chance` / `chanceExpr`, effects with `to` / `within`).
A1: abilities from skills arrive through `SkillEffect::Kind::Ability` (`ability` is resolved against
`db.abilities` in `resolveSkillRefs`, so `loadAbilities` must fill `db.abilities` before it, as the load order says).
P1: `learnUpTo(unit)` is as F1 declared it. U1 / E2: skills on a unit are read with `skillsOf`; `Unit::acted`,
`moveSpent` and `Database::turnFlagSkills` are what the player needs for "move again".

**Readings of the contract (keep to these).**
- Learning order. `learnUpTo` adds every entry at or below the level, lowest level first, the type's before the
  character's on a tie ("oldest first"). A new unit therefore has the learned skills of its level, and a roster
  unit gets back what it forgot by story if the entry is at or below its level.
- A maximum HP that changes because a skill is learned or forgotten (level up, `teachSkill`, `forgetSkill`) raises the
  current HP by the gain, or cuts it down to the new maximum (never below 1). Items and equipment keep their old rule.
- A unit that is not in `Battle::units` yet (being made, or read from a save) only has its blocks without conditions
  looked at; its HP stat only ever does.
- `teachSkill` / `forgetSkill` need their `to` / `from` filter (`{}` = every unit). `forgetSkill` makes no event.
  `hasSkill`'s `unit` defaults to any unit.
- Conditions. `"who": "foe"` is accepted on `hp`, `weapon`, `terrain`, `structure`, `unit` and `carrying` only; `near`,
  `round`, `initiating`, `distance` and `formula` are owner / battle wide. `structure: false` = on no structure.
  A `formula` in an aura block is allowed (`b` is nil there). `near` counts nobody for a unit off the board.
- Effects. Ranges: a number of hit, crit, damage, avoid, critAvoid, damageTaken and each stat bonus or growth is
  -1000..1000, percents 0..1000 (`xpPercent` 1..1000), `strikes` -7..7, `multiStrike` 2..4, `drain` 1..100,
  `maxMoveCost` 1..100, `heal` and `hurt` 1..1,000,000 (`heal` percent 1..100). `to` / `within` where the table allows
  them; `within` defaults to 1; `hurt` defaults to `foe` in `afterCombat` and to `enemies` in `turnStart`. Flags must be
  `true`. A `combat` block's `stats` may not change the HP or the move stat, whoever receives them. `chance` is any
  whole number (clamped to 0..100 when rolled).
- Formulas are registered with `Database::addExpr` and compiled by `Formulas::init`, like every formula, so a syntax
  error appears there (`data/skills.json: skill "x": block 1: condition 2: "formula": <Lua message>`), not in `loadGame`.
- `maxMoveCost` lowers what a square costs after the impassable test (a wall stays a wall) and `pass` leaves enemies
  out of the walls; with fog `pathStop` (in `fog_rules.cpp`, the one line of another area K1 touched) lets a walk
  through an unseen enemy go on unless that enemy is on the last square. A unit with `acted` has the budget
  `movePoints - moveSpent` (`movement.cpp`); fuel still limits it.
- Saves. `moveSpent` / `acted` / `refreshed` are written only in a game where `turnFlagSkills` is set, and refused
  elsewhere (`... this game has no "moveAgain" or "actAgain" skills`; in a game without any skill: `this game has no
  skills`). `moveSpent` is written only above 0. A skill list must name valid skills, none twice.
- The accessory rule now says `an accessory needs a "bonus" that changes at least one stat, or "skills"`.
- `hasMoveAgain` stays private (only `endOrder` asks); the combat and timed effects (K2) parse and load but do nothing
  in a fight yet.

### K2. Skills: combat and timed effects (core) — depends on K1 and F2
- **Scope.** `skill_combat.cpp` (`skillCombatMods`, `auraCombatMods`, `strikeActivations`, forecast notes),
  `skill_timed.cpp` (`turnStart`, `defeat`, `afterCombat`, `actAgain`, `UnitHurt`), `SkillActivated` events.
- **Owns.** Those two files. **Must not touch.** `combat.cpp` beyond bug fixes agreed with F2's notes.
- **Accept.** The Skills combat tests, roll-order test, the fixture `srpg-skills` sim on 20 seeds.

### P1. Promotion (core) — depends on C1; parallel with K1, K2, A1, R1, S1
- **Scope.** Section 2: `loader_srpg_rules.cpp` (`rules.promotion`), promotions on unit types and `promotes` on
  items (`loader_content.cpp` calls a reader in `loader_promotion.cpp`), `promotion.cpp` (`promote`,
  `promotionOptions`, the Promote action, the item path inside `items.cpp`'s UseItem through one call, automatic
  promotion in `srpgLevelUp` / `srpgTurnStart`), story command and trigger. Learned skills on promotion go through
  K1's `learnUpTo(unit)` (a stub that does nothing until K1 lands).
- **Owns.** `loader_promotion.cpp`, `promotion.cpp`. **Must not touch.** skills, combat.
- **Accept.** The Promotion tests.

#### P1 as built

Promotion loads and plays as section 2 says; games without `promotions` / `promotes` are unchanged (compat pins
untouched, sims identical).

- Loading: `loader_promotion.cpp` (`readUnitPromotions`, `readItemPromotes`, `resolvePromotionRefs`, the story
  command `promote` and trigger `promoted`), `loadPromotionRules` in `loader_srpg_rules.cpp`. Every refusal of the
  contract is worded as written; the message of an entry starts `"promotions" entry N: `. `"to"` is checked in
  `resolvePromotionRefs` (it may name a type further down), the other checks while reading. An item named in `items`
  that does not exist is refused with the same "not a promotion item" message. More than 8 entries (`kMaxPromotions`):
  `too many "promotions" (at most 8)`. `rules.promotion` has one key (`levelAfter`); any other is `unknown key`. The
  "this game has no promotions" refusal of F1 stays for a game none of whose types promote.
- Running: `promotion.cpp`. `promote` follows the eight steps of the contract in order; the event's `data` is the real
  change per stat after the floor at 0. The shared questions are `promotionCandidates` (by level and by item or offer)
  and `chosenPromotion` (adds the choice by type and the "could stand" check, and says why not): `promotionOptions`,
  the Promote action and the item path all use them. The item path is three small edits in `items.cpp` (`useTargets`
  counts a promotion as a use, the validator names the reason, `applyItemAction` calls `promoteByItem` after the
  heal / raise and the use is spent).
- Refusal wording: `that item cannot promote this unit now`, `that unit has no promotion on offer`, `that promotion is
  not on offer`, `a <Type name> could not stand on this square`.
- Tests: `tests/test_srpg_promotion_loader.cpp`, `tests/test_srpg_promotion.cpp`, helper `srpg_promo_fixture.h` (a
  small game written into a temp folder). The promotion lines of `test_srpg_stubs.cpp` were removed (they now load).

Where the contract was silent:

- **Automatic retry is stateless** (nothing new is saved): at the start of a side's turn every unit of that side that
  meets an `auto` entry (level at or above `fromLevel`, can stand there) promotes. So a unit that begins a battle
  already at that level promotes at its side's first turn start, not only one that just levelled up. The first
  `auto` entry that fits wins; one promotion per unit per pass (a new type's own `auto` entries wait for the next
  level up or turn start).
- **Order inside an action:** an item or an offer ends the unit's turn (`endOrder`) before the promotion runs, so the
  story events a promotion starts see a finished unit; for an item the use is spent and `ItemBroke` comes before
  `UnitPromoted`.
- **A promotion item on an ally** is never a promotion (only the unit using it on itself promotes); `target: "ally"`
  items are not refused for having `promotes`, they just heal or raise.
- **`unitType` that is not on offer** is refused even if the item would otherwise be drunk for its heal; with `-1` a
  `heal + promotes` item used by a unit that cannot promote just heals.
- **Story `promote` without `to`:** the one-promotion rule is a load error only for a unit the map places itself;
  for one an `addUnit` brings in, a type with a different count logs a warning and does nothing when it runs. A
  story promotion needs no level and does not use `fromLevel` / `items` / `atLevel`; a listed pair still gives its
  `gains` and `levelAfter`.
- `promotionOptions` leaves out types the unit could not stand as (the Promote action and item then say why); two
  entries with the same `to` give it once.
- Vision: a promotion stamps earlier events with the old sight first (`markSight`), like a move, since the new type
  may see differently.

### A1. Abilities and summons (core) — depends on K1 (conditions); parallel with K2, P1, R1, S1
- **Scope.** Section 4: `loader_abilities.cpp`, `abilities.cpp` (options, targets, validate, apply, costs, turn
  ticks), `ability_moves.cpp` (push, pull, swap, moveBehind through `Grid`), `summons.cpp` (create, limit, vanish,
  lifetime, the defeat-rule and filter exceptions in `turn.cpp` `checkDefeat`, `objectives.cpp`, `story.cpp`),
  `save_abilities.cpp`.
- **Owns.** Those files and the three small summon exceptions. **Must not touch.** `combat.cpp`, rescue files.
- **Accept.** The Abilities tests on square and both hexagon layouts, with and without fog.

### R1. Rescue and pair up (core) — depends on F1; parallel with everything after F1
- **Scope.** Section 5: `loader_srpg_rules.cpp` (`rules.rescue`; shared file with P1: each owns its own function),
  `pairBonus` on unit types, `rescue.cpp` (carry actions, `carrierStats`, the carrier falls), `pair_up.cpp` (PairUp,
  SwitchPair, bonus), the `exists` call sites (`turn.cpp` `checkDefeat`, `story.cpp` conditions and filter commands,
  `objectives.cpp`, `campaign/chapter.cpp` `takeUnits`), `save_rescue.cpp`.
- **Owns.** `rescue.cpp`, `pair_up.cpp`, `save_rescue.cpp`. **Must not touch.** `combat.cpp`, abilities files; where
  it edits the same functions as A1 (`checkDefeat`, `objectives.cpp`), each change is one clearly marked line.
- **Accept.** The Rescue / pair up tests; an audit note listing every `.alive` use it looked at and why it is right.

#### R1 as built

Carry and pair up are in, tested and inert for every game without `rules.rescue` (the compat pins, all sims and all
player / editor suites are unchanged).

**Where things are.** `data/loader_srpg_rules.cpp` (`loadRescueRules`, `readUnitPairBonus`); `battle/rescue.cpp`
(queries, refusals, Rescue / Drop / Give / Take, the carrier falling, `rescueStat`); `battle/pair_up.cpp`
(`SwitchPair`, `canSwitchPair`); `battle/save_rescue.cpp`. New private members were appended to
`srpg_internal.h` (the `*Why` refusal functions, `linkCarried` / `unlinkCarried`, `putDownCarried`, `defeatCarried`,
`storySeparate`, `storyMoveCarried`, `storyRemoveCarried`, `flushLostCarried`, `lostCarried_`); nothing was added to
`srpg_api.h`, the public queries were already declared. `checkUnitsSrpg` / `checkUnitsRescue` now take a non-const
`Battle&` because the check also derives `Unit::carriedBy` (the contract's header comment already said so).

**Readings of the contract (keep to these).**
- Refusal wording (the contract fixes none): `this game uses pair up, not carrying` / `this game uses carrying, not
  pair up` for an action of the other mode (`Drop` is shared); then per action `that unit is not on the board`, `a
  unit cannot rescue itself` / `... pair up with itself`, `only a unit of the same side can be rescued` / `... paired
  with`, `that unit is not next to it`, `that unit already carries somebody` / `that unit is already in a pair`, `it
  cannot carry that unit` (Rescue, TakeUnit) / `that unit cannot carry it` (GiveUnit) / `that unit cannot take it
  behind` (PairUp), `that unit is not carrying anybody` / `that unit has nobody behind it`, `that square is not next
  to the unit`, `the unit could not stand on that square`, `that square is occupied`, `the unit behind could not stand
  on this square`. The unit checks of `validate` (`that unit cannot act now`, `already acted`, ...) come first, so a
  game without rescue still answers `this game has no rescue or pair up`.
- `canCarry` is evaluated with `a` = the unit that stays on the board and `b` = the one that leaves it; for `PairUp`
  that is `a` = the partner in front, `b` = the unit that goes behind; for `GiveUnit` / `TakeUnit` `a` = the unit
  that would receive, `b` = the carried unit (no square, so no `tb`; `dist` is 0). A formula that fails means no.
- Stats: `carrierStats` first (percent, then the added number, never below 0), then the pair bonus is added; the
  health stat is never touched by either (the loader also refuses `hp` in `pairBonus`, a case the contract's list did
  not name). A unit type's `pairBonus` is always stored with one entry per stat, so `{}` is a real zero bonus that
  replaces the game's; no key = the game's bonus.
- Every action that ends a turn goes through `endOrder`. `PairUp` ends the turn of the unit that goes behind (the
  contract's "ends `unit`'s turn"); the unit in front keeps its turn. Give / Take set `moved` on the acting unit and
  nothing else; they can be repeated without limit (an AI must not loop on them: AI1).
- A unit put down (`Drop`, forced) gets `done` and `moved`; `SwitchPair` also hands over `refreshed` with the three
  flags the contract names, and is refused when the unit behind could not stand on the front unit's square
  (`canSwitchPair` is false then, and also for a unit that has acted).
- `Drop` onto a square an unseen enemy holds (fog): `UnitDropped` with `kBlocked`, the blocker's unit id in `d`, the
  action is spent and nothing else changes. Without fog an occupied square is refused. A voluntary `Drop` (and the
  story's `moveUnit` on a carried unit) fires the story's "reach" trigger for the unit that was put down; a forced
  drop does not.
- The carrier falls: `rescueUnitGone` reads the square it fell on from the `UnitDied` / `UnitRemoved` event just
  pushed (`killUnit` has cleared the cell already), then `nearestFreeCell`. A unit with no square is killed through
  `killUnit` (so `UnitDied`, the leader rule and its own summons follow) and its `drop` items go to the killer; its
  story `defeated` events run from `settle()` (`flushLostCarried`), not inside `killUnit`, which may be in the
  middle of a sweep.
- A defeated side takes the units it carries along: `checkDefeat` kills them (`defeatCarried`, `UnitDied` on the
  carrier's square, no `UnitDropped`) before it kills the carriers, and their `defeated` events run after the sweep
  like everybody's.
- Story: `changeSide` on a carrier or a carried unit puts the carried unit down on the nearest free square to the
  carrier first (`UnitDropped`, `kForced`); no square = neither unit changes (logged). `removeUnit` on a carried unit
  is `UnitRemoved` from the carrier's square (the carrier is unharmed). `giveItem`, `focus` and the talk menu keep
  skipping a carried unit (they need a square).
- `restoreBattleState` also refuses (in every game, not only with rescue) a unit that is not alive but has hit points
  and nobody carries it. Existing saves never have one.

**The `.alive` audit.** About 90 uses in `engine/core`, `tools` (the player's 17 are U0's / U4's). Changed to
`Battle::exists` (a carried unit counts): `turn.cpp` `checkDefeat` (the `noUnits` rule); `objectives.cpp` kind
`defeated`; `story.cpp` conditions `alive` / `dead` and `carries` (a carried unit keeps its items; the contract names
only alive / dead and the filter commands, this one is the same question); `story_commands.cpp` `removeUnit`,
`moveUnit`, `changeSide`; `campaign/chapter.cpp` `takeUnits` (a carried unit goes to the roster as living);
`research.cpp` `finishResearch` / `setStartingResearch` (a carried unit's hit points must stay within a maximum that
research changes, or the next save would be refused); `tools/sim/main.cpp` (survivor count). Left on `alive`, because
a carried unit must not take part: `canAct`, movement / reach, `attackOptions`, `forecast`, `applyAttack` and
`doStrike`, `killUnit` (a no-op for a carried unit, which is why the lost / defeated paths set `alive` for a moment),
`awardXp`, sight (`computeTeamSight`, `unitVisibleTo`), items (use, give, trade, equip: nobody trades with a carried
unit), supply, repair, fuel, capture (`canCaptureAt`, a capture is abandoned on rescue), `beginTurn` flags (all
units are reset, no `alive` test), `endOrder`, the story's `sees` condition and the talk menu / `focus`, `giveItem`,
the `EndTurn` talker, the saves (`alive` / `cell` / `hp` / capturer checks: a carried unit is saved as not alive with
its hit points and no cell), the scene conditions `alive` / `dead` of the campaign (they read the roster, not units),
and the whole computer opponent (`ai*.cpp`: a carried unit is not a target, a threat or an actor; AI1 adds rescue
moves). Both build trees compile with no new warning.

**Left for others.** AI1: `rescueTargets` / `dropCells` / `giveTargets` / `takeTargets` / `canSwitchPair` are the
queries to build on (they judge as if the unit stood on `from`, ignore whose turn it is, and are fog-aware only in
`dropCells`); the actions are `Action::rescue / drop / giveUnit / takeUnit / pairUp / switchPair`. U4: the events are
as the contract says, with `UnitDropped::d` (blocker) and the order `UnitDied`, then `UnitDropped` for a fall;
`Unit::carrying` / `carriedBy` tell the player which units to draw behind another; the player still treats a carried
unit as dead (`!u.alive`), so it needs `exists` / `carriedBy` where it lists units. No game turns rescue on yet.

### S1. Supports (core and campaign) — depends on C1 and F2; parallel with K1, K2, P1, A1, R1
- **Scope.** Section 6: `loader_supports.cpp`, `supports.cpp` (state, point sources at `srpgTurnEnd`, after combat
  and on heals, levels, the Support action, `supportCombatMods`, `storyText` base), story command / condition /
  trigger in map events and scenes (`loader_campaign_scene.cpp`, `campaign/scene.cpp`), `supports_campaign.cpp`
  (`setStartingSupports`, take-over in `finishChapterBattle`, `pendingSupports`, `playSupport`), `save_supports.cpp`
  and the playthrough save.
- **Owns.** Those files. **Must not touch.** skills files, the player.
- **Accept.** The Supports tests; campaign save pins of games without supports unchanged.

#### S1 as built

Supports work as section 6 says. A game without `data/supports.json` is untouched (every entry point returns at
once on an empty pair list; `test_srpg_compat.cpp`, the golden fights and all `ote-sim` summaries are identical).

**Where things are.**
- `core/battle/support_rules.h/.cpp`: the rules that need no battle, used by `Battle` and by the camp alike:
  `startingSupports`, `supportReady`, `supportTalkPending`, `supportAddPoints`, `supportSettle`, `supportStateFits`,
  `supportLevelsHeld`, `supportLevelName(rules, level)` ("1", "2", ... unless `levelNames`).
- `core/battle/supports.cpp` (state, sources, bonus), `supports_action.cpp` (Support action, options, story command
  and condition). Helper declarations are in the supports section of `srpg_internal.h` (`supportUnit`,
  `supportNear`, `supportWhy`, `supportGain`, ...).
- `core/data/loader_supports.cpp` (the file), `loader_supports_story.cpp` (command / condition / trigger of map events
  and the scene versions, plus `resolveSupportRefs`). `campaign.json` is read before `supports.json`, so a scene's
  pair is only checked at the end (`data/campaign.json: chapter "ch1": "after": command 1 (addSupport): ... are not
  a support pair`); `SceneCommand` / `SceneCondition` store the two character ids (`unit`, `with`) and look the
  pair up when they run.
- `core/campaign/supports_campaign.cpp`: `beginSupports`, `takeSupports`, `pendingSupports`, `playSupport`, and
  `sceneAddSupport` (declared in `playthrough.h`).
- `core/battle/save_supports.cpp`. Tests: `test_srpg_supports*.cpp`, `srpg_supports_fixture.h` (`SupWorld`: a battle
  with characters and pairs made in code); `campaign_fixture.h` gained `charactersJson`, `supportsJson`, `fordJson`,
  `millJson`. `test_srpg_stubs.cpp` lost the support refusals.

**What the player (U5) and the editor (E4) build against.** `Battle::supportOptions(unit, from, out)`,
`Action::support(unit, target)`, `Battle::supports`, the events `SupportPoints` / `SupportReached` / `Dialogue`
(text index at or above `kSupportTextBase`, `Battle::storyText` resolves it), `supportLevelName`,
`supportReady` / `supportTalkPending`, `pendingSupports` / `playSupport` (lines come back as `SceneLine`s),
`Database::supports`.

**Readings of the contract (keep to these).**
- Levels without a conversation are gained at once and several in a row if the points allow (one `SupportReached`
  each); at the start of a battle or playthrough the ones the `start` points already reach are gained silently.
  A level with a conversation is gained only when it is played, one at a time.
- `kReady` is set on the `SupportPoints` of the gain that made a conversation ready (not for levels gained at once).
- A negative `by` can take points below a level's: the level is then lost (it never stands above what the points
  allow, which is what the save check demands). No extra event; the `SupportPoints` carries the new points.
  `addSupport` of a pair whose gain changes nothing (at 0 or at the limit) makes no event.
- "Near": the two units of a pair-up pair always are; otherwise both are on the board, on one team, and within the
  range (1 for ending a turn). A carried unit (carry mode) or anybody defeated is not near anybody. The bonus is
  judged where the unit fights from (`CombatSetup::from`), so the forecast of a move not yet made is right.
- `endTurnAdjacent` pairs are those whose character `a` is on the side that ends its turn; both must be on one team.
- `heal` is paid by `supportsHealed(healer, target, events)`. The item call in `items.cpp` moved to after the item is
  spent (a level gained by it may run story events that add units, which would leave `u` / `t` dangling); events order
  in games with pairs only. A1's heal ability must call it too.
- `addSupport` in a battle works on any pair (a character defeated in the battle included); in a scene it does nothing
  when one of the two fell for good (`gone`). A scene's points to a pair not yet in the roster are kept.
- Speaker of a conversation line: its unit when on the board, else `Dialogue.c` holds the name (copied from
  `characters.json` into `supports.texts` at load; `speakerName` lines use theirs).
- Refusals of the Support action: `those two are not a support pair`, `those two have no support conversation
  waiting`, `those two are not on the same team`, `those two are not next to each other`, `that is not a unit to talk
  to`, `support conversations are played between chapters in this game` (`conversations: "camp"`), and F1's `this game
  has no supports`. The unit checks of `validate` come first (a unit that has acted cannot).
- Loader limits not spelled out in the contract: `pairs` is required (may be empty), a level's `points` 0..1,000,000,
  `rules.points.*` 0..1,000,000, `levelNames` at most 5 names (more names than a pair has levels is fine), a bonus
  number within +-1,000,000, a `support` condition's `value` 0..5, a trigger's `level` 1..the pair's levels, `by`
  within +-1,000,000.
- Saves: `supports` is written only when it differs from the starting state; a missing key reads as that state.
  Messages are `battle.supports.points[2]: ...` / `playthrough.supports.levels: has 2 entries, expected 3`.
- `pendingSupports` is empty when `conversations` is `"battle"`.

**Left for others.** The player's menu entry, the camp screen and the editor form (U5, E4). Nothing else is open.

### AI1. Computer opponent — depends on P1, A1, R1 (and K1 for `acted`)
- **Scope.** The section "AI": `ai_srpg.cpp` and its two call sites in `ai.cpp` / `ai_order.cpp`.
- **Accept.** The AI tests; 200-turn sims on every fixture never hang; sims of existing games unchanged.

### U0. Player skeleton — depends on F1; parallel with all core steps
- **Scope.** The action menu's new entry groups as hooks into new files (`battle/battle_srpg_menu.cpp`: id ranges
  `kAbilityBase`, `kSupportBase`, fixed ids for the carry / pair / promote / info entries; each group's builder and
  handler empty), a generic "pick a unit" and "pick a square" targeting mode usable by all groups
  (`battle_srpg_target.cpp`), `Mode::UnitInfo` with an empty overlay, view handling of the new events as no-ops with
  a log line (`battle_view_srpg.cpp`), carried units hidden in `BattleView`.
- **Owns.** Those files; one round of edits to `battle_scene.h`, `battle_menus.cpp`, `battle_input.cpp`,
  `battle_view.cpp`. **Accept.** Existing player tests unchanged; the new modes reachable in a unit test with fakes.

#### U0 as built

Inert: no game gains an entry, a mode change or a log line (a game that never emits the new events plays as before;
all player tests are unchanged). Everything is under `engine/player/battle/` plus `tests/test_srpg_player.cpp`.

**Where each later step plugs in (each in its own small file; nobody edits another step's file).**
- Menu entries (`BattleScene` members, declared once in `battle_scene.h`): `battle_srpg_menu.cpp` only fixes the
  order and sends an id to its group; the groups are `battle_menu_supports.cpp` (U5), `battle_menu_abilities.cpp`
  (U3), `battle_menu_rescue.cpp` (U4), `battle_menu_promotion.cpp` (U2), `battle_menu_info.cpp` (U1: Info in the
  action menu and Unit in the battle menu). Each file has `add...Entries(items)` (builder) and `...Chosen(id)`
  (handler, true = taken). Ids are in `srpg_menu_ids.h` (`kAbilityBase` 2000 + n, `kSupportBase` 3000 + n, fixed
  `kRescue`..`kSwitch`, `kPromote`, `kInfo`, `kUnitEntry`; Talk stays 1000 + n and is now limited to 1000..1999).
  `battle_menus.cpp` calls the builders after Talk (`addSrpgEntries`), before Wait (`addSrpgInfoEntry`) and in the
  battle menu after Objective (`addSrpgEndEntries`).
- Targeting: `beginPickUnit(units, chosen, mark)` and `beginPickCell(cells, chosen, mark)` (`battle_srpg_target.cpp`,
  logic in `srpg_pick.*`). Cancel returns to the action menu; Confirm clears the mode and calls `chosen` (which
  normally submits and calls `finishOrder()`). `mark(candidate, overlay)` fills the overlay for the highlighted
  candidate (`reach` = blue, `attack` = red); square picks mark all legal squares themselves.
- Unit details: `openUnitInfo(unit)` (`battle_srpg_unitinfo.cpp`); the frame and tabs are `UnitInfoView`
  (`unit_info.*`). A page is one function `add<Name>Page(pages, battle, unit)` in `unit_info_stats / _skills /
  _items.cpp` (U1), `_abilities.cpp` (U3), `_supports.cpp` (U5); it adds a `UnitInfoPage` (tab title and a draw
  function) or nothing. Tab order is fixed in `buildUnitInfoPages`: Stats, Skills, Abilities, Supports, Items.
- View: `battle_view_srpg.cpp` dispatches the eleven new events to `beginX / progressX / finishX` of one file per
  area: `battle_view_skills.cpp` (SkillLearned, SkillActivated, UnitHurt; U1), `_promotion.cpp` (U2),
  `_abilities.cpp` (U3), `_rescue.cpp` (UnitRescued, UnitDropped, UnitHanded, PairSwitched; U4), `_supports.cpp` (U5).
  Same contract as the story hooks: return true when the entry was yours. An entry nobody took logs
  `srpg event (not shown yet): <Name> a= b= c= d= flags=` and takes no time.

**Readings of the contract.**
- The contract names one file per kind of thing (`battle_srpg_menu.cpp`, `battle_view_srpg.cpp`); the per-area files
  above are the split asked for by "so the later steps can run in parallel". `battle_scene.cpp` got three one-line
  edits (hint, overlay choice while picking, drawing the details overlay) and the root `CMakeLists.txt` a block that
  adds the SDL-free player files to `ote_tests`; neither was named in "Owns".
- Two new `Mode`s instead of one: `PickUnit` and `PickCell`; plus `Mode::UnitInfo`.
- Carried units: `BattleView::resync` draws a unit only when `alive && carriedBy < 0`. Without a step-U4 animation,
  `UnitRescued` hides the carried unit and `UnitDropped` / `PairSwitched` show the one that comes back, at the square
  of the event (done in `battle_view_srpg.cpp` when no area took the entry).
- The tests reach the new modes through the pure parts (`PickList`, `UnitInfoView` with fake pages and a fake
  renderer, `srpgGroupOf`, a `BattleView` fed all eleven events); the scene itself needs a window and is covered by
  the unchanged player scripts.

### U1. Player: skills and unit details — depends on K2, U0
Forecast rows and notes, activation labels, `SkillLearned` in the level-up box, the info panel rows, the Unit
details overlay (Stats, Skills, Items pages; the other pages are added by U3 and U5 as their own files in
`battle/unit_info_*.cpp`), `acted` units' menu and range. Owns `forecast.cpp`, `panel.cpp`, `unit_info*.cpp`.

### U2. Player: promotion — depends on P1, U0; parallel with U1, U3, U4, U5
Promote entry, the question box, the promotion screen (`battle/promotion_screen.cpp`), `UnitPromoted` animation
and banner, sprite change.

### U3. Player: abilities and summons — depends on A1, U0
Ability entries with reasons, targeting with the end squares marked, `AbilityUsed` / blocked animation, the
Abilities page of Unit details.

### U4. Player: rescue and pair up — depends on R1, U0
The six entries, targeting, the four events' animations, the carrier marker, the panel row.

### U5. Player: supports and camp — depends on S1, U0
Support entry and conversation, labels and banner, the Supports page, `campaign/camp_scene.*` and its place in
`app_campaign.cpp` (`doCampaignNext`), saving after a camp conversation, `--autoplay`.

Each U step's acceptance is its part of `tests/srpg_player_test.py` plus screenshots for the owner (desktop and
phone size).

### E0. Editor skeleton — depends on F1 (for Problems); parallel with all core and player steps
Helper: the four sections (read, write, remove) in `editor/server/projects.cpp` / `project_write.cpp`, documented
in docs/EDITOR_API.md by this step; `tests/editor_api_srpg_checks.py`. Web: types (`model/srpgTypes.ts`), document
fields, sidebar entries and routes with empty screens, reference and id helpers for the new lists
(`model/srpgRefs.ts`), empty card slots in `UnitForm`, `ItemForm`, `WeaponForm` and the Rules screen (one component
file per future card, so later steps do not edit the forms).

#### E0 as built

The shared places are in; every screen is an empty frame. Details for the editor are in docs/EDITOR_UI.md "Strategy-RPG
skeleton" and docs/EDITOR_API.md; the short version for the steps that follow:

- **Helper**: `editor/server/srpg_files.{h,cpp}` reads and writes the four files; `projects.cpp` and `project_write.cpp` only call it.
  An empty list removes its file, `supports: false` removes `data/supports.json`, a map may not use these names as its file.
  `tests/editor_api_srpg_checks.py`.
- **Web**: `model/srpgTypes.ts` (shapes with the `Extra` index; E1 to E4 type their own list there), `model/srpgFeatures.ts` (switches,
  sidebar rule), `model/srpgRefs.ts` (ids, "used by", delete clean-up for the three lists), the four sections in `ProjectDoc`,
  `Section` and the autosave body. Screens: `screens/{characters,skills,abilities,supports}/<Name>Screen.tsx` inside
  `screens/srpg/SrpgScreen.tsx`. Card slots, one file each: Rules `CarryingCard`, `PromotionRulesCard`; unit type `UnitSkillsCard`
  (E2), `UnitAbilitiesCard`, `UnitPromotionCard`, `UnitPairBonusCard` (E3); weapon `WeaponSkillsCard` (E2); item `ItemSkillsCard` (E2),
  `ItemPromotionCard` (E3). The forms mount the group files, so no later step edits `UnitForm`, `WeaponForm`, `ItemForm` or `RulesScreen`.
- **Readings of the contract.** (1) "Class" wording (open question 1) is not in the E0 section and is not done; it belongs to E1. (2) The four
  Rules switches are in E0 (the contract lists them with the Rules card, and without them no screen could be reached); the Carrying and
  Promotion cards are E3's. (3) A feature is "on" when its list or file exists in the editor's document; switching on creates an empty list,
  which is never written, so an empty feature is forgotten on reload (the contract: an empty list and no file are the same game). Supports
  with no pair and no setting count as empty. (4) Switching a feature off removes its items from everything that names them (units,
  characters, weapons, items, skills, map units, pairs) in one undo step; it asks first only when the feature holds something.
  (5) Map units: only `character?: string` was added to `MapUnit`; `type` is still required in the type, E1 relaxes it with the Characters tab.
  (6) Problems: the loader's words are shown as they are; the editor-side checks are left to E1 to E4 (they need the data forms).
  (7) Deleting a unit type does not yet clean characters of that type, promotions into it or summons of it (E1 / E3).

### E1. Editor: Characters and map placement — depends on E0 (C1 for Problems)
The Characters screen, the map editor's Characters tab and unit form, checks.

### E2. Editor: Skills — depends on E1; parallel with E3, E4
The Skills screen with the block builder and summaries, the starter set and its installer, the Skills cards on
units, characters, items and weapons, story forms for teach / forget / has skill.

### E3. Editor: Abilities, Promotion, Carrying — depends on E0; parallel with E1, E2, E4
The Abilities screen, the Abilities and Promotion cards, the Rules cards for carrying and promotion, the story
forms for Promote and "is promoted".

### E4. Editor: Supports — depends on E1; parallel with E2, E3
The Supports screen (reusing `SceneEditor`), story and scene forms for support points, condition and trigger.

Each E step's acceptance is its vitest and Playwright tests of "Tests" and screenshots for the owner.

### D1. Demo and template content — depends on K2, P1, A1, R1, S1, AI1 (content), U1–U5 (player tests)
The section "Demo and template content", the fixtures not yet made, the sims, updating the demo's pins, CREDITS
unchanged (no new art is required: new unit types reuse existing sprites until art is made).

### T1. Tutorial lesson and document sync — depends on E1, E2, D1
The lesson "Characters and skills"; pointers to this contract from docs/GAME_FORMAT.md, docs/SAVES.md,
docs/EVENTS.md, docs/ITEMS.md, docs/EDITOR_UI.md and docs/ARCHITECTURE.md (one paragraph each, no duplication).

**In short** (a step can start when everything in "needs" has landed):

| Step | Needs | Can run beside |
|---|---|---|
| F1 Skeleton and expressions | nothing | nothing (everything waits for it) |
| F2 Combat pipeline | F1 | C1, K1, P1, R1, U0, E0 |
| C1 Characters | F1 | F2, R1, U0, E0 |
| K1 Skills: data, standing, learning | C1 | F2, P1, R1, S1 (once F2 landed), U0, E0, E1 |
| K2 Skills: combat and timed | K1, F2 | P1, A1, R1, S1 |
| P1 Promotion | C1 | F2, K1, K2, A1, R1, S1 |
| A1 Abilities and summons | K1 | K2, P1, R1, S1 |
| R1 Rescue and pair up | F1 | every other core step |
| S1 Supports | C1, F2 | K1, K2, P1, A1, R1 |
| AI1 Computer opponent | K1, P1, A1, R1 | K2, S1, player and editor steps |
| U0 Player skeleton | F1 | every core and editor step |
| U1 Skills and unit details | K2, U0 | U2, U3, U4, U5 |
| U2 Promotion | P1, U0 | U1, U3, U4, U5 |
| U3 Abilities and summons | A1, U0 | U1, U2, U4, U5 |
| U4 Rescue and pair up | R1, U0 | U1, U2, U3, U5 |
| U5 Supports and camp | S1, U0 | U1, U2, U3, U4 |
| E0 Editor skeleton | F1 | every core and player step |
| E1 Characters, map placement | E0 | E3 |
| E2 Skills | E1 | E3, E4 |
| E3 Abilities, Promotion, Carrying | E0 | E1, E2, E4 |
| E4 Supports | E1 | E2, E3 |
| D1 Demo and template | K2, P1, A1, R1, S1, AI1; U1 to U5 for its player tests | E steps |
| T1 Tutorial lesson, document sync | E1, E2, D1 | nothing |

---

## Interpretations

Where the owner's decisions had to be extended to make a complete design:

1. **A character's id is its story id.** No second naming scheme: events, the roster, portraits and supports all
   use the one id, and nothing new is saved for characters.
2. **Personal stats and growths are added** to the unit type's (not replacing them), so a promotion or a class
   change keeps the person's edge.
3. **Learned skills are kept for good**, including through promotion; a unit type's own (not learned) skills go
   with the type.
4. **"Blocks"** became: when / if (a list, all must hold) / an optional chance / do (a list), and a skill may have
   several blocks. Conditions inside one block are "and"; "or" is written as two blocks.
5. **Chance skills never change the forecast's numbers**; the forecast lists them with their chance instead.
6. **Weapon and item skills**: a weapon's skills count only while it is the weapon in use; an accessory's while it
   is carried; usable items give none.
7. **Abilities reach units through three doors**: the unit type, the character, or a skill (which covers items,
   weapons and levels), rather than five separate lists.
8. **A free ability is limited to once per turn per unit**, so "free" can never loop.
9. **"Wait of some turns"** counts the unit's own side's turns.
10. **Summoned units do not keep a side alive** and are ignored by "all units of a side" objectives; they never
    drop items.
11. **Rescue is same side only**, Give and Take are free but count as the unit's move, and a dropped unit's turn is
    over.
12. **A pair acts as one unit**: one move and one action a turn whoever is in front; the partner behind gives flat
    stat bonuses set by the maker (per game, or per the partner's unit type). Dual attacks and guards are left out.
13. **"Fighting together"** means: one of the two took part in a combat and survived it while the other was within
    the game's support range; plus one healing the other with an item or ability.
14. **A support level is gained when its conversation has been seen** (or at once when the level has no
    conversation); the bonus belongs to the level gained.
15. **The between-chapters menu** is a "Camp" screen shown before the next chapter's opening, only when there is a
    conversation to see.
16. **Promotion by an offered level** is a menu command that ends the unit's turn, like using an item does; the
    automatic kind costs nothing.
17. **Promotion never happens on a square the new type could not stand on**; the automatic kind waits.

## Open questions for the owner

Each has a default the build proceeds with; none blocks any step.

1. **"Unit type" or "class" in the editor?** In a game that has characters, should the editor (and the player's
   unit details) say "class" where it says "unit type" today? *Default: yes, automatically when the game has at
   least one character; games without characters keep "unit type".*
2. **Names of the starter skills and of the features.** The thirty names in section 3, and "Camp", "Support",
   "Pair up", "Carry", are placeholders. *Default: ship them as written until you rename them.*
3. **Does a support bonus need the conversation to be seen first?** *Default: yes (interpretation 14).* The other
   choice is "the bonus applies as soon as the points are reached and the conversation is extra".
4. **Does a Support conversation in battle use the unit's action?** *Default: no, it is free like Talk.*
5. **Do summoned units keep a side in the battle?** *Default: no (interpretation 10).* The other choice makes a
   lone summon enough to avoid defeat.
6. **Which rescue mode does the strategy-RPG template start with?** The demo uses carry. *Default: the template
   uses pair up, so each mode can be tried in a shipped game; say if new games should start with carry or
   neither.*
7. **Promotion and the level number.** *Default: per game "starts again at level 1", with a per-promotion
   override.* Say if "level stays" should be the default instead.
8. **Default names of support levels.** *Default: "1", "2", "3" (shown as "support level 1").* Letters or words
   are a rename away.
9. **Should defeating a summoned unit give experience?** *Default: yes, with a per-ability switch to turn it
   off.*
10. **May the computer use Shove, Swap and Pull back, or carry its own units?** *Default: not in this version; it
    only summons and heals with abilities.*
