# Jobs

An optional job system in the spirit of job-based tactics RPGs. A character can **change between jobs**, earns **job points (JP)** in the job it is using, spends them to **learn that job's
skills and abilities**, and can **equip** a limited number of learned entries from its other jobs. A game without `data/jobs.json` has none of this and loads, plays, saves and simulates
exactly as before. All wording on screen is placeholder.

Built: the data (`data/jobs.json`, `rules.jobs`, the job keys of characters and map units), the rules (job levels, unlocking, learning, equipping), the battle (the unit plays as its job,
JP is earned), the camp and the preparation screen, the unit details, saves, and the computer opponent (it uses what its units know; nothing new to decide). Not built: see "Left out".
Code: `core/jobs/` (`job_rules.*` the rules over a job state, `job_camp.*` the roster operations, `job_battle.cpp` the battle side), `core/data/jobs.h` and `loader_jobs.cpp` (data and loader),
`core/battle/save_jobs.cpp` (saves), `player/jobs/` and `player/campaign/jobs_scene.*` (the screens). Tests: `tests/test_jobs_data.cpp`, `test_jobs_battle.cpp`, `test_jobs_camp.cpp`,
`test_jobs_text.cpp` (fixture `tests/fixtures/jobs`, helpers `tests/jobs_fixture.h`), `tests/jobs_player_test.py` (the real player, headless), `tests/editor_api_jobs_checks.py`. Overlay jobs and the combination with levels, promotion and merit ranks: `tests/test_combo_ranks_jobs.cpp` (fixture `tests/fixtures/combo`, `tests/combo_fixture.h`), `tests/combo_player_test.py`, `tests/editor_api_merit_checks.py`.

## The data

### `data/jobs.json`

```json
{ "jobs": [
  { "id": "squire", "name": "Squire", "description": "A plain soldier; everyone starts here.", "unitType": "squire",
    "learn": [ { "skill": "guard", "cost": 30, "slot": "support" },
               { "ability": "shove", "cost": 40, "slot": "action" },
               { "skill": "light_step", "cost": 25, "slot": "movement" } ] },
  { "id": "knight", "name": "Knight", "unitType": "knight",
    "requires": [ { "job": "squire", "level": 2 } ],
    "learn": [ { "skill": "iron_will", "cost": 60, "slot": "reaction" } ] },
  { "id": "mage", "name": "Mage", "unitType": "mage",
    "requires": [ { "job": "squire", "level": 2 } ],
    "jpLevels": [30, 90],
    "learn": [ { "ability": "mend", "cost": 50, "slot": "action" }, { "skill": "focus", "cost": 40, "slot": "support" } ] } ] }
```

| key | meaning |
|---|---|
| `id` | letters, digits, `_`, `-`; unique. |
| `name` | 1 to 24 characters; default the id. `description` is optional text. |
| `unitType` | optional. The unit type (`data/units.json`) a unit plays as while it has this job: its stats, movement type, weapons, growths (a **class job**). Several jobs may share one. **Without it the job is an overlay** (below). |
| `bonus` | optional `{ "str": 1, "mov": 1 }`: added to the unit's stats (in `unitStat`, next to rank and accessory bonuses, so formulas, forecasts, the AI and the unit details see it) while this is its current job. Allowed on class jobs too. |
| `requires` | optional list of `{ "job": id, "level": n }`: the job levels needed in other jobs before this one may be taken. A job that names itself, a cycle, an unknown job or a level the job cannot reach is refused. Entries may also ask for mastered entries ("Unlocking by mastered entries"). |
| `jpLevels` | optional list of total JP for job level 2, 3, ...; rising, at most 19 steps. Replaces the game-wide table for this job. |
| `learn` | what the job teaches (at most 40 entries): `{ "skill": id }` or `{ "ability": id }` (exactly one), `cost` in JP (0 to 99999; required, and refused when `rules.jobs.spendJp` is false: `"cost" is not used: rules.jobs "spendJp" is false (entries are learned from equipment)`) and `slot`: `"action"`, `"reaction"`, `"support"` or `"movement"`. |

A **skill** is a skill of `data/skills.json` (hidden condition skills cannot be named); an **ability** is a menu ability of `data/abilities.json`. A skill or ability is taught by **one job
only** (the same id twice anywhere in the file is refused), and a skill and an ability that are both taught may not share an id, so a plain id in a character's `learned` / `equipped` list
always means one entry. The slot is only the kind of place the entry takes when it is equipped from another job (below); it does not otherwise change what the entry does.

