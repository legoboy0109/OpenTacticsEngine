# Difficulties

Each game's maker writes its own list of difficulties and says which one is the default. A difficulty is a full toolkit:
enemy level and stat shifts (for the whole game, and per map), units that only appear on some difficulties, experience and
resource multipliers, and a value that story events and formulas can read so makers can vary anything else.

A game **without** `"difficulties"` in `game.json` has no difficulty anywhere: no choice screen, no menu row, no extra key in
any save, and every battle is built exactly as before (the demos have none).

The difficulty is chosen when a campaign starts (New Game) or a free battle begins. Each game says whether it can be changed
later: `never`, `lowerOnly` (the default) or `free`.

Contents: 1 the data, 2 who counts as an enemy and how it is built, 3 a map's own numbers, 4 units on some difficulties only,
5 experience and resources, 6 what the story and formulas read, 7 state and saves, 8 the player, 9 `ote-sim`, 10 the editor,
11 refusals, 12 worked examples.

---

## 1. The data

```jsonc
// game.json
"difficulties": {
  "default": "normal",             // required: the id chosen unless the player picks (preselected on the choice screen)
  "change": "lowerOnly",           // optional: "never" | "lowerOnly" (default) | "free" - can it be changed after the start?
  "percentStats": ["hp", "str"],   // optional: the stats "statsPercent" scales; missing = every stat except the movement stat
  "list": [                        // required, 1 to 12 entries, EASIEST FIRST: the order is what "lower" and "at least" mean
    { "id": "easy",   "name": "Easy",   "description": "Gentler foes for a relaxed story.",
      "enemy": { "levels": -1 }, "xpPercent": 150 },
    { "id": "normal", "name": "Normal", "description": "The game as its maker meant it." },
    { "id": "hard",   "name": "Hard",   "description": "Foes two levels stronger, and a little tougher.",
      "enemy": { "levels": 2, "stats": { "def": 1 }, "statsPercent": 110 }, "xpPercent": 80, "resourcePercent": 80 } ]
}
```

| Key of an entry | Meaning | Default |
|---|---|---|
| `id` | letters, digits, `_` and `-`; unique; what saves, maps, events and `--difficulty` name | required |
| `name` | what the player reads | the id |
| `description` | one or two lines on the choice screen | none |
| `enemy.levels` | enemies are built as if placed this many levels higher (negative: lower); -98 to 98 | 0 |
| `enemy.stats` | per stat id: a flat amount added after the levels; -999 to 999 | none |
| `enemy.statsPercent` | after the added amounts, the stats of `percentStats` are scaled by this percent; 0 to 1000 | 100 |
| `xpPercent` | experience gained by units of human sides, percent; 0 to 1000 | 100 |
| `resourcePercent` | resources gained by human sides, percent; 0 to 1000 | 100 |

`name`, `description` and the other cosmetic keys are not part of a save's data hash: `"difficulties"` is **not hashed at all**,
so changing a difficulty's numbers or deleting one never makes old saves "from a different version" (a vanished id falls back to
the default, section 7). Map files are hashed whole as always, so a map's own `"difficulty"` and a placement's `"difficulty"`
count like any other map data.

## 2. Who counts as an enemy, and how an enemy is built

**An enemy** is a unit of a computer-controlled side (`"controller": "ai"`) of a team other than that of at least one human
side in the battle, unless the map side says `"difficultyShift": false`. Allies (a computer side on a human side's team), a
battle between computer sides only, and units of human sides are never touched. Units a summon creates, units a structure builds
and units that join through `changeSide` are not shifted either (they are not "placed").

How a placed enemy is built (map units and `addUnit`; `Battle::makeUnit`, `core/battle/difficulty.cpp`):

1. **Level.** The unit is built at `level + enemy.levels`, never below 1 and never above the game's top level
   (`rules.progression.maxLevel`); a unit already placed above it stays where it is. Skills a unit type or character *learns at
   a level* are learned for the level it is built at (docs/SRPG.md "Learning").
2. **Stats from the levels.** The engine does **not** derive a placed unit's stats from its `level` (the level is a label and the
   map's stats are the unit's stats: docs/SRPG.md section 1), so there is no existing "auto-level" rule to reuse. A shift is
   therefore defined as *that many level-ups at the average growth*: for each stat, `growth% x levels shifted`, **rounded to the
   nearest whole number** (halves away from zero), added (or taken off for a negative shift). `growth` is
   `Battle::growthOf` (the unit type's, plus its character's, plus standing skills' bonuses) - the same number a real level-up uses.
   **No dice**: the same game always builds the same unit, and the battle's random numbers are not touched (so seeds and replays
   are unchanged). A stat cap (docs/SRPG.md section 7) cuts a rise as it does at a level-up.
   Example: a soldier with growths 70 / 40 placed at level 2 on `+2`: level 4, `hp +1` (1.4), `str +1` (0.8).
