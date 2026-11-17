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
game.json              rules.rescue, rules.promotion, rules.places           (optional blocks)
data/characters.json   characters                                            (optional file)
data/skills.json       skills made of blocks                                 (optional file)
data/abilities.json    menu abilities, summons                               (optional file)
data/supports.json     support rules, pairs, levels, conversations           (optional file; needs characters)
data/ranks.json        weapon rank ladder and growth                         (optional file; section 7)
data/caps.json         game-wide stat caps                                   (optional file; section 7)
data/conditions.json   status conditions made of blocks                      (optional file; section 9)
data/structures.json   doors, chests and visitable places: "door", "chest", "visit", "blocksSight" (section 10)
data/units.json        per unit type: skills, learns, abilities, promotions, pairBonus
data/weapons.json      per weapon: skills, inflict (section 9)
data/items.json        accessory: skills; usable: promotes, cure, inflict, opens (keys, section 10); staves (section 8; inflict, cure: section 9; unlock: section 10)
maps/*.json            per unit: character, goesFor; per structure: id, contents, locked, visitSides, visitUnits; new story commands, conditions and triggers
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

**A weapon's id and group.** Besides its `props` (numbers), `w` and `fw` carry the weapon's `id` and `group` as
text, so a formula can write `w.group == "sword"` or `fw.id == "iron_lance"` without a number prop per kind. A weapon
without a group has `group = ""`; the empty `fw` (below) has `id = ""` and `group = ""`, so such comparisons are simply
false. A prop a game named `id` or `group` itself wins over these.

**`fw`, the foe's weapon** (an eighth variable, after `vs`): the numbers (`props`) of the weapon the *other* fighter
fights with, in the same shape as `w`. In a strike `a` makes, `w` is `a`'s weapon and `fw` the weapon `b` answers
with (its answering weapon, "Combat pipeline" step 1); in the counterattack the roles swap: `a` is the defender,
`w` its weapon and `fw` the attacker's. It is set in the strike formulas (`damage`, weapon `damage`, `hit`, `crit`,
`strikes`) so the forecast, the fight and the computer opponent agree, and in a skill's `formula` condition and
`chance` expression of a combat (it is the foe's weapon in use there). Everywhere else (xp, capture, rescue, a
condition judged outside a fight) there is no foe weapon and `fw` is empty.

**No weapon** (unarmed, only a weapon it cannot wield yet - "Needs rank B" -, nothing that answers, or no fight):
`fw` is still a table, never `nil`. It holds `none = 1`, and every other number reads 0 (`fw.might`, `fw.weight`),
so a formula may test `fw.none` or just compute with it; with a real weapon `fw.none` is `nil` and a prop it lacks is
`nil` like in `w`. A weapon triangle (props `kind` = 1 sword, 2 axe, 3 lance; sword beats axe beats lance beats sword):

```lua
function tri(w, fw)                         -- +1 advantage, -1 disadvantage, 0 neutral or no foe weapon
  if fw.none or not w.kind then return 0 end
  local d = (fw.kind - w.kind) % 3
  return d == 1 and 1 or (d == 2 and -1 or 0)
end
```
```jsonc
"damage":  "a.str + w.might - b.def + 3 * tri(w, fw)",
"hit":     "70 + 2 * (a.spd - b.spd) + 15 * tri(w, fw) + 2 * fw.weight",   // the foe's heavy weapon is easier to hit
"strikes": "(a.spd - w.weight >= b.spd - fw.weight + 5) and 2 or 1"        // its weight slows the foe's attack speed
```
A game that never writes `fw` computes exactly as before (the extra variable costs nothing it can see).

**`fw.answers`: does the foe really answer?** `fw` is the foe's answering weapon even when it is out of range for a counter
(the default, unchanged). In the strike formulas (`damage`, a weapon's `damage`, `hit`, `crit`, `strikes`) `fw` also carries
`answers = 1` when the foe can actually counter in this fight with it (the rule `counterAttacks` is on, the weapon is in range,
and nothing like a "cannot be countered" skill stops it: the very answer the combat pipeline already judged, `CombatSetup::counters`).
When the foe cannot answer, `fw.answers` is `nil` (false) - also on the empty `fw` (`fw.none`), where it is not the 0 that
every other missing number reads. In the counterattack, where the foe is the attacker, whose weapon is in use by definition, it
is `1`. A weapon prop of the same name wins, like `id` and `group`. It is not set in skills' `formula` conditions and `chance`
expressions (they are judged while the combat's answer is still being worked out) nor outside a fight.
```jsonc
"damage": "a.str + w.might - b.def + ((fw.answers or fw.none) and 0 or 3)"   // +3 against a foe that holds a weapon but cannot use it at this range
```

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
    "wields": ["bow"],                            // optional: weapon groups it can wield beyond its class's (section 7)
    "items": ["iron_sword", "salve"],         // optional: replaces the unit type's starting items
    "quotes": { "battle": [ { "against": "gorm", "lines": [ { "say": "Your raids end today." } ] } ],
                "defeat": { "falls": [ { "say": "Tell them I tried." } ] } } }   // optional: docs/EVENTS.md "Battle and defeat quotes"
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
`do` (required, at least one effect). An aura may add `"match"`; a combat number may be a formula (see "More skill blocks"). Each condition and each effect is an object with **exactly one** kind key
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

### WHEN: the eight points

| `when` | The block is judged | Has a foe |
|---|---|---|
| `always` | whenever the unit's stats or movement are asked for (a standing bonus) | no |
| `turnStart` | at the start of each turn of the unit's side, after repair and supply, before the story's turn events | no |
| `combat` | for a combat the unit takes part in, before the first strike and again before every strike; shown in the forecast | yes |
| `strike` | each time the unit is about to strike | yes |
| `struck` | each time the unit is about to be struck | yes |
| `defeat` | after a combat in which the unit defeated its foe | yes (fallen) |
| `afterCombat` | after a combat the unit took part in and survived | yes (may be fallen) |
| `staffUse` | after the unit used a staff (the use is spent, its order has ended); the `weapon` condition tests the staff's group | no |

### CONDITIONS (`if`)

Every condition is about the skill's owner unless it says `"who": "foe"`, which is allowed only where there is a
foe. `op` is one of `= != < <= > >=` (`==` accepted), default `>=`.

| Condition | True when |
|---|---|
| `{ "hp": { "op": "<=", "percent": 50 } }` | the unit's HP compared with that share of its maximum (0..100), computed as `hp * 100 <op> percent * maxhp` |
| `{ "near": "ally", "within": 1, "op": ">=", "count": 1, "match": "knight" }` | counts other units on the board within `within` steps (1..10, default 1): `"ally"` = same team, `"enemy"` = another team **and seen by the unit's team**; `match` (optional) = a unit type id or tag. `count` default 1. "Nobody next to me" = `"op": "=", "count": 0`. Owner only |
| `{ "weapon": "sword" }` | the unit's weapon in use is that weapon id or has that `group` |
| `{ "terrain": "forest" }`, `{ "terrain": ["forest", "swamp"] }` | the square the unit stands on (in a forecast: would stand on) is that terrain id or has that terrain tag; the list form is true when any one matches (see "Skill and ability blocks, second set") |
| `{ "structure": true }` / `{ "structure": "fort" }` | the unit stands on a structure / on one with that id or tag |
| `{ "unit": "armored" }` | the unit's type has that id or tag (`unitTypeMatchesKey`) |
| `{ "carrying": true }` | the unit carries someone / has a unit behind it (section 5); `false` = it does not |
| `{ "round": { "op": ">=", "value": 5 } }` | the battle's round number |
| `{ "initiating": true }` | the unit started this combat (`false` = it is defending). Foe points only |
| `{ "distance": { "op": "=", "value": 1 } }` | grid distance between the two fighters. Foe points only |
| `{ "formula": "a.level >= 10 and b.tags.flier" }` | the formula is true or non-zero: `a` = the owner, `b` = the foe. Not in `always` blocks |
| `{ "weaponKind": "magic" }`, `{ "turn": { "odd": true } }`, `{ "counterattack": false }`, `{ "carries": "potion" }`, `{ "inventory": "full" }` | see "More skill blocks" below |

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
| `{ "heal": 5 }` / `{ "heal": { "percent": 20 } }` | restores HP (percent of the receiver's maximum, at least 1). `"to"`: `"self"` (default) or `"allies"` with `"within": 1..10` (other units of its team on the board that close; not in `staffUse`) | `turnStart`, `defeat`, `afterCombat`, `staffUse` |
| `{ "hurt": 5 }` | the receiver loses HP but never falls below 1. `"to"`: `"foe"` (`afterCombat`, a foe still alive) or `"enemies"` with `"within"` (`turnStart`; enemies the unit's team sees) | `afterCombat`, `turnStart` |
| `{ "actAgain": true }` | the unit may move and act again this turn. At most once per unit per turn | `defeat`, `afterCombat` (only on its own side's turn) |
| `{ "followUp": "force" }`, `{ "followShift": 2 }`, `{ "trianglePercent": 200 }` (also `"to": "foe"`) | follow-up control and the triangle multiplier; see "More skill blocks" | `combat` |
| `{ "weaponXpPercent": 150 }`, `{ "noCarryPenalty": true }`, `{ "cancelEffective": true }`, `{ "moveAgain": { "notAfterAttack": true } }` | see "More skill blocks" | `always` |
| `{ "cure": ["poison"] }` / `{ "cure": "all" }` | cures the owner | `turnStart`, `defeat`, `afterCombat`, `staffUse` |
| `{ "nullifySkills": true }`, `{ "nullifySkills": ["id"] }`, `{ "nullifySkills": { "tag": "word" } }` | the foe's skills do not apply in this fight; see "Skill and ability blocks, second set" | `combat` |
| `{ "staffRange": 2 }`, `{ "staffRange": { "amount": 2, "group": "heal" } }` | squares added to the reach of staves the unit uses | `always` |
| `{ "carryBonus": 2 }` | a number the `canCarry` formula reads as `a.carryBonus` | `always` |

An effect in a block whose `when` the table does not list is refused: `skill "x": block 2: "drain" cannot be used in
a "combat" block (it belongs to: strike)`. A skill may also have `"tags"` (a list of the maker's words, used by `nullifySkills`).

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

### More skill blocks

General blocks added after the first set (PLAN 6e). Each one is plain data in the blocks above, judged by the same code, so the forecast, the real
fight and the computer opponent agree. Every game that does not use a new key loads and plays exactly as before (the data hash is unchanged).

**Tags.** A unit type's `tags` already exist (`units.json`). A **character** may now have `tags` too (`characters.json`, a list of free words): a
character matches a tag key like its unit type does. Everything that takes "a unit type id or tag" (the `unit` and `near.match` conditions, an
aura's `match`, below) sees both, and a formula's `a.tags.x` / `b.tags.x` includes the character's tags. (`versus` tables and square bonuses keep
using the unit type alone.)

**An aura limited to a kind of unit.** `"aura": { "to": "allies", "within": 2, "match": "scout" }`: only units whose type or character has that id /
tag receive the effects. `match` is about the receiver; the owner's own conditions are unchanged. "+X when fighting a foe with tag T" is the existing
`{ "unit": "T", "who": "foe" }`. An unknown id: `skill "x": block 1: "aura"."match" names no unit type id or tag "nobody"`.

**The kind of damage a weapon deals.** The engine has no built-in physical / magical (damage is the maker's formula). A weapon may say
`"damageKind": "magic"` (any word, like `group`; `weapons.json`). Condition `{ "weaponKind": "magic" }` is true when the weapon in use has that kind;
`"who": "foe"` asks about the foe's weapon in this fight. Usable wherever a skill test works. Formulas read it as `w.damageKind` / `fw.damageKind`
(`""` when none). A kind no weapon has is refused: `"weaponKind" names no weapon damage kind "x"`.

**Turn-number tests.** `{ "turn": { "odd": true } }`, `{ "even": true }`, `{ "from": 3, "to": 6 }` (either may be left out), `{ "every": 3, "offset": 1 }`
(turns whose number divided by `every` leaves `offset`; offset default 0, must be below `every`). The turn is the battle's round number (the same
number as the `round` condition); allowed in every block. Formulas read it as `turn` (a ninth variable after `fw`; 0 outside a battle).

**Formula-sized combat bonuses.** Wherever a combat bonus takes a number, it may be a formula (text) with the usual variables: `hit`, `crit`,
`damage`, `avoid`, `critAvoid`, `damageTaken`, `damagePercent`, `damageTakenPercent` in `combat`, `strike` and `struck` blocks, and each stat of a
`combat` block's `stats` (`{ "stats": { "str": "turn" } }`; not in `always` blocks, where a stat would ask itself). `a` = the owner, `b` = the foe;
in an aura `b` is the fighter that receives it. The result is rounded down (percents kept within 0..1000, numbers within +-1000). The formula reads
stats as the rules see them but without this combat's own bonuses (no circularity). A scaling bonus: `{ "damage": "turn * 2" }`. **Ignore part of the
foe's defence** needs no block of its own: `{ "stats": { "def": "-math.floor(b.def * 0.5)" }, "to": "foe" }` lowers the foe's Defense by half for this
fight, before the damage formula reads it. (Syntax errors name the place: `data/skills.json: skill "x": block 1: effect 1: "damage": ...`.)

**Follow-up strike control** (`combat` blocks). The engine does not decide doubling: the maker's `strikes` formula does (`strikes >= 2` is a
follow-up). So: `{ "followUp": "force" }` makes the unit's strikes at least 2 after the formula; `{ "followUp": "forbid" }` makes them 1; with `"to":
"foe"` it is aimed at the foe's strikes (`"forbid"` wins when both are given). `{ "followShift": 2 }` is a number the strikes formula reads as
`a.followShift` / `b.followShift` (0 when no skill says it): `"strikes": "(a.spd >= b.spd + 5 - a.followShift) and 2 or 1"` moves the threshold
(`"to": "foe"` shifts the foe's). A game whose formula never reads it ignores it. The forecast and the fight use the same numbers.

**Weapon-triangle multiplier** (`combat`). `{ "trianglePercent": 200 }` (0..1000, percents multiply, `"to": "foe"` allowed) is read by formulas as
`a.trianglePercent` / `b.trianglePercent` (100 by default): `3 * tri(w, fw) * a.trianglePercent / 100`. Each strike's formula sees its own striker's value
as `a`, so when both fighters have one each scales its own strikes; a formula that wants the foe's reads `b.trianglePercent`.

**Weapon experience** (`always`). `{ "weaponXpPercent": 150 }` (1..1000): weapon experience the unit gains (section 7: fights and staves) is multiplied.

**Move again, but not after attacking** (`always`). `{ "moveAgain": { "notAfterAttack": true } }`: the unit may still move after any action but a
fight. (`{ "moveAgain": true }` is as before: after any action.)

**No carry penalty** (`always`). `{ "noCarryPenalty": true }`: the unit carrying someone (`rules.rescue.mode "carry"`) ignores `carrierStats`.
(Pair up's `pairBonus` still applies. A carry capacity bonus is not built: `canCarry` is the maker's formula, which can read a tag or a skill's stat.)

**Cancel effectiveness** (`always`). `{ "cancelEffective": true }`: weapons' `versus` values above 1 against this unit count as 1 (a `versus` that
makes the weapon unable to target it is unchanged). It applies in the forecast, the fight and the computer opponent alike.

**The foe answers** (`strike` and `struck` blocks only). `{ "counterattack": false }` is true when this fight has no counterattack (out of range, a
`noCounter`, an unarmed foe...); `true` is the inverse. A `combat` block cannot ask it (its answer is still being worked out): `"counterattack" can
only be asked in "strike" and "struck" blocks`. The forecast shows strike blocks without a chance, so it follows.

**Items carried.** `{ "carries": "potion" }` (an item id), `{ "inventory": "full" }`, `{ "inventory": "notFull" }`, `{ "inventory": { "op": ">=", "value": 2 } }`
(the number of items in the pack; "full" = `rules.items.inventorySize`). `"who": "foe"` allowed.

**Cure** (`turnStart`, `defeat`, `afterCombat`). `{ "cure": ["poison"] }` or `{ "cure": "all" }`: the owner is cured (section 9; no immunity or
inflict rules apply; event `ConditionEnded`).

**A map unit with skills of its own.** A unit in a map's `units` (and an `addUnit` command) accepts `"skills": ["id"]`, added to those of its type and
character. They are learned for good when the unit is made (so `forgetSkill` can remove them, they are saved with the unit like any learned skill).

**Class skills that go away on promotion.** `game.json` `rules.promotion.classSkills`: `"keep"` (default, today's behaviour) or `"replace"`: on
promotion the skills the OLD class's `learns` gave are removed (except ones the character or the new class teaches too), then the new class's
entries at or below the new level are added. Skills from the type's `skills`, accessories and weapons are always re-derived from the current state.

**Built later** (docs below, "Skill and ability blocks, second set"): steal, refresh an ally, move past a neighbour, switch off the foe's skills, a
`staffUse` point, a staff range bonus, a list for the `terrain` test, cure as an ability effect and a carry capacity bonus. Summon parameters already exist
(section 4: the unit type, `level`, `limit`). Still not built: a test for terrain that "grants a bonus" (terrain bonuses are the maker's own numbers; there is no
fixed notion of one, so only the list form of `terrain` was made).

**The computer opponent.** Every passive block above flows through the existing forecast, so it values them like any other combat bonus. There is no
new active ability, so nothing new is ignored.

**Saves.** No new state: learned skills (map-unit skills, promotion changes) were already saved. Old saves load.

**Tests.** `tests/test_srpg_skillblocks.cpp`, `test_srpg_skillblocks_more.cpp`, `test_srpg_skillblocks_units.cpp` (fixture
`tests/fixtures/srpg-skillblocks`); editor: `tests/unit/skillblocks.test.ts`, `tests/e2e/srpg-skillblocks.spec.ts`, `tests/editor_api_skills_checks.py`.

### Skill and ability blocks, second set

Added after the general blocks above. Every game that does not use a new key loads, plays, saves and hashes exactly as before (the maker's files are hashed
whole, so a new key counts only in a game that has it). Fixture `tests/fixtures/srpg-skillblocks2`.

**Steal** (ability effect). `{ "id": "pickpocket", "target": "enemy", "effect": { "steal": { "if": "a.spd > b.spd", "weapons": false, "equipped": false } } }`;
`"steal": true` is the same with every option at its default. The ability has `"target": "enemy"` and range `[1, 1]`; `uses`, `cooldown`, `cost`, `afterMove` and
`if` work as for every ability.
- `if` (optional): a formula (`a` = the user, `b` = the foe). When it is 0, the ability cannot be attempted on that foe at all: `cannot steal from that unit`.
- `weapons` (default false): carried weapons can be taken. `equipped` (default false; needs `weapons: true`): the weapon the foe holds in hand (its first
  wieldable weapon) can be taken as well. An item the maker marked `"stealable": false` in `items.json` can never be taken.
- What can be taken is `Battle::stealOptions(unit, ability, from, foe, out)`: one entry per slot of the foe's inventory with `ok` and the plain reason
  (`cannot be stolen`, `weapons cannot be stolen`, `the equipped weapon`). A foe with nothing takeable is not a target (`there is nothing it can take`, `it carries nothing`).
- The action is `Action::useAbility(user, ability, foe)` with `slot` = the item's slot in the foe's inventory (`choose an item to steal` when it is missing,
  `that item cannot be stolen` and so on when it may not be taken).
- The user's full inventory follows the rule of the chests (section 10): with no shared storage (`rules.items.storage` off) a full pack greys the ability out
  (`the unit cannot carry more: drop something first`); with a storage the item goes to the side's storage like any overflow.
- The foe's inventory loses the item; its equipped weapon falls back to the next one it can wield, as for every removed item (`equippedSlot`), and an accessory
  that raised its maximum HP is dealt with by `clampHp`. The stack keeps its uses. What a computer unit steals it marks `drop`, like what it loots from a chest.
- Events: `AbilityUsed`, `ItemStolen` (a = thief, b = the unit robbed, c = item, d = uses left or -1), then `ItemGained` (or into the storage) for the thief.
- **Computer opponent.** A unit marked `"goesFor": ["steal"]` (map unit and `addUnit`; a computer side only) that has a steal ability, and whose planned order is
  a plain Wait / advance or an attack that does not defeat its target, steals instead: from where it would stand, the first foe (unit-id order) with the first
  item (slot order) it may take. It walks to a foe only as far as its normal plan does; it does not seek out foes to rob. An unmarked unit never steals. The
  ability's own `uses` / `cooldown` / the turn it ends make a loop impossible.
- **Player.** Choosing the ability picks the foe first (the hint says `Steal: 2 of 3 items can be taken`), then a `Steal which?` list of the foe's items (name and
  uses left); the ones that cannot be taken are greyed with the reason on the right (`weapons cannot be taken`, `the weapon in hand`, `cannot be taken`; a short
  form on a narrow screen, none when there is no room). Keys, pointer and touch work as in the other menus; Cancel in the list goes back to the foe pick, Cancel
  there to the action menu. The thief floats `Stole <item>`.

**Refresh an ally** (ability effect). `{ "refresh": true }` or `{ "refresh": { "perTurn": 2 } }` with `"target": "ally"` (range as the maker likes, default
[1, 1]). The target must be a unit of the user's own side that has used its action (`done`, or `acted` with move again) and that can act (a condition that
stops acting is respected: `that unit cannot act`). It gets its `moved`, `done`, `acted` and `moveSpent` cleared. Guards: a unit can be refreshed at most
`perTurn` times in one turn of its side (default 1: `that unit was already refreshed this turn`), counted over every refresher; and a unit that refreshed you
this turn cannot be refreshed by you (`that unit refreshed you this turn`), so two refreshers cannot ping-pong. State: `Unit::refreshedBy` (the refreshers'
unit ids, cleared when the unit's side starts its turn), saved as `"refreshedBy": [ids]` only when not empty and only in a game with a refresh ability.
Event `UnitRefreshed` (a = refresher, b = the unit, c = ability). The computer opponent never refreshes. Player: the ordinary ally pick; the unit floats `Ready again`.

**Move past a neighbour** (ability effect). `{ "movePast": true }` with `"target"` `ally`, `enemy` or `unit` and range `[1, 1]`: the USER moves to the square on the
far side of the target (the same direction), if it can stand there (on the map, the user's move type can enter it, nobody on it:
`there is no room beyond that unit`). It is the fifth member of the reposition family: the same direction maths (`Grid::directionTo` / `neighbor`, so squares and
both hexagon layouts), the same fog rules (an unseen unit on the square makes the use blocked: paid for, nothing moves, `kBlocked`), `UnitRelocated`, a released
capture, and the story's `reach` events for the square it ends on. Player: the pick marks the end square red.

**Cure as an ability** (ability effect). `{ "cure": ["poison"] }` or `{ "cure": "all" }` with `"target": "self"` or `"ally"`. A valid target has at least one of the
listed conditions (`that unit has nothing to cure`, `nothing to cure`). It works like a staff's or item's cure: no immunity or inflict rules, `ConditionEnded`
with the "cured" reason. A game without `data/conditions.json` refuses it.

**Switch off the foe's skills** (`combat` effect). `{ "nullifySkills": true }` (every skill of the foe), `{ "nullifySkills": ["heavy_blow", "guard"] }` (those skill ids)
or `{ "nullifySkills": { "tag": "bonus" } }` (the skills whose `"tags"` hold that word). Exactly which blocks are switched off:
- The foe's skills are not used for this fight in its `combat` blocks (stat bonuses of a combat block included), its `strike` and `struck` blocks, and the
  auras the foe itself owns. Forecast, strikes, notes and the roll order all read a fighter's skills through one function (`Battle::skillCombatList`), so the
  forecast always equals the fight. Auras of third units, `always` blocks (they are part of the unit's shown stats: Strength, Move, ...), `turnStart`,
  `defeat`, `afterCombat` and `staffUse` blocks, supports and the weapon's own numbers are never switched off.
- Conditions' hidden skills (section 9) are never switched off. The block with the effect is judged normally, for the unit that owns it: its conditions
  must hold. A skill that itself contains a `nullifySkills` effect is never switched off, so when both fighters have one each loses everything except the
  nullifying skill. It works for the counterattack as for the attack.
- Loader: `"nullifySkills" names unknown skill "x"`, `names no skill tag "x" (a skill's "tags")`, `must be true (every skill of the foe), a list of skill ids, or { "tag": "word" }`;
  only in `combat` blocks, not in auras or conditions.

**A "staff used" moment** (`"when": "staffUse"`). The block runs on the unit that used a staff, after the use is spent and its experience given and its order has
ended (so it also runs for light and unlock staves). Allowed effects: `heal` (to the user: a number, `{ "percent": n }` or a formula with `a` = the user; `"to":
"allies"` is refused) and `cure`. Conditions are those of a `turnStart` block; the `weapon` condition tests the group of the staff that was used
(`{ "weapon": "heal" }`); a `chance` is rolled with the battle's dice. The result is the usual `SkillActivated` and `UnitHealed` events.

**Staff range bonus** (`always` effect). `{ "staffRange": 2 }` or `{ "staffRange": { "amount": 2, "group": "heal" } }` (1..10; with a group only staves of that group
gain). The number and `[min, max]` forms of a staff's `range` get the bonus on their maximum, a formula form adds it to its result (at most 20, the staff limit).
It is applied in `Battle::staffRange`, the one place reach is worked out, so the rules, the player's reach display and the computer opponent agree. A warp's
destination range (`"warp": { "range": ... }`) is the staff's own and is not changed.

**Terrain tests.** `{ "terrain": ["forest", "swamp"] }`: a list of ids and tags, true when any one matches (every entry must exist: `"terrain" names no terrain id or tag "x"`).
`{ "terrainBonus": true }` was **not built**: terrain numbers (`defense`, `avoid`, ...) are the maker's own formula inputs, with no fixed notion of a bonus.

**Carry capacity.** `{ "carryBonus": 2 }` (always; -100..100): formulas read the sum of a unit's `carryBonus` effects as `a.carryBonus` / `b.carryBonus` (0 for a unit
without one), so a game's `rules.rescue.canCarry` can say `"a.str + a.carryBonus >= b.weight"`. The engine adds nothing else; a game whose formula never reads it ignores it.

**Saves.** New state: `Unit::refreshedBy` (above) and `goes` may now include the steal marker (8). Old saves load. **Tests.** `tests/test_srpg_skillblocks2_abilities.cpp`,
`tests/test_srpg_skillblocks2_skills.cpp`, `tests/srpg_player_skillblocks_test.py`.

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
| `{ "movePast": true }` | same | the USER moves to the square on the far side of the target (same line), if it can stand there |
| `{ "heal": 10 }` / `{ "heal": "full" }` | `self`, `ally`, `unit` | the target regains HP (refused at full HP) |
| `{ "steal": ... }` | `enemy`, range [1, 1] | takes one item of the target into the user's inventory ("Steal", below) |
| `{ "refresh": true }` / `{ "refresh": { "perTurn": 2 } }` | `ally` | a unit of the user's side that has acted may act again this turn ("Refresh an ally", below) |
| `{ "cure": ["poison"] }` / `{ "cure": "all" }` | `self`, `ally` | cures the target; valid targets are units that have something it cures |
| `{ "summon": { … } }` | `cell` | a new unit appears on the square (below) |

Refusals (all start `data/abilities.json: ability "<id>": `): `"target" must be "self", "ally", "enemy", "unit" or
"cell"`; `"effect" needs exactly one of push, pull, swap, moveBehind, heal, summon, unlock, steal, refresh, movePast, cure`; `"push" moves a neighbour:
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

## 7. Weapon ranks and stat caps

Two optional features, each switched on by its own data file. Owner decisions (2026-11-16): a unit has a **rank per
weapon group**; a weapon may **require a rank**; the **ladder is the maker's** (new games start with E, D, C, B, A,
S); growth is **per game either fixed or by use**; a weapon whose rank the unit lacks can be carried, traded, stored
and bought but **cannot be equipped or attacked with**, and the computer never picks it. **Stat caps** are the
highest a permanent stat can reach: a game-wide cap per stat, overridden by the class, plus a character's personal
adjustment. A game with neither file plays, saves and hashes exactly as before.

### Weapon ranks

**Scope.** Ranks belong to the weapons a unit *wields*: they need `rules.items.weapons` = `"carried"` (a game with
fixed weapons has no choice to restrict; the loader refuses a ladder there). A weapon takes part when it has a
`group` (docs/ITEMS.md "Who can wield what"); a weapon without one never needs a rank and never trains one. Usable
items are not wielded things in this engine (there are no staves), so a rank is a key of weapon items only.

**Data: data/ranks.json** (optional; refuses unknown keys)
```jsonc
{ "growth": "use",             // "fixed" (default) | "use"
  "ranks": [                   // the ladder, lowest first; 1..12 entries
    { "id": "e", "name": "E", "xp": 0 },     // the first must have xp 0
    { "id": "d", "name": "D", "xp": 30 },    // weapon experience needed; strictly rising, 0..9999
    { "id": "c", "name": "C", "xp": 70 },
    { "id": "b", "name": "B", "xp": 120 },
    { "id": "a", "name": "A", "xp": 180 },
    { "id": "s", "name": "S", "xp": 250 } ] }
```
Ids are lowercase letters, digits, `_`, `-`. A file whose `ranks` list is empty is the same game as no file (the
editor writes one to remember the feature was switched on), whatever else it says. Core: `Database::ranks`
(`RankRules`: `ladder`, `growByUse`, `groups`).

**Weapon groups** are the distinct `group` strings of data/weapons.json, in file order; a unit's ranks are indexed by
that list (`RankRules::groups`).

**Data: weapons.json** (per weapon; both keys need a `group`, and the game needs the ladder)
```jsonc
{ "id": "steel_sword", "group": "sword", "rank": "d", "rankXp": 2 }
```
`rank`: the lowest rank that may wield it (an id of the ladder); missing = any. `rankXp`: weapon experience the user
gains in the group per use when growth is `"use"`, 0..99, default 1 (when the key is absent). `0` is allowed: a weapon (or
staff) that trains nothing.

**Data: starting ranks.** A value is a rank id (the experience that rank starts at) or a whole number of weapon
experience (0 .. the top rank's `xp`); the group must be one some weapon has.
```jsonc
// units.json, per unit type: what a unit of this class starts with
{ "id": "lancer", "ranks": { "lance": "c", "sword": 45 } }
// characters.json, per character: personal ranks
{ "id": "kell", "ranks": { "lance": "b" } }
// maps/*.json, per unit (and in an addUnit command): replaces that group's starting value
{ "character": "kell", "ranks": { "lance": "a" } }
```
A unit holds one number per group, its **weapon experience** (`Unit::weaponXp`, indexed like `RankRules::groups`);
its **rank** in a group is the highest ladder entry whose `xp` is not above that number. A group nobody gave a value
is at the lowest rank. Both growth modes use this one number; with `"fixed"` it never changes except as below.

**Rules (decisions where the owner was silent)**
- *Class and character:* a unit starts with, per group, the **higher** of its class's value and its character's
  value. Characters stack on classes by adding elsewhere (stats, growths), but a rank is a level, not an amount, and
  "the higher wins" never lets a personal entry lower a class. A map unit's `ranks` then **replaces** the value of
  the groups it names (a map may place a weak lancer on purpose).
- *Promotion:* the unit keeps its experience and is raised, per group, to the new class's value where that is higher
  (the same "raise to the class's value" that runs when a unit is built, added or dressed from a roster, so it is
  idempotent and never lowers anything); a `RankUp` is emitted for each group whose rank rose.
- *Growth `"use"`:* once per combat, after the experience awards, each fighter that **struck at least once** with a
  weapon that has a group gains that weapon's `rankXp` in the group (the counterattacker too; a unit that died gains
  nothing; a missed strike counts). Experience stops at the top rank's `xp`. When it crosses a ladder entry the unit
  ranks up: event `RankUp`. Crossing several entries at once gives one event with the final rank.
- *Growth `"fixed"`:* nothing is gained by use.
- *Equip check:* a unit **can wield** a weapon when its class can (`Database::canWield`) **and** its rank in the
  weapon's group is at least the weapon's `rank`. `weaponsOf`, `weaponSlot`, `equippedSlot` and the `Equip`
  action use this one test (`Battle::canWieldNow`), so attack options, forecasts, counterattacks, the equipped
  weapon (the first inventory weapon the unit can wield *now*) and the AI all follow. Giving, taking, storing,
  buying and discarding never check ranks. The refusal text is `Needs rank B` (the ladder's display name).
  Nothing about "wieldable now" is stored: a rank-up makes a carried weapon usable at once.
- *A character's own groups:* `characters.json` `"wields": ["bow"]` (the same list of group names as a unit type's `wields`,
  docs/ITEMS.md "Who can wield what") lets that character wield those groups **in addition** to whatever its class can; it never
  removes one. A unit takes it with its character (so after a promotion too), a generic unit has none. It is part of the class
  side of the one wield test (`Database::canWield(type, weapon, character)`, used by `Battle::canWieldNow` and `canWieldStaff`), so
  locks and ranks still apply on top, and attack options, forecasts, counters, the equipped weapon, `Equip`, the AI and the player's
  greyed rows all follow. A staff group works the same (`"wields": ["staff"]`). Like the class key, group names are not checked
  against the weapons (a misspelt group simply grants nothing).
- *Formulas:* unit tables gain `rank`, a table of group id to rank index (0 = lowest), only in a game with ranks:
  `a.rank.sword >= 2`.
- *Story commands:* none in this version (see "Not in this version").

**Events.** `RankUp`: a = unit, b = weapon group (index into `RankRules::groups`), c = new rank (index into the
ladder), d = old rank. Seen by the unit's own team.

### Character-locked weapons

A weapon can be meant for named people ("only Kell can wield the Dawn Blade"). The class side stays what it is today: a
class can wield a weapon when the weapon is in its `weapons` list or its `group` is in its `wields` list (docs/ITEMS.md "Who
can wield what"); there is no list of classes on the weapon. The character lock is a second, narrowing check on top.

**Data: weapons.json** (optional key, needs data/characters.json)
```jsonc
{ "id": "dawn_blade", "name": "Dawn Blade", "group": "sword", "wieldedBy": ["kell", "mara"], "props": { "might": 9 } }
```
`"wieldedBy"` is a list of character ids (docs/SRPG.md "Characters"). Absent or an empty list = no lock (the same game as
without the key). Next to it, `"wieldedByTypes"` (optional, needs nothing else) is a list of **unit type ids** (data/units.json):
```jsonc
{ "id": "knight_lance", "group": "lance", "wieldedByTypes": ["knight", "paladin"], "props": { "might": 8 } }
{ "id": "dawn_blade",   "group": "sword", "wieldedBy": ["kell"], "wieldedByTypes": ["paladin"], "props": { "might": 9 } }
```

**The rule.** `canWield` is the class test (`Database::canWield`). A unit may wield the weapon now when

    class can wield  AND  (no list  OR  the unit's character is on `wieldedBy`  OR  its unit type is on `wieldedByTypes`)
                     AND  rank is enough

A weapon with either list is locked; a unit passes it if it matches **either** one (so the dawn blade above is for Kell and for
every paladin). A unit's type is its current one: a promotion that changes the class changes the answer at once. Both lists empty
or absent = no lock.

| class can wield | weapon has a list | unit's character          | result                                   |
|-----------------|-------------------|---------------------------|------------------------------------------|
| no              | any               | any                       | no (the class, as before)                |
| yes             | no                | any, or none              | rank decides, as before                  |
| yes             | yes               | on the list               | rank decides                             |
| yes             | yes               | not on the list           | no: "Only Kell and Mara can wield this"  |
| yes             | yes               | none (a generic unit)     | no: the same words                       |

- A lock never **grants**: a named character whose class cannot wield the weapon still cannot (so a character-only sword
  needs a `group` or `weapons` entry its wearer's class already has). A lock does **not bypass ranks**: rank is checked
  after it, as in section 7 (a one-person weapon normally has no `rank`).
- Everything that asks "can this unit fight with this weapon now" already goes through `Battle::canWieldNow`, so
  attack options, forecasts, counterattacks, the equipped weapon, the `Equip` action and the AI all follow. Carrying,
  trading, storing and buying never check it.
- The refusal text is `Only Kell can wield this`; with two names `Only Kell and Mara can wield this`; with more, the first two then
  "and others": `Only Kell, Mara and others can wield this`. A character's display name is used; with both lists the characters
  come first, then the unit types' display names (`Only Kell and Paladin can wield this`; the same short forms in the player).
  The wording is a placeholder the owner may change. When the lock and the rank both refuse, the lock is said.
- The player greys such a weapon like a rank-locked one and shows the reason (item screens, Equip entry).
- Generic units (no character) never match a list. The unit's character is the one its story id names (`Unit::character`).
- Loader errors (weapons.json, `weapon "dawn_blade"`): `"wieldedBy" must be a list of character ids`;
  `"wieldedBy" is given but this game has no characters (data/characters.json)`; `"wieldedBy" names unknown character "x"`;
  a character named twice. `weapon "x": "wieldedByTypes" must be a list of unit type ids`;
  `"wieldedByTypes" names unknown unit type "x" (data/units.json)`; a type named twice. The same on a staff (`item "x": ...`).
- Off unless used: no key, same game; the rules fingerprint changes only when a weapon has a non-empty list.

**As built.** Core: `WeaponDef::wielderIds` / `wielders` (database.h), `engine/core/data/loader_weapon_locks.cpp`
(`readWeaponWielders` reads the shape while weapons load; `resolveWeaponWielders` turns ids into indices as the first step of
`resolveSrpgRefs`, because characters.json loads after weapons.json), `Battle::meetsCharacterLock` and `canWieldNow`
(ranks.cpp; the order is class, then lock, then rank). `Unit::character` is what "the unit's character" means. The fingerprint
reads weapons.json whole, so a used key changes it by itself; `rulesOnly` (save_hash.cpp) skips an empty `"wieldedBy": []` and `"wieldedByTypes": []`.
`wielderTypeIds` / `wielderTypes` (database.h) are filled in the same step as `wielders` (`resolveGearWielders`, which staves share); `meetsLockFor` holds the OR rule. The
player needed no change: it shows the core's reason text. Tests: tests/test_srpg_locks_types.cpp.
Player: `weaponBlockReason` (rank_text.cpp) now answers from `canWieldNow`, so the Items screen, its choice menu (Equip greyed with
the reason), the unit details' Items page and the side panel need no code of their own; `Menu::draw` shortens an `extra` that is
wider than the box (a phone: "Equip  Only .."; the full words are on the Items list and the details page, which also shortens
with ".." at 270 px). Promotion previews (`cannotWield`) look at the class only and do not mention locks. Tests:
tests/test_srpg_locks.cpp, tests/srpg_player_locks_test.py, fixture tests/fixtures/srpg-locks.

**Editor as built.** No helper change (the helper keeps `wieldedBy` and the loader reports its errors). Weapons screen: the card "Only these characters can wield it"
(`WeaponLockCard`, a checkbox list of the characters; one column at 360 px), shown only when weapons are carried and the game has characters; the key is written only
when the list is not empty, and left untouched while the card is hidden. Follow-through (`model/locks.ts`): deleting a character takes it off every list (named in the
delete question, one Undo step); an emptied list loses its key; switching Characters off removes all lists (the confirmation says how many weapons had one); renaming a
character id is `renameCharacterLocks` (the editor has no screen that renames a character id yet, so nothing calls it). Checks (`model/lockChecks.ts`, "Worth a look" on the
card): a name that is no character, and a list where no character's class can use the weapon (class `weapons` or `wields` of the weapon's group). Characters screen: a
read-only card "Weapons locked to this character". Tests: `tests/unit/locks.test.ts`, `editor/web/tests/e2e/srpg-locks.spec.ts`.

### Stat caps

**Data: data/caps.json** (optional; refuses unknown keys): the game-wide cap per stat.
```jsonc
{ "caps": { "strength": 30, "speed": 30, "hp": 60 } }   // stat ids; 1..99999; a stat not listed has no game-wide cap
```
An empty `caps` object (or no file) is the same game as no file. Core: `Database::statCaps` (per stat index, -1 =
none).

**Class: units.json** `"caps": { "strength": 40 }` overrides the game-wide cap of those stats for the class (higher
or lower; a class cap is a cap even where the game has none). Core: `UnitTypeDef::caps`.

**Character: characters.json** `"capAdjust": { "strength": 3, "speed": -2 }` is added to whatever cap the unit's
class or game gives that stat (a stat with no cap anywhere stays uncapped; the result is never below 0). Core:
`CharacterDef::capAdjust`.

**The rule.** `Battle::statCap(unit, stat)` = (class cap, else game cap) + the character's adjustment; -1 = none.
It acts where a **permanent** stat (`Unit::stats`) rises: level-ups (growth gains), promotion gains and
stat-raising items (`raise`); no story command raises a stat today. One function, `Battle::raiseStat`, applies a
gain: a positive gain is cut to what room is left under the cap, a gain never pushes a stat that is already at or
over its cap lower (a unit the map places above its cap stays there), negative gains always apply in full. The
events carry what really happened (`LevelUp` data, `StatsRaised` data, `UnitPromoted` change). Dice are rolled as
before, so a capped stat costs the same random numbers. A promotion judges the cap of the **new** class. HP: the
max-HP stat can be capped like any other; current HP is clamped to the maximum as today and gains of the HP stat
still arrive filled. An item whose `raise` could change nothing (every listed stat at its cap) counts as doing
nothing, like a heal at full HP.

**Temporary bonuses are not capped**: accessories, research, skills, supports, pair-up, carrying, terrain and
wielded-weapon bonuses are added on top of the permanent stat by `Battle::stat`, which never looks at caps. The cap
limits *growing*, not *being*.

### Saves and hash
- Battle save, only in a game with ranks: `units[i].ranks` = `{ "sword": 120 }`, group id to weapon experience, only
  the groups above 0; every key a group of the game, every value 0 .. the top rank's `xp`. Roster entries of a
  playthrough get the same `ranks` key. Refused in a game without ranks.
- Caps add no state. Hash: `data/ranks.json` (only with a non-empty ladder) and `data/caps.json` (only with a
  non-empty `caps`) are mixed after the supports file; a rank's `name` is a cosmetic key. Everything on units,
  weapons, characters and maps is already hashed whole.

### AI
Nothing to decide: the AI picks from `weaponsOf`, which only holds weapons it may wield now. A unit that carries
only weapons above its rank has no attack and behaves like an unarmed one.

### Player
- **Unit details, Stats page:** a capped stat shows `MAX` when it is at its cap. **Items page:** a weapon the unit
  cannot wield now is greyed with its reason (`Needs rank B`). A **Ranks** block (only in a game with ranks): per
  group the rank's display name and, with growth `"use"`, the experience towards the next rank (`Sword  C  45/70`;
  the top rank shows `MAX`).
- **Item menu / equip / attack picks:** an unwieldable weapon is listed greyed with `Needs rank B` and cannot be
  chosen.
- **Level-up box:** a stat that is at its cap shows `MAX` next to the gain.
- **`RankUp`:** a floating label over the unit, `Sword rank C`, like `SkillLearned` outside a level-up box.

### Editor (built later, see "Editor: as built" below)
Screens: a **Weapon ranks** section in Rules (growth mode, the ladder as an editable list with id / name /
experience; a "default E to S ladder" button), the weapon form (`rank`, `rankXp`), the unit type form (`ranks` per
group, `caps`), the character form (`ranks`, `capAdjust`), the map unit form (`ranks`), and **Stat caps** in Rules
(one optional number per stat). Problems to report: a rank id not in the ladder, a group no weapon has, `rank`
without a `group`, an `xp` that is not rising, a ladder with fixed weapons. The editor must keep the keys it does
not edit (see "as built").

### Not in this version
Story commands that set or raise a rank or a cap; experience from healing or other non-combat use; ranks for fixed
weapons; rank-gated skills.

### Weapon ranks and stat caps: as built

Core: `engine/core/data/ranks.h` (`RankRules`, limits), `loader_ranks.cpp` (`ranks.json`, `caps.json`, the keys of weapons, unit types,
characters and map units; loaded before the weapons, groups built right after them), `engine/core/battle/ranks.cpp` (`weaponRank`,
`meetsRank`, `canWieldNow`, `rankRaiseToClass`, `rankOverride`, `ranksAfterCombat`), `caps.cpp` (`statCap`, `raiseStat`, `canRaiseAny`),
`save_ranks.cpp` (`ranks` key of a unit and a roster entry). Differences from the text above, and what the text left open:

- `Unit::weaponXp` / `RosterUnit::weaponXp` / `MapUnitDef::weaponXp` (-1 = not said) hold the experience; `dressUnit` calls
  `rankRaiseToClass` (so building, adding, summoning and a roster all get the class's ranks), `makeUnit` applies the map's override after it.
- Experience from use is given **after the XP awards** of the same combat (a level-up box comes first) and before the end-of-combat skills;
  the strikes are read off the combat's `Attack` events, so a weapon that broke during the fight still counts.
- A rank-up by promotion is emitted right after `UnitPromoted`. A unit that dressed from a roster or was built is raised silently.
- The refusal text of `Equip` is `Needs rank B` (the rank's display `name`); `validate` of a weapon the class cannot wield at all still says
  "the unit cannot wield that".
- Caps: `statCap` is cached nowhere; `raiseStat` is the only place a permanent stat grows (`progression.cpp`, `promotion.cpp`, `items.cpp`).
  Item `raise` events report the real gain for positive amounts and the written amount for negative ones (as before).
- The loader refuses `rank` / `rankXp` without a `group`, ranks without carried weapons, and a ladder whose first `xp` is not 0, with messages
  that name the file and the entry (tests: `tests/test_srpg_ranks_loader.cpp`).
- An `addUnit` command reads `ranks` exactly like a placed unit (`readUnitBody`, so the same group / rank / experience checks and
  messages, also inside `if` / `then` / `else`); the added unit is dressed from its class and then takes the command's values.
- Not built: story commands (see "Not in this version" above). The editor screens came later ("editor as built" below).
- Player: `engine/player/battle/rank_text.*` (words), `battle_view_ranks.cpp` (the `RankUp` label), cap and rank rows in
  `unit_info_rows.cpp`, greyed weapons in `battle_items.cpp` / `panel.cpp`, the cap line of the level-up box in `battle_view.cpp`.
  Tests: `tests/test_srpg_ranks*.cpp`, `tests/test_srpg_caps.cpp`, `tests/test_srpg_player_ranks.cpp`, `tests/srpg_player_ranks_test.py`, the
  fixture `tests/fixtures/srpg-ranks`.

### The foe's weapon in formulas (`fw`): as built

`FormulaCtx::fw` (`formulas.h`) is pushed as the eighth chunk argument (`Formulas::pushFoeWeapon`); an empty one is a table with
`none = 1` and a shared metatable (`emptyWeaponMeta_`) whose `__index` returns 0. Set in `Battle::strikeForecast` (the one place the
forecast, `doStrike` and every AI score go through; foe weapon = `counter ? setup.weaponA : setup.weaponD`) and in the skill
`formula` condition and `chance` expression (`skill_cond.cpp`, `skill_fx.cpp`, from `SkillCtx::foeWeapon`, which was already there).
- `fw` is the foe's **answering** weapon: the first of its weapons with `counter` that may target `a` (the equipped one with carried
  weapons, which `canWieldNow` already filters), also when it is out of range; `fw.answers` (`FormulaCtx::fwAnswers`, set in
  `strikeForecast` from `CombatSetup::counters`, so no extra pass) says whether it really can answer. A foe holding only a non-`counter` weapon (a bow with
  `counter: false`) counts as having none.
- The xp, capture and `canCarry` formulas do not have `w` either, so `fw` is empty there. Passing the fight's two weapons to the xp
  formulas was left out.
- `w` and `fw` are the prop table plus `id` and `group` as text (`pushWeapon`; the empty `fw` has both as `""`).
- The editor has no list of formula variable names (checked `editor/web/src`: no help or hint list enumerates `a`, `b`, `w`), so
  nothing there changed.
- Tests: `tests/test_srpg_foe_weapon.cpp`, `tests/srpg_triangle_fixture.h`, the fixture `tests/fixtures/srpg-triangle`.

### Weapon ranks and stat caps: editor as built

The editor screens are in; the helper only gained two sections. Details for the editor are in docs/EDITOR_UI.md "Weapon ranks and stat caps".

- **Helper/API**: `ranks` and `caps` are two more optional object sections beside `supports` (`editor/server/srpg_files.cpp`): GET gives the file's object or `null`,
  PUT writes the object (an empty ladder and empty `caps` too, so a switch that is on stays on; the engine reads them as no file), `false` removes the file.
  `tests/editor_api_ranks_checks.py` starts from `tests/fixtures/srpg-ranks`.
- **Screens**: Rules switches and the two cards; class (starting ranks, caps), character (personal ranks, cap adjustment with the resulting cap), weapon (needed rank,
  experience per use when growth is "use"), placed unit (rank override). Everything fits 360 px.
- **Readings of the contract.** (1) *Ladder editing*: the first rank is always 0; a number that is not above the rank before is refused with "At least N."; raising a
  rank moves the ones above up; moving a rank moves its id and name while the experience numbers stay at their steps. At most 12 ranks. The default ladder button only
  shows while the ladder is empty. (2) *Switch on*: ranks need carried weapons (the switch is disabled with the reason; if weapons are set back to fixed later a check says
  so). (3) *Removing a rank* takes it off weapons that needed it and drops starting entries naming it (those units start at the lowest rank); a rank id change follows
  everywhere. (4) *Groups*: typing a new group on the only weapon of a group is a rename (classes that wield it and starting ranks follow); clearing or merging a group keeps
  the entries and the checks name them (nothing is lost while typing); deleting the last weapon of a group drops its starting ranks (the delete question lists them).
  (5) *Deleting a stat* removes its game cap, class caps and character adjustments. (6) *Switching ranks off* asks first (when a ladder exists) and removes every weapon
  `rank` / `rankXp` and every `ranks` map; *switching caps off* removes the file, every class `caps` and every `capAdjust` (a class cap works without the file in the engine, so
  it is treated as part of the feature). `rankXp` is kept when growth goes back to "fixed" (hidden, harmless). (7) Starting ranks written as experience numbers in a file are
  kept and shown ("N experience (kept)"). (8) Checks ("Worth a look", beside the loader's own words in Problems): ladder shape, a weapon rank without a group or naming an unknown
  rank, a starting rank in a group no weapon has or an unknown rank id or a number above the top, a cap that is not 1 to 99999, a ladder that no weapon asks for.
- **Tests**: vitest `ranks.test.ts`; `tests/editor_api_ranks_checks.py`; Playwright `srpg-ranks.spec.ts` (build a ladder, Play, sticky switches) and `srpg-ranks-screens.spec.ts`.

---

## 8. Staves

A **staff** is an item a unit carries, wields by group and rank like a weapon, and **uses on another unit at range** as its
action. Owner's answers (PLAN item 3l, 2026-11-16): staves come in **stages** (ranged and area healing; moving units; utility;
then status conditions built by the maker from blocks, with a starter set and curing staves); whether a staff can miss is **the
maker's choice per staff** (healing defaults to certain, a hostile one gets a hit chance). This section is the contract for
**all** stages, so the later ones fit without rework. Stages 1 (healing) and 2 (moving units: warp, pull, swap; utility: repair, light)
and 3 (status conditions: `inflict` and `cure`, section 9) are built. A game with no staff plays, saves and hashes exactly as before.

### Where a staff is defined, and why

A staff is an **item of kind `"staff"` in `data/items.json`**. Not a weapon (weapons are shaped around strikes: damage formulas,
counters, `weaponsOf`, `Attack` events) and not an ability (abilities belong to the unit: no inventory, no wear). As an item it
gets, with no new code, everything items already have: uses and breaking (`ItemStack::uses`, `ItemBroke`), price, shops,
starting items, drops, trading, the shared storage, the campaign roster and the save format. What it takes from the other two:
the **group, rank, rank experience and character lock** of a weapon (it is wielded), and the **conditions** of an ability
(it acts on a unit). Both are reused, not copied: the staff carries a small `WeaponDef` of its own (`StaffDef::gear`: id, group,
props, rank fields) that is never in `Database::weapons`, so the same rank and lock code answers for both and a formula sees the
staff as `w`.

```jsonc
{ "id": "mend_staff", "name": "Mend Staff", "kind": "staff",
  "description": "Heals an ally.",             // optional, one line
  "group": "staff",                            // required: what a class lists in "wields" (units.json) and ranks are kept in
  "rank": "d", "rankXp": 2,                    // optional, as on a weapon (needs data/ranks.json); rankXp 0..99, default 1
  "wieldedBy": ["mara"],                       // optional character lock, as on a weapon (needs characters)
  "wieldedByTypes": ["cleric"],                // optional unit type lock, as on a weapon (character OR type)
  "props": { "power": 6 },                     // optional numbers a formula reads as w.power
  "uses": 8, "price": { "gold": 400 },         // as every item; no "uses" = unlimited
  "range": "1 + a.mag / 3",                    // reach, see below. Default [1, 1]
  "target": "ally",                            // who it works on, see below. Default "ally"
  "area": "single",                            // "single" (default) | { "radius": 2 } | "all"
  "effects": [ { "heal": "w.power + a.mag / 2" } ],   // 1..4 effects, run in order on each unit hit
  "hit": "80 + a.skill - b.res",               // optional percent chance per unit. Missing = always works
  "xp": 15,                                    // optional experience for the user: a number or a formula. Missing = none
  "if": [ { "hp": { "op": "<=", "percent": 50 }, "who": "foe" } ] }   // optional: the conditions of ability `if`
```

- **`range`**: a whole number `n` = reach 1 to n; a list `[min, max]`; or a **formula** (text) giving the **maximum** (the minimum
  is 1): `a` = the user, `w` = the staff, `b` = nil. Rounded down, 0 to 20; 0 = nobody is in reach. Judged on the user's stats as
  the rules see them now, so a promotion or a boost changes it. Distance is the grid distance (squares and hexagons alike) from
  the square the unit would use it on.
- **`target`**: `"ally"` (another unit of the user's team), `"self"` (the user; no range, nothing to pick), `"team"` (an ally or
  the user), `"foe"` (a unit of another team the user's team sees), `"any"` (any other unit the team sees). A stage's effect may
  refuse some (healing refuses `"foe"`).
- **`area`**: `"single"` = the picked unit only; `{ "radius": n }` (1 to 10) = the picked unit and everyone that fits `target`
  within that distance of it (the picked unit must itself be a legal target in range); `"all"` = every unit that fits `target`
  and is in range, nothing is picked. In an area each unit is judged on its own (a healthy ally in it is simply not affected,
  gets no roll and no event). The user is in an area only when `target` lets it be (`"team"`).
- **`effects`**: the engine's existing effect vocabulary, in the shape of the skill effect blocks (section 3) and abilities
  (section 4); a staff adds no second language. Stage 1: `{ "heal": 10 }`, `{ "heal": "full" }`, `{ "heal": { "percent": 30 } }`
  and `{ "heal": "<formula>" }` (a number; `a` = user, `b` = the unit healed, `w` = the staff, `dist` = distance between them,
  `ta` / `tb` the squares). A formula that fails counts as 0. Stage 2 adds `warp`, `pull`, `swap`, `repair`, `light` ("Staves: stage
  two, as built"); stage 3 adds `inflict` and `cure` (section 9), and `unlock` (opens a door or chest at range) came with doors (section 10).
- **`hit`**: a formula, a percent (rounded, 0 to 100), same variables as `heal`. Rolled **once per unit in the area**, in
  ascending unit id, with `Battle::rng` (`rng.chance`), so a battle replays from its seed. Absent = no roll at all (no random
  number is spent). A miss still spends the use and earns the experience. Queries never roll.
- **`xp`**: experience for the **user**, once per use (not per unit), also when it missed. A number (0 to 1000) or a formula
  (`a`, `b` = the picked unit or nil for `"all"`, `w`, `dist`), through the normal award (`xpPercent` skills, level-ups,
  promotion). No `xp` = none. Needs `rules.progression` (the loader says so).
- **`if`**: the conditions of an ability (section 3: CONDITIONS), judged for the picked unit and each unit of an area (`"who":
  "foe"` = that unit, as in abilities; without it, the user).
- Loader refusals follow the existing style, e.g. `data/items.json: item "mend_staff": "group" is required for a staff`,
  `"range" must be a whole number from 0 to 20, a list [min, max] or a formula`, `"target" must be "ally", "self", "team", "foe"
  or "any"`, `"area": "radius" must be from 1 to 10`, `"effects" needs 1 to 4 effects`, `"heal" cannot target "foe"`, `"hit":
  <Lua message>`, `"rank" is given but this game has no weapon ranks (data/ranks.json)`, `"xp" needs rules.progression`.

**Groups.** The weapon groups of a game are the distinct `group` strings of weapons.json, in file order; **the groups of staves
are added after them**, so no existing index moves. A staff's group is a normal group: `ranks` maps of classes, characters and
map units name it (`"ranks": { "staff": "c" }`), `Unit::weaponXp` holds its experience, the ladder and the growth are the
game's. A game without ranks needs no rank anywhere.

### Who can use it (the one wield test)

`Battle::canWieldStaff(unit, item, why)` is the staff's twin of `canWieldNow`, with the same three steps and the same words:
the **class** can (`"staff"` in the unit type's `wields`; a staff is never in a `weapons` list), the **character lock**
(`wieldedBy`: `Only Mara can wield this`) and the **rank** (`Needs rank C`). A staff that fails can still be carried, traded,
stored and bought; it is shown greyed with the reason. The lock and rank tests are the very functions of section 7, now taking
the weapon data (`meetsRankFor`, `meetsLockFor`), so one change is a change in both.

### How it is used

One new action, `ActionType::UseStaff` (`Action::useStaff(unit, slot, target)`), appended at the end of the enum.

1. The unit can act, has not finished (`done`) and has not used its action (`acted`). It may have moved first, like an attack.
   Using a staff **ends the unit's turn** (`endOrder`), like Attack; the player's "undo move" is gone after it.
2. The slot holds a staff the unit can wield now.
3. The target is legal: alive, on the map, **seen by the user's team**, a legal `target` kind, within range, passes `if`, and
   **at least one unit in the area would really change** (for healing: is hurt). A staff that would do nothing is refused:
   `nobody in reach needs that`. This keeps the AI from wasting uses and is what greys the entry. `"all"` ignores `target`
   (the action carries -1 or the user).
4. Carrying out: `StaffUsed`; for each unit in the area, ascending unit id: the `hit` roll, then the effects (`UnitHealed`, or
   `StaffMissed`); supports (`supportsHealed`, per healed ally); then **the use is spent** (`spendUse`; at 0 the staff breaks,
   `ItemBroke`); then the user's experience (`xp`, the normal award); then the rank experience (growth `"use"`: the staff's
   `rankXp` into its group, one `RankUp` if a rank is crossed); then `endOrder`. Story events that add units run last, so
   nothing reads a unit after them.

**Queries** (never roll, change nothing; `from` = where the unit would stand, a menu after a move): `staffOptions(unit, from, out)`
(one entry per staff carried: usable now or the plain reason), `staffTargets(unit, slot, from, out)` (the units that may be
picked; `"all"` returns the user alone, meaning "confirm"), `staffForecast(unit, slot, target, from, out)` (units hit, what each
would get, the hit chance, the experience, the uses left, whether it breaks) and `staffRange(unit, slot, min, max)`.

### Player

- **Menu.** The action menu gains **Staff** (placeholder) after the abilities when the unit carries any staff, greyed with the
  reason of the first staff when none can be used now. It opens a list of the staves carried: name, uses left ("3/8"), and the
  reason when one cannot be used (`Needs rank C`, `Only Mara can wield this`, `Cannot wield`, `Nobody in reach needs it`).
  Cancel returns to the action menu. A reason that does not fit a narrow menu (the phone) is shortened in two steps ("Nobody needs it",
  "No one"; "Only Mara", "Locked"; "Rank C"; "Class") and left out when even that does not fit; the Items screen says it in full.
- **Picking.** Choosing a staff shows its **range** (every square in reach in the blue mask) and picks a unit like Attack
  (arrows, Tab, a tap); the area of the highlighted unit is marked red (a radius, or everyone for `"all"`). Cancel goes back to the
  action menu.
- **Forecast box**, the look of the attack forecast: the staff's name and uses left, one line per unit hit ("Kell  HP 7 > 17"), a
  "Hit 80%" line only when the staff can miss, "Exp 15" when it gives any, "Breaks" on its last use; at most four unit lines,
  then "+N more".
- **Playing it.** `StaffUsed` floats the staff's name over the user (like an ability) and the camera looks at the user; each
  `UnitHealed` is the usual green "+N" with the HP bar filling; `StaffMissed` floats "Missed" over the unit; `ItemBroke` says
  "broke"; `XpGained`, `LevelUp`, `RankUp` are as after a fight. Each use writes `[info]` lines the windowless tests read.
- **Everything else follows.** The Items screen, the unit details and the info panel list staves like any item (the Items
  screen greys one the unit cannot wield, with the reason).

### AI

Stateless like the rest. For every square a unit may stand on this turn (the stops of its reach) the AI asks `staffOptions`,
`staffTargets` and `staffForecast`, scores each usable staff, and keeps the best if it **beats the unit's best attack, capture or
advance** on the same scale (`kAttackBase` + value). A **heal's value** is the share of the target's HP it would really restore
times the target's value (production cost, default 1000; a leader whose death ends the game counts double), times the hit
chance, summed over the units the area hits. A heal that would restore less than half of the staff's nominal amount to a unit
above half HP is **not worth a use** (it keeps the staff for a wounded one). Only a unit that would change is a legal target, so
it never spends a use on an unhurt unit. The exposure of the square it would stand on is subtracted as for an attack. It never
uses a staff it cannot wield or that reaches nobody, and (stage 1) never one on foes. The unit order is the existing one (best
score first), so a healer that must walk to the wounded goes by its score like any other unit.

### Events

Appended to `EventType` (existing numbers keep their values):

| Entry | a, b, c, d | Seen by (fog) |
|---|---|---|
| `StaffUsed` | a = user, b = item, c = the picked unit or -1 (`"all"`), d = units that were really affected | the user's team and the teams that see the user or the picked unit |
| `StaffMissed` | a = the unit missed, b = user, c = item | the teams that see either unit |

What the effects did arrives as the existing entries: `UnitHealed` (a = unit, b = amount), `ItemBroke`, `XpGained`, `LevelUp`,
`RankUp`, and (stage 2) `UnitRelocated`, with `ItemRepaired` and `AreaLit` appended to the list; stage 3's condition events are in section 9.

### Saves, hash, campaign

- **No new saved state in stage 1** (stage 2 saves the lights that still last, see "Staves: stage two, as built"). A staff's uses are the item's (`ItemStack::uses`: saved and restored like a weapon's, in a
  battle's units, the storage and a campaign roster entry); its experience is `Unit::weaponXp` (`ranks`, section 7).
- **Hash.** `data/items.json` is hashed whole when it exists, so staff keys are part of a game's rules and change its fingerprint
  **only in a game that has them** (a name and description are cosmetic keys). Nothing else is mixed in.
- **Off unless used.** No staff, no staff group, no extra event, no random number: every existing game, save and sim stays byte
  for byte the same.

### Staves: as built (stage 1)

Core: `engine/core/data/staff.h` (`StaffDef`, `StaffEffect`; `Database::staves`, `ItemDef::staff` = the index), `loader_staves.cpp`
(the item's own keys, the staff groups appended to `RankRules::groups`, `"wieldedBy"` and `"if"` resolved in `resolveSrpgRefs`),
`engine/core/battle/staves.cpp` (wield test, range, `staffAffected`, `staffTargets`, `staffOptions`, `staffForecast`) and
`staves_apply.cpp` (`validateStaff`, `applyStaff`), `ranks.cpp` (`meetsRankFor`, `meetsLockFor`, `addRankXp`, the weapon-index
functions now delegate), `ai/ai_staves.cpp`. `awardXp` gained a `fromCombat` flag (a summon's `givesXp` is about fights). Events
`StaffUsed`, `StaffMissed` (visibility in `event_visibility.cpp`; the sim's event printer names them). Player:
`battle_menu_staves.cpp` (the entry, the list, the pick), `staff_forecast.cpp`, `staff_text.*` (all the words), `battle_view_staves.cpp`,
`rank_text.cpp` (`weaponBlockReason` also answers for staves, so the Items screen, the unit details and the side panel grey them).
Tests: `tests/test_srpg_staves*.cpp`, `tests/srpg_player_staves_test.py`, the fixture `tests/fixtures/srpg-staves` (maps `clinic` and
`skirmish`), `tests/srpg_content_test.py` (no stalls, same seed twice).

What the contract above left open, and how it was settled (the owner may change any of these):
- *Ends the turn.* A staff use ends the unit's turn like an attack; it can follow a move but not another action (`acted`).
- *Nothing to do is refused*, not offered: a heal needs a hurt unit in the area. A picked unit of an area may itself be whole when a
  neighbour in the area is hurt.
- *Uses*: one per use, whatever number of units it reached or whether it missed. A staff whose last use is spent breaks (`ItemBroke`).
- *Experience*: none unless the staff says `xp`; then once per use, also on a miss (the casting is the work), through the normal award.
  Rank experience (growth `"use"`) is the staff's `rankXp` per use, also on a miss, stopping at the top rank's number.
- *Range*: a number is 1 to n; a formula is the maximum, minimum 1, taken on the user's stats (not the square's). `"self"` staves take
  no range and no area. Reach is measured from the square the unit would use it from.
- *Heal amount*: a formula's number is rounded down, never below 0; `"percent"` is of the receiver's maximum, at least 1; the heal never
  exceeds the maximum; several `heal` effects add up and are capped together.
- *Supports*: healing another unit counts for supports like a heal item or ability does (`supportsHealed`), once per unit healed.
- *Ranks need carried weapons* (section 7): a game with fixed weapons has staves without ranks (`wields` still decides the class).
- *Fog*: units of a foe or `"any"` staff must be seen by the user's team; allies always are. Events: `StaffUsed` is seen like an
  ability use, `StaffMissed` by the teams that see either unit; the experience and rank events stay the user's team's.
- *Greyed reasons*: the first reason that applies, in this order: the class ("the unit cannot wield that"), the lock, the rank, then
  "nobody in reach needs that".
- *AI*: as in the AI section; the value of a heal is its share of the target's HP times the target's value, doubled for a game-ending
  leader, times the hit chance; the AI heals before it advances and compares fairly with an attack through the same `kAttackBase`.
  It does not move towards the wounded (a healer with nothing to heal behaves like any unit with no attack: it advances).
- *Not built in stage 1*: foe-targeting effects (nothing but the heal exists), the `"cell"` target, picking among several items,
  staff sounds, the editor screens (a separate step). The wording of every player text is placeholder (`staff_text.h`).

### Staves: the editor, as built

The Items screen has the kind **Staff** (editor step; no engine or helper change). Cards: *How it is used* (group; who it can be used on: another unit on the user's side / only the user /
anyone on the user's side including the user / any other unit the user's side can see; reach: up to N squares, from N to M squares, or a formula, with an example; area: one unit, everyone within N
squares of the unit picked, everyone in reach), *What it does* (healing as a number, a percentage, all HP or a formula; "It can miss" off = no `hit` key, on = a chance box taking a percent or a formula;
experience for the user, a number or a formula), *Numbers for formulas* (`props`), then the weapons' rank card and character-lock card, the abilities' "Only if" condition editor (`if`), and a "Worth a look"
banner. How it was settled where the owner was silent (any can be changed):
- *Weapons fixed*: a staff is wielded by group whether or not weapons are carried, so its lock card shows whenever the game has characters and a class can be given its group (Units: "Can wield groups").
  The rank card follows the weapons' rule (shown once the game has a ladder). The ladder itself still needs carried weapons (existing check).
- *Healing only (stage one; superseded by the stage-two part below)*: the form edited the first effect, which had to be a heal; further effects, and any key a hand-edited file carries, were kept untouched. "An enemy" is offered
  as a target only to a staff that does not heal, or when the file already has it.
- *Only the user* has no reach and no area (the loader refuses both), so choosing it removes them in the same step.
- *Kind switch*: a new staff gets a group (one an existing staff has, else "staff"), reach 2, a heal of 10 and 8 uses. Leaving the kind removes `group`, `rank`, `rankXp`, `wieldedBy`, `props`,
  `range`, `target`, `area`, `effects`, `hit`, `xp` and `if`, and starting ranks in a group nothing has any more.
- *Checks* mirror the loader (group, reach 1 to 20 and min not above max, radius 1 to 10, heal numbers, hit, xp needing experience on, a rank with no ladder, a lock with no characters, a heal on foes) and add
  "nobody can use it yet: no class can wield its group". Code: `editor/web/src/model/staff.ts`, `staffChecks.ts`, `gear.ts`; screens `screens/items/staff/`. See docs/EDITOR_UI.md "Staves".

### Staves: the editor, stage two, as built

The Staff form's "What it does" card is now a set of parts a maker switches on, matching what the loader allows together; the helper (C++) is unchanged. Placeholder wording throughout (`screens/items/staff/text.ts`).
- *Parts*: **It heals** (number, percentage, all HP, formula; as before), **Moving a unit** (none / warp with its reach as a number, from-to or formula, measured from the user / pull / swap),
  **It repairs an item** (all uses or a number of uses, 1 to 999), **It lights up an area** (radius 1 to 10, turns 1 to 10). Heal + move and heal + repair are allowed; a staff needs one effect at least
  (the last one cannot be switched off) and at most four.
- *Order*: each part is found by its kind wherever it stands, edited in place, a new one is appended, a removed one only leaves its own place. A file is never rewritten by opening it. Effects the form
  does not know (an unknown kind, a second heal, a second move, a malformed one) are kept and the card says "N more effects that the editor cannot show are kept as they are".
- *Illegal combinations are impossible, not just flagged*: with repair on, the move choices are greyed (and the reverse); pull and swap are greyed while "Only the user" is chosen and "Only the user" is
  greyed while pull or swap is on (warp and repair may use it); a move or repair brings a wider area back to one unit in the same step and greys the wider choices; "An enemy" is offered only to a staff
  that does not heal and healing cannot be switched on for one; one move kind per staff (choosing another replaces it). The model functions refuse the same edits, so a script cannot make them either.
  A hand-edited file that has such a combination still opens; the checks name it (the loader's wording style, `blocks`).
- *Light*: switching it on replaces **all** other effects and removes `target`, `area`, `hit` and `if` in the same step (the pattern of "Only the user"; no question is asked, one Undo brings it all back);
  `range` and `xp` stay (a staff that had no reach gets 2). The form then hides the target, area, chance of missing and conditions. It is refused while the staff has an effect the form cannot show
  (nothing is ever dropped). Switching it off leaves a heal of 10.
- *`repairable`*: the uses card of **any** item with uses shows "A repair staff can mend it" when the game has a staff with a repair effect, or the item already has the key. On = key absent; off =
  `"repairable": false`. Making an item unlimited (or an accessory) removes the key (the loader refuses it on an item without uses); the item checks name a hand-written one.
- *Checks* (`staffChecks.ts`, `itemChecks.ts`): every stage-two refusal of the loader (warp reach shapes and keys, `pull` / `swap` not true, `repair` 1 to 999 / full, light radius / turns / keys, unknown
  effect, 1 to 4 effects, one move / repair / light, repair with a move, light alone with no target / area / hit / if, move or repair needing area "single", pull / swap not on the user, `repairable` not
  a boolean or on an item with no uses). Worth a look (not blocking): a light staff while no map has `fog`; a repair staff while no item with uses is repairable.
- *The "Only if" box* of a staff says "The unit using the staff" (the abilities' editor takes a label).
- Code: `model/staffFx.ts`, `staffEffects.ts`, `staff.ts`, `staffChecks.ts`; screens `screens/items/staff/StaffEffectCard.tsx`, `StaffOtherParts.tsx`. Tests: `tests/unit/staves2.test.ts`,
  `tests/e2e/srpg-staves2.spec.ts`, `tests/editor_api_staves_checks.py` (stage-two refusals).

### Staves: stage two, as built (warp, pull, swap, repair, light)

Same item, action (`ActionType::UseStaff`), wield test, uses, `hit`, `xp`, `if`, rank and lock handling as stage 1; five effect kinds
more, and the action grew three optional fields. `unlock` is **not** part of stage two: it came with doors and keys (section 10), where it
is described. The editor form covers these kinds (see "Staves: the editor, stage two, as built" above;
stage one edited a first heal only); the editor helper keeps every key of them untouched through GET and PUT
(`tests/editor_api_staves_checks.py`), and the loader reads them, so a hand-written file works.

```jsonc
{ "id": "warp_staff",   "kind": "staff", "group": "staff", "range": 3, "target": "ally",
  "effects": [ { "warp": { "range": 4 } } ] }                 // pick the unit, then a free square within 4 of the USER
{ "id": "recall_staff", "kind": "staff", "group": "staff", "range": 8, "target": "ally",
  "effects": [ { "pull": true } ] }                           // the unit lands on the free square next to the user nearest to it
{ "id": "swap_staff",   "kind": "staff", "group": "staff", "range": 4, "target": "ally",
  "effects": [ { "swap": true } ] }                           // the user and the unit trade squares
{ "id": "fix_staff",    "kind": "staff", "group": "staff", "range": 2, "target": "ally",
  "effects": [ { "repair": "full" } ] }                       // or "repair": 3 (uses); the player picks which item
{ "id": "lantern_staff","kind": "staff", "group": "staff", "range": 6,
  "effects": [ { "light": { "radius": 2, "turns": 2 } } ] }   // pick a SQUARE; "turns" optional, default 1
{ "id": "worn_blade", "kind": "weapon", "weapon": "blade", "uses": 4, "repairable": false }   // any item: a repair never offers it
```

**The effects**
- **`warp`**: `{ "warp": { "range": n | [min, max] | "<formula>" } }`, the forms of the staff's own `range` (a number is 1 to n, a list
  is `[min, max]`, a formula gives the maximum with minimum 1 and sees `a` = the user and `w` = the staff; 0 to 20). The distance is
  **from the user**, not from the target. The use is two picks: the unit (per `target`, `range`, `if`), then an **empty square the unit
  could legally stand on** within that reach (`Battle::staffWarpCells`). `target: "self"` warps the user itself (no unit pick, no
  `range` key, the square pick is all there is); `"foe"` and `"any"` may warp an enemy (it must be seen). A unit with no such square
  in reach is not a valid target.
- **`pull`** (rescue-style): `{ "pull": true }`. No second pick. The unit lands on the **free square next to the user that the unit
  could stand on and that is nearest to the square the unit is on now** (grid distance, squares and hexagons alike); **a tie goes to the
  lower cell number** (`y * width + x`: the northernmost, then the westernmost), so the result is the same on every machine. A unit
  that already stands next to the user is not a valid target (nothing would change), and neither is one with no such square
  (`Battle::staffPullCell`). The squares next to the user are judged on the truth: the user's neighbours are always in its team's sight.
- **`swap`**: `{ "swap": true }`. The user takes the unit's square and the unit takes the user's (the user's square being `from`, where it
  stands when the staff is used). Not usable when either could not legally stand on the other's square: a walker cannot swap with a flyer
  hovering over water. Events: the unit's `UnitRelocated` first, then the user's.
- **`repair`**: `{ "repair": "full" | n }` (n = uses, 1 to 999). After the unit, the player picks which **of the unit's items** to mend
  (`Battle::staffRepairSlots`): an item with a limited number of uses that is **below full** and not marked `"repairable": false`;
  items with unlimited uses are never offered, nor is **the staff being used** when the unit is the user (a staff cannot mend itself). The
  uses go up by `n` (or to full) and never above the item's own `uses`. A unit with no such item is **not a valid target** (reason `that
  unit carries nothing that needs repair`). `"repairable": false` is a key of **any** item in `data/items.json` (boolean, default true;
  refused on an item without `uses`, which never wears).
- **`light`**: `{ "light": { "radius": 1 to 10, "turns": 1 to 10 } }`, `turns` default 1. In **fog of war** (docs/ECONOMY.md) the user's
  **team** sees every square within `radius` of the picked **square** (not a unit; within the staff's `range` of where the user stands,
  and only a square where something is hidden now) for `turns` of **the user's side's** turns: it ends when that side's turn has started
  `turns` times after the use (so 1 = until that side's next turn starts, 2 = through the side's next turn). It is **extra sight on
  the same cache as `visionAt`** (`Battle::lights`, read in `computeTeamSight`): a lit square is seen like a square a unit with that
  vision sees, so a unit standing on a square that **hides** (brush, a forest) is still hidden from afar. No new fog machinery. On a map
  **without fog** the staff cannot be used: the list greys it with `this map has no fog of war`, and the action is refused with `this map
  has no fog of war, so there is nothing to light`.

**What the loader refuses** (each with the file, the item and the key, like stage 1)
- `warp`: not an object, no `range`, an unknown key, a `range` that is not 1 to 20 / `[min, max]` / a formula; `pull` / `swap` not `true`;
  `repair` not `"full"` or 1 to 999; `light` without a `radius` of 1 to 10, a `turns` outside 1 to 10, an unknown key.
- `warp`, `pull`, `swap` and `repair` with an `area` that is not `"single"` (`"warp" works on the one unit picked, so "area" must be
  "single"`); `pull` and `swap` with `target: "self"` (they need another unit; `"team"` is allowed, the user is never in its own single
  reach, as in stage 1). `warp` and `repair` may use `"self"`.
- **Combinations**: at most one of `warp` / `pull` / `swap` per staff (each decides where the unit ends up), at most one `repair`, at
  most one `light`; `repair` with a moving effect (two second picks); `light` with **anything else**: a light staff is exactly one
  `light` effect and has **no** `target`, `area`, `hit` or `if` (there is no unit to judge or roll for; `xp`, `range`, `rank`, locks and
  `props` work). `heal` with `pull` / `swap` / `warp` / `repair` is allowed (`heal` still cannot target `"foe"`).
- `repairable` that is not true / false, or on an item with no `uses`.

**Several effects, in what order.** Per unit hit, after the `hit` roll: every `heal` (added, capped as in stage 1), then `repair`, then
the move, **whatever order the staff lists them in**: formulas of the heal see the unit where it stood when it was picked, and the move
is the last thing done to it. A unit is a valid target when **any** effect would change it, and an effect that cannot work on it is
skipped while the others still happen (a heal + pull staff on a hurt unit with no free square next to the user only heals; a pull on a
whole unit far away only moves it). A miss (`hit`) does nothing at all to the unit. The greyed/refusal reason is the **first** effect's.

**Where a unit may stand, and what a move touches.** One rule everywhere (`Battle::staffCanStand`): on the map, and the unit's movement
type can enter that terrain (`enterCost > 0`: walls, water for a walker); nobody on the square. Moves go through the ability machinery
(`doAbilityMoves`): a capture the unit was making is abandoned (`releaseCapture`), the team sight is dropped and recomputed (a moved
unit sees from its new square: events before the move are stamped with the old sight), the units' `moved` / `done` / `acted` flags are
**not touched**, and `UnitRelocated` is the event. Terrain effects: the engine has none that act on arrival (terrain only acts at turn
start), so there is nothing to fire. A **carried or paired** unit is off the board (rescue, `alive` false, no square): it cannot be picked,
and a carrier that is moved takes nobody anywhere (the carried unit has no square; where it is dropped is judged when it is dropped).
**Story and objectives**: a staff move counts as the unit **ending a move there**, the same decision as the ability moves and the rescue
"drop": the story's `reach` events run (target first, then the user), **last** of all (after experience, the order's end and the supports;
they may add units or end the battle), and a `reach` victory or defeat objective is judged by the usual check after every action, so a warp
onto the goal wins. **Fog**: with fog, the destination of a warp must be a square the team **sees**; a square that looks empty but holds
an enemy the team does not see (a hideout) is offered anyway, and the warp then fails when carried out: the use is spent, the other
effects still happen, nothing moves, and `StaffUsed` carries the flag `kBlocked` and the blocker in `data` (as `AbilityUsed` does).

**Used-up and cost.** One use, the `xp`, the rank experience and the end of the order are exactly stage 1's, a light and a miss included.

**Queries and action.** `Action::useStaff(unit, slot, target, cell = kNoCell, itemSlot = -1)`: a warp's square goes in `cell`, a repair's
item (a slot in the **target's** inventory) in `itemSlot`, a light's square in `cell` with `target` -1. New queries: `staffWarpCells`,
`staffRepairSlots`, `staffLightCells`, `staffPullCell`, `staffWarpRange`, `staffTargetWhy` (the plain reason a unit cannot be picked, ""
when it can), `staffCanStand`; `staffForecast` takes the optional `dest` and `itemSlot` and says in the result what is still to pick
(`needsDest`, `needsItem`), where a unit would go (`StaffHit::moveTo`, `userTo`), the item's uses before and after, and for a light
`isLight`, `area`, `newSquares`, `lightTurns`. New reasons of `staffOptions` (a staff of another kind finds nobody): `nobody in reach can
be moved`, `nobody in reach has an item to repair`, and a light's `this map has no fog of war` / `nothing in reach is hidden`; a heal's
stays `nobody in reach needs that`.

**Events** (appended; a moved unit is the existing `UnitRelocated`: a = unit, b = the square it left, c = the square it stands on):

| Entry | a, b, c, d | Seen by (fog) |
|---|---|---|
| `ItemRepaired` | a = the unit, b = the item, c = uses restored, d = uses it has now | the unit's team |
| `AreaLit` | a = the user, b = the square, c = radius, d = turns | the user's team |

`StaffUsed` for a light has c = -1 and d = the number of squares that came into sight; with `kBlocked` (a blocked warp) the blocker is in
`data`. Order: `StaffUsed`, per unit `UnitHealed`, `ItemRepaired`, `UnitRelocated` (or `AreaLit` for a light), `ItemBroke`, the user's
experience, `RankUp`, the supports, then the story's `reach` events.

**Saves and hash.** The only new state is the **lights** that still last: `"lights": [ { "side", "cell", "radius", "turns" } ]` in the
battle (`turns` = the side turns left), written only when there are some (every other save is byte for byte what it was; an old save
has no key and loads with none), refused on a battle without fog (`battle.lights: this battle has no fog of war`), each entry checked
(a side, a square on the map, radius 1 to 10, turns 1 to 10) with a path in the message. A light is on the undo copy like the rest of
the battle. The data hash is `data/items.json` whole, so the new keys change the fingerprint of a game that has them and no other.

**AI.** The computer **ignores every staff that has an effect other than a heal** (a heal + pull staff too; `inflict` and `cure` are the exception, section 9 "Computer opponent"): the new effects need a
second pick (a square, an item) or change what a whole side sees, and judging them takes a plan the stateless AI does not make; ignoring
them can never crash or stall it (`ai_staves.cpp`; `tests/srpg_content_test.py` plays a fixture whose two sides carry such staves,
and the fog map is left out of its stall check because the AI's hunt for units it cannot see stalls there with or without staves). Heal-only
staves are used as in stage 1.

**Player.** The same Staff entry and list (greyed with the reason; three lengths of wording for narrow screens), then, **one stage at a
time and Cancel steps back one stage**:
- unit pick (blue reach, red for the units and the squares they would end on, and the forecast box: `Bram: pick a square`, `Ren is pulled
  next to you`, `Bram and Sage swap places`, `Pick an item to repair`; a heal still shows its HP line);
- warp: a **square pick** (the legal squares in blue, the forecast says `Bram warps there`, hint `Pick a square to send it to`); a repair:
  a menu **"Repair which?"** (name, uses before > after); a light: the **square pick** straight away (the area it would light in red,
  `Lights 7 new squares / 2 turns`, hint `Pick a square to light`). A staff of `"self"` that warps or repairs starts at its second stage and
  Cancel there goes back to the action menu; Cancel at the unit pick goes to the action menu as in stage 1.
- keyboard (arrows / Tab, Z, X), mouse and touch (a tap highlights, a second tap on the highlighted one chooses; a tap on a unit or a square
  that cannot be picked says why in the hint line: `Out of range`, `Already next to you`, `No room next to you`, `Cannot stand there`,
  `Nothing to repair`, `That square is taken`, `Not in sight`, `Nothing hidden there`).
- playing it: the unit slides to its new square (`UnitRelocated`; a long way it is simply there), `ItemRepaired` floats `Iron Blade +5`,
  `AreaLit` floats `Lit` and the camera looks at the square, a blocked warp floats `Blocked!` over the user; the lit squares come into sight by themselves (sight is derived).
- views: every pick works through the generic pick modes and the overlay masks, so it works in **all four kinds of view** and on **square
  and hexagon** maps (`tests/srpg_player_staves2_test.py` runs warp and pull in each view, light in each view, pull / swap / warp on
  pointy and flat hexagons, and the keyboard flows at 480x270 and a 270x480 phone). A keyboard player cannot land on a unit that is not
  a candidate, so the "why" is for taps; the greyed list entry carries the staff-level reasons.

What the stage-two contract left open, and how it was settled (the owner may change any of these):
- *Warp reach is from the user*, as asked; a destination is never the user's own square nor the unit's. *Pull* tie-break = lower cell number.
- *A move fires `reach` events and counts for objectives*, like the ability moves and the rescue drop (see above).
- *The user is never in its own single reach* (stage 1's decision, kept): `"team"` single-target staves pick allies only; use `"self"`.
- *`light` is one effect and nothing else* (no target, area, hit, if): it has no unit. `xp` works.
- *Heal + repair* is allowed: the player is asked for an item only when the unit carries one that can be repaired.
- *Light and hiding*: a lit hideout square is seen, a unit hiding in it is not (as with a unit's own vision), until a unit is next to it.
- *The AI ignores* every staff with a non-heal effect; it never lights, warps, pulls, swaps or repairs.
- *All wording is placeholder* (`staff_text.h`). New strings: see the list in the stage-two report; all in `StaffWords`, `staffReasonText`,
  `staffWhyText`, `staffEffectText`, `staffMoveLines`, `staffLightLines`.
- Code: `engine/core/battle/staves_effects.cpp` (the questions), `staves_apply.cpp` (validate / apply, now with `finishStaffUse`,
  `applyStaffLight`), `sight.cpp` (lights as sight), `turn.cpp` (`lightsTurnStart`), `save_fog.cpp` (`lights`), `ai/ai_staves.cpp`; player
  `battle_menu_staves.cpp` (the stages), `battle_srpg_target.cpp` (`pickBack_`, `pickWhy_`), `staff_text.*`, `staff_forecast.cpp`,
  `battle_view_staves.cpp`. Tests: `tests/test_srpg_staves2*.cpp`, `tests/srpg_player_staves2_test.py`, the fixture
  `tests/fixtures/srpg-staves2` (maps `yard`, `mist` (fog), `gate` (story + objective), `skirmish` (AI)).

### Stage 3: status conditions

Built: see **section 9**. (The outline that stood here proposed `data/statuses.json` and `{ "status": ... }`; the build uses
`data/conditions.json` and `{ "inflict": { "condition": ... } }` / `{ "cure": ... }`.)

---

## 9. Status conditions

A **condition** is a temporary state a unit is in: poisoned, asleep, silenced, berserk, stunned, cursed. The maker builds them in a new
optional file, `data/conditions.json`, from the **same blocks skills use** plus four new effects for what a skill cannot do. Staves,
usable items, weapons and skills **inflict** them; staves and items **cure** them. This is stage 3 of the staves contract (section 8,
owner's answers in PLAN item 3l); everything here is built: rules, player and the loader. A game without `data/conditions.json` plays,
saves and hashes exactly as before (same `dataHash`, same random numbers, same save bytes; the demo sims are line for line the same).

All wording is placeholder. The file is called `conditions.json` and the effects `inflict` / `cure` (the stage-3 outline of this document
said `statuses.json` / `status`; the owner's brief for the build used "condition", which is also what the skill documents already call
`if` entries, so they are told apart by place: a skill's `if` is a *test*, a unit's *condition* is a state).

### Data: data/conditions.json

```jsonc
{ "conditions": [
  { "id": "poison", "name": "Poison", "description": "Loses a little HP at the start of each of its turns.",
    "tag": "Po",                         // optional: 1 to 4 characters shown on the map (default: the first two letters of the name)
    "turns": 3,                          // how long: the afflicted unit's OWN turns (1 to 99) ... or
    // "until": "cured" | "mapEnd",      // ... until it is cured / the map ends (exactly one of "turns" and "until")
    "wearOff": 25,                       // optional: percent chance (1 to 99) each own turn that it ends early. Default: never
    "endsWhenStruck": false,             // optional: a strike that does damage to the unit ends it (sleep). Default false
    "replaces": ["ember"],               // optional: applying this removes these conditions first
    "blocks": [ { "when": "turnStart", "do": [ { "hurt": { "percent": 10 }, "to": "self" } ] } ] }
] }
```

Keys: `id` (required, unique), `name`, `description`, `tag`, `turns` or `until` (one is required), `wearOff`, `endsWhenStruck`, `replaces`,
`blocks` (required, 1 to 8). **Unknown keys are refused**, like the other new files. At most 100 conditions. `name`, `description` and
`tag` are cosmetic for the data hash (renaming a condition keeps saves valid); everything else counts.

**Icons.** The engine has no small picture for a unit (the map draws a unit as art or a letter block), so a condition has a short text
**tag** instead; it is drawn in a small tab at the bottom left of the unit in every kind of view.

### The blocks

A condition's `blocks` are skill blocks (section 3: `when`, `if`, `chance`, `do`; every `if` test and every effect that makes sense on a
temporary state works). Internally each condition is a **hidden skill** (`SkillDef::hidden`, index in `ConditionDef::skill`, id
`condition:<id>`), and `Battle::skillsOf` appends the hidden skills of the conditions a unit has **after** the skills it has (type,
learned, character, accessories, weapon), in the order the conditions were applied. So stat changes, combat numbers (`hit`, `avoid`,
`crit`, `damage`, ... in `combat` blocks), `strike` / `struck` blocks, `turnStart` blocks, auras and the info panel all work with no code of
their own, and the forecasts see them. Hidden skills are never listed as skills (the panel, the Skills page) and cannot be found by id.

| Behaviour | Block | Notes |
|---|---|---|
| Damage each turn | `turnStart`: `{ "hurt": 5, "to": "self" }` | The amount is a number, `{ "percent": 10 }` (of the unit's maximum, at least 1), or a formula (`"hurt": "a.mag + 1"` / `{ "formula": "..." }`; `a` = the unit, `b` = nil). `to: "self"` is new (it joins `foe` and `enemies`). **Can it kill? No by default**: it leaves 1 HP, like every `hurt`. `{ "hurt": { "amount": 5, "canKill": true }, "to": "self" }` lets it defeat the unit (a normal death: `UnitHurt`, then `UnitDied`, drops are not paid, the story's "defeated" events and the win checks run once every unit's blocks of the turn have run) |
| Heal each turn | `turnStart`: `{ "heal": 4 }` | A number, `{ "percent": n }`, or now also a formula. `to: "self"` (default) or `allies` |
| Cannot act | `always`: `{ "cannotAct": true }` or `{ "cannotAct": { "counter": true, "dodge": true } }` | The unit's turn is skipped (it is finished at the start of it). **Counterattack and dodge: both off by default** (a sleeper answers nothing and every strike on it hits, hit 100); `"counter": true` lets it still answer, `"dodge": true` lets its hit chance count as usual. The most restrictive of several conditions wins |
| Cannot move | `always`: `{ "cannotMove": true }` | Its reach is its own square; it can still act |
| Silence | `always`: `{ "forbid": { "groups": ["tome"], "kinds": ["staff"] } }` | `groups` = weapon or staff groups of the game (checked against `weapons.json` and the staves), `kinds` = `"staff"`, `"usable"`, `"weapon"`. A forbidden weapon is not wieldable (`canWieldNow`, so it is not equipped, cannot answer a strike and the AI never picks it), a forbidden staff is greyed ("Silence stops that"), a forbidden usable item cannot be used ("Bind stops the unit using items") |
| Acts on its own | `always`: `{ "berserk": true }` | "Berserk": see below |
| Stat changes | `always`: `{ "stats": { "str": -2, "def": -2 } }` | The existing effect. **Not the HP stat** (a maximum that flickers would cut current HP): refused, use `hurt` / `heal` |
| Hit / avoid / crit / damage | `combat` blocks, e.g. `{ "hit": 10 }` | The existing effects |
| Immunity | `always`: `{ "immune": ["poison", "sleep"] }` | Works in **any skill** too (a unit type, character, accessory or weapon skill), see "Immunity" |
| Inflict on hit | `strike` / `struck`: `{ "inflict": { "condition": "poison", "turns": 2 } }` | See "Weapons and skills" |

`cannotAct`, `cannotMove`, `forbid` and `berserk` belong to conditions: in a skill the loader says `"cannotAct" belongs to the blocks of a
condition (data/conditions.json), not to a skill`. `immune` and `inflict` work in skills and conditions. A new skill test,
`{ "condition": "poison" }` (with `"who": "foe"` where a foe exists), is true while the unit has that condition: in a block's `if`, a
staff's `if` and an ability's `if`.

### When a turn is counted (the one rule)

At the start of **each turn of the afflicted unit's side**, units in id order, each unit's conditions **in the order they were applied**:

1. **Wear-off roll.** A condition with `wearOff` is rolled (`Battle::rng`, only when the chance is 1 to 99); a condition that wears off
   is gone for this whole turn (`ConditionEnded`, reason "wore off").
2. **Count down.** A counted condition (`turns`) has one fewer turn left (`ConditionState::left`). A fresh condition applied with
   `turns: 3` therefore affects the unit's next three turns: at its first turn start `left` becomes 2, at the second 1, at the third 0.
3. The skills' and conditions' **`turnStart` blocks run** (poison ticks, regeneration), skills first, then conditions in order.
   A condition that an earlier block of this same pass **cured** (a skill's `{ "cure": ... }` runs before the condition's own block) is
   gone for the rest of the pass: its `turnStart` block does not run, and it no longer stops the unit (steps 4 and 5 look at what the
   unit has now), so a cured poison does not tick and a cured sleep does not end the turn.
4. A unit that **cannot act** is finished for the turn (`done`, `moved`; `ConditionTick`, kind "cannot act"), so neither the AI nor the
   player's menus offer it.
5. **Berserk** units take their own action (below).

A counted condition **lasts through the turn in which its count reached 0 and wears off when that turn ends** (`EndTurn`,
`ConditionEnded`, reason "ran out"). So "3 turns" means three turns of effect, and a sleeper with 1 turn left still sleeps through its next
turn. A condition applied **during the unit's own turn** (its own item, a counter) is not counted that turn (the count starts at its next
turn start), and a unit that cannot act loses the rest of that turn at once. `until: cured` and `until: mapEnd` never count; **both last
until the battle ends** (conditions are not carried between campaign maps, see "Not built"); they differ only in what the maker means. The
turns left are shown as the stored `left` (3, 2, 1, "ends" at 0).

A unit that is **carried or paired** (rescue, section 5) is off the board: its conditions are **frozen** (no tick, no count, no wear-off
roll) until it is put down. A unit that is defeated has none. With **fog**, a condition's events are seen by the teams that see the unit.

### Applying, re-applying, replacing, curing

- **Applying** (`Battle::inflictCondition`): an immune unit gets `ConditionResisted` and nothing else. Otherwise the conditions in the new
  one's `replaces` list are removed first (`ConditionEnded`, reason "replaced"), then it is added at the end of the unit's list
  (`ConditionApplied`, `c` = turns, -1 = uncounted, `d` = the unit that did it, `flags` `kRefreshed` when it was there already).
- **Re-applying** a condition the unit already has **starts the duration over** (the count becomes the new full duration, never a sum, never
  the larger of the two) and **keeps its place** in the order. There is no stacking option. A staff or item does not offer a target that
  has it for at least as long already ("that unit has it already"); a weapon or skill that hits again simply refreshes it.
- **Several different conditions** at once are fine; their blocks add up as skills do (numbers add, percents multiply in order, yes/no
  effects are "any", the most restrictive `cannotAct` wins).
- **Curing** (`cureConditions`): `ConditionEnded`, reason "cured"; max-HP style clamping is not needed (conditions cannot change HP stat).
- **Struck** (`endsWhenStruck`): a strike that did damage (and did not defeat the unit) ends the condition after the `Attack` event
  (reason "struck"), **before** the strike's own `inflict` effects run, so a strike can wake a sleeper and put it back to sleep.

### Immunity

The smallest consistent design: **the skill effect `{ "immune": [ids] }`** (any `when` that allows `always`, with conditions allowed).
A unit type, a character, an accessory and a weapon already carry skills, so each of them can grant immunity by listing such a skill;
there is no separate list on unit types or characters. A condition can also grant immunity (a "Ward" condition). A staff does not offer
an immune unit as a target and says "that unit is immune to it"; a weapon or skill that tries gives `ConditionResisted` (player: floats
"Immune").

### Berserk

A unit with a condition holding `{ "berserk": true }` (and not stopped by `cannotAct`; a unit that cannot act does not go berserk) acts **by
itself at the start of its side's turn, after the ticks**, outside the control of the player and of its own AI; its turn is then over (it
takes no orders: an order to it is refused, `that unit acts on its own (Berserk)`). It attacks **the nearest unit of any side** with its
equipped weapon, by the same move and attack code as an order (same events, same dice, counterattacks, experience, drops, story events,
win checks); only the "may it fight that unit" test is its own (it may hit an ally). The choice (`Battle::berserkPlan`), all on the truth
**limited to what its team sees** (with fog a berserk unit does not hit an enemy hiding unseen; its own side is always seen; carried
units are off the board):

1. Candidates: every other unit on the board that its team sees, of **any** side.
2. Weapon: the first weapon it could attack with (the equipped one; with fixed weapons the type's first with ammunition). A weapon with
   `afterMove: false` is used from the square it stands on only.
3. Of the candidates it can hit this turn (from a square it can reach, in the weapon's range, `versus` allowing it) it takes the **nearest
   to the square it stands on** (grid distance); a **tie goes to the lower unit id**. It stands on the square from which it can hit with
   the **least movement** (staying put when that works); a tie goes to the lower square number (`y * width + x`).
4. When nothing is in reach it walks as far as it can towards the **nearest** candidate (the reachable square nearest to it; ties: the
   cheaper walk, then the lower square number) and does not attack. With no candidate in sight, or no weapon, it stands still.
5. It hits first, so an ally can strike back (counter) and can defeat a berserk unit; a defeat that ends the battle ends it at once.

### Ways to inflict and cure

**Staves** (section 8; stage 3): two effect kinds in a staff's `effects` (each at most once per staff).

```jsonc
{ "id": "venom_staff", "name": "Venom Staff", "kind": "staff", "group": "staff", "uses": 5, "range": 3, "target": "foe",
  "effects": [ { "inflict": { "condition": "poison", "turns": 4 } } ],        // "turns" optional: overrides the condition's own (counted conditions only)
  "hit": "70 + a.mag * 5 - b.res * 5 - dist * 2", "xp": 10 }
{ "id": "cure_staff", "kind": "staff", "group": "staff", "range": 3, "target": "ally", "effects": [ { "cure": ["poison"] } ] }
{ "id": "purify_staff", "kind": "staff", "group": "staff", "range": 6, "target": "ally", "area": "all", "effects": [ { "cure": "all" } ] }
```

They follow every rule of stages 1 and 2: reach, `target`, `area` (**allowed** for both: `single`, `{ "radius": n }`, `"all"`), `if`, `hit`,
`xp`, ranks, locks, uses, one `StaffUsed` event, supports unchanged. Where they sit in the effect order, per unit hit, after the `hit` roll:
**heal, repair, cure, inflict, then the move** (a staff lists them in any order; a "cure all" that also inflicts X ends with X). A unit
is a valid target when some effect would change it: for `cure` it has a condition the staff cures ("that unit has nothing it cures"; the
staff's own reason when none in reach: "nobody in reach has anything it cures"); for `inflict` it is not immune and does not have the
condition for as long already ("that unit is immune to it", "that unit has it already"; "nobody in reach can be affected"). In an area an
immune unit is simply not affected (no roll, no event for it unless another effect changes it, then `ConditionResisted`). A miss does
nothing to the unit (`StaffMissed`). A staff with `inflict` or `cure` may be combined with `heal`, `repair` and a move effect; `light` stays
alone. **The `hit` formula already sees both units**: `a` = the user, `b` = the unit, `w` = the staff, `dist` = the distance, `ta` / `tb`
= their squares (judged as the user would stand when it uses the staff; section 8). So "base + the user's magic - the target's
resistance, and distance" is the maker's formula (above); nothing had to be extended. Forecast (`staffForecast`): each `StaffHit` gains
`inflict` / `inflictTurns` (what it would get, -1 when it would not take hold) and `cures` (what it would remove).
A staff that picks nobody (area `all`, target `self`) is used at once, as before.

**Usable items** (docs/ITEMS.md), flat keys in the style of `heal` / `raise`:
`{ "kind": "usable", "cure": ["poison"] }`, `"cure": "all"`, `{ "kind": "usable", "inflict": { "condition": "curse", "turns": 2 } }`. The effect
goes to the unit the item is used on (the user for `target: "self"`, a neighbouring ally for `"ally"`; an item that inflicts is meant for
the user). Order: raise, heal, cure, inflict. An item that would change nothing (nothing to cure, immune, has it already) is not offered.
A usable item may have only `cure` / `inflict`.

**Weapons** (data/weapons.json): `{ "inflict": { "condition": "poison", "chance": 30, "turns": 2 } }`: **an on-hit chance to inflict the
condition on the unit struck**. `chance` is a percent from 1 to 100 or a formula (`a` = the wielder, `b` = the unit struck; default 100 =
every hit, no roll). It is kept as a hidden skill with one `strike` block, so it works exactly like the skill below: the chance is rolled
at the moment of the strike like every activation (before the hit roll; a forecast lists it as a note, "Poison 30%"), and the condition
takes hold **only if the strike hits**. **Skills** do the same with the same effect: `{ "when": "strike", "chance": 30, "do": [ { "inflict":
{ "condition": "poison" } } ] }` (to the unit struck) and `{ "when": "struck", ... }` (to the unit that struck it, a thorns skill).
Inflicting is `strike` / `struck` only; an `afterCombat` or `turnStart` block cannot inflict.

**Story commands** (map events only; docs/EVENTS.md "Status conditions"): `{ "addCondition": "poison", "to": <unit filter>, "turns": 2 }` (turns
optional), `{ "removeCondition": "poison" | "all", "from": <unit filter> }`, and the story condition `{ "hasCondition": "poison", "unit": <unit filter> }`
(a living unit matches). They use `inflictCondition` / `cureConditions` like every other route, so **immunity is respected** (`ConditionResisted`),
`replaces` runs and the same events come (the giver is -1); removing is "cured". A campaign scene refuses all three (conditions end with the map).
A skill / staff / ability test `{ "condition": id }` exists too (it lives with the skill conditions). Code: `battle/story_conditions.cpp`,
`data/loader_story_conditions.cpp`; tests `tests/test_story_conditions.cpp`.

### Loader refusals

Plain English in the existing style, naming the file, the condition / item / weapon and the key, e.g. `data/conditions.json: condition
"poison": needs a duration: "turns": 3 (the unit's own turns) or "until": "cured" or "mapEnd"`. Refused: unknown keys; duplicate or empty
ids; no duration, both durations, `turns` outside 1 to 99, `until` other than `cured` / `mapEnd`; `wearOff` outside 1 to 99; a `tag` of 0 or
more than 4 characters; `replaces` naming an unknown condition or the condition itself; no blocks, more than 8; every error of a skill block
(unknown effect, wrong `when`, ...); `cannotAct` and `berserk` in one condition (contradict: make two); a condition changing the HP stat;
`hurt` / `heal` with a bad amount, `percent` outside 1 to 100, `canKill` that is not true / false; `forbid` naming a group no weapon or
staff has, a kind other than `staff` / `usable` / `weapon`, or nothing; `immune` naming an unknown condition or none; `inflict` naming an
unknown condition (or in a game with no conditions file, saying so), a `turns` on a condition that does not count turns, a `chance`
outside 1 to 100 / not a formula, an unknown key; `cure` naming an unknown condition, listing one twice, empty, or not `"all"`; `inflict` /
`cure` on an item that is not usable; two `inflict` or two `cure` in one staff; `area` with `target: "self"` (as always); a skill's
`condition` test naming an unknown condition. A syntax error in a formula names the place as for skills.

### Events

Appended to `EventType` (existing numbers keep their values); new flag `kRefreshed = 2048`.

| Entry | a, b, c, d, flags | Seen by (fog) |
|---|---|---|
| `ConditionApplied` | a = unit, b = condition, c = own turns it lasts (-1 = uncounted), d = the unit that did it or -1, `kRefreshed` | the teams that see the unit |
| `ConditionEnded` | a = unit, b = condition, c = why: 0 ran out, 1 wore off early, 2 cured, 3 replaced, 4 struck | the teams that see the unit |
| `ConditionResisted` | a = the immune unit, b = condition, c = the unit that tried | the teams that see either |
| `ConditionTick` | a = unit, b = condition, c = 1 acts on its own (berserk, its own action follows) / 2 cannot act (its turn is skipped) | the teams that see the unit |

A condition's damage and healing arrive as the usual `SkillActivated` (b = the condition's hidden skill, named after the condition: the
float), `UnitHurt` / `UnitHealed` (and `UnitDied` for `canKill`). A berserk unit's action is the usual `UnitMoved` / `Attack` and the rest.

### Saves, hash, campaign

- Battle save, per unit, only for a unit that has some: `"conditions": [ { "condition": index, "left": turns } ]`, oldest first; `left`
  only for a counted condition (0 to 99). A key in a game without conditions is refused (`units[3].conditions: this game has no conditions`);
  every entry is checked (index, no repeats, `left` forbidden on an uncounted condition) with a path. An old save has no key and loads with
  none. Conditions are saved only for units that exist (a defeated unit has none).
- **Hash**: `data/conditions.json` is mixed in only when it holds something (an empty list is the same game as no file), in the strategy-RPG
  order after `data/supports.json`; `name`, `description` and `tag` are cosmetic. A weapon's `inflict`, an item's `cure` / `inflict`, a staff's
  effects and a skill's `immune` / `inflict` are keys of files that are hashed whole.
- **Campaign**: conditions end when the map ends and the roster carries none (**carry-over between maps: not built**; a maker would need a
  per-condition `carryOver` flag, a roster key and a rule for the counts; the data hash and the roster format are ready for it).

### Computer opponent

Every rule above applies to computer units without special cases: a unit that cannot act is finished at the start of its turn (so
`aiTakeStep` never picks it), a berserk unit has acted before the AI looks, a stunned unit's reach is its own square, a silenced unit's
tome, staves and items are not offered by `weaponsOf` / `staffOptions` / `useTargets` (it never tries a forbidden one). Its heal staves are
unchanged. **New (cheap and safe): a staff whose effects are only heal, inflict and cure is used** when it beats the unit's best attack,
capture or advance on the same scale: a cure is worth a quarter of the unit's value (as an attack scores it: production cost, default
1000; a game-ending leader counts double) for each **ally** it would free of something, an inflict a quarter for each **foe** it would really
afflict (not immune, not afflicted already: those are not legal targets), both times the hit chance, plus the heal part as before. It never
inflicts on its own side (it cannot tell a blessing from a curse) and never cures a foe. Only a unit that would change is a legal target, so
even an unlimited staff cannot loop. Usable cure items: the AI does not seek them (a tonic that also heals is used as a heal).
`tests/srpg_content_test.py` plays the fixture's armies (sleep, poison and cure staves, venom blades) on 20 seeds: no stalls, same text
twice.

### Player

Built in `engine/player/battle/`: `condition_text.*` (every word, in `ConditionWords`), `condition_marker.*`, `battle_view_conditions.cpp`
(the events), `unit_info_conditions.cpp` (the Status page); changes in `panel.cpp`, `staff_text.cpp`, `staff_forecast.cpp`,
`battle_input.cpp` and the three unit-drawing places.

- **Marker**: a small dark-violet tab with the first condition's tag ("Po", "Zz"), and a "+" when there are more, at the bottom left of the
  unit, in **all four kinds of view** and on hexagons (top-down and hexagon units, the isometric figure, the 3D billboards) and on the phone.
  It follows the events (it appears with "Poison!" and goes with "Poison ended").
- **Info panel**: under the stats and the weapon, up to three rows "Poison  3" (name in violet, the turns left; "cure" / "map" for
  uncounted ones, "ends" on the last turn), then "+N more". The unit details gain a **Status** page (only for an afflicted unit): each
  condition's name, "3 turns left" / "Until cured" / "Until the map ends" and its description.
- **Staff flow**: nothing new to learn: the same Staff entry and list (greyed with the reason; three lengths of wording), the pick, the
  forecast box (below), Cancel. The forecast lists per unit **what it would be afflicted with** ("Rook  Curse 3 turns") and **what it would
  be cured of** ("Kay  Cures Curse"), the "Hit 91%" line when the staff can miss, and the usual "Exp" and "Breaks". Floats on the result:
  "Poison!" (applied), "Missed", "Cured Poison", "Poison ended", "Immune". The weapon-inflict result shows after the hit, and the combat
  forecast lists a weapon's chance as a note ("Sleep 50%") and **reflects stat changes** and a sleeper's missing counterattack and dodge.
- **Turn flow**: at the start of a unit's turn its condition's name floats, then the "-2" and the HP bar (and "+4" for a heal); a
  condition wearing off floats "Poison ended"; a berserk unit floats "Berserk!" and then visibly moves and attacks; a sleeping unit floats
  "Sleep" and takes no orders: **tapping it (Confirm on it) says why in the hint line**, "Sleep: cannot act" / "Berserk: acts on its own";
  selecting a stunned unit says "Stun: cannot move". Staves that pick nobody (area `all`, target `self`) work at once on the list entry and
  show no forecast, exactly as healing ones always did.
- Keyboard, mouse and touch: nothing new is needed. Desktop size and the 270 x 480 phone are both covered by the tests.

### The starter set

Six conditions shipped as data only (no special-casing by id), in `editor/templates/strategy-rpg/data/conditions.json` (new games made from
the strategy-RPG template get them) and in `games/demo-srpg/data/conditions.json` (data only: the demo gains an **Antidote** in the fort shop
and no staff, because no demo class wields staves; adding the file and the shop line changes the demo's data hash, so old demo saves are
refused, and changes no `ote-sim` result). Placeholders for the owner:

| id | name (tag) | duration | blocks |
|---|---|---|---|
| `poison` | Poison (Po) | 3 turns | `turnStart`: hurt 10% of max HP (at least 1) to self, never defeats |
| `sleep` | Sleep (Zz) | 3 turns, ends when struck | `always`: cannotAct, cannotMove (defaults: no counter, no dodge) |
| `silence` | Silence (Si) | 3 turns | `always`: forbid kinds `staff` (add `"groups": ["tome"]` where the game has a magic group) |
| `berserk` | Berserk (Br) | 3 turns | `always`: berserk |
| `stun` | Stun (St) | 2 turns | `always`: cannotMove |
| `curse` | Curse (Cu) | 3 turns | `always`: stats str -2, def -2 |

### Decisions (the owner may change any of these)

- A condition's blocks are the skill blocks, not a second language; new effects only for what did not exist (`cannotAct`, `cannotMove`,
  `forbid`, `berserk`, `immune`, `inflict`, and `hurt` to self / formulas / `canKill`).
- A damage tick leaves 1 HP unless the maker writes `canKill`.
- A sleeper answers nothing and cannot dodge unless the maker allows it.
- Turns are counted at the start of the afflicted unit's side's turn; the condition works through the turn its count reaches 0 and ends when
  that turn ends. Re-applying starts the duration over; no stacking. `until cured` and `until mapEnd` behave alike.
- Immunity is a skill effect only.
- Berserk chooses the nearest *attackable* unit it can see (ties: lower unit id); it walks towards the nearest when none is attackable.
- Carried units' conditions are frozen.
- A staff's inflict / cure order is heal, repair, cure, inflict, move.
- The AI uses inflict / cure staves with a simple worth rule, ignores the rest.
- Staves that pick nobody show no forecast (as before).

### Not built (follow-ups)

Carrying a condition over to the next campaign map; condition sounds; a picture instead of the text tag; the AI inflicting beneficial conditions, seeking
cure items, or using the area-all staves' value beyond the simple rule; a berserk or sleeping unit's own portrait pose.

### Tests

`tests/test_srpg_conditions.cpp` (rules: every block, the count of turns, re-applying, replacing, immunity, each route to inflict and cure,
berserk target choice and tie-break, fog, a tick that defeats the last unit or a leader, carried units),
`tests/test_srpg_conditions_data.cpp` (loader refusals, the data hash, saves mid-condition and damaged saves), the fixture
`tests/fixtures/srpg-conditions` (maps `field`, `mist` (fog), `skirmish` (AI)), `tests/srpg_player_conditions_test.py` (the player's flows,
all four views, the phone), `tests/editor_api_conditions_checks.py` (the helper keeps the file and every key), `tests/srpg_content_test.py`
(no stalls).

---

## 10. Doors, keys, chests and visiting

Built (PLAN item 3l, step 5; staves stage two's `unlock` waited for this). A game with none of it plays, saves and hashes exactly as before:
every key below is optional, and the demos have no doors.

**Owner's answers** (PLAN, 2026-11-16):
- "A locked door opens by: a key item used next to it (maker decides which keys fit, and whether used up); a unit trait (thief-style,
  optionally needing a lockpick item with limited uses); a story event; or by being broken down (doors with hit points that can be attacked)."
- "Chests and villages: the maker chooses per game between explicit Open / Visit actions in the unit's menu (uses the turn) and the current
  'stand on it' trigger."
- "Computer opponent: the maker marks per placed unit whether it goes for chests (taking the item), destroys villages, or opens doors with its
  own keys; unmarked units ignore them."

"The current 'stand on it' trigger" is the `reach` event (docs/EVENTS.md), which keeps working for any square, structure or not. For the
built-in doors, chests and places the second choice is `rules.places.actions: "auto"`: the engine does the opening / visiting by itself when a
unit ends a move there (the same thing a `reach` event would do, without the maker writing it).

### Where they live, and why: structures

A door, a chest and a visitable place are **map structures** (docs/STRUCTURES.md): a structure *kind* with a `"door"`, `"chest"` or
`"visit"` block. Opening, shutting, visiting and destroying one **changes the structure's kind** (the engine's upgrade mechanism). The other
two designs were weighed and rejected:
- *Terrain with state.* Terrain is per square and has no state or id; a wide gate would be several squares that must open together; a
  chest or village on top of ground would need its own layer anyway.
- *A new list of map objects.* It would repeat what structures already give for free: a footprint of several squares (a gate is 2 x 1), a
  movement cost per movement type (a closed door costs 0, the open kind costs what the ground costs), art per kind (the open and shut look
  are two kinds with their own pictures in every view), the fog memory (`seenKind`: a team remembers the door as it last saw it), the info
  panel line, the AI's distance fields (a closed door is a wall in them without a single change), saves (`type` is saved when it differs from
  the map's) and hexagon games (`rings`).
New state is small and lives on `StructureState`: `id` (the map's name for it), `locked`, `hp` (a door that can be attacked), `used` (a place
that was visited) and `loot` (a chest's contents), plus `visitSides` / `visitUnits`.

### Data

`data/structures.json`, one block per kind (a kind has at most one of them):

```jsonc
{ "id": "gate", "width": 2, "height": 1, "moveCost": { "foot": 0, "fly": 0 },   // the closed look blocks; 0 for every type it stops
  "blocksSight": true,                    // optional (fog): units do not see past its squares in a straight line; the squares themselves are seen
  "door": {                               // a shut door
    "opens": "gate_open",                 // required: the kind it becomes (same footprint; the maker picks what it looks like and costs)
    "locked": true,                       // default true; a map placement may say otherwise
    "keys": ["brass_key"],                // optional: only these items fit; absent = any item whose "opens" names "door" or this kind
    "by": ["key", "trait", "break"],      // optional ways it can be opened; default key + trait, plus break when "hp" is given
    "hp": 20, "stats": { "def": 3 },      // optional: it can be attacked; "stats" are the numbers its damage formula reads as b.<stat>
    "from": "next" } }                    // "on" | "next" | "either": where a unit may stand to use it; default either
{ "id": "chest", "chest": { "opens": "chest_open", "locked": true, "keys": [..], "by": ["key", "trait"], "from": "either" } }
{ "id": "hamlet", "visit": { "becomes": "hamlet_shut",   // optional: the kind after a visit (the "gate shut" look); it then cannot be visited again
                             "ruin": "hamlet_ruin",       // optional: the kind after it is destroyed; without it it cannot be destroyed
                             "repeat": false,             // true = may be visited again and again (cannot go with "becomes")
                             "from": "on" } }             // default "on" (the place's own square); "next" / "either" allowed
```

A story command and an unlock staff open any door or chest whatever `by` says; a trait (ability) needs `byTrait`, a key `byKey`, an attack `byBreak`.

`data/items.json`: a key is a usable item with `"opens": ["door"]`, `["chest"]`, both, and/or structure kind ids (`["strongbox"]`); it is valid
without `heal` / `raise`. Its `uses` work as for any item (the opening spends one; at 0 it breaks); no `uses` = unlimited. A *master* key is
simply `["door", "chest"]`.

`data/abilities.json`: the effect `{ "unlock": ... }`: `true` (doors and chests), `["door"]`, `["chest"]`, or
`{ "kinds": ["chest"], "needs": "lockpick" }` (the user must carry that item; one use is spent per opening; it must have `uses`). The
ability has `"target": "cell"` (a square of the door or chest) and its `range` (default `[1, 1]`); `uses`, `cooldown`, `cost` and `if` work as for
every ability. This is the thief's trait; give it to a unit type or a character like any ability.

`data/items.json`, a staff: the effect `{ "unlock": true | ["door"] | ["chest"] }`; the staff picks a **square** (like `light`), so it has no
`target`, `area` or `if`; `range`, `hit`, `xp`, `uses`, rank and lock work as for every staff.

`maps/*.json`, a placed structure:

```jsonc
{ "type": "chest", "x": 2, "y": 0, "id": "c1",                                  // "id": the story name (letters, digits, _ -; unique among structures)
  "contents": [ "potion", { "item": "heavy_axe", "uses": 8 }, { "resource": "gold", "amount": 50 } ],   // a chest needs 1 to 8
  "locked": false,                                                              // door / chest: overrides the kind's default
  "side": "red",                                                                // as every structure: the owner (a place's owner decides who may visit / destroy)
  "visitSides": ["blue"], "visitUnits": ["tink"] }                              // visit: only these (sides, units by story id) may visit
```

`maps/*.json`, a unit (and an `addUnit` command): `"goesFor": ["chests", "villages", "doors", "steal"]`, only for a unit of a computer side (`"steal"`: see "Steal").

`game.json`: `"rules": { "places": { "actions": "menu" | "auto" } }`; default `"menu"`.

### Rules

- **Reach.** A structure is in reach of a unit standing on `from` when `from` is one of its squares (`"on"`) or next to one (`"next"`), as its
  kind's `from` says (door and chest: either; place: its own square). A multi-square gate is one structure: it is next to the unit from any
  of its squares' neighbours, and it opens as one.
- **Open** (`Action::openPlace(unit, structure, keySlot)`). A locked door / chest needs the inventory slot of a key that fits (the first of
  them is the menu's default); an unlocked one needs none. The key's use is spent, the structure becomes the kind in `opens`, the lock goes,
  `DoorOpened` / `ChestOpened`, a chest's contents, then the story's `place` events, then the unit's turn ends. Refused with a plain reason
  (`it is locked and needs a key`, `that key does not fit`, `it is out of reach`, ...).
- **Visit** (`Action::visitPlace(unit, structure)`). The place must be a visit kind not yet used (or `repeat`). Who may visit: the placement's
  `visitSides` / `visitUnits` when it has any (a side or a listed unit); else the owner's team when the place has an owner; else anybody.
  A visit sets `used`, turns the place into `becomes` when it has one, fires `PlaceVisited` and the story's `place ... "visited"` events; the
  visit's gifts are those events' commands (lines, `giveItem`, `giveResource`, `addUnit`...). Nothing else is a "visit" rule: it is the
  thinnest layer over events.
- **Destroy** (the same action, when the unit may not visit). A place with a `ruin` and an owner is destroyed by units of another team than the
  owner's: it becomes the `ruin` kind (different art), `PlaceDestroyed`, the story's `"destroyed"` events; a destroyed place cannot be visited.
- **Break** (`Action::breakPlace(unit, structure, weapon)`, the Attack menu). A shut door with `hp` can be attacked with any weapon the unit
  can use at the distance to the nearest square of the door (`afterMove`, ammunition and wear as for a fight). One strike is the weapon's damage
  formula with the door as the foe `b` (`b.<stat>` = the kind's `stats`, `b.hp` = its hit points; no skills of the attacker, as the door is
  not a unit; no dodge, no counter, no experience, no rank growth). `PlaceHit` per strike; at 0 the door opens (`DoorOpened`, how = break).
  The forecast (`Battle::placeForecast`) gives the damage per strike, the strikes, the hit points before and after and whether it opens.
- **Trait.** The unlock ability (menu abilities, Cell target) opens a door / chest of its kinds in its range without a key, spending its
  lockpick use when it has a `needs`. **Staff.** The unlock staff picks a square in its range and, if its `hit` roll succeeds (rolled once with
  the battle's dice; a miss still spends the use and gives the experience; `StaffMissed` with a = -1), opens the structure; it ignores `by`.
- **Chests.** The contents go to the opener's inventory, else the side's shared storage (`rules.items.storage`), the engine's existing
  overflow handling. With no storage a chest with items **cannot be opened** by a unit whose inventory is full (`the unit cannot carry more:
  drop something first`): it does not destroy the loot (the player discards from the Items menu, then opens). Amounts go to the opener's side.
  A story command with no unit sends the contents to the first human side's storage / funds (and, with no storage, an item is lost like
  `giveItem` to a full unit). The chest's `loot` is emptied; a chest cannot be opened twice.
- **Enemy looting.** A computer unit opens a chest like anyone; what it takes is marked `drop`, so whoever defeats it gets the item (the engine's
  existing drops).
- **Movement and sight.** The kind's `moveCost` is what blocks: a closed door's 0 is a wall for pathing, reach, the AI's fields and fog alike.
  `blocksSight` hides what is behind its squares from a unit in a straight line (`Grid::line`: squares and hexagons); only with fog. Opening
  changes sight at once (`markSight`) and the events are stamped with the teams that saw them.
- **Shutting again** is a story matter (below): a door cannot be shut by a unit.
- **`rules.places.actions`.** `"menu"` (default): the actions above, each ending the unit's turn. `"auto"`: after a unit **ends a move**, every
  door / chest it could open (an unlocked one, or one it carries a fitting key for) and every place it may visit is done by itself, without ending the
  turn; destroying and breaking stay explicit, and unlock abilities / staves stay in their menus. A computer unit does only what it is marked for.

### Story: triggers and commands

Trigger (docs/EVENTS.md): `{ "place": "d1", "is": "opened" | "visited" | "destroyed", "by": <unit filter> }` (`by` optional; a story-made opening
has no unit). Commands: `openDoor`, `closeDoor`, `lockDoor` (shuts it and locks it, or only locks a shut one), `unlockDoor` (a door or chest:
the lock goes, it stays shut), `openChest` (`"by": "<unit id>"` gets the contents). `closeDoor` goes back to the kind that opens into the present
one; it is skipped while a unit stands in the doorway. Every id is checked against the map at load.

### Events

Appended before `Choice`: `DoorOpened` (a = structure, b = unit or -1, c = how: `kOpenKey` 0, `kOpenTrait` 1, `kOpenBreak` 2, `kOpenStory` 3,
`kOpenStaff` 4, `kOpenFree` 5, d = the kind it was), `DoorClosed` (a, b = old kind, c = new kind, flag `kLocked`), `ChestOpened` (as `DoorOpened`),
`PlaceHit` (a = unit, b = structure, c = weapon, d = damage, flag `kKill`), `PlaceVisited` (a = structure, b = unit, c = old kind), `PlaceDestroyed`,
`PlaceUnlocked`. New flag `kLooted = 4096` on the `ItemGained` / `ResourceChanged` of a chest's contents. Fog: the teams that see the structure,
and the one that did it.

### Saves and hash

Battle `structures[i]` gains `locked` (a door / chest kind), `hp` (a door with hit points), `used`, `loot` (a chest: `{item, uses}` /
`{resource, amount}`); a unit gains `goes` when it is marked. A save of a game without these is unchanged and old saves load (missing keys = the kind's
defaults); `type` may follow `opens` / `becomes` / `ruin` and the reverse of a door's `opens` (the closed kind). Hash: nothing new: the structure,
item, ability and map files are hashed whole, so the new keys count only in a game that has them.

### Computer opponent

Only a marked unit does anything here (`ai_places.cpp`). `chests`: it opens one in reach (unlocked, or it has a key and room) and walks toward the
nearest. `villages`: it visits one it may and destroys one that is its enemy's, and walks toward the nearest. `doors`: only while the enemy cannot
be reached at all (a closed door is a wall), it opens a door in reach (unlocked, or with a key it carries) and walks toward the nearest. Scores sit
between advancing and attacking, so a marked unit that can fight, fights. With `"auto"` the move itself does it. An unmarked unit never opens,
visits or goes toward any of them; a unit that cannot get through simply finds no way and waits (no oscillation). It does not use unlock abilities
or staves, does not break doors down and does not take a chest it has no room for.

### Player

All wording is a placeholder, in one file (`engine/player/battle/place_text.h`; the plain reasons in `place_text.cpp`), like `staff_text`.

- **Menu** (`"menu"` mode). After the abilities and the Staff entry: **Open**, **Visit**, **Destroy**, one per entry of `Battle::placeOptions`; with
  several of one verb the label names the structure ("Open Cell Door"). A greyed entry shows a short reason (`Needs a key`, `Locked`, `Pack full`; dropped
  when it does not fit a narrow menu, the entry stays greyed). Choosing one uses the action and ends the turn. With **two or more fitting keys** a
  **Which key?** list (name, uses) opens with the first highlighted; Cancel returns to the action menu. In `"auto"` mode there are no Open / Visit /
  Destroy entries (the move does it); Break door, abilities and staves stay.
- **Break door** is its own entry (not inside Attack: the Attack picker is built around units), shown when `placeAttackOptions` has something; it picks a
  square of an attackable door with the weapon that does the most damage. The forecast box: `<door>  <weapon>`, `Damage 13 x2` (the `x2` only with several
  strikes), `HP 20 > 7`, `Breaks` when it would open.
- **Unlock ability**: the normal square pick over `abilityTargets`. **Unlock staff**: the light staff's square pick, the whole structure marked; forecast
  `Opens <name>`, `Hit N%` only when it can miss, `Exp N`. Cancel steps back as for a light.
- **Result**: floating `<kind> opens` / `<kind> shuts`, `Unlocked`, `Visited`, `Destroyed`, the damage on a door hit, `Missed` over the door square for an
  unlock staff miss, `Found <item>` / `Found 50 Gold` for a chest's contents (`kLooted`); the camera looks at the structure. One `[info]` line per beat for
  the windowless tests. The structure's new kind is drawn from the start of the event sequence (the battle is already final when events play), the floats
  and camera are at their own beat.
- **Info panel**: after the structure's name a `State` row (`Locked`, `Shut`, `Open`, `Visited`, `Ruined`) and `HP 7/20` for a door with hit points. Out of
  sight only the remembered kind shows (`Shut`, never the lock or the hit points).
- **Fog**: the shown sight respects line of sight past `blocksSight` kinds and is recomputed when one changes kind during playback.
- **Four view kinds**: structures are drawn from the shown kind in 2D, 2D isometric, 3D isometric and full 3D, so the change shows in all four
  (`tests/srpg_player_doors_test.py` checks the picture changes in each). A distinct 3D door model or opening animation is not practical: structures there
  are plain blocks per kind with art only through `graphics.json`. No sound cues for the new events.

### Decisions (the owner may change any of these)

- A door / chest is "locked" by default; "not locked" doors open for anyone adjacent (menu: Open, no key).
- A neutral place cannot be destroyed; only the owner's enemies destroy a place that has a `ruin`.
- Doors do not give experience and the attacker's skills are not part of the door's damage formula.
- The AI opens doors only when a door is what keeps it from the enemy.
- Strings are placeholders (see the Player part).

### Not built

- **Steal** is built (section 3 "Skill and ability blocks, second set", Steal): an ability that takes an item from an adjacent foe. A stolen item is no
  longer in the foe's inventory, so it cannot drop when the foe is defeated.
- No editor forms (docs/EDITOR_UI.md says what to hand-edit); the helper keeps the keys.
- The AI does not use unlock abilities / staves and does not break doors down.

### Tests

`tests/test_srpg_doors.cpp` (rules), `test_srpg_doors_data.cpp` (loader refusals), `test_srpg_doors_ai.cpp`, `tests/srpg_player_doors_test.py`,
`tests/editor_api_doors_checks.py`; fixture `tests/fixtures/srpg-doors` (layout in `tests/srpg_doors_fixture.h`).

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
| `RankUp` | a = unit, b = weapon group, c = new rank, d = old rank (section 7) | the unit's own team |

Section 8 appended `StaffUsed`, `StaffMissed`, `ItemRepaired` and `AreaLit`, and section 9 appends `ConditionApplied`,
`ConditionEnded`, `ConditionResisted` and `ConditionTick` (with the flag `kRefreshed = 2048`); section 10 appends `DoorOpened`, `DoorClosed`,
`ChestOpened`, `PlaceHit`, `PlaceVisited`, `PlaceDestroyed` and `PlaceUnlocked` (flags `kLooted = 4096`, `kLocked = 8192`), all before `Choice`.
The second set of skill and ability blocks appends `ItemStolen` (a = thief, b = the unit robbed, c = item, d = uses left or -1) and `UnitRefreshed` (a = the refresher,
b = the unit that may act again, c = ability), both before `Choice` and both seen by the teams that see either unit.

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
| | `goes` | the map marked the unit for the computer opponent (section 10) |
| battle `structures[i]` | `locked`, `hp`, `used`, `loot` | the structure is a door / chest, has hit points, was visited, holds loot (section 10) |
| | `moveSpent`, `acted`, `refreshed` | the game has `moveAgain` / `actAgain` skills and the value is not 0 / false |
| | `abilities` | the unit has used an ability this battle |
| | `summon` | the unit was summoned |
| | `carrying` | the unit carries / has a unit behind it |
| | `conditions` | the unit has status conditions (section 9) |
| battle | `supports` | the game has support pairs |
| playthrough `roster[i]` | `skills` | the unit has learned skills |
| playthrough | `supports` | the game has support pairs |

Restoring checks every new field's type and range and the cross-unit rules above (carry links, summoner ids), fails
with a path and a plain reason (`units[4].summon.by: unit 9 does not exist`), and applies nothing unless everything
holds. Round trip stays exact, including the middle of a turn with `acted` units, cooldowns and carried units.

Data hash (`save_hash.cpp`, `mixRules`): after `data/research.json`, each only when the file exists and in this
order: `data/characters.json`, `data/skills.json`, `data/abilities.json`, `data/supports.json` (the last with the
map-file wording treatment), then `data/conditions.json` (section 9; `tag` is cosmetic too). `rules.rescue` and `rules.promotion` are part of `game.json`'s `rules`, which is hashed
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
  is available. `push`, `pull`, `swap`, `moveBehind`, `movePast`, `refresh`, `cure`: never used. `steal`: only by a unit
  marked `"goesFor": ["steal"]` ("Steal", below). A free ability is used at most once per unit per turn by rule, so no loop is possible.
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
`supports` (the file's one object, like `campaign`; `false` removes the file). An empty list is written as an empty file and
`false` removes a list file (owner, 2026-11-15: a feature switched on in the editor stays on; the empty file is what
remembers it). A game without the file and a game with an empty list are still the same game: the data hash skips a
file that holds nothing, so switching a feature on never makes a save invalid. Unknown keys and key order are kept. `GET .../problems` runs the real loader, so every refusal in this
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
- `rules.rescue`: `carry`, `canCarry`: not units tagged `mounted` (Cavalier and the sky rider get the tag) and
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
in `always` blocks. Skills that change vision, weapon range or weapon wear;
reflected damage. Story commands for conditions, and carrying a condition to the next campaign map (section 9, "Not built"). Abilities with an area, with a formula effect, or that attack. Per-class maximum levels and
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

#### K2 as built

The combat and timed effects work. Games without skills are untouched (every pin of `test_srpg_compat.cpp` and
`test_srpg_golden_fights.cpp` unchanged, the four sims identical, saves unchanged: K2 adds no saved state).

**Where things are.** `skill_combat.cpp` (`skillCombatMods`, `auraCombatMods`), `skill_strike.cpp` (`strikeActivations`,
`strikePassives`, `forecastNotes`, all three on one function `gatherStrike`), `skill_timed.cpp` (`skillsTurnStart`,
`skillsAfterCombat`, `runTimedBlock`), `skill_fx.cpp` (the context a fighter's blocks are judged with, conditions,
chance). Declarations are appended to `srpg_internal.h` under "skills, step K2". Tests: `test_srpg_skills_combat.cpp`,
`_strike.cpp` (roll order, no roll at 0 / 100, events before `Attack`, with real skills), `_timed.cpp`, `_sim.cpp` (20
seeds, AI on both sides, never hangs, same seed gives the same battle), `_promotion.cpp` (a promotion teaches the new
type's entries through `learnUpTo`); helper `tests/srpg_skills_duel.h`.

**What the player (U1) builds against.**
- `Forecast::notes`: one `ForecastNote {unit, skill, chance}` per activation that could fire, in roll order (the
  attacker's strike blocks, the defender's struck blocks, then, if it counters, the defender's strike blocks and the
  attacker's struck blocks). A chance of 0 is not listed; 100 is. `strike` / `struck` blocks without a chance are
  already in the forecast numbers, so do not list them again.
- `SkillActivated` (a = owner, b = skill, c = foe or -1) before `Attack` (strike and, on a hit, struck activations); after
  a combat: the attacker's, then the defender's, each followed by the `UnitHealed` / `UnitHurt` it caused. `UnitHurt`:
  a = receiver, b = HP lost (real, never to below 1), c = the owner. `SkillLearned` is as K1.
- `actAgain` makes no event of its own beyond its `SkillActivated`; the unit's flags are cleared (`Unit::refreshed`).

**Readings of the contract (keep to these).**
- **`actAgain` and `endOrder`.** F2 runs `srpgAfterCombat` and then `endOrder(attacker)`, which would end the turn the
  skill just gave back. `actAgain` therefore clears the flags and records the unit in `Battle::actAgainPending_`;
  `endOrder` (`srpg_hooks.cpp`, one added early return) skips the unit once and forgets it. `combat.cpp` is untouched.
  A `defeat` block of the defender never refreshes (it is not its side's turn).
- **The weapon in use after a combat** (`defeat` / `afterCombat` conditions): the weapon of the unit's last `Attack`
  event in the action's events; without a strike the attacker's resting weapon and no weapon for the defender.
  `distance` and `who: foe` terrain / structure conditions are false for a foe that has fallen (it has no square).
- **Auras**: the owner never receives its own aura; the owner's conditions use the weapon it fights with if it is a
  fighter, else its resting weapon; distance is measured from the attacker's `setup.from`. Auras of units off the board
  do nothing; owners more than 10 steps from both fighters are skipped before their skills are read.
- **Order**: a fighter's own `combat` blocks go into the bags in the order attacker, defender; `to: foe` effects go
  into the other bag at the moment their block is read.
- **Strike blocks** use the weapon the striker fights with (a defender with no answering weapon has none). The extra
  strike of `strikeAgain` judges and rolls its blocks again (its `strikeAgain` is ignored), so it can show its own
  `SkillActivated`.
- **Chance blocks** that fire make a `SkillActivated` even when the effect changes nothing (a heal at full HP); blocks
  without a chance make one only when something changed. `heal` / `hurt` never go past the missing HP / 1 HP and make
  no event for 0. Heal to allies and `hurt` to enemies need the owner on the board; allies are other units of its team,
  enemies those its team sees. Units off the board get no `turnStart` blocks.
- `heal` by a skill does not call the supports' "healed" hook (S1 owns it for items).
- Formula chances (`"a.skl"`) read stats as the rules see them, with `b` = the foe (nil in `turnStart`).

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

#### A1 as built

Section 4 is implemented as written. Every earlier game loads, plays, saves and hashes as before (the compatibility
and golden pins are untouched; the four sims print the same text as the build before this step).

**Where things are.** `data/loader_abilities.cpp` (file, unit types, late references); `battle/abilities.cpp`
(`abilitiesOf`, `abilityOptions`, `abilityTargets`, `validateAbility`, `noteAbilityUse`, the turn tick),
`ability_target.cpp` (`abilityStateOk` = the unit's own state, `abilityTargetOk` = one target; both are what validation
and the menu queries share), `ability_apply.cpp` (`applyAbility`), `ability_moves.cpp` (`planAbilityMove`,
`doAbilityMoves`), `summons.cpp` (`summonUnit`, `removeSummon`, `summonsUnitGone`), `save_abilities.cpp`. Added
declarations (appended): `abilityUseOf`, `summonCount`, `isSummoned` in `srpg_api.h`; the helpers above,
`squareFreeFor`, `summonIgnored`, `summonGivesNoXp` in `srpg_internal.h`; `AbilityMove` / `AbilityPlan` in
`srpg_state.h`. The three exceptions are one line each: `turn.cpp` `checkDefeat` (`!isSummoned(u)`), `objectives.cpp`
`Defeated` and `story.cpp` `alive` / `dead` (`summonIgnored(filter, u)`); `progression.cpp` `awardXp` gained one line
for `givesXp` (see below). `combat.cpp` and the rescue files are untouched.

**What the menu queries mean (for the computer opponent and the player).** `abilityOptions(unit, from, out)` lists every
ability the unit has, in `abilitiesOf` order. A reason is one of: `can only be used before moving` (`afterMove: false`
and the unit has moved, or `from` is not its square), `no uses left this battle`, `ready in N turn(s)` ("1 turn" in the
singular), `already used this turn`, `already has N summoned <Type>(s)` (the type's name; "Hounds" from two), `cannot be
used now` (an `if` condition about the user), `nothing to target`. `abilityTargets` judges geometry, fog and the
conditions about the target only, not uses or cooldown, and returns nothing for an ability the unit does not have.
`validate` gives the first reason that applies, in the order of section 4's list, then target reasons (`that unit is out
of range`, `you cannot see that unit`, `there is no room to push it`, `there is no room behind the user`, `that square is
occupied`, `that unit is at full health`, ...). A "self" ability takes target -1 or the unit itself; its event names the
unit in `c`.

**Interpretations where the contract was silent.**
- Extra loader refusals (all start `data/abilities.json: ability "x": `): `"target"` and `"effect"` are required;
  `"range"` must be `[min, max]` with whole numbers from 1 to 20 and min not above max, and is refused with `"self"`;
  push / pull / swap / moveBehind need `"target"` ally, enemy or unit; `"push"` is 1 to 5, `"heal"` 1 to 999 or `"full"`,
  `"pull"`, `"swap"`, `"moveBehind"` must be `true`; unknown keys in the ability, its effect and its summon. A
  condition's `"who": "foe"` is refused in an ability that targets a square; `initiating` and `distance` are refused (no
  combat); `formula` is allowed. A summon's unit type and the unit / terrain keys of an `if` are checked last (the
  summoned type may be defined after the ability).
- `kBlocked` means "the unit in the way was one the user's team could not see". A push stopped by a unit everybody sees
  (the second square of a push of 3) is the normal end of a push, no flag. A blocked push keeps the squares it already
  moved; a blocked pull, moveBehind or summon does nothing.
- `free` abilities do not set `moved`; only they carry the `turn` flag (an `action` ability ends the turn anyway).
- Order inside `applyAbility`: `AbilityUsed`, the effect's events, the use / cooldown and `endOrder`, and last the
  story's `reach` events for moved units (target first). A heal calls `supportsHealed` (S1's hook) for another unit.
- A summon is made with the type's base stats at the chosen level (no growth is rolled for the levels above 1), the
  type's items with `drop` cleared, no name of its own (the type's), `moved` and `done` set unless `canAct`.
- `lifetime: n`: `summonLeft` starts at n; at each start of its side's turn it drops by one, and a summon that finds it
  at 0 leaves (`UnitRemoved`), so n = 1 gives one turn of its own. A summon that is being carried vanishes out of its
  carrier as a story `removeUnit` would.
- `givesXp: false` is applied in `awardXp` (the one place both fighters' awards pass) by reading the last `Attack` event
  of the action for the foe, because `combat.cpp` may not be touched. The only caller of `awardXp` is the fight, so no
  other award can be mistaken for it. If a later step awards XP from somewhere else it must pass the foe a better way.
- Summoned units are ignored for "side", "team" and "any" in `defeated` objectives and `alive` / `dead` conditions;
  `defeated` triggers and every other filter use are unchanged.
- Saves: `summon.left` must be present exactly when the ability has a `lifetime`; `used` stops counting at 999.

**Tests.** `tests/test_abilities_{loader,rules,moves,fog,summons,summons_rules,save}.cpp` with `abilities_fixture.h`
(square, pointy and flat, fog on or off, abilities in code), `abilities_summons_fixture.h`, `abilities_files_fixture.h`
and the game `tests/fixtures/srpg-abilities`. `test_srpg_stubs.cpp` lost the two refusals that are real now.

**Left for others.** The computer opponent (AI1) and the player (U3) do not use any of this yet. The editor's ability
screen is E3. Summons are not handled in `changeSide` (a summon whose summoner changes side keeps its own side), and no
story trigger or condition names "summoned".

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

#### AI1 as built

The computer opponent follows the section "AI". Games without promotions, abilities, move-again skills or rescue play
exactly as before (all eight reference sims print the same text, compat and golden pins untouched).

**Where things are.** `ai_srpg.cpp` (`considerSrpg`, promotion; called from `bestOrder` in `ai_order.cpp` after the
healing-item rule and before supply), `ai_abilities.cpp` (summon and heal), `ai_again.cpp` (`moveAgainOrder`).
`Order` gained `ability` and `cell`; `aiTakeStep` (`ai.cpp`) applies `Promote` and `Ability` orders. Tests:
`tests/test_srpg_ai.cpp`.

**What the AI does.** Promotion: the first offered type, in place, instead of a plain Wait or advance (an attack,
capture, item or supply order is kept; no promotion items). Heal ability: the unit itself or an adjacent ally below half
HP, lowest share first, unless the planned attack kills; it replaces an attack or a Wait. Summon: instead of a plain
Wait or advance, from where the unit would stand after the advance it planned (else where it stands), on the legal
square nearest the nearest known enemy (lowest cell number on a tie). Move again (`acted`): a step to the square with
the fewest known enemies able to reach it (walk plus longest weapon), better cover on a tie, only if strictly better
than staying; else Wait. Act again needs nothing: the unit is planned afresh. Never used: push, pull, swap, moveBehind,
rescue, drop, give, take, pair up, switch, `Support`.

**Where the contract was silent.**
- A summoned unit never summons (each summoner has its own limit, so summons of summons would otherwise grow without end).
- A free ability is used where the unit stands, before any move, so the unit is then planned afresh with its turn open
  (once per turn by rule, so it cannot loop); an action ability may follow the advance the unit planned.
- A heal takes the place of an attack that does not kill (the same as the healing-item rule); a summon never replaces
  an attack. Healing only looks at the moment the unit is planned: an ally that walks off first is not healed.
- Units are planned best order first, so a wounded ally may leave before a healer plans; nothing is held back for it.
- An `acted` unit no longer plans attacks or captures (it cannot act); before AI1 it could pick an attack square and
  merely walk there.

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

#### U1 as built

Skills and unit details are in the player. Games with no strategy-RPG data look and play as before (every other player
test is unchanged). Files: `battle/unit_info_rows.*` (the rows of the three pages, built from the `Battle` with no
renderer, and their drawing), `unit_info_stats / _skills / _items.cpp` (one `add...Page` each), `battle_menu_info.cpp`,
`battle_view_skills.cpp`, `forecast.cpp`, `panel.cpp`. Tests: `tests/test_srpg_player_skills.cpp` (rows, pages, view
hooks), `tests/srpg_player_skills_test.py` (ctest `srpg_player_skills`: U1's part of the player test; headless scripts
that read the log lines below and the pixels of the screenshots, at 480 x 270 and 320 x 480).

**Wording (placeholder).** Forecast: "Strikes first"; a note "Heavy Blow 40%"; "+2 more" after three notes of a column;
strikes "Dmg 7 x2 x2" (strikes, then in a row), "Dmg 7 x1 x2" for one strike that lands twice. Fight: the skill's name
floats over its owner; "-3" over a unit a skill hurt; "Survived!" over a unit a `survive` skill kept up; a skill
learned outside a level-up (a story's `teachSkill`) floats "Learned Heavy Blow". Level-up box: a line "New skill:
Heavy Blow" under the stat lines. Panel: skill names in gold (three, then "+2 more"), "Carrying: Bram" / "With: Bram"
(pair up), a small gold "S" tag at the right end of the name row when a support conversation is ready. Menus: **Info**
(action menu, before Wait) and **Unit** (battle menu, after Objective). Unit details: tabs **Stats**, **Skills**,
**Items**; Stats rows "Class", "Level" (with XP), then every stat, "12/14" for hit points, and "(+2)" / "(-1)" after a
value that skills, items or partners raised or lowered (green / red); Skills: name, "Class" / "Learned" / "Personal" /
"Item" / "Weapon" in small print at the right, the description under it, "No skills"; Items: name, uses left ("3/3"), a
small gold square for the weapon in hand, the accessory's bonus or the description under it, "No items"; a list that
does not fit ends with "+N more".

**Log lines the script tests read** (`OTE_INFO`): `action menu: Attack, Items, Info, Wait` (when the game has Info),
`unit details: Eirik (Stats, Skills, Items)`, `forecast: <lines of the left column>; | <right column>; |` (once per
change), `skill activated: Eirik - Sure Strike`, `unit hurt: Grunt -2`, `skill learned: ...`, `level-up box: New skill:
...`, `Survived! Grunt`.

**Readings of the contract (keep to these).**
- **Info and Unit exist only in a game that uses strategy-RPG data** (skills, characters, abilities, supports pairs, a
  promotion, or `rules.rescue.mode`); any other game keeps its menus exactly as they were. The pages follow what there
  is to show: Skills only with a skills file, Items only when the game has items (or the unit carries some).
- **Unit** is offered over a unit that is alive and visible to the viewing team (the panel's own test); it opens the
  details of that unit, whoever's it is. Cancel goes back to the map; from Info it goes back to the action menu.
- **Strikes**: the forecast's damage per strike times `strikes` times `multi` gives the hit points shown after the fight.
  "Strikes first" is written under the defender's column. The forecast box may grow over the info panel on a narrow
  window (up to the whole screen width), so a note's name is not cut.
- **Notes** are listed under the fighter that owns the skill, in roll order, at most three per column; a chance of
  100 is written "100%".
- **`SkillLearned` in the level-up box**: the box (`BattleView::addLearnedToBox`, called from the one line of
  `LevelUp` in `battle_view.cpp`) takes the `SkillLearned` entries of the same unit that follow the `LevelUp` entry out
  of the queue and lists them; a promotion that teaches skills makes no entries (the core's `learnUpTo`).
- **`UnitHurt`** shows "-N" over the unit and runs its displayed hit points down; the label of the skill that did it
  was announced just before by its `SkillActivated`.
- **A unit that has acted** (`acted`, not `done`): picking it shows its remaining move range (blue squares, no red
  attack squares), Confirm on a reachable square moves it (which ends its turn), on its own square opens a menu of **Info**
  and **Wait** only; a tap or Confirm on an enemy does nothing (an error cue).
- **Not covered by a screenshot**: the "Carrying" / "With" row and the support tag (their data needs a rescue or
  supports game; the rows are written against `Unit::carrying` and `supportOptions`); the stats page of a game with
  more stats than fit (it flows into up to three columns, covered by a unit test).

**Edits outside U1's files** (each the smallest possible): `battle_menus.cpp` (the `acted` branch at the top of
`openActionMenu`), `battle_input.cpp` (`selectUnit` and `computeOverlay` for an `acted` unit, one guard on attacking from the
selected mode), `battle_view.cpp` (the `addLearnedToBox` call, the "Survived!" label in `impact`, a `log.h` include),
`battle_view.h` (one declaration), `CMakeLists.txt` (the ctest entry), `tests/test_srpg_player.cpp` (two expectations that
U0 wrote for empty hooks: the details of a game without skills have the Stats page; the skill entries play for a moment).

### U2. Player: promotion — depends on P1, U0; parallel with U1, U3, U4, U5
Promote entry, the question box, the promotion screen (`battle/promotion_screen.cpp`), `UnitPromoted` animation
and banner, sprite change.

#### U2 as built

Promotion plays in the player as "Player UI > Promotion" says; a game without promotions shows nothing new (the
player scripts of every other game are unchanged).

- **Files.** `battle_menu_promotion.cpp` (the Promote entry, the question box, opening and answering the screen, the
  picture of a type), `promotion_screen.*` (the screen: list and details, keyboard, pointer, touch), `promotion_text.*`
  (SDL-free: what a promotion gives as text, `previewPromotion`, the question, the banner), `battle_view_promotion.cpp`
  (`UnitPromoted`). Tests: `tests/test_srpg_player_promotion.cpp` (text and view timing), `tests/srpg_player_promotion_test.py`
  with its games in `tests/srpg_promo_games.py` (ctest `srpg_promotion_player`).
- **Flow.** Promote (an offer at a level) or a promotion item in the item list, used on the unit itself, both go
  through `openPromotion(slot, types)` with `Battle::promotionOptions`: one type asks, several open the screen
  (new `Mode::Promotion`). Confirm submits `Action::promote(unit, to)` or `Action::useItem(unit, slot, unit)` with
  `unitType = to`, then `finishOrder()`. Cancel / No / a tap outside go back to the action menu (Promote) or the item
  list (item). The screen's list answers like the item panel: a first tap selects, a tap on the selected row picks.
- **Animation.** `UnitPromoted`: the unit flashes white (the same wash as a side change); at the brightest moment its
  picture changes to the new type (`UnitView::type` holds the OLD type from `push` until then and every drawer reads
  it through `shownType`: sprite, placeholder letter, 3D model in both 3D views, 2D isometric); then the banner for
  0.9 s; then the stat box until Confirm (an AI side and autoplay: 1.2 s). An automatic promotion is the same entry
  right after its level-up box, so its banner follows that box.
- **Log lines the script tests read:** `promote entry for Kell: Captain, Scout`, `promotion question: Captain` /
  `promotion screen: Captain, Scout`, `promotion plays: Kell, Recruit to Scout, level 3`.

New wording (all placeholder): entry **Promote**; question `Promote Kell to Captain?` then the changes (`+6 hp, +3
atk, +2 def`, or `No stat changes`) and, when the level resets, `Back to level 1`, with buttons **Yes** / **No**;
screen title `Promote Kell`, details `Gains: +3 hp, +1 mov` (or `No stat changes`), `Back to level 1`, `Skills:
Tough, Light Step`, `Cannot wield: Blade`; hints `Left/Right + Z:answer  X:no` (question) and `Up/Down + Z:choose
X:back` (screen); banner `Kell is now a Captain` (`an` before a vowel); stat box title `Stat changes` with lines like
`+3 hp` / `-1 mov` (`No stat changes`).

Where the contract was silent:

- **The Promote entry is shown only when the promotion can be done now** (`validate` passes); there is no greyed
  entry with a reason. A unit that may only move or wait (`acted`) therefore has none.
- **The question box always asks, even for an item.** A promotion item used by a unit that could promote with it (and
  where "itself" is among its targets) goes straight to the question / screen instead of the "Use on" list, so such
  an item's heal on another unit cannot be chosen from the item list. Used by a unit that cannot promote with it,
  the item behaves as before.
- **The question shows the same facts as the screen** (changes, level reset) but not skills or lost weapons.
- **"Skills" on the screen** lists the new type's own skills the old type did not have, plus what it learns at or
  below the level the unit will have; a skill the unit already learned is listed too. **"Cannot wield"** lists
  carried weapons the old type could wield and the new one cannot (only with carried weapons).
- **The picture** on the screen is the unit sprite of the style in use in the unit's side colours; a style with only
  3D models (or a type with no art) shows the side-coloured block with the type's letter.
- **Order:** flash first, then the banner (the banner covers the middle of the screen, where the unit usually is).
- The info panel and the unit's details show the new type at once (they read the rules state, not the animation).
- No new sound cue (the sound tables belong to docs/SOUND.md). Events the viewer's team does not see are not shown
  (as for every event).
- A phone-width screen (under 300 px wide, a phone held upright) stacks the list above the details.

Shared files touched (smallest possible edits): `battle_scene.h` (`Mode::Promotion`, seven declarations, four
members, one include), `battle_input.cpp` (three lines: send input to the screen), `battle_scene.cpp` (a hint case and
a draw case), `battle_items.cpp` (one line at the top of `chooseUseTarget`), `battle_view.h` / `battle_view.cpp`
(`UnitView::type`, `pushPromotion`, one call in `push`), `battle_draw.h` (`shownType`) and the five drawers that read
a unit's type (`sprites.cpp`, `map3d_models.cpp`, `battle_draw.cpp`, `iso_unit.cpp`, `map3d_things.cpp`), root
`CMakeLists.txt` (`promotion_text.cpp` into `ote_tests`, one ctest), `tests/test_srpg_player.cpp` (UnitPromoted is no
longer in the list of events that are no-ops).

### U3. Player: abilities and summons — depends on A1, U0
Ability entries with reasons, targeting with the end squares marked, `AbilityUsed` / blocked animation, the
Abilities page of Unit details.

#### U3 as built

A game without abilities looks and plays as before (no entry, no page, no log line). The files, all under
`engine/player/battle/`: `battle_menu_abilities.cpp` (entries and targeting), `battle_view_abilities.cpp` (the
`AbilityUsed` hooks), `unit_info_abilities.cpp` (the page), and three pure helpers: `ability_text.h` with
`unit_info_abilities_text.cpp` (all the wording), `ability_preview.h` with `battle_view_abilities_preview.cpp` (where
units end up). The two helper `.cpp` files carry `unit_info_` / `battle_view_` in their names only so the existing
source globs of `ote_tests` pick them up (no CMake edit). Tests: `tests/test_srpg_player_abilities.cpp` (words,
preview against the core's real result on all three grids in every direction, the view, the page with a recording
renderer) and `tests/srpg_player_abilities_test.py` (registered as `srpg_player_abilities`: the menu, targeting,
every ability kind on square, pointy and flat maps, a blocked push in fog, the phone-sized screen).

**Menu.** One entry per ability the unit has, in `abilityOptions` order, after Talk / Support and before Rescue.
A usable one shows its uses left on the right when it has a limit ("2 left"); one that cannot be used stays in the
menu, greyed, with the core's reason on the right (the menu cannot select a greyed entry). When the full reason does
not fit next to the widest label, a short form is shown ("before moving", "no uses left", "used this turn", "not now",
"no target", "limit reached"; "ready in N turns" is already short); when not even that fits (the phone-sized screen)
the entry is greyed without a reason. A free ability (`cost: free`) leaves the unit its turn: the menu opens again with
the new reasons. An action ability finishes the order like Wait.

**Targeting.** `self`: used at once. `unit` targets: `beginPickUnit`; every legal unit's square is blue, and for the
highlighted one the squares the units would end on (and, for a push, pass) are red: push, pull (the user's step back;
the target's end is the user's own square), swap (both squares, under the units), moveBehind. `cell` targets:
`beginPickCell`; the legal squares are blue, the highlighted one red. The preview is worked out on what the user's team
sees: a hidden unit never shows in it, so in fog the use can still turn out blocked. While a target is picked the hint
line says what the ability does ("Pushes it 2 squares away  Z:use  X:back"), since swap and pull end squares are
hidden under units.

**Animation.** `AbilityUsed`: the ability's name floats over its user (light blue) for 0.45 s with the camera on the
user; a blocked one (`kBlocked`) lasts 0.2 s longer and also floats "Blocked!" (orange) under the square of the unit in
the way, which is how a player in fog learns something stands there. The moves, heal, summon and vanish arrive as the
events the core already emits (`UnitRelocated`, `UnitHealed`, `UnitAdded`, `UnitRemoved`) and are played by the
existing code. One `[info]` line per use: `ability: <user> uses <ability> on unit <id> square <cell>` plus ` (blocked)`,
and `ability chosen: <ability> by <user>` when the menu entry is taken (the windowless tests read them).

**Abilities page.** One block per ability: its name; what it does; "Use on <target>, range <r>; <ends the turn | free>";
its limits; what is left now (uses left, cooldown, used this turn); the game's description. No page for a unit without
abilities. A block is drawn whole; when the rest does not fit the box the page ends with "+N more...".

**New wording (placeholder, for the owner's review; all in `ability_text.h` / `unit_info_abilities_text.cpp`).**
"2 left"; "Nothing to target" (hint line, if a pick has no candidate); "Blocked!"; "+N more..."; the hint line
"<effect>  Z:use  X:back"; effects: "Pushes it N square(s) away", "Pulls it next to you", "Swaps places with it", "Moves
it behind you", "Heals N HP", "Heals all HP", "Calls a <Type> (level N | your level, up to N at a time[, lasts N
turn(s)][, waits a turn])"; targets: "yourself", "an ally", "an enemy", "any unit", "a square"; "range N" / "range
N-M"; "ends the turn" / "free"; limits: "N use(s) per battle", "ready again after N turn(s)", "only before moving";
status: "N of M uses left", "ready in N turn(s)", "used this turn"; the short reasons listed above. The core's own
reasons are shown as the core words them.

**Readings of the contract.**
- The Abilities page cannot be reached from the battle until step U1 adds the Info entry (`battle_menu_info.cpp`);
  until then it is tested through `UnitInfoView` with a recording renderer, and was looked at with a local, uncommitted
  Info entry.
- Greyed entries show the reason in the entry itself (not only in a hint line) because the menu never highlights a
  greyed entry.
- Pull's second end square (the user stepping back) is marked as well as the target's.
- Nothing is played for sound (no cue for abilities exists yet).
- A summon that runs out (`lifetime`) or vanishes with its summoner uses the existing `UnitRemoved` fade.

### U4. Player: rescue and pair up — depends on R1, U0
The six entries, targeting, the four events' animations, the carrier marker, the panel row.

#### U4 as built

Carrying and pair up are playable in all four kinds of view and with hexagons. Games without `rules.rescue` look and
play as before (no entry, no marker, no row, no log line).

**Where things are.** `battle/rescue_menu.*` (no SDL, unit tested: which entries a unit gets, the pick candidates, the
panel row text); `battle_menu_rescue.cpp` (items, picks, what each action does next); `battle_view_rescue.cpp` (the four
animations); `carrier_marker.*` (the marker, one function for every kind of view, and the declaration of the panel row);
`panel_rescue.cpp` (the row). One-line additions elsewhere: a call to `drawCarrierMarker` in `battle_draw.cpp` (top-down
and hexagons), `iso_unit.cpp` (2D isometric, squares and hexagons) and `map3d_things.cpp` (3D kinds); a call to
`drawRescueRow` in `panel.cpp`; three cases in `battle_view_fog.cpp` `applyUnseen` (below); one line in `CMakeLists.txt`
(`rescue_menu.cpp` into `ote_tests`) and the ctest entry `srpg_player_rescue`. The tests: `tests/test_srpg_player_rescue.cpp`
(menu, row text and the view's animations) and `tests/srpg_player_rescue_test.py` with its games in
`tests/srpg_rescue_games.py`.

**Menu.** Carry: `Rescue` (a unit with nobody, when a neighbour can be rescued), `Take` (when a neighbour carries someone
this unit can carry); a unit that carries: `Drop`, `Give` (when a neighbour can receive it). Pair up: `Pair up` (the unit
goes behind the partner it picks), `Switch` (the unit in front of a pair), `Drop` (the unit behind steps out). Order as in
the contract: Rescue, Take / Drop, Give / Pair up / Switch, Drop. Targets are picked with the generic unit pick (Rescue,
Give, Take, Pair up) and square pick (Drop) of U0; the hint lines are U0's. The log line the script tests read:
`rescue entries of Ann: Drop (no room), Give` (the entries of one menu, in order; a greyed entry shows its reason).

**What happens after.** Rescue, Pair up and Drop end the order (back to the map). Give, Take and Switch are free: the
action menu comes back (rebuilt, the cursor on the acting unit), the move can no longer be undone (the existing refusal
"Cannot undo after using items" is reused). After Switch the menu is that of the unit now in front, whose turn flags the
core has handed over.

**Marker.** A small tab in the carried unit's side colour with a dark rim at the top right corner of the carrier (the
leader mark is top left): a third of the unit's picture wide (at least 4 x 5 pixels), 5/12 high. Shown while the view no
longer draws the other unit, so it appears when a rescue animation ends and goes when a drop starts.

**Animations** (`battle_view_rescue.cpp`). `UnitRescued`: the unit slides from its square onto the carrier in 0.28 s and
fades out in the last 40 per cent. `UnitDropped`: it slides out of the carrier's spot (a fall: from where the carrier
lay) onto the square in 0.28 s. A blocked drop (fog): half way out and back in 0.5 s, "Blocked!" over the carrier, the
unit in the way is shown until the run of events ends; the unit stays carried. `UnitHanded`: the carried unit slides
from the unit that gave to the one that took, 0.28 s. `PairSwitched`: the two sprites cross-fade in 0.3 s. The camera
follows each. With fog, an unseen drop or switch still puts the unit's remembered spot on the right square
(`applyUnseen`), so it is where it should be when seen again.

**Panel row** (under the unit's type, over the level / HP rows; dropped when it does not fit): `Carrying: Bram` in carry
mode, `With: Bram` in pair up. With fog, for a unit of another team: `Carrying: someone` / `With: someone`.

**New wording (placeholder).** `Rescue`, `Drop`, `Give`, `Take`, `Pair up`, `Switch`, the reason `no room`, the float
`Blocked!`, the rows `Carrying: <name>`, `With: <name>`, `Carrying: someone`, `With: someone`.

**Readings of the contract.**
- `Drop` is offered, greyed with `no room`, to a carrier with no free square (the contract says each entry only when it
  applies; a vanished Drop would look like a bug). The other entries are left out when they have no target. `Switch` is
  left out when the core says it cannot (the unit behind could not stand on that square).
- A unit that has `acted` gets none of the entries (U1's rule: only Info and Wait).
- Pair up: the unit whose menu it is goes behind the one it picks (the core's `PairUp(unit, target)`); the front unit
  keeps its turn.
- The marker is also shown on a visible enemy carrier (that a unit carries somebody is on show); only the name is hidden.
- The player's unit lists (`!u.alive` in `battle_input.cpp`, `battle_scene.cpp`, `battle_story.cpp`, the panel, the menu
  anchoring, `battle_view_fog.cpp`) all mean "on the board and can act" and were left on `alive`: a carried unit must not
  be cycled to, ordered, shown or seen; nothing needed `exists`.
- The two U0 view tests that expected the four events to be instant no-ops (`srpg_view_plays_every_new_event_as_a_no_op...`
  and `srpg_view_hides_a_rescued_unit...`) were narrowed / replaced by the animation tests of `test_srpg_player_rescue.cpp`.
- On a phone-sized window the hint line of the pick modes (U0's text) is cut with `..`; the menus, the panel row and the
  marker fit.

### U5. Player: supports and camp — depends on S1, U0
Support entry and conversation, labels and banner, the Supports page, `campaign/camp_scene.*` and its place in
`app_campaign.cpp` (`doCampaignNext`), saving after a camp conversation, `--autoplay`.

#### U5 as built

Supports and the camp work as "Player UI" says; a game without `data/supports.json` (or a campaign without pairs)
looks and plays as before (no entry, no camp, no log line).

**Where things are.**
- `battle/battle_menu_supports.cpp`: one entry per partner, ids `kSupportBase + n`, from `Battle::supportOptions`. The
  partner is fixed by the entry, so there is no target to pick (`beginPickUnit` is not used). Choosing submits
  `Action::support` through `submit` and then rebuilds the action menu like after an item (free: the unit still acts, the
  move can no longer be taken back). The partners are asked for again when an entry is chosen (same list, same order).
- `battle/battle_view_supports.cpp`: `SupportPoints` with `kReady` makes a "Support!" float over each unit that is
  drawn (the second one 10 pixels higher, the pair stands side by side); other gains take no time and show nothing.
  `SupportReached` shows the banner for 1.4 seconds. The conversation lines are plain `Dialogue` events, which
  `battle_view_story.cpp` and the scene already show. An event whose pair is not one of the game's (the U0 test feeds
  made-up events) stays a logged no-op.
- `battle/unit_info_supports.cpp`: the Supports page (none for a game without pairs or a unit in no pair).
- `battle/support_text.*`: every wording and the numbers behind the page and the camp list, free of windows (unit tests).
- `campaign/camp_scene.*`: `CampScene` and `campStep()`. `app_campaign.cpp` calls `campStep` first in the
  `BeforeScene` case of `doCampaignNext`; it returns the camp, or the conversation just chosen (a `StoryScene` without a
  title card), or nothing (go on to the before scene). `CampaignRun` got `campPlay` (the pair chosen, -1 = none) and
  `campDone` ("Continue the story" was chosen; cleared when the before scene starts). The camp is not a phase: choosing a
  pair sets `campPlay` and calls `campaignNext()`; the conversation scene ends in `campaignSceneDone()` (which saves
  the playthrough like after any scene) and so brings the camp back with the pair gone.
- Tests: `tests/test_srpg_player_supports.cpp` (words, rows, page, view), `tests/srpg_player_supports_test.py`
  (games for `conversations` battle, camp and both, built from `games/demo-srpg` in a temp folder; menu, conversation,
  banner, camp, saves after a camp conversation, `--autoplay`, a game without supports) and ctest `srpg_supports_player`.

**New wording (placeholder).** Action menu: "Support: Lyra". Label over both units: "Support!". Banner: "Eirik and Lyra:
support level 1" (the level's own name: `levelNames` or "1", "2"...). Supports page: tab "Supports"; "Level B" /
"No support yet" per partner; "Conversation ready"; "Highest level"; "+2 more" when the page is too short. Camp: title
"Camp", under it "Conversations waiting", rows "Eirik and Lyra" with "Level C" (the level the conversation unlocks) on
the right when it fits, and "Continue the story" last; footer "Up/Down + Z: choose". Log lines the tests read:
`support entries: Support: Lyra` (or `none`, only in a game with pairs), `support: Eirik talks with Lyra`,
`support ready: Eirik and Kell`, `support level: <banner>`, `camp: <rows>`, `camp: playing <pair>`.

**Readings of the contract (keep to these).**
- The camp shows before the title card of every chapter after the first, whenever `pendingSupports` is not empty, also
  when a Continue save is loaded in phase Before (a pair left unplayed asks again at the next chapter). Nothing pending =
  no camp at all (it is skipped, no "nothing to do" screen). `--autoplay` skips it entirely (also nothing saved, as
  `saveAfterScene` never writes then).
- Cancel (or Skip) in the camp is "Continue the story"; the camp opens on the first pair. After a conversation the
  list starts at the top again.
- A camp conversation is a `StoryScene` of the lines `playSupport` returns; it has no title card, so the first line opens
  at once. The level it gains is not announced by a banner (the camp list shows the next level instead).
- The level column of a Supports row is "Level " + the level's name; the bar runs from the points of the current level to
  those of the next, full at the top level, and is drawn in the accent colour while a conversation is ready.
- Narrow screens: labels wrap and the camp rows grow to two lines; the Supports page puts "Conversation ready" under the
  bar when it does not fit beside it; the banner wraps (existing banner code).
- The support mark in the info panel (contract line "a support mark next to the name when a conversation is ready") is
  `panel.cpp`, which U1 owns; not done here. The Supports page is reachable only through U1's Info / Unit entries
  (`openUnitInfo`); until U1 lands it can only be seen with a temporary entry.

**Edits outside the owned files.** Root `CMakeLists.txt` (`support_text.cpp` into the unit tests' player sources; ctest
`srpg_supports_player`), `campaign/campaign_run.h` (the two camp fields), `app_campaign.cpp` (the camp's place).

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

#### E1 as built

The Characters screen, characters on the map and the "class" wording are in; the helper needed no change (E0's
`characters` section and the loader's problems already carry everything; `tests/editor_api_characters_checks.py` proves it).

- **Screen** (`screens/characters/`): list + form like Units inside E0's frame. Form cards: name / class / "Joins at level" (only when
  progression is on) / description; **Portrait** (Art's own `PortraitPicker`, keyed by the character's id); **Stats** with columns
  *class | Personal | Total* (class value greyed; personal may be negative; total never below 0) and the same for **Growth % each level**
  (progression on); **Starts with its own items** (off = the class's items, shown greyed in the help; on = a list that replaces them,
  an empty list allowed); Advanced: internal name. Duplicate puts a copy after the original (not placed anywhere); delete asks, lists
  the maps, events and campaign scenes that name the character and removes them in one undo step.
- **Slots for the next steps** (empty files, mounted by `screens/characters/srpg/CharacterSrpgCards.tsx`): `CharacterSkillsCard` (E2: `skills`,
  `learns`), `CharacterAbilitiesCard` (E3: `abilities`). Edit with `updateCharacter` in `model/characters.ts`. E4 reads `doc.characters`
  and `classWord`; `charactersInUse` / `characterChecks` (`model/characterChecks.ts`) are where a support check can start.
- **Map**: `MapUnit.type` is optional. A character is placed as `{ "character", "side", "x", "y" }`; the unit's type is
  `typeIdOf(characters, unit)` (`model/characterUnits.ts`), its story id `storyIdOf(unit)` (the character id). `MapCtx` gained `characters`.
  Events, objectives and campaign scenes see a placed character as a named unit. Unit form for a character: its name with "Open character", no
  story-name field, name / level boxes show the character's as placeholders, "Own stats" start from class + personal bonus. Picking another
  class stores `type` (this map only); picking its own class removes the key again.
- **Deleting a unit type** now also deletes the characters of that type (named in the question) with their placed units, pairs and story lines.
- **Checks**: "Not used on any map." (no map places it, no event adds it); a banner on the Characters screen, one line per character with a link.
  Skipped while any map is unreadable. The loader's refusals reach the Problems panel as before.
- **Wording** (placeholders): screen intro "Named people with their own stats, skills and story."; empty "No characters yet. Add one, such as the hero of your
  story."; "New character" / "Name of the character"; "Class" (the word is "class" once the game has a character, else "unit type"; the map picker tab
  reads "Classes" / "Unit types"); "Joins at level"; "Personal" / "Total"; "Growth % each level"; "Starts with its own items"; "Portrait" /
  "Add a portrait" / "Choose picture…" / "Remove portrait"; map: tab "Characters", "On the map", "Open character", "Its own" (the class), "Pick a
  character that is not on this map yet.", "This character is already on the map."; check "Not used on any map."; unit-type delete line
  "Character “X” has this class (the character goes with it)".
- **Readings of the contract.** (1) "Class" is applied on the Characters screen, the map's Units picker and unit form, and the unit-type delete
  line; other screens (Units, story forms, Rules) still say "unit type". (2) The story `addUnit` form does not offer characters yet (its
  `character` key is kept and counts for the check). (3) Personal skills / abilities pickers are E2 / E3's slots. (4) A deleted character's portrait
  entry stays in Art. (5) Level shows only with progression on. (6) Shared CSS: `.md-detail` column may shrink (a long name overflowed at 360 px).

### E2. Editor: Skills — depends on E1; parallel with E3, E4
The Skills screen with the block builder and summaries, the starter set and its installer, the Skills cards on
units, characters, items and weapons, story forms for teach / forget / has skill.

#### E2 as built

The Skills screen, the starter set and its installer, the Skills cards and the story forms are in; the helper needed no change (`tests/editor_api_skills_checks.py`
saves every kind of rule and the whole set and checks the loader and `ote-sim`). Details in docs/EDITOR_UI.md "Strategy-RPG: skills (E2)".

- **Where**: model `skillRules.ts` (the tables of section 3 as data), `skillBlocks.ts`, `skillChance.ts`, `skillText.ts`, `skillChecks.ts`, `skills.ts`, `starterSkills.ts`;
  screens `screens/skills/*`; the E0 / E1 slots `UnitSkillsCard`, `CharacterSkillsCard`, `ItemSkillsCard`, `WeaponSkillsCard`; story `forms/SkillForms.tsx`.
- **Shared files touched** (small): `srpgRefs.ts` (a deleted skill also scrubs events; "used by" names them), `items.ts` (skills leave with the accessory kind),
  `itemChecks.ts` and `screens/items/text.ts` (accessory wording), story files (`storyTypes`, `storyDefaults`, `storyWalk`, `storySummary`, `storyChecks`, `CommandForm`,
  `CommandList`, `ConditionList`, `pickers`, `story/text.ts`), `tests/editor_api_test.py` (one import and call), `tests/editor_api_srpg_checks.py` (its "plain game" now starts
  from `economy-tactics`, since the strategy-rpg template has skills), and the template file below.
- **Template**: `editor/templates/strategy-rpg/data/skills.json` is the starter set with the template's stat ids (the contract names it); so new strategy-RPG games have the
  skills feature on with thirty skills nobody has yet (D1 gives them out).
- **Readings of the contract.** (1) The contract says the thirty use every effect except `ability`; `critAvoid` is not among them (the test says so). (2) A skill whose stat
  is unset is skipped, so with "skill" left at none, Gifted, Heavy Blow and Draining Blow are skipped (Gifted needs all four); Tough and Light Step use the game's hit points
  and movement roles. (3) Changing "When" removes what the new point cannot have instead of refusing the change. (4) The last effect of a rule may be deleted (the rule then says
  "It does nothing yet"); the last rule of a skill cannot. (5) Pickers for weapon, terrain, structure and unit conditions list the game's ids and then its tags / groups; a kind the game
  cannot fill (no weapon yet) is shown disabled. (6) "Learns at a level" shows only when levelling up is on in Rules. (7) E3's own "Only if" rows in the ability form were NOT swapped for
  this builder (abilities allow a narrower set). (8) Many unused skills are one line in the check banner ("N skills are not given to anyone yet"); a skill an event teaches counts as used.
  (9) Stat chance is written `a.<stat>` or `a.<stat> * N`; anything else shows as a custom formula (Advanced). (10) The story commands are not offered in campaign scenes.
- **Wording** (placeholders): screen "Skills"; "Add starter skills…" (installer: "Which of your stats…", "Is attack? / skill? / speed? / defense?", "None (skip skills that need it)",
  "Add N skills", "Starter skills added", "Skipped because you already have a skill with that name: …"); "What it does", "Rule N", "Add a rule", "When" (Always / At the start of its turn /
  In combat / When it strikes / When it is struck / After defeating a foe / After combat), "Only if", "Chance" (Every time / A percent chance / A stat as a percent / A custom formula (advanced)),
  "Reaches" (The unit itself / Allies within reach / Enemies within reach), "Then", "Add an effect…"; effects "Stat bonus, Growth bonus, Squares cost at most, Move through enemy units, Move again
  after acting, Experience gained, Gives a menu ability, Hit chance, Critical chance, Damage, Avoid, Critical avoid, Damage taken, Damage dealt as a percent, Damage taken as a percent, Strikes,
  Each strike repeated, Strikes first when defending, Counterattacks from any distance, The foe cannot counterattack, Heals itself from the damage, Strikes once more, Survives a deadly strike,
  Heals, Hurts, Move and act again"; conditions "Hit points, Units nearby, Weapon, Terrain, Structure, Unit type or tag, Carrying someone, The round, Who started the fight, Distance between the
  fighters, A custom formula (advanced)"; "In words"; "Not valid yet"; card "Skills" / "Learns at a level" / "Add a learned skill"; story "Teach a skill", "Make units forget a skill",
  "A unit has a skill"; accessory check "an accessory needs a bonus to at least one stat, or at least one skill."

### E3. Editor: Abilities, Promotion, Carrying — depends on E0; parallel with E1, E2, E4
The Abilities screen, the Abilities and Promotion cards, the Rules cards for carrying and promotion, the story
forms for Promote and "is promoted".

#### E3 as built

Abilities, promotion and carrying are editable; the helper needed no change (the `abilities` section and the game / unit / item
documents already carried every key). Details in docs/EDITOR_UI.md "Strategy-RPG: abilities, promotion, carrying (E3)".

- **Where**: model `abilities.ts`, `abilityConditions.ts`, `abilityText.ts`, `abilityChecks.ts`, `promotion.ts`, `promotionChecks.ts`,
  `promotionRefs.ts`, `rescue.ts`, `rescueChecks.ts`; screens `screens/abilities/*`, the E0 slots `UnitAbilitiesCard`, `UnitPromotionCard`,
  `UnitPairBonusCard`, `ItemPromotionCard`, `CarryingCard`, `PromotionRulesCard`; story `forms/PromoteForm.tsx`.
- **Shared files touched** (small): `srpgTypes.ts` (Ability, Summon types), `references.ts` / `itemRefs.ts` (one call each: promotions into a deleted
  unit type, abilities that summon it, promotion items, stat clean-up), `items.ts` (`promotes` leaves with the usable kind), `itemChecks.ts` (a promotion
  item needs no heal), `units.ts` (`deleteUnit` now keeps the clean-up of `units`: it used the old list), story files (`storyTypes`, `storyDefaults`,
  `storyWalk`, `storySummary`, `CommandForm`, `CommandList`, `TriggerForm`, `story/text.ts`), `tests/editor_api_test.py` (one import and call).
- **Readings of the contract.** (1) "The 'is promoted' condition" is the contract's trigger `{ "promoted": <filter> }` (the engine has no such
  condition): it is the trigger "A unit is promoted". (2) An ability's conditions: E2's skill builder does not exist yet, so the ability has its own
  small "Only if" rows (hit points, units nearby, unit type or tag, carrying, round; "the target" for hit points and type); any other condition
  in a file is shown as kept and can be removed. E2 may swap in its builder. (3) Switching Carrying to a mode that refuses settings
  (pair up to carry, anything to off) asks first and removes them, including every unit type's "Gives when behind". (4) A unit type's Abilities
  card shows once the game has abilities. (5) A promotion item card shows on usable items only; switching it off or changing the kind removes the
  item from promotions. (6) Deleting a unit type also deletes abilities that summon it (listed in the question). (7) Checks sit in the card they are
  about (Units: promotion, pair bonus; Items: promotion item; Rules: carrying; Abilities screen: banner), not in extra banners on the Units / Items screens.
  (8) "Its only promotion" on the Promote command leaves `to` out; the editor does not check that the unit's type has exactly one promotion (the loader's
  message shows in Problems).
- **Wording** (placeholders for the owner): screen "Abilities": effects "Push it away / Pull it back / Swap places / Move it to the other side / Heal /
  Summon a unit"; targets "Itself / An ally next to it / An enemy next to it / Any unit next to it / A free square within reach"; "Cost and limits":
  "Uses the unit's action / Free: the unit can still act", "Uses per battle", "Wait between uses", "Only before moving"; "Only if"; "The summoned unit":
  "Its level: The same as the summoner / A set level", "How many at once", "How long it stays: Until it is defeated / A number of turns",
  "Vanish when the summoner is defeated", "Can act at once", "Gives experience". Unit type cards "Abilities", "Promotion" (Becomes, From level, By item,
  On reaching the level: Nothing happens by itself / It is offered to the player / It happens automatically; Level afterwards; Stat changes: The difference
  between the two unit types / Set by hand), "Gives when behind". Item card "Promotion item". Rules cards "Carrying" (How units help each other: Off / Carry /
  Pair up; Who can carry whom: Anyone can carry anyone / The carrier's number is at least the other's / Not units with a tag / A custom rule; While carrying:
  Halved / Minus / Percent of normal / Plus; The partner behind gives) and "Promotion" (After promoting, the level: Starts again at 1 / Stays as it was).
  Story: "Promote a unit" (Which unit is promoted, It becomes, Its only promotion), trigger "A unit is promoted".
- **Tests**: vitest `abilities.test.ts`, `promotion.test.ts`, `rescue.test.ts`; `tests/editor_api_abilities_checks.py` (saves all of it and checks the loader and
  `ote-sim`); Playwright `srpg-abilities.spec.ts`, `srpg-promotion.spec.ts` (with the 360 px checks).
- **Not done / for others**: no check that a Promote command without `to` fits its unit's type; the summon of a deleted unit type is cleaned, but characters
  of a deleted unit type (E1) are not.

### E4. Editor: Supports — depends on E1; parallel with E2, E3
The Supports screen (reusing `SceneEditor`), story and scene forms for support points, condition and trigger.

#### E4 as built

The Supports screen and the story pieces about supports are editable; the helper and the engine needed no change (E0's `supports`
section and the loader's problems carry everything; `tests/editor_api_supports_checks.py` proves it). Details in docs/EDITOR_UI.md
"Supports (E4)".

- **Where**: model `supports.ts` (edits), `supportChecks.ts`, `supportStoryChecks.ts`, `supportTalk.ts`; screens `screens/supports/*`; story
  `forms/SupportForms.tsx`; `screens/campaign/SceneCommands.tsx` (the command list of `SceneEditor`, now shared).
- **Shared files touched** (small): `srpgTypes.ts` (typed `Supports`), `storyTypes.ts` / `storyDefaults.ts` / `storySummary.ts` / `storyWalk.ts` (command
  `addSupport`, condition and trigger `support`; ref kinds `character` and `pair`), `storyRefs.ts` unchanged, `storyChecks.ts`, `campaignChecks.ts`,
  `campaignRefs.ts` (a deleted character or pair is cleaned out of scenes), `srpgRefs.ts` (pair delete / retarget / usage lines; character delete also
  names the character in story clean-up), `CommandList.tsx`, `CommandForm.tsx`, `ConditionList.tsx`, `TriggerForm.tsx`, `story/text.ts`, `SceneEditor.tsx`
  (its body moved to `SceneCommands.tsx`), `tests/unit/srpg.test.ts` (pair fixtures gained `levels`), `tests/editor_api_test.py` (one import and call).
- **Readings of the contract.** (1) A pair has no id: the screen addresses it as `<a>.<b>` (`/p/<id>/supports/eirik.lyra`); a pair is "the same" in
  either order. (2) **New pair** adds the first two characters that are not a pair yet with one level of 20 points; there is no name dialog and no Duplicate.
  (3) Levels always rise: typing a value not above the level before raises it, and later levels move up with it; the last level cannot be removed; adding a
  level asks 20 more; names in `levelNames` grow with the biggest pair. (4) Defaults are never written: range 3, conversations "both", a point source of 0 / empty,
  no `start`, no `maxLevels`; an empty `rules` is removed. (5) The conversation reuses the scene editor through `SceneCommands`, offering only "Say something"; the
  speakers are all characters of the game (not the campaign's roster). (6) "Support level" condition and "Add support points" are offered in map events and in
  campaign scenes once the game has a pair (greyed "no support pairs yet" otherwise); the trigger "A support level is reached" in map events. (7) Choosing another
  character for a pair makes it a different pair and the events and scenes that named the old pair follow it; deleting a pair removes the commands, conditions
  and triggers about it (an event whose trigger or condition names it goes whole) and lists them in the question. (8) Checks: "These two never meet: no map has
  both of them" (placed or added by an event; skipped while a map cannot be read); "Level N can never be reached" when no point source is on, no story gives
  the pair positive points and the start points do not reach it; and "Conversations are only played between chapters, and this game has no campaign". The
  story checks add "X and Y are not a support pair" and a level / number range check, in map events and scenes.
- **Wording** (placeholders): screen "Supports" / "Pairs", "New pair", "No pairs yet. A pair is two characters who can grow closer."; card "Settings": "What the
  levels are called", "Ways to earn points" ("Next to each other at the end of a turn", "Fighting side by side", "One heals the other", "Off", "points"), "How close
  counts as near" ("squares"), "Most levels one character can hold" ("No limit"), "Conversations are played" ("In battle" / "Between chapters" / "Both"); pair:
  "The pair", "First character", "Second character", "Points at the start", "Level C" (or its number), "Points needed", "Add a level", "Remove", "Bonus while at
  this level" ("Hit chance", "Avoid chance", "Critical chance", "Critical avoid", "Damage dealt", "Damage taken", and the game's stats; "Add a bonus"),
  "Conversation" ("No conversation: this level is gained as soon as the points are reached."); check "These two never meet: no map has both of them."; story:
  "Add support points" (Which pair, Points to add), "A support level compares to a number" (Their support level, Level), "A support level is reached" (Which
  pair, Which level, Any level).
- **Tests**: vitest `supports.test.ts`; `tests/editor_api_supports_checks.py` (a full set loads, `ote-sim` plays it, the sticky switch); Playwright
  `srpg-supports.spec.ts` (with the 360 px check and the sticky-switch follow-up).
- **Not done**: the camp / battle player screens are the player's; no check that a level's conversation fits the pair (any character may speak); the
  `Learn` tutorial has no support lesson.

Each E step's acceptance is its vitest and Playwright tests of "Tests" and screenshots for the owner.

### D1. Demo and template content — depends on K2, P1, A1, R1, S1, AI1 (content), U1–U5 (player tests)
The section "Demo and template content", the fixtures not yet made, the sims, updating the demo's pins, CREDITS
unchanged (D1 needed no new art: the new unit types reused existing sprites; they have art of their own now, see docs/GRAPHICS.md).

#### D1 as built

`games/demo-srpg` and `editor/templates/strategy-rpg` carry the strategy-RPG parts; four more frozen fixtures exist; the
demo's pins were re-taken. Games and fixtures D1 did not touch give identical sims (demo-economy, demo-hex, hex-pointy,
hex-flat, 30 seeds each, compared line by line).

**Renames.** The unit type `pegasus` is `sky_rider` (name "Sky Rider", glyph `R`, tags `flier` and `mounted`), the item
`vulnerary` is `salve` ("Salve"). The item that was called `salve` ("Fully heals a neighbouring ally") is now
`mending_salve`, "Mending Salve" (two items cannot share an id; the contract gave no name for it). Changed everywhere: data,
maps, campaign, graphics and style files, models (`models/sky_rider.glb`, `models/detailed/sky_rider.glb`), the sound and
item docs, `tests/test_loader.cpp`, `tests/test_demos.cpp`, `tests/campaign_player_test.py`, `tools/art/*`, CREDITS (file
names only). The prose "pegasus knight" is "sky rider".

**Demo content (`games/demo-srpg`).**
- `data/characters.json`: Eirik, Kell, Lyra, Bram, Sera (company) and Gorm, Hald (bosses): the old map overrides moved in
  as personal stats, personal growths, one personal skill each (Eirik Rallying Presence, Kell Second Charge, Lyra Steady Aim,
  Bram Hang On, Sera Catch Breath, Gorm Heavy Blow, Hald Back to the Wall), Eirik also Rally, Sera also Pull Back. Eirik
  starts with the new Keen Blade, Kell with the new Runner's Charm. The maps place everyone with `"character"`.
- `data/skills.json`: the thirty starter skills (generated from one table, identical to the template's) plus Armor
  Breaker, Woodcraft, Sword Dancer. Every unit type has one or two skills and `learns` at levels 3 and 5. New items:
  `keen_blade` (sword, carries Sword Dancer), `runners_charm` (accessory, Light Step), both sold at the forts,
  `officers_writ` (promotion item, dropped by Gorm with the ring), `fangs`.
- Promotion: Captain (Lord, by the story command at the start of chapter 2, level kept), Lancer and Outrider (Cavalier,
  Officer's Writ from level 2), Marksman (Archer, offered at level 4), Brute (Fighter, automatic at level 5).
  `rules.promotion.levelAfter: "reset"`.
- `data/abilities.json`: Shove (Fighter, Knight, Brute; action), Swap (Cavalier, Lancer, Outrider; free, cooldown 2), Pull
  Back (Sera; free, 2 uses), Rally (Eirik; heals 5; action), Call Hound (new enemy unit type Houndmaster; limit 2, vanish,
  lifetime 3, **4 uses per battle**). Chapter 2's Brun is a Houndmaster now.
- `rules.rescue`: `carry`, `canCarry: "a.str >= b.def and not b.tags.mounted"`, carrier Skill and Speed halved.
- `data/supports.json`: four pairs, two levels each, `levelNames` C/B/A, `maxLevels` 3, all three point sources on
  (5 / 3 / 4), `conversations: "both"`; Eirik and Kell start at 10 points. Chapter 1's after scene adds 15 points for Eirik
  and Sera when she joined; chapter 2 has a `support` trigger event (Eirik and Kell).
- Graphics: the new unit types first drew as the type they grow from (`art_units.py` `ALIASES`); they now have art of
  their own in every style (docs/GRAPHICS.md "Unit pictures and models of the story demo"), and `ALIASES` is gone.
  Portraits follow map units with a `character`. `make_starter_art.py` was re-run (only `games/demo-srpg` and the template changed).

**Template (`editor/templates/strategy-rpg`).** `data/skills.json` is the thirty starter skills, exactly as the table of
section 3 (stats `str`, `skl`, `spd`, `def`; `hp` and `mov` for Tough and Light Step) with a description each; Lord has
Opening Blow, Archer Steady Aim. Characters Eirik, Kell, Gorm, Vex and a new Bram (a Fighter, so Shove has a bearer; the
template had none) plus a plain red soldier to keep the sides even. Pair up with `pairBonus` Strength +1, Defense +1.
`rules.items` (fixed weapons, three slots) and `data/items.json` with the Officer's Writ: Kell carries it and a Cavalier
becomes an Outrider with it. `abilities.json` holds only Shove. One support pair, Eirik and Kell, one level.

**Fixtures.** `srpg-carry`, `srpg-pair`, `srpg-supports` (a two-chapter campaign), `srpg-hex` (pointy; abilities in six
directions), `tests/test_srpg_fixtures.cpp`; `plain-srpg` is a frozen copy of the pre-D1 demo (data only) for the editor's
unit tests, like `plain-economy`.

**Interpretations.** (1) "not units tagged mounted" in `canCarry` is about the unit being carried (`b`). (2) Captain's
promotion entry says `levelAfter: "keep"` so Eirik does not drop to level 1 at the start of chapter 2. (3) A game with skills
gets a "Unit" entry second in the battle menu (U1), so menu scripts of older tests moved one down. (4) Knights have On My Own
and Hald Back to the Wall, not defensive skills: with Hold the Line and a fort the computer's lone leader could not hurt
Hald and nobody could finish the battle. (5) Call Hound has 4 uses: without a limit a summoner and a lone leader that will
not walk into a crowd made 10 of 30 sims of chapter 2 stall (a hound is killed every turn for ever). (6) Marksman and Brute
are reachable in a long game only (level 4 and 5; the campaign reaches about level 4).

**Pins re-taken, and why** (the demo's data changed on purpose): `tests/test_srpg_compat.cpp` the demo-srpg golden
battles (5 lines), the demo-srpg data hash (`424b5fd70a0bf539` to `97b4001954d2bcf9`), its campaign hash (`e4f411e0ca888510`
to `3453e134072a2766`) and its campaign save hashes (`4899cc10dbbb4724` to `751f8eb40ef07113`, `71019abb4e87edfc` to
`1b37c1ef7391fe0d`); `tests/test_srpg_golden_fights.cpp` the six demo-srpg digests moved to their own table (the demo has
modifier sources now, so it cannot be "as before the pipeline"); `tests/test_loader.cpp` (14 unit types, 6 weapons, sky rider
has 2 tags), `tests/test_demos.cpp` (Eirik carries 3 items, Gorm drops 2), `tests/test_srpg_skeleton.cpp` (stat equality
adds the skills' standing bonuses), `tests/test_save_damage.cpp` and `tests/test_views.cpp` (their demo copies take the new
data files). All other pins are untouched. Player tests: `campaign_player_test.py`, `srpg_player_supports_test.py` (copies of
the demo drop its characters / supports), `sound_player_test.py` (autoplay seed 2 to 4), `tools/webtest/suites/saves.mjs`
(menu index). Editor tests: `tests/editor_api_*` (14 demo unit types; the template now has the new files, so the
"nothing there" checks use the blank template; the E3 test keeps the template's own writ), vitest `fixtures.ts`
(`loadDemo('demo-srpg')` reads `plain-srpg`; `loadLiveDemoSrpg` the real one) and `demoContent.test.ts`.

**Sims** (30 seeds unless noted; before to after). bridge: Blue 17 / Red 13, 6.9 rounds to Blue 19 / Red 11, 6.0; north_road:
Blue 19 / Red 11 to Blue 21 / Red 9, 12.0 rounds; campaign (20): 5 finished to 19; template ford (20): Blue 18 / Red 2 to Blue
18 / Red 2 (28 of 30 for 30 seeds); the four new fixtures: 20 of 20 each. No stalls anywhere. `tests/srpg_content_test.py`
(ctest `srpg_content`) runs these, checks determinism and that the event printer names promotion, summons, skills and
supports.

**Names invented here.** Items: Mending Salve, Keen Blade, Runner's Charm, Fangs (Officer's Writ is the contract's). Unit
types: Houndmaster, Hound (contract); ids `sky_rider`, `mending_salve`, `keen_blade`, `runners_charm`, `fangs`,
`officers_writ`, weapon group `fang`; support level names C, B, A; every character description, every support conversation
and the lines "The company has chosen: you lead us as Captain now, Eirik." and "Whatever the pass throws at us, I ride at your
side."

**Not done.** (Since then: the seven new types have their own art in every style, docs/GRAPHICS.md, and the promotion
screen of a model style borrows the picture of a sprite style.) Playtest of Rally and of Brute's automatic promotion was not reached by input scripts.

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

#### T1 as built

The lesson "Characters and skills" (`editor/web/src/learn/lessons/characters.ts`, helpers `learn/helpers/characters.ts`, unit test
`tests/unit/learnCharacters.test.ts`, e2e `tests/e2e/learn-characters.spec.ts`, tagged `@smoke`; text in docs/EDITOR_UI.md "Learn" and
docs/TUTORIAL.md) and the document pointers.

- **Starting point: the blank template**, like the Items lesson. It has no characters or skills files, one class (Soldier), the stats HP and
  Move, levelling up off and one map with a blue and a red soldier, so every step is real work. The strategy-rpg template ships all of it
  (D1), which would leave nothing to do. Cost: with only HP and Move the starter installer skips the skills that need attack, skill, speed or
  defense (it lists them); the ones the lesson uses (Tough, Light Step) need only HP and Move.
- **Ten steps**, each ticking from the game's data (or the Play signal) and never from a click: switch on characters (Rules);
  make a character and give it a personal bonus (a number other than 0, after a typing pause); put it on the map from the Characters tab
  (a plain soldier from the Classes tab does not count); switch on skills; add the starter skills; give the character a skill; switch on
  levelling up; give it a skill learned at a level; make a skill of your own (a new skill is valid at once, so the step needs its rule changed
  from the one a new skill starts with, and valid); Play (the player reported ready; opening the unit's details is in the text but cannot be seen
  from the editor).
- **Interpretations.** (1) "Learned at a level" needs levelling up, which the blank template lacks, so it is its own step (the arc had nine).
  (2) A character's class is not a step of its own: the Soldier is the only one. (3) The own-skill step needs a changed rule, not just a new skill.
  (4) No "check for problems" step (the lesson is about the screens it uses); Play is the last step.
- **Shared files touched (small `data-learn` hooks, no behaviour change):** `ui/Toggle.tsx` (it took a `learn` prop and ignored it; now it sets
  `data-learn`), `FeatureSwitchesCard`, `FeaturesCard` (progression switch), `UnitPicker` (Characters tab), `CharacterStatsCard` (first personal
  box), `SkillsCard` (add box, learns block), `SkillsScreen` (starter button), `SkillForm` / `BlockCard` (the When box of the first rule, only for a
  skill that is not in the starter set, so the ring never points at Tough while the maker should edit their own). Helper C++, templates, engine
  and games: untouched.
- **Known limit (as in the Story lesson):** on a phone the map editor's Characters tab sits in the options sheet under the sticky canvas and the
  lesson sheet; folding the lesson ("Hide lesson") shows it.
- **Document sync.** One paragraph each, pointing here and repeating nothing, in docs/GAME_FORMAT.md ("Strategy-RPG data (optional)"),
  docs/SAVES.md ("Strategy-RPG state"), docs/EVENTS.md ("Strategy-RPG additions"), docs/ITEMS.md ("Strategy-RPG additions") and
  docs/ARCHITECTURE.md ("Strategy-RPG features"). docs/EDITOR_UI.md already had a section per step; it gained the lesson's bullet in "Learn".
  docs/TUTORIAL.md lists the lesson as number 9.

---

## Preparation and the roster (docs/EVENTS.md "Preparation")

The campaign roster may now hold more characters than a map deploys: a map with a `"prep"` block lets the player choose who fights, swap
starting squares, trade items between any roster characters and the shared storage, and buy from a preparation shop. Benched
characters keep everything and join the next chapter; rescue, supports and skills are untouched (a benched unit is simply not on the
board). Inventory-full handling in the screen: a purchase for a full character goes to the storage when the game has one, else it is
refused.

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