### `game.json` `rules.jobs` (optional; every key optional)

```json
"jobs": { "jp": { "perAction": 10, "perDefeat": 20 },
          "slots": { "action": 1, "reaction": 1, "support": 1, "movement": 1 },
          "jpLevels": [100, 300, 600],
          "changeIn": "camp+prep" }
```

| key | default | meaning |
|---|---|---|
| `jp.perAction` | 10 | JP a unit earns **wherever experience is awarded** (see "Earning JP"). |
| `jp.perDefeat` | 0 | extra JP for defeating a unit of another side. |
| `jp.share` | 0 | percent of every JP gain the unit's allies get in the same job ("Sharing JP with allies"). |
| `spendJp` | true | false = learn entries are learned only from equipment, never with JP ("Learning from equipment"). |
| `slots.<kind>` | 1 each | how many entries of that kind learned in **other** jobs a unit may have equipped at once (0 to 8). |
| `jpLevels` | none | the game-wide table of total JP for job level 2, 3, ...; a job's own `jpLevels` replaces it. With no table anywhere a job has the one level. |
| `changeIn` | `"camp"` | `"camp"`: jobs are changed between chapters only; `"camp+prep"`: also on the preparation screen. |

`rules.jobs` without `data/jobs.json` is refused (it would do nothing); `data/jobs.json` with an empty list is the same game as no file (the editor writes one to remember the feature is on).

### Characters and map units

A character (`data/characters.json`) and a map unit (also an `addUnit` command) may write these keys. A unit in a class job plays as the **job's unit type**: the character's `type` is replaced
by it, and a map unit that has a class `job` needs no `type` (and may not write one); with an overlay job the unit keeps its own type and the map unit needs a `type` ("Class jobs and overlay jobs").

```json
{ "id": "cora", "name": "Cora", "type": "mage", "level": 1, "job": "mage",
  "jobs": { "squire": { "jp": 25, "learned": ["guard"] }, "mage": { "jp": 10 } },
  "equipped": ["guard"] }
```