3. **Flat additions** (`enemy.stats`), then 4. **the percent** (`enemy.statsPercent`) of the stats of `percentStats` (default:
   every stat except the movement stat; a percent applied to Move would make units cross the map or not move). A percent is
   rounded to the nearest whole number. A stat never ends below 0 and hit points never below 1.
5. The unit then starts at full hit points.

**Never shifted:**
- a stat the placement wrote itself (`"stats": { "atk": 20 }` on a map unit or `addUnit`) is **exact**: that stat is skipped by
  every step above (the others of the same unit are shifted);
- a placement with `"difficultyShift": false` (a boss) is not shifted at all - level, flat and percent;
- a side with `"difficultyShift": false` (a map side key) - none of its units are.

**Promoted units.** A promoted unit is built with the growths of its *current* type (the promotion reset the level in the data
already: docs/SRPG.md section 2); nothing simulates the unit's history.

**Hexagon games** behave identically (`tests` cover `games/demo-hex`).

## 3. A map's own numbers

A map can change what a difficulty does on that map only:

```jsonc
// maps/ch5.json
"difficulty": { "hard": { "enemy": { "levels": 3 }, "xpPercent": 100 } }
```

The keys are difficulty ids; each holds the same `enemy`, `xpPercent` and `resourcePercent` as an entry of the list. A number
named here **replaces** the game's number for this map (it does not add to it): `levels: 3` on Hard means Hard is `+3` here, not
`+2 +3`. What the map does not name stays the game's. For `enemy.stats` the replacement is per stat. A `0` or `100` named on a
map is a real choice ("no shift on this map"). A difficulty the map does not name uses the game's numbers unchanged.

## 4. Units on some difficulties only

```jsonc
{ "id": "ogre", "type": "brute", "side": "red", "x": 9, "y": 4, "difficulty": ["hard"] }
{ "addUnit": { "type": "brute", "side": "red", "x": 9, "y": 4, "difficulty": ["normal", "hard"] } }
```

A placement (map unit or story `addUnit`) with `"difficulty": [ids]` appears **only** on those difficulties; without the key
it appears on all. The list may not be empty, and may only name ids of the game's list. **Units of computer sides only**: a unit
of a human side belongs to the party (roster, preparation screen), so the loader refuses the key there. Structures cannot be
limited (only units; a structure could always be given by a story event that reads the condition of section 6).

`"difficultyShift": false` (section 2) is valid on the same two places.

