# Jobs

An optional job system in the spirit of job-based tactics RPGs. A character can **change between jobs**, earns **job points (JP)** in the job it is using, spends them to **learn that job's
skills and abilities**, and can **equip** a limited number of learned entries from its other jobs. A game without `data/jobs.json` has none of this and loads, plays, saves and simulates
exactly as before. All wording on screen is placeholder.

Built: the data (`data/jobs.json`, `rules.jobs`, the job keys of characters and map units), the rules (job levels, unlocking, learning, equipping), the battle (the unit plays as its job,
JP is earned), the camp and the preparation screen, the unit details, saves, and the computer opponent (it uses what its units know; nothing new to decide). Not built: see "Left out".
Code: `core/jobs/` (`job_rules.*` the rules over a job state, `job_camp.*` the roster operations, `job_battle.cpp` the battle side), `core/data/jobs.h` and `loader_jobs.cpp` (data and loader),
`core/battle/save_jobs.cpp` (saves), `player/jobs/` and `player/campaign/jobs_scene.*` (the screens). Tests: `tests/test_jobs_data.cpp`, `test_jobs_battle.cpp`, `test_jobs_camp.cpp`,
`test_jobs_text.cpp` (fixture `tests/fixtures/jobs`, helpers `tests/jobs_fixture.h`), `tests/jobs_player_test.py` (the real player, headless), `tests/editor_api_jobs_checks.py`.

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
| `unitType` | the unit type (`data/units.json`) a unit plays as while it has this job: its stats, movement type, weapons, growths. Several jobs may share one. |
| `requires` | optional list of `{ "job": id, "level": n }`: the job levels needed in other jobs before this one may be taken. A job that names itself, a cycle, an unknown job or a level the job cannot reach is refused. |
| `jpLevels` | optional list of total JP for job level 2, 3, ...; rising, at most 19 steps. Replaces the game-wide table for this job. |
| `learn` | what the job teaches (at most 40 entries): `{ "skill": id }` or `{ "ability": id }` (exactly one), `cost` in JP (0 to 99999) and `slot`: `"action"`, `"reaction"`, `"support"` or `"movement"`. |

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
| `slots.<kind>` | 1 each | how many entries of that kind learned in **other** jobs a unit may have equipped at once (0 to 8). |
| `jpLevels` | none | the game-wide table of total JP for job level 2, 3, ...; a job's own `jpLevels` replaces it. With no table anywhere a job has the one level. |
| `changeIn` | `"camp"` | `"camp"`: jobs are changed between chapters only; `"camp+prep"`: also on the preparation screen. |

`rules.jobs` without `data/jobs.json` is refused (it would do nothing); `data/jobs.json` with an empty list is the same game as no file (the editor writes one to remember the feature is on).

### Characters and map units

A character (`data/characters.json`) and a map unit (also an `addUnit` command) may write these keys. A unit with a job plays as the **job's unit type**: the character's `type` is replaced
by it, and a map unit that has a `job` needs no `type` (and may not write one).

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

**Changing job** (outside a battle only). The unit type becomes the job's. The stats follow the way a **promotion** moves them: each stat moves by the difference between the new type's base
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

## Left out (v1)

- Changing a job during a battle; an action that does it.
- The computer opponent choosing jobs, learning or equipping (a designed enemy has them from its map entry).
- Dual-wield / equipment-slot changes with jobs; per-job stat growth tables beyond what the job's unit type gives; per-job stat caps.
- Sharing JP with allies (`shareWithAllies`), JP for abilities, JP for units of the computer's sides.
- Editor forms (the editor server only carries `data/jobs.json` as the `jobs` list, `rules.jobs` in the game and the job keys of characters and map units; docs/EDITOR_API.md).
- A job requirement on the entries themselves (a learn entry that needs a job level), and unit-type changes by promotion mixed with jobs (a promoted unit keeps its job state; avoid both in one game).
- The camp's Jobs row before chapter 1.