| key | meaning |
|---|---|
| `job` | the job it starts in. **A character or unit without `job` does not use the job system** (monsters, generic units, a game's other characters): no job row, no JP, no events. |
| `jobs` | starting progress per job id: `jp` (total JP earned there; job levels follow) and `learned` (entries of that job it already knows; they cost nothing). |
| `learned` | shorthand for the current job's learned entries (`"jobs": { "<job>": { "learned": [...] } }`). |
| `equipped` | the entries equipped from other jobs. Each must be learned and taught by a job other than the current one, and the slot counts may not be exceeded. |

On a map unit the keys **replace** what its character brings. This is how a designed enemy uses the system: `{ "id": "shaman", "side": "red", "job": "mage", "learned": ["mend"] }` is a Mage that
knows Mend. All refusals name the file, the entry and the key (`character "cora": equipped "focus" belongs to the current job (it is active anyway)`), and the job keys in a game without
`data/jobs.json` are refused.

## Class jobs and overlay jobs

A job with a `unitType` is a **class job**: the unit plays as that type (as above). A job **without** `unitType` is an **overlay job**: the unit keeps its own unit type, level, stats, weapons and
promotion path, and the job only adds what it teaches (learned skills and abilities, as for any job) and its optional `bonus`. A game may mix both kinds, and all its jobs may be overlays.

```json
{ "id": "scout", "name": "Scout", "description": "Keeps its own class; quick and keen.",
  "bonus": { "str": 1, "mov": 1 }, "learn": [ { "skill": "eagle", "cost": 20, "slot": "support" } ] }
```

**What a unit remembers.** In a game that has at least one overlay job, a unit that uses jobs also keeps its **base type** (`JobState::baseType`): its own class, the one its character or map entry
names, as promotion later moves it. The type a unit plays as is then **always derived**: a class job's own `unitType`; for an overlay job the base type. Stats follow the way they always did
(each stat moves by the difference between the new playing type's base and the old one's, never below 0), so:

| from -> to | what happens |
|---|---|
| class -> class | the type becomes the new job's; stats shift by the difference (as before). The base type is untouched. |
| class -> overlay | the type becomes the **base type**; stats shift by (base type - the class job's type). |
| overlay -> class | the type becomes the job's; stats shift by (the job's type - base type). |
| overlay -> overlay | nothing changes (only the bonus and the learned entries). |

Changing back is exact (apart from a stat that hit 0 on the way). A game without overlay jobs records no base type and behaves exactly as before.

**Starting jobs.** A character with a class job gets the job's type as always, and in a mixed game its own `type` becomes its base type; with an overlay job its `type` stays. A **map unit**
(or `addUnit`) with a class job needs and may have no `type` (the job's type is its base); with an overlay job it **needs a `type`** (or a `character`), which is its class
(`job "scout" keeps the unit's own class, so the unit needs a "type"`); a `type` together with a class job stays refused. A `job` given to a unit with a `character` replaces only the job.

**Promotion.** A unit whose current job is an **overlay job** promotes its base type exactly as a unit in a game without jobs does (by item, by level, from the menu, by a story command): the
type, the stats and the base type move, the job state stays, and an overlay job plays as the promoted class from then on. A unit in a **class job** does **not** promote (no offer, no automatic
promotion, a story command does nothing): its type is the job's. Change to an overlay job first. (In games without overlay jobs this is the same rule: a unit with any job cannot promote.)

**Where the code assumed a type.** The preparation screen's "could not stand on slot n" test judges the type the unit would *play as* under the new job (for an overlay job the base type); the
Jobs screen shows `Plays as a Knight.` only for class jobs and `Bonus: +1 Strength, +1 Move.` for a job that has one; the unit details, the AI and the roster read the unit's real type; saves check
that a unit's type fits its job state in games with overlay jobs (docs/SAVES.md).

## Combining levels, ranks and jobs

Levels, promotion, ranks and jobs are independent of each other. Levels and promotion (`rules.progression`, a unit type's `promotions`) work on the unit; ranks on their own counter
(`rules.ranks`, docs/SRPG.md "Unit ranks": Merit) give small standing bonuses; jobs add JP, skills and abilities, and class jobs also change the type. A worked example (the test fixture
`tests/fixtures/combo`):

```jsonc
// game.json "rules"
"progression": { "enabled": true, "xpPerLevel": 20, "maxLevel": 10 },
"ranks": { "name": "Merit", "perHit": 5, "perDefeat": 15,
           "levels": [ { "at": 10, "name": "Veteran", "bonus": { "str": 1 } }, { "at": 30, "name": "Elite", "bonus": { "str": 2, "hp": 4 }, "skills": ["steady"] } ] },
"jobs": { "jp": { "perAction": 10, "perDefeat": 20 }, "jpLevels": [20, 60, 120], "changeIn": "camp+prep" }
// data/units.json: "squire" promotes to "captain" at level 2 (automatic); data/jobs.json: class jobs squire, knight, mage and overlay jobs scout (bonus str+1, mov+1), warden (bonus def+2)
// data/characters.json: Bryn { "type": "squire", "job": "scout" }  Aldric { "type": "squire", "job": "squire" }
```

One fight pays the attacker experience (10), merit (5) and JP (10) in one go. Bryn (a Scout, an overlay job) reaches level 2 and promotes to Captain; he stays a Scout, his stats are the Captain's
plus the Scout's bonus, and his Merit and JP go on. Aldric reaches level 2 too but, being a Squire (a class job), does not promote; in the camp he becomes a Warden (overlay): still a Squire by
type, +2 defense from the job, and from then on he *could* promote. If he is made a Knight (class), he plays as one and cannot; made a Warden again he is a Squire (or the Captain he
promoted to) again, stats exact. The roster carries level, experience, merit, type, job state and base type from chapter to chapter, and saves hold all of it.

## The rules

**Job level.** The total JP earned in a job and its table (its own `jpLevels`, else the game's) give the job level: level 1 until the first step, then +1 for every step reached.

**Unlocking.** A job with no `requires` can always be taken. With them, every listed job must have the stated level in the unit's progress. A locked job says why: `needs Squire level 2`.

**Learning.** The JP of a job are spent only on **that job's own entries** (JP earned as a Squire buy Squire entries). The job must be unlocked, the entry not yet learned, and the
unspent JP (`earned - spent`) at least its cost. A refusal says `costs 40 JP, 20 left in Squire`.

**What is active.** In a battle a unit has **every learned entry of its current job** (always, no slot needed) plus the **equipped** entries from other jobs. Skills join its skills after its
rank's and before its side's (`Battle::skillsOf`), abilities join its menu after its character's (`Battle::abilitiesOf`), so everything else (the computer opponent, forecasts, the unit
details) sees them like any other skill or ability.

**Equipping.** An entry learned in another job may be equipped while the number of equipped entries of its slot kind is below `rules.jobs.slots.<kind>`. The slot kinds only separate those
numbers (a game may allow one reaction entry and three support entries); they do not decide whether an ability is used in the menu. Taking a job that teaches an equipped entry makes it the
job's own, so it leaves the equipped list. The entries of the current job cannot be equipped (they are active already).

**Changing job** (outside a battle only). The unit type becomes the job's (a class job; for an overlay job see "Class jobs and overlay jobs" above). The stats follow the way a **promotion** moves them: each stat moves by the difference between the new type's base
and the old type's, never below 0, so the character's own stats and the growth it earned stay, and going back restores the exact numbers (unless a stat hit 0 on the way). Level,
experience, items, learned skills (also those a unit type's `learns` taught) and weapon experience stay. A battle starts at full hit points, so there are none to keep. No stat cap is
applied on the way. Leveling after the change uses the **new job's unit type growths** (`growthOf` reads the unit's type), and a unit type's own `learns` entries still teach at their levels
(a game with jobs should leave those out). A job change is not possible in a battle.

## Earning JP

`Battle::awardXp` is the one place experience is awarded: for the attacker of a fight that hit or struck, for a defender that struck back and for a staff user (a staff awards experience only where its `xp` is above 0). It calls the job hook first, so
**the same moments pay JP**, whatever the experience comes to (a game with `"progression": { "enabled": false }`, a unit at its level cap or `xp` 0 still earns JP):

- `jp.perAction` goes to the unit's **current job**; only units of **human sides** earn JP (an enemy's job progress is whatever the map said).
- `jp.perDefeat` goes to the unit that defeated a unit of another side (`killUnit`), in the same job.
- Menu abilities pay none (they award no experience either).

Each award is a `JpGained` event: `a` unit, `b` JP, `c` job, `d` its job level now, flags `kJobLevelUp` when the level rose. A game without jobs never produces one.

## Between chapters (the camp) and on the preparation screen

The roster entry of every character carries its job state (`RosterUnit::jobs`): job, JP per job, what it learned and equipped, and its unit type and stats as the last job change left them. The next
chapter's battle dresses the unit from it (`Battle::applyRoster`): its type, stats, level and experience, and the job state. A character that first appears in a chapter comes in as its
character data says. Characters without a job pass through untouched.

**Camp.** After a won chapter the camp (the screen that lists support conversations) gets a **Jobs** row whenever a roster character uses jobs; the camp appears even when no conversation waits.
It opens the Jobs screen. Not shown before chapter 1 (there is no roster yet) and never in `--autoplay`.

**Jobs screen** (`player/jobs/`). First the characters that use jobs; then one character with three tabs (Left/Right or a tap on the tab name):

- **Jobs**: every job, the current one marked and its level on the right; a locked one is greyed and its explanation says `Locked: needs Squire level 2.` Choosing a job takes it.
- **Learn**: a heading per job (`Squire  Lv 2  JP 40` = level and unspent JP) and its entries with the cost, `Learned` when known; greyed when it cannot be afforded or the job is locked; the
  explanation names the kind, the slot and the reason (`Costs 40 JP, 20 left in Squire.`). Choosing one learns it.
- **Equip**: a heading per slot kind with how many are in use (`Support  1/1`), under it the entries learned in other jobs; the equipped ones are marked. Choosing toggles.

A refusal says why in the head line and plays the error sound. Every change is written to the Continue save at once. The screen scrolls its lists and fits UI scale 100% and 150% on the
480 x 270 canvas (the panel uses the whole width, also on the preparation screen).

**Preparation screen.** With `"changeIn": "camp+prep"` the preparation menu has a **Jobs** entry (between Items and the shop) that opens the same screen over the map; every change rebuilds
the battle from the roster, so the map and the unit details show it. A character deployed on a slot whose terrain the new job's unit type cannot stand on cannot take that job (`a Knight
could not stand on slot 2`). A game with `"changeIn": "camp"` has no such entry. Free battles (no campaign) have no roster, so no Jobs screen: the units keep the jobs their map gives them and
JP earned there is not kept.

## In the player

- **Unit details**: the Stats page has a **Job** row (`Squire  Lv 2` and, small, `JP 40` left to spend); the Skills page marks a skill that comes from the job (learned in it or equipped from another)
  as **Job**; an ability from a job shows `(Job)` after its name on the Abilities page.
- `+10 JP` floats over the unit when it earns JP, and `Job Lv 3!` under it when the level rose.
- The simulator (`ote-sim`) prints `X earns 10 JP in squire (job level 3)`.

## Saves (docs/SAVES.md "Jobs")

A unit of a battle save and a roster entry of a campaign save has `"jobs": { "job": n, "progress": [ { "job": n, "earned": n, "spent": n, "learned": [flat indices] } ], "equipped": [flat indices] }`
when the game has jobs and the unit uses them; no other save changes. The data hash covers `data/jobs.json` (names and descriptions are cosmetic) and `rules.jobs` (part of `game.json` rules).

## The computer opponent (docs/AI.md "Jobs")

Nothing new to decide: its units use what they have (skills in the forecast, menu abilities in `considerAbilities`), and learned or equipped entries of an enemy's job state are part of that.
The computer never changes a job and never spends JP.

## Sharing JP with allies

`"rules": { "jobs": { "jp": { "share": 25 } } }` (a whole number 0 to 100, default 0 = off): whenever a unit earns JP (`perAction` or `perDefeat`), every
**other** unit of the **same side** that uses jobs and is on the board gets that percent of it (rounded down; nothing when it comes to 0), **in the job the
earner used**, not its own. So a unit can collect JP in a job it does not have, or has not even unlocked (it can spend them once it may take that job).
Shared JP is not shared again. Each share is a `JpGained` event with the flag `kJobShared` (and `kJobLevelUp` when that job's level rose); the player
floats it like any JP gain. Only human sides earn JP, so only they share. Code: `Battle::awardJp` (`core/jobs/job_battle.cpp`).

## Unlocking by mastered entries

A `requires` entry may ask for **mastered** (learned) entries instead of a job level; all kinds may be mixed in one list (every entry must be met):

```json
"requires": [ { "job": "squire", "level": 2 },
              { "job": "soldier", "mastered": 2 },
              { "job": "soldier", "mastered": 1, "slot": "action" },
              { "mastered": "first_aid" } ]
```

| entry | met when |
|---|---|
| `{ "job": id, "level": n }` | (as before) the job level in that job is at least n. |
| `{ "job": id, "mastered": n }` | at least n of that job's learn entries are learned (bought with JP or mastered from equipment, below). Optional `"slot"` counts only entries of that slot kind. |
| `{ "mastered": id }` | that one entry is mastered: a job's learn entry that is learned, or an entry an item teaches (below) whose points are full. |

Refusals (each names the job and the entry): `"requires" entry 2: give "level" or "mastered" with "job"` (both or neither), `"mastered" must be a whole number
from 1 to 3 (job "soldier" teaches 3 entries)` (with `slot`: `teaches 1 action entry`), `"slot" goes with "job" and a number of "mastered" entries`,
`"mastered" names "x", which no job or item teaches`, `a job cannot require what it teaches itself`, unknown keys. A circle of requirements through mastered
entries is refused like one through levels (a specific entry counts as a requirement on the job that teaches it). A locked job says why:
`needs 2 Soldier entries mastered`, `needs First Aid mastered`.

## Learning from equipment (ability points)

An item can **teach** skills and abilities: while the unit has it equipped it may use them, and the **ability points** it earns fill each one until it is
**mastered** (from then on the unit keeps it for good, item or not). Each entry has its own points; a unit can fill several at once. This works with or
without jobs.

```jsonc
// data/items.json (a weapon or an accessory)
{ "id": "shortsword", "kind": "weapon", "weapon": "sword", "teaches": [ { "ability": "first_aid", "points": 100 }, { "skill": "counter", "points": 200 } ] }
// game.json "rules"
"abilityPoints": { "name": "AP", "perBattle": 10, "perAction": 0, "perDefeat": 0 }
```

| key | meaning |
|---|---|
| item `"teaches"` | 1 to 8 entries `{ "ability": id }` or `{ "skill": id }` (exactly one) with `"points"` (1 to 9999): what it costs to master. Only on items of kind `weapon` and `accessory`. A skill that is a condition's hidden skill cannot be named. |
| `rules.abilityPoints.name` | the short word the player shows (1 to 8 characters, default `"AP"`). |
| `perBattle` | points every unit of a human side gets when its side **wins** the battle (default 10). Only units alive and on the board. A map may set its own (below). |
| `perAction` | points at every moment experience is awarded (as JP: a fight it took part in, a staff it used; default 0). |
| `perDefeat` | extra points for defeating a unit of another side (default 0). |

**A map's own points for a win.** `maps/*.json` may say `"abilityPoints": { "perBattle": 40 }` (a whole number 0 to 9999): a battle on that map gives that
many for the win instead of the game's `perBattle` (0 = none for the win); `perAction` and `perDefeat` stay the game's. Refused: `"abilityPoints" needs items that
teach (data/items.json "teaches"): without them nobody earns AP`, a missing `perBattle` (`needs "perBattle": ...`), any other key, a number out of range. Not saved
(map data; `Battle::mapApPerBattle`). *Owner decisions (placeholders):* the key `"abilityPoints": { "perBattle" }` on the map (only the win amount), 0 allowed. Tests:
`tests/test_map_ability_points.cpp`.

**Equipped** means: the weapon in hand (a weapon item in its inventory's equipped slot) and every accessory it carries (accessories work while carried,
docs/ITEMS.md). A weapon that is not an item (a unit type's own weapon) teaches nothing.

**Points.** At each of the three moments the unit's equipped items' entries that are not mastered yet get the points (an entry taught by two equipped items
gets them once). When the points reach the cost the entry is **mastered**. Only units of human sides earn points (as JP). Each gain is a `PointsGained` event
(`a` unit, `b` points, `c` the entry, `d` its points now; flag `kMastered` when it became mastered). The player floats `+10 AP` and `Mastered: First Aid`.

**With jobs.** An entry an item teaches may also be a job's learn entry (the same skill or ability id). Then it belongs to that job:
- it is usable from the item, and fills, **only while the unit's current job is that job** (the job's own entries are what a job can use);
- mastering it **learns it in that job** (as if bought, no JP spent): from then on it follows the job rules (active in that job, equippable from other jobs);
- `"rules": { "jobs": { "spendJp": false } }` makes equipment the **only** way to learn: JP are never spent (they still make job levels), the learn entries
  then take no `"cost"`, and every learn entry must be taught by some item (`job "soldier", "learn" entry 2: "first_aid" is learned only from equipment
  ("spendJp": false), but no item teaches it`). The Learn tab shows the points instead of a price.

An entry no job teaches (or any entry in a game without jobs) is usable from the item by any unit and fills in any job.

**Refusals** (the item, the entry): `"teaches" lists 1 to 8 entries such as { "ability": "first_aid", "points": 100 }`, `give "skill" or "ability", one of
them`, `"points" must be a whole number from 1 to 9999`, `"teaches" belongs to a weapon or an accessory`, unknown skill / ability, `"first_aid" costs 100
points here but 80 on item "dagger": an entry has one cost`, `"first_aid" is taught as a skill and as an ability (...)`, the same entry twice on one item;
`rules.abilityPoints: no item "teaches" anything (data/items.json); the block does nothing without it`.

**Saves.** A unit and a roster entry carry `"mastery": [ { "entry": n, "points": n } ]` (entry = index into the game's list of taught entries, items in file
order, entries in order, each once): only entries with points that are not yet learned in a job. Old saves have none. Campaign: carried with the roster.

**Player.** Unit details, Abilities and Skills pages: an entry usable from an item shows `(AP 40/100)`; a mastered one is listed like a learned one. Jobs
screen, Learn tab: an entry an item teaches shows its points `AP 40/100` (with `spendJp: false` instead of a price). The computer opponent uses what its units
have (equipped items' entries included) and never thinks about points.

## Left out (v1)

- Changing a job during a battle; an action that does it.
- The computer opponent choosing jobs, learning or equipping (a designed enemy has them from its map entry).
- Dual-wield / equipment-slot changes with jobs; per-job stat growth tables beyond what the job's unit type gives; per-job stat caps.
- JP for menu abilities that do not fight (an ability that strikes pays as a fight does), JP and points for units of the computer's sides.
- Starting points for characters and map units (`mastery` keys in data); items that teach for every job at once while restricting to some.
- Editor forms (the editor server only carries `data/jobs.json` as the `jobs` list, `rules.jobs` in the game and the job keys of characters and map units; docs/EDITOR_API.md).
- A job requirement on the entries themselves (a learn entry that needs a job level). (Promotion mixed with jobs is built: overlay jobs promote, class jobs do not.)
- The camp's Jobs row before chapter 1.