**Variants: one unit, different numbers per difficulty.** Two placements conflict only if they can exist in the same battle,
that is, if their difficulty sets **intersect** (a placement without `"difficulty"` is on every difficulty). So placements whose
lists are **disjoint** may stand on the same square and share an `id` (or a `character`); they are variants of one unit and
exactly one of them is built per battle. This holds for map units among themselves, for `addUnit` ids against map units' ids and
against other `addUnit`s (the loader checks those statically), and for a character placed more than once. What still refuses:
two placements whose lists share a difficulty (`shares a cell with units[1] on difficulty "normal"`, `"id" "gorm" is already used
by units[1] on difficulty "normal"`, `"character" "lyra" is already placed by units[0] on difficulty "hard"`; an `addUnit`:
`"id" "gorm" is already used by another unit of the map on difficulty "hard"`), and a variant next to an unrestricted unit (the
unrestricted one exists on every difficulty, so the message names the variant's first difficulty). Two unrestricted placements
keep the plain messages (`shares a cell with units[0]`) with no difficulty named. Preparation slots, "fixed" and "forbidden" lists
only concern units of human sides, which cannot be limited, so they are unaffected; a slot on a square where a computer variant
exists on some difficulty is refused as before.

**Referring to a shared id** (events, objectives, behaviours, talk, quotes, `goesFor`, AI `seek`, `setBehaviour`, filters...): the
id means the one variant that exists in the battle. The loader accepts a reference if **any** placement with that id exists
(map unit or `addUnit`); it does not try to prove the id exists on every difficulty the reference can run on. On a difficulty where
no variant exists the reference behaves as it always did for a unit that is not in the battle (docs/EVENTS.md, docs/AI.md): a
`defeated` trigger never fires, a `defeated` objective is not met (it is true only once something matching existed and none is
alive), a command aimed at it does nothing, a `seek` unit plays as `advance`. So give every difficulty a variant of any unit that a
win objective names, or make the objective itself depend on the difficulty with events.

**Building.** Units are built in placement order among those that exist on the battle's difficulty, so the unit order, ids in
saves and everything that follows from it are those of the built battle; a save made on one difficulty loads exactly as before (the
save holds the built units, not the placements).

## 5. Experience and resources

- **`xpPercent`** scales every experience award to a unit of a human side (experience from fights, staves and the like - all of
  it goes through `awardXp`; weapon-rank experience is not scaled): `amount x percent / 100`, rounded to the nearest, and at least 1 while the percent is above 0 and the
  award was above 0. Computer units earn as the data says.
- **`resourcePercent`** scales what a human side **gains**: the per-turn income of owned structures (docs/ECONOMY.md), the
  story command `giveResource` (in a battle and in a campaign scene; only amounts above 0), and resource loot of chests and
  places (docs/SRPG.md section 10). Costs, prices, a negative `giveResource` and what a computer side gets are never scaled. The
  map's own `resourcePercent` (section 3) applies to a battle's gains; a scene has no map, so it uses the game's.

## 6. What the story and formulas read

**Condition** (map events, `if` commands, dialogue-answer conditions, and campaign scenes):

```jsonc
{ "difficulty": "hard" }                       // is exactly Hard
{ "difficulty": { "atLeast": "normal" } }      // Normal or harder (by list position)
{ "difficulty": { "atMost": "normal" } }       // Normal or easier
```

In a battle it reads the difficulty the battle was built with; in a scene, the playthrough's. A game without difficulties
refuses the condition.

**Formulas** (Lua; damage, hit, xp, rule expressions - everything that receives `a`, `b`, `turn`...): two more inputs,
`difficulty` (a number: the position in the list, 0 = easiest; always 0 in a game without difficulties or outside a battle) and
`difficultyId` (text, `""` without). Example: `"damage": "a.str - b.def + (difficultyId == 'hard' and 1 or 0)"`.

## 7. State and saves

- A **campaign playthrough** stores its difficulty (`Playthrough::difficulty`, chosen at New Game; the default when nothing was
  chosen). A **battle** stores the difficulty it was built with (free battles too). Both are written **only in a game that has
  difficulties**: `"difficulty": "<id>"` in the battle state and in the campaign save's `"playthrough"`. Saves of games without
  them are byte for byte what they were.
- Loading a save whose difficulty id no longer exists falls back to the game's default, with a note in the log. A value that is
  not text is a damaged save (refused with the field's path). A save made before the game had difficulties loads on the default.
- Changing the difficulty in a campaign (when the rule allows it) edits the playthrough: it takes effect from the **next battle
  built** (the next chapter, a retry after a defeat). A battle in progress, and its save, keep the difficulty they were built with.
- `lowerOnly` compares list positions: from `hard` the player may go to `normal` or `easy`, never up. `free` allows any other.
  `never` allows none. (Core: `DifficultyRules::mayChange`, `setPlaythroughDifficulty` in `core/campaign/difficulty.h`.)
- A story `addUnit` run later in a battle uses the difficulty the battle holds, so it follows a restored save.

## 8. The player

- **New Game** (a campaign) and a **free battle** start show the choice when the game has more than one difficulty: one row
  per difficulty (easiest first, the default preselected and tagged), the selected one's description and what it changes
  ("Enemies: +2 levels, +1 Defense. Experience 80%."). Arrows and Confirm, a tap (it picks), Cancel goes back to the game's page.
  It lays out for a phone-shaped screen (270 x 480) as well as 480 x 270. `--autoplay` takes the default without asking.
- **The game page** (the page of a game with a campaign and a Continue save) has a **Difficulty** row, between Load and Game options, showing the difficulty the
  Continue save's playthrough is on. It follows the **same rule and wording as the battle menu's row** (`engine/player/difficulty_rows.*` is the one place for both):
  greyed with `(fixed)` for `never`, `(lowest)` for `lowerOnly` on the easiest (choosing it explains why); otherwise it opens the list of what the rule allows
  and writes the pick into the Continue save's `playthrough.difficulty` (**only that field**; a battle in the save keeps its own, and the row then says
  `(next battle)`). The next battle built (the next chapter, a retry) uses it. The save's detail line (the game page's Continue row and the save list) ends with the
  difficulty's name in a game that has difficulties (`The Ford - Turn 1 - Hard`); other games show nothing new. Log: `game page: difficulty row "Hard"` /
  `game page: difficulty menu offers easy, normal` / `campaign: difficulty changed to "easy" (applies from the next battle)`. Not offered without a Continue save
  (New Game has its own choice screen) or in a game without difficulties. Tests: `tests/music_player_test.py` (section "game page").
- **The battle menu** (the End Turn menu, also the pause menu) has a **Difficulty** row in a game that has difficulties. Its
  right-hand column names the current difficulty. When the game's rule does not allow a change the row is **shown greyed with the
  reason** next to the name (`(fixed)` for `never`, `(lowest)` for `lowerOnly` on the easiest, `(this battle)` in a free battle,
  where there is nothing to change later) rather than hidden, so the player sees it exists and why it cannot be used. Otherwise it
  opens a list of the difficulties the rule allows; choosing one says "from the next battle" and writes the Continue save.
- **`--difficulty ID`** (command line; `embed.html?...&difficulty=ID` in the editor's Play panel) names the difficulty of a new
  campaign or a free battle: no choice screen. It beats the default; an unknown id, or any id in a game without difficulties, is
  a clear error ("no such difficulty (this game has: easy, normal, hard)") and nothing starts. A loaded save keeps its own.
- Log: every battle start logs `battle: difficulty "hard" (enemy levels +2, xp 80%, resources 80%)`; the choice screen and the
  menu log what they offer and what is picked (tests read these).
- New wording (placeholder like all wording): `engine/player/scene_difficulty.cpp` (title, "default", the effects line),
  `engine/player/battle/battle_difficulty.cpp` ("Difficulty", "(fixed)", "(lowest)", "(this battle)", "(next battle)",
  "Difficulty: X - from the next battle"), `engine/player/app_difficulty.cpp` (the `--difficulty` errors).

## 9. `ote-sim`

`ote-sim <game> <map> --difficulty ID` and `ote-sim <game> --campaign --difficulty ID` play at one difficulty (the default
without it). An unknown id is an error with exit code 1. Without the option nothing changes: the demos' sims are identical to
before.

## 10. The editor

See docs/EDITOR_UI.md "Difficulties": the **Difficulty** card on the Rules screen (the list, default, change rule, each entry's
numbers with plain-language help, add / rename / reorder / delete with follow-through), a map's **Difficulty on this map** card
on the Map tab, **Only on** checkboxes and the keep-the-same tick on a placed unit and on the Add a unit command, the side's
**Difficulty changes this side** tick, the story condition form, the checks, and the Play panel's difficulty choice.

## 10b. Test level (a maker's testing aid; shares the rule of section 2)

`ote-player --test-level auto|N` (the web player's `testlevel=auto|N`, the editor's Play panel, and `ote-sim --test-level`) lifts the
player's units to a level **for this run only**, so a late map can be tested without playing the earlier ones. Without the option nothing
changes: sims, saves and behaviour are identical (the battle's random numbers are never used either way).

- **Who.** Every unit of a **human-controlled side** when the battle is built (free battle: when it is built; the map's placements, named
  or not: plain human units are lifted too, because they are the player's units on that map). Units a story adds later (`addUnit`,
  `changeSide`, summons, structures) are not lifted, except that a **campaign roster** is lifted as a whole (below). Computer sides are
  never touched.
- **How.** A unit below the level is raised **to** it by "that many level-ups at the average growth", the exact mechanism of section 2
  (`Battle::growStatByLevels`: `growth% x levels`, rounded to the nearest, no dice, through the stat caps; `Battle::levelUnitTo`). It stops
  at the unit type's `maxLevel` (else the game's top level). It is **not promoted**: the unit stays in its class (the normal rule still
  applies when a turn starts: a class whose automatic promotion level the unit now meets promotes it then, an offered promotion is offered,
  exactly as for any unit placed at that level). The skills the new levels teach are learned. Experience becomes 0 and hit points full.
  A unit already at or above the level is untouched (nothing is lowered, its experience stays).
- **`auto`.** The rounded (halves up) average level of the units **on the map when the battle is built** that belong to computer-controlled
  sides on another team than the **first** human side (difficulty shifts already applied, units that appear only on other difficulties
  absent), **plus 2** (`Battle::kAutoTestLevelBonus`: a player fields their best units, which sit above the average; the usual caps still
  stop the climb). With `rules.promotion.testAt` the average is of the units' effective levels (below). No such unit: no change (logged).
  An explicit number is not touched by the bonus.
- **Weapon ranks.** A unit raised by L levels gains, in every weapon group it **can wield** (its class's or character's `wields`, or a
  weapon its class lists; staves and any other ranked group included), `perLevel x L` weapon experience, with
  `perLevel = ceil(top rank xp / (2 x the game's top level))`, at least 1: a unit that climbs the whole level range twice (before and after a
  promotion) just reaches the top rank. The engine cannot tell a group "at the lowest rank" from one nobody gave a value, so every wieldable
  group counts; a group the unit cannot wield gets nothing. The top rank's xp is the cap (there are no per-class rank caps); no events, no
  dice; nothing in a game without `data/ranks.json`. Only the levels actually gained count, so raising the same roster unit again in a later
  chapter adds only the new levels (`Battle::raiseTestRanks`).
- **Promotion (`rules.promotion.testAt`, optional; missing = the test level never promotes).** With `testAt` the test level works in
  **effective levels**: a unit that can still promote counts its level, a unit that already is promoted counts level + `testAt`. "Can
  promote" = its type has a promotion entry a player could trigger (`items`, or `atLevel` `"offer"` / `"auto"`; story-only entries with
  neither are ignored, wherever they point); "is promoted" = its type is the `to` of such an entry of some type, or carries the tag named by
  the optional `rules.promotion.testPromotedTag` (for promoted classes nothing in the data promotes into: enemy-only or pre-promoted
  ones; the key needs `testAt`) (a type that is both counts as
  promoted: one promotion per unit, never a third tier). `auto` averages the hostile units' effective levels (+ 2); an explicit number is an
  effective level too (with `testAt` 15, `--test-level 20` means level 5 in the new class for a unit that can promote). Raising a unit that can
  promote to effective level E: below `testAt` it simply climbs to E; from `testAt` up it climbs to `testAt` in its own class (so that class's
  skills up to there are learned), takes **the first triggerable entry** with the engine's own promotion (`Battle::changeType`, the core of
  `promote`: stat gains, hit points, level rule `levelAfter`, skills, ranks raised to the new class), then climbs on: level `1 + (E - testAt)`
  after a reset, or E when the promotion keeps the level, within the new type's level cap. If the unit could not stand on its square as the new
  type it stays what it is at `testAt`. A promoted unit is raised to `E - testAt`. Nobody is lowered, and the weapon-rank rule above applies on
  both sides of the promotion. A character's own promotion level does not matter: `testAt` decides. No events or animations.
- **Campaign (`--campaign`, docs/SAVES.md "Test playthroughs").** Before each chapter's battle is built the **whole roster** (every party,
  the benched too) is raised the same way and stays raised in the playthrough, so reserves and later chapters keep it. A number is applied
  at every chapter (upward only: a unit that is already higher keeps its level). `auto` is **re-evaluated at each chapter start from that
  chapter's map, upward only**: the roster never goes down.
- **Telling you.** The log says `[info] test level: auto aims at enemy average 7 + 2 = 9`, then `[info] test level: 9 (auto) (3 units raised)`
  (or `test level: 7 (3 units raised)`; the web player reads the number off this line) and one line per unit, e.g.
  `test level: Kell Lv 2 -> 9, sword E -> C` or `test level: Kell Lv 4 -> 15, promoted to Hero, Lv 3, sword D -> B` (the rank names are the
  game's own; the roster ones say `(roster)`); the battle menu has a greyed row **Test level**, "9 (auto, +2)" (new wording in
  `engine/player/battle/battle_difficulty.cpp`; `ote-sim --test-level auto|N` also works for a map and for `--campaign`). Nothing new is saved: the
  raised stats are ordinary unit state, so a save made in such a run loads the same.
- Code: `core/battle/test_level.cpp`, `core/campaign/testplay.{h,cpp}`. Tests: `tests/test_testplay.cpp`, `tests/test_testplay_campaign.cpp`,
  `tests/testplay_player_test.py`.

## 11. Refusals

All start with the file and the place: `game.json: difficulties: ...`, `maps/x.json: ...`.

`"list" needs at least one difficulty`; `"list" has more than 12`; `"default" names unknown difficulty "x"` (and a missing
`"default"`); `"change" must be "never", "lowerOnly" or "free"`; `"id" must be letters, digits, "_" and "-"`; `duplicate id`;
`"name" must not be empty`; `"levels" must be from -98 to 98`; `"xpPercent" must be from 0 to 1000 (percent)`;
`"stats" names unknown stat "x"`; `"percentStats" names unknown stat "x"`; unknown keys (`unknown key "x"`).
On a map: `difficulty "x": unknown difficulty "x"`, the same unknown-key and range refusals, and `the map names a difficulty but
the game has none`. On a unit: `"difficulty" names unknown difficulty "x"`, `"difficulty" must list at least one difficulty`,
`"difficulty" is for units of computer sides`, `"difficulty" is given but the game has no difficulties`; two placements that can coexist and share a square or an id are
refused naming the shared difficulty (section 4). A condition on an unknown
difficulty, or in a game without any: `"difficulty" names unknown difficulty "x"` / `... the game has no difficulties`.

## 12. Worked examples

### Easy / Normal / Hard

```jsonc
"difficulties": {
  "default": "normal",
  "change": "lowerOnly",
  "list": [
    { "id": "easy",   "name": "Easy",   "description": "Foes a little weaker; experience comes faster.",
      "enemy": { "levels": -2 }, "xpPercent": 130, "resourcePercent": 120 },
    { "id": "normal", "name": "Normal", "description": "The game as it was designed." },
    { "id": "hard",   "name": "Hard",   "description": "Stronger foes, a harder bite, and leaner pockets.",
      "enemy": { "levels": 2, "stats": { "def": 1 }, "statsPercent": 105 }, "xpPercent": 85, "resourcePercent": 85 } ]
}
```

On Hard every enemy soldier placed at level 3 is built at level 5 with two levels of average growth, `+1 Defense`, and then 105%
of its non-movement stats. Its boss, with `"difficultyShift": false`, is as drawn on the map. A reinforcement
`"difficulty": ["hard"]` joins only on Hard. A story event can say `{ "difficulty": { "atLeast": "normal" } }` to give a
different line.

### "Hard adds 3 enemy levels from chapter 5"

Keep Hard at `+2` in `game.json` and, in `maps/chapter5.json` (and the later chapters' maps), add:

```jsonc
"difficulty": { "hard": { "enemy": { "levels": 3 } } }
```

Hard is `+3` on those maps (a replacement, not `+2 +3`), `+2` on every other map, and every other number of Hard (added stats,
percent, experience, resources) is unchanged because the map did not name them. The editor's "Difficulty on this map" card writes
exactly this.

### One unit, different numbers per difficulty

The same enemy on one square, with its own stats on each difficulty: three placements, one `id`, each limited to one difficulty.
The `"stats"` written on a placement are exact (section 2), so each variant is built with exactly the numbers written.

```jsonc
"units": [
  { "id": "gorm", "type": "brute", "side": "red", "x": 9, "y": 4, "difficulty": ["easy"],   "stats": { "hp": 18, "str": 6 } },
  { "id": "gorm", "type": "brute", "side": "red", "x": 9, "y": 4, "difficulty": ["normal"], "stats": { "hp": 24, "str": 8 } },
  { "id": "gorm", "type": "brute", "side": "red", "x": 9, "y": 4, "difficulty": ["hard"],   "stats": { "hp": 32, "str": 10 } } ],
"objectives": { "win": [ { "defeated": { "unit": "gorm" } } ] },
"events": [ { "id": "gorm-falls", "when": { "defeated": { "unit": "gorm" } }, "do": [ { "say": "Gorm falls." } ] } ]
```

Whichever difficulty is played, `gorm` is the one variant that exists there, and the objective and the event refer to it by that one
id. A fourth placement for `["hard"]` would be refused (`on difficulty "hard"`), as would one without `"difficulty"`. In the editor:
select the unit, press **Add a variant for another difficulty**, then change the copy's numbers.

### Reading the difficulty in a formula

```jsonc
"formulas": { "hit": "90 - (difficulty > 1 and 5 or 0)" }   // 5 points less to hit from the third difficulty up
```

---

Tests: `tests/test_difficulty_variants.cpp` (variants), `tests/test_difficulty_rules.cpp` (the shift, exemptions, multipliers, Lua, hex), `test_difficulty_data.cpp` (loader and
refusals, the arena at each difficulty, conditions, map override), `test_difficulty_save.cpp` (saves, fallback, rules,
campaign), `tests/difficulty_player_test.py` (the real player), `tests/editor_api_difficulty_checks.py`,
`editor/web/tests/unit/difficulty.test.ts`, `editor/web/tests/e2e/difficulty.spec.ts`.
