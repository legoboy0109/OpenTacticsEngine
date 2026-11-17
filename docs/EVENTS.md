# Story events, dialogue and campaigns

Contract for PLAN item 3f. Owner decisions (2026-11-14):

- **Triggers**: battle start / end and turn number; a unit reaching a
  square or area; a unit defeated or fighting; a Talk command between two
  units.
- **Actions** besides dialogue: add / remove / move units; give items or
  resources; set win / lose conditions; switches and variables.
- The first version **includes a campaign**: chapters in order, surviving
  units with levels and items carried over, scenes between maps.
- **Dialogue** is a box across the bottom with the speaker's name and, when
  the game has one, the speaker's portrait beside it.
- **Dialogue choices**: built after the first version (PLAN k item 6); see "Dialogue choices" below.
- **Fallen units: the maker chooses per game** (gone for good, or back next
  chapter).
- **Single maps next to a campaign: the maker chooses** (campaign only, or
  campaign plus free battles).

Everything below is engine design and may change; all wording shown to
players and makers is placeholder until the owner approves it.

Status: steps 1 to 4 of the "Build order" are built and tested, and
`demo-srpg` has a story and a two-chapter campaign; still to do: the
editor's Campaign screen (the rest of step 5). Where building step 1
refined or corrected the first contract, the text below is the corrected
version and says so ("refined").

## Principles

- Events are **data**, edited as lists of commands like RPG Maker's event
  commands. No scripting is needed; a Lua hook can come later.
- Events run in the rules core, so they are deterministic, saved, and
  played by `ote-sim`. A command changes the battle at once and leaves
  entries in the event stream; the player shows them in order (a line of
  dialogue is one such entry). Nothing waits for input inside the core: a question that a person must answer is done by replaying
  the action with the answer (see "Dialogue choices").
- A game without events, objectives, switches or variables behaves exactly
  as today and keeps its save hash and its saves byte for byte.
- Wording (what is said, names, labels, objective text) is kept apart from
  rules: it lives in one text table that the Battle owns, events carry only
  indices into it, and it does not count for the save hash.

## Named units

A map unit may have `"id": "eirik"`: letters, digits, `_` and `-` (up to 64),
unique among the map's units and the units its `addUnit` commands create.
Events, objectives and the campaign point at units by id; ids that nothing
knows are loader errors. A **unit filter**, used wherever "which unit" is
asked, is one of: `{ "unit": "eirik" }`, `{ "side": "blue" }`,
`{ "team": "blue" }`, `{ "type": "knight" }`, `{ "tag": "flier" }`, or `{}` =
any unit. Exactly one key (refined: no combinations). A plain string where a
filter is expected means `{ "unit": that }`.

## Switches and variables

`game.json`: `"switches": [ { "id": "village_saved", "name": "Village saved" } ]`,
`"variables": [ { "id": "gold_found", "name": "Gold found" } ]` (ids as for
units, unique). Switches are off / on, variables whole numbers (start 0;
refined: they stay within plus or minus 1,000,000,000, going past it stops
there, so a save can check them). They belong to the playthrough: in a
campaign they carry from chapter to chapter; in a single battle they start
fresh. A caller that carries them (the campaign, later) calls
`Battle::setStartingStory(switches, variables)` after `init()` and before
`start()` and reads `battle.storyState` afterwards. Switch and variable
lists in `game.json` count as rules for the save hash; their names do not.

## Events (map file)

```jsonc
"events": [
  { "id": "ambush",                         // unique in the map (letters, digits, _ and -)
    "when": { "turn": 4, "side": "red" },   // the trigger, see below
    "if": [ { "switch": "village_saved", "is": false } ],   // optional, all must hold
    "repeat": false,                        // default: runs once
    "do": [ { "say": "Now! Strike from the forest!", "speaker": "gorm" },
            { "addUnit": { "type": "soldier", "side": "red", "x": 9, "y": 1 } } ] }
]
```

An unknown key in an event, a trigger, a condition, a command or an
objective is a loader error (a typo must not pass silently), and so is every
unknown id (unit, side, team, type, item, resource, switch,
variable). Messages name the map, the event and the command:
`maps/bridge.json: map "bridge": event "ambush": "do": command 2 (addUnit): unknown unit type "dragon"`.

### Triggers (`when`)

| Trigger | Fires |
|---|---|
| `{ "battle": "start" }` | before the first turn (before the first `TurnStarted` entry) |
| `{ "battle": "won" }` / `"lost"` / `"end"` | when the battle is decided, right after the `BattleEnded` entry: `won` if the human team won, `lost` if it did not (also a draw), `end` either way |
| `{ "turn": 4, "side": "red" }` | at the start of that side's turn in round 4 (after income); `side` optional = the first side; `"turn": "every"` = every round (refined: it implies `repeat`) |
| `{ "reach": { "x": 5, "y": 2, "w": 2, "h": 1 }, "by": <filter> }` | when a matching unit **ends** a move inside the rectangle (`w`, `h` default 1). Walking through does not count. Refined: units moved by a `moveUnit` command also "end a move"; units of the computer's side count like any other |
| `{ "defeated": <filter> }` | when a matching unit is defeated (including the units that leave with a defeated side); `"by": <filter>` optional = by anything. Refined: a side wiped out by the defeat rules has no killer, so an event with `by` does not fire for those units |
| `{ "fight": <filter>, "with": <filter> }` | just before a fight begins, when one of the two fighters matches `fight` and the other `with` (`with` optional), whichever one attacks. If the event takes a fighter off the map or ends the battle there is no fight (the attacker's action is spent) |
| `{ "talk": "eirik", "to": "lyra", "label": "Talk" }` | adds **Talk** to the first unit's action menu while the two units (named by id) are alive and next to each other; fires when chosen (does not end the turn unless the event says so). `label` defaults to "Talk". Only the first unit can start it; the computer's side never does |

### Conditions (`if`, and inside the `if` command)

`{ "switch": id, "is": true }` (`is` default true),
`{ "variable": id, "op": "=", "value": 3 }` (`= != < <= > >=`, `==` also
accepted), `{ "alive": <filter> }` / `{ "dead": <filter> }`,
`{ "round": { "op": ">=", "value": 5 } }`,
`{ "carries": "salve", "unit": <filter> }` (`unit` default any),
`{ "sees": <unit filter>, "team": "<team>" }` (fog of war, docs/ECONOMY.md; `team` is required: a team name or a side id
for that side's team).
Refined: `alive` = some living unit matches; `dead` = a unit that matches
exists (or existed, also when it was removed) and none is alive, so a unit
that an event has not added yet is neither. `carries` = a living matching
unit has the item in its inventory (not the storage). `sees` = the team sees at least one living unit that
matches the filter: a unit of the team itself counts, a unit on a hiding square counts only with a unit of the team
next to it, and without fog on the map every living matching unit counts. It is judged on the team's sight at the
moment the event is about to run; story events and objectives otherwise act on the truth, not on what a team has seen.

Conditions are judged when the event is **about to run**, so an event sees
what earlier ones did. An event whose condition fails is not used up: a
non-repeating event can still run at a later occurrence of its trigger.

### Commands (`do`)

| Command | What |
|---|---|
| `{ "say": "text", "speaker": "eirik" }` | a line of dialogue (entry `Dialogue`); `speaker` is a unit id or a free name (`"speakerName": "Villager"`, not both); optional `"portrait"` id (just text for the player to look up) |
| `{ "addUnit": { …a map unit… } }` | places a unit (keys as in the map's `units`, including `id` and `ranks`); if the square is taken or the unit cannot stand there, the nearest free square it can stand on (grid distance, the lowest cell number on a tie). A repeated `addUnit` with an id that a unit already has creates the unit without the id (logged). No free square at all: nothing is added (logged) |
| `{ "removeUnit": <filter> }` | leaves the map (not a defeat: no drops, no defeat events; the defeat rules still apply, so removing a side's last unit ends it like any other loss) |
| `{ "moveUnit": "eirik", "x": 3, "y": 4 }` | refined: the unit is put there (no path is walked; the entry gives the squares it left and reached, the player may animate it); if the square is taken, the nearest free one as for `addUnit`. Does not spend its move. Fires `reach` events |
| `{ "changeSide": <filter>, "side": "blue" }` | matching living units join another side (recruiting); an unfinished capture is dropped |
| `{ "giveItem": "salve", "to": <filter> }` | every matching living unit gets one, following the item rules (room: inventory, else the shared storage if the game has one, else lost) |
| `{ "giveResource": { "gold": 500 }, "side": "blue" }` | may be negative; a side's funds never go below zero |
| `{ "setSwitch": id, "to": true }`, `{ "setVariable": id, "to": 3 }`, `{ "addVariable": id, "by": 1 }` | |
| `{ "objective": { … } }` | replaces the current objectives (below) |
| `{ "win": "blue" }` / `{ "lose": "blue" }` | ends the battle for that team (refined: a team name or a side id). `lose` leaves the win to the only other team still in the battle, nobody if there are several. The first decision stands; the rest of the event still runs |
| `{ "if": [conditions], "then": [commands], "else": [commands] }` | `then` required, `else` optional |
| `{ "endTurn": true }` | only in an event whose trigger is `talk` (a loader error anywhere else): the talking unit's turn ends |
| `{ "focus": { "x": 3, "y": 4 } }` or `{ "focus": "eirik" }` | the view moves there before the next entry (a unit that is not on the map: nothing) |
| `{ "playMusic": "boss", "fade": 1.0 }` | sound (docs/SOUND.md): the track `boss` of `data/audio.json` plays until another command changes it or the battle ends; `fade` seconds (0..60, optional: not given = the player's 0.5 s cross-fade, 0 = a cut). Presentation only: entry `MusicChanged`; the core keeps only "the music the story asked for" (saved with the battle, docs/SAVES.md). The id is opaque to the core (it goes into the text table, so which track it is does not count for the save hash, but the command does); the player refuses a game whose ids are not in `audio.json` |
| `{ "stopMusic": true, "fade": 1.0 }` | silence until a later `playMusic` or the end of the battle (entry `MusicChanged` with no track) |
| `{ "playSound": "thunder" }` | one sound effect (entry `SoundRequested`) |

**Running.** Events run in file order for one occurrence, and an event that
fires while another runs (a `reach` caused by `moveUnit`, a `won` event caused
by `win`) waits in a queue, so every event sees what the previous one left.
At most 100 events run in one go; the next one is cut with a logged error.

### What the player gets (event stream)

New `EventType`s, in the stream in order with everything else (texts are
indices for `Battle::storyText(i)`):

| Entry | a, b, c, d |
|---|---|
| `Dialogue` | a = speaker unit (-1 none), b = text, c = free speaker name (-1 none), d = portrait id (-1 none) |
| `Focus` | a = cell, b = the unit followed (-1 just the square) |
| `UnitAdded` | a = unit, b = cell |
| `UnitRemoved` | a = unit, b = the cell it stood on |
| `UnitRelocated` | a = unit, b = cell it left, c = cell it reached |
| `SideChanged` | a = unit, b = old side, c = new side |
| `ObjectiveChanged` | a = text of the new objective (-1 none), b = objective set |
| `MusicChanged` | a = text index of the track id (-1 = the story stopped the music), b = fade in milliseconds (-1 = not given) |
| `SoundRequested` | a = text index of the sound id |

With fog of war every event, these included, also carries `Event::seen` (a bit per team: dialogue, focus and
objective changes and the two sound entries are seen by every team; the others by the teams that saw the unit or the square, see
docs/ECONOMY.md "Fog of war") and the stream gains `MoveBlocked`.

Gifts use the existing item and resource entries. The speaker's name for a
unit is `units[a].name`. Queries: `talkOptions(unit, fromCell)` returns
(target, event, label) for the Talk menu; `Action::talk(unit, target[, event])`
performs it (free like the item actions, cannot be undone);
`objectiveText()` is the text to show (at battle start and in the menu; no
entry is made at the start); `humanTeam()`; `unitByStoryId(id)`.

## Objectives (map file)

```jsonc
"objectives": {
  "text": "Defeat Gorm",                         // shown to the player
  "win":  [ { "defeated": { "unit": "gorm" } } ],        // any one wins
  "lose": [ { "defeated": { "unit": "eirik" } }, { "round": 15 } ]   // any one loses
}
```
Kinds: `defeated` (filter; a side or team = all of its units; true when
something matching exists or existed and none is alive), `reach`
(rectangle + `by`; true while a matching living unit stands inside; refined:
without `by`, under `win` any unit of the human team counts and under `lose`
any unit of another team), `round` (true when that round has begun),
`survive` (rounds; the same as `round` but only under `win`: a loader error
under `lose`).

Objectives are judged for the **human team**: the team of the first side
whose controller is human, or the first side's team if there is none. They
are checked after every action and at battle start; if a `lose` and a `win`
hold at the same moment, the team has lost. Missing `objectives` = the
game's `rules.defeat`, as today, which also keeps applying to every side
(a human team that is wiped out has lost whatever the objectives say). An
objective block with only a `text` shows the text and judges nothing.

## Dialogue (player)

Built (step 2): `engine/player/ui/dialogue_{text,box}.*` (text logic and the
box), `engine/player/battle/battle_view_story.cpp` (how the view plays the
story entries) and `battle_story.cpp` (the scene side).

- **The box**: across the bottom of the screen, just above the hint line, in
  every kind of view (it is a plain 2D overlay). The speaker's name on the
  first row (the unit's `name`, or the free name; a line with neither has no
  name row); the text appears at 60 letters a second
  (`kDialogueLettersPerSecond`), word-wrapped to the box, up to three lines
  a page (fewer with a very large font). A line too long continues on a
  second page: Confirm turns it. A blinking `v` shows that the page is
  complete. The map stays visible above; its camera, cursor and orders do not
  react while a line is up.
- **Input**: Confirm / tap = show the whole page, then turn the page, then
  end the line. Holding Cancel or Menu for 0.5 s, tapping the **Skip** tab
  on the box's top-right corner, or the `--input` token `skip` skips every
  remaining line of the **current run of events** (one `Battle::apply` /
  `start` / AI step's entries; also fights and moves still play). A tap of
  Cancel alone does nothing. The hint line shows the keys.
- **Portrait**: `"portraits": { "<id>": { "sheet": …, "cell": [x, y] } }` in
  `data/graphics.json` (docs/GRAPHICS.md "Portraits"). A line uses its own
  `portrait` id, else the speaking unit's story id (`Unit::storyId`), else
  none; an id with no portrait shows the box without one. It is drawn to the
  left of the box at 48 x 48 px with the 8 px font (it grows with the font
  size and the UI scale), keeping the cell's proportions, sitting on the
  box's bottom edge.
- **Pausing**: a `Dialogue` entry becomes the view's current entry with no
  end time; the view's queue does not advance, so everything after it (a
  reinforcement the line announces, the AI's next step) waits until the scene
  calls `BattleView::skip` (Confirm on the last page) or `skipDialogues` (skip
  all: lines then pass at once until the queue drains). In `--autoplay` a
  line ends by itself after 1 s (and nothing else is waited for), so runs and
  the embedded editor's Play cannot hang on dialogue. A line triggered on the
  AI's turn pauses the AI the same way. Loading a save replays nothing (the
  view starts empty; the story state is saved, the stream is not).
- **Other entries**: `Focus` moves the view's focus to the square or unit and
  keeps it there for the rest of the run (so the camera stays on it while the
  next line is up); `UnitAdded` pops in like a built unit; `UnitRemoved`
  fades out (no death effect); `UnitRelocated` slides (snaps when more than 8
  squares away); `SideChanged` keeps the old colour until its entry plays,
  then takes the new side's colour with a short white flash and a "Joins
  <side>" float; `ObjectiveChanged` shows a "New objective" banner with the
  text (nothing when the objective was cleared).
- **Talk**: the action menu has an entry per `talkOptions` result before
  "Items": the event's label ("Talk" when it has none), and with several
  results "<label>: <partner name>". Choosing it submits `Action::talk`; the
  menu is then rebuilt and the move can no longer be undone (like the item
  actions), unless the event ended the turn, removed the unit or ended the
  battle.
- **Objective**: when a new battle starts, its objective text is a second
  line under the first turn banner ("Objective: …", the banner stays up
  2.4 s; Confirm skips it). The battle menu (the one with End Turn) has an
  "Objective" entry, only when there is text, that shows it in a small box.
  A save restores the current objective. A battle ending through objectives
  shows the usual VICTORY / DEFEAT after the `won` / `lost` events' lines.

## Campaign

Built (step 4: the rules core and the player; the editor part is step 5). `data/campaign.json`
is optional; a game without it behaves exactly as before and keeps its save
hashes. Code: `engine/core/data/campaign.h` and `loader_campaign*.cpp` (the
file), `engine/core/campaign/` (playing it and saving it), `Battle::setStartingRoster`
(`battle/carried.*`); tests `tests/test_campaign_*.cpp`; `ote-sim <game> --campaign`.

```jsonc
{ "name": "The Ford War",
  "rules": { "fallen": "gone",          // "gone" (default): a defeated unit never returns; "return": it is back next chapter
             "onDefeat": "retry" },     // "retry" (default): the chapter can be started again; "end": the campaign is over
  "freeBattles": "none",                // "none" (default) | "all" | ["map_id", ...]: maps that can also be started on their own
  "chapters": [
    { "id": "ch1", "name": "The Ford", "map": "bridge",
      "before": [ { "say": "…", "speaker": "eirik" } ],   // scenes, see below
      "after":  [ { "if": [ { "switch": "village_saved" } ], "then": [ { "goto": "ch3b" } ] } ],
      "next": "ch2" }                    // optional; default = the following chapter in the list
  ] }
```

Keys are checked like the rest of the game files: an unknown key, a bad
`rules` / `freeBattles` value, an empty `chapters` list, a duplicate chapter
id, an unknown `map` / `next` / `goto` / `freeBattles` map, a `goto` in a
`before` scene to its own chapter, or a scene command that is not one of the list below is a
loader error naming the file, the chapter and the command
(`data/campaign.json: chapter "ch1": "before": command 2 (say): …`). A chapter
nobody can reach is **not** an error. `name` of the campaign and the chapters
is wording (default: "Campaign" / the chapter id).

**Scenes** are lists of a subset of the event commands, same JSON shapes: `playMusic`, `stopMusic` and `playSound`
(as in the map events, docs/SOUND.md; they come back between the `say` lines as `SceneLine`s of kind `PlayMusic` / `StopMusic` / `PlaySound`
with `text` = the id and `fadeMs`, and the player acts on them when it reaches them; nothing in the playthrough changes), `say`
(`speaker` = a roster unit id, only checked for being a well-formed id since the
unit may join later; or `speakerName`; optional `portrait`), `setSwitch`,
`setVariable`, `addVariable`, `giveResource` (`"side": "<side id>"`: added to what that
side carries; a side id is only checked for being well-formed because scenes
know no map), `if` with the conditions `switch`, `variable`, `alive` / `dead` (a
roster id) and `{ "goto": "<chapter id>" }` (also inside `if`; in a `before` scene it skips the chapter's battle, see "Dialogue choices"), `choice` ("Dialogue choices").
A chapter's `before` / `after` may also be written `{ "music": "scene_theme", "commands": [ ... ] }`: the same
scene with a `playMusic` in front (`commands` optional; the data hash sees the plain list, so adding music to a scene changes no save).
Refined: `alive` = the id is in the roster and not gone, `dead` = it is in the
roster and gone (a unit that never joined is neither). A scene runs at once
with no input: state changes apply immediately and the `say` lines come back in
order as `SceneLine`s (speaker id, the name to show, portrait id, text), resolved like a battle's
`Dialogue` entry: a roster unit's own name, else the free `speakerName`, else
nobody; so the player can use one dialogue box for both.

**Where the player is** (`Playthrough`): the chapter and a **phase**: `Before` (the
chapter's before scene is due), `Battle` (scene done; the battle is to be played
or is under way), `After` (battle won and taken over; the after scene is due),
`Finished` (the last chapter's after scene ran), `Over` (a battle was lost and
`onDefeat` is `end`). Also the roster, what the sides carry, switches and
variables. The next chapter after an `after` scene is the last `goto` that ran,
else the chapter's `next`, else the following one in the list; after the last one
the playthrough is `Finished`.

**Carry-over.** The **roster** holds every named unit (`id`) of a human side: id, type,
name, level, xp, stats, items with uses left, the side id it last fought on, and
whether it is gone. Units without an id are extras and never carried. "Human
side" = a side the map does not give to the computer.
- *Starting a chapter's battle* (`startChapterBattle`): the map is loaded and the
  Battle initialised as usual, then every map unit with an `id` on a human side is
  looked up in the roster: found, it takes the roster's type, name, level, xp, stats
  and items, starts at full HP (maximum from its stats and accessories) on the map's
  square; marked gone, it is not placed (taken off the board before anything
  happens, not "defeated", so no event sees it); not in the roster, it starts as
  the map says. The same goes for a named unit that an `addUnit` event creates later
  on a human side (a gone one is not added; logged). The roster dresses a unit only
  where it is placed or added on a human side: a unit that starts on the computer's side
  and is recruited by `changeSide` stays as the map says until the chapter's end.
  The side's storage = carried storage followed by the map's `storage` entries;
  resources = carried + the map's (the map's amount is that chapter's allowance).
  Switches and variables go in through `Battle::setStartingStory`.
- *After a won battle* (the human team won, `finishChapterBattle`): every living
  named unit on a human side is written to the roster (including ones that joined
  through `addUnit` or `changeSide`); a named human-side unit that was defeated
  becomes gone (`fallen: gone`; a unit that joined and fell in the same chapter
  gets a gone entry too) or stays as the roster had it at the start of the chapter,
  gaining nothing (`fallen: return`; one that was not in the roster stays out of
  it, so it starts as its next map says); a named unit that is on a non-human side at
  the end leaves the roster; storage, resources, switches and variables of the human
  sides are taken from the battle. Refined: a unit removed by a `removeUnit` event is
  counted like a defeated one (the Battle does not tell them apart). Then the `after`
  scene runs, then the next chapter.
- *After a lost battle* (a draw counts): `retry` leaves the playthrough exactly as it
  was before the battle (phase stays `Battle`; the chapter can be started again; the
  before scene is not replayed); `end` makes the phase `Over`.

**Saves.** One save slot is one playthrough: `"playthrough": { … }` and, in phase
`Battle` while a battle is under way, `"battle": { … }`. Format, hash and the
exact calls: docs/SAVES.md "Campaign saves". Saves of games without a campaign are
unchanged byte for byte.

**Free battles.** `freeBattleAllowed(db, mapId)`: true for every map when the game has no
campaign; otherwise per `freeBattles`. A free battle is an ordinary `Battle::init` +
`start` and never touches a playthrough.

### Campaign (player)

Built (step 4, player half). Code: `engine/player/app_campaign.cpp` (the flow), `campaign/` (`campaign_logic.*` the
decisions, `story_scene.*`, `end_scenes.*`), `battle/battle_campaign.cpp`, `scene_game.*` (the page); the saves are in
docs/SAVES.md "Campaign saves in the player". All wording below is placeholder.

- **Game page** of a game with `data/campaign.json`: **Continue** (when a Continue save exists: the chapter's name, and
  "Turn N" in the middle of a battle, else where the story stands), **New game**, **Load** (saving `any`), Game options,
  Back, then under the heading **Free battles** the maps that `freeBattleAllowed` allows (none = no heading). New game
  while a Continue save exists asks first ("This replaces your Continue save."). A free battle ends back on this page
  (there is no separate map list for such a game).
- **Scene screen** (`StoryScene`): the style's background colour; when a chapter begins, a **title card**
  ("Chapter N" small above the chapter's name, N = its position in the list; Confirm / tap leaves it, or it leaves by
  itself after 2.5 s); then the lines in the **same `DialogueBox` as a battle** (portrait from the line's `portrait`, else
  the speaker's roster id; same controls: Confirm / tap, pages, hold Cancel or Menu for half a second, the Skip tab,
  the `skip` script token). An empty before scene is only the card; an empty after scene is not shown at all. Note:
  a roster id names a unit only once it is in the roster, so the first chapter's before scene should use
  `speakerName` (the roster is empty before chapter 1's battle).
- **Flow**: New game -> chapter 1's card + before scene -> `startChapterBattle` -> battle -> `finishChapterBattle` when it is
  over. **Won**: the normal VICTORY banner ("Confirm: continue"), then the after scene, then the next chapter's card +
  before scene; after the last chapter's after scene an end screen ("The End" + the campaign's name) -> the game
  page. **Lost, `retry`**: the normal DEFEAT banner, then a choice **Try again** / **Back to title**; Try again starts the
  chapter's battle again with a new seed (`args.seed` + the number of battles started so far) and without the before scene;
  Back to title (or Cancel) goes to the game page, where Continue starts the chapter again. **Lost, `end`**: DEFEAT, then
  the end screen "The story ends here." -> the game page. A draw counts as lost. "Leave battle" in the battle menu
  saves and returns to the game page as in any battle.
- **`--autoplay`** plays the story through (see docs/SAVES.md "Command line"): the AI plays the human side and every
  screen in between passes by itself (card 1 s, each line 1 s, result 1.5 s, end screens 1 s, retry = Try again).
- **Web**: the same code; `tools/webtest/suites/campaign.mjs` plays New game -> scene -> battle -> reload -> Continue on
  the test game in a temp folder that serves the web build with its own `games/`.

## Dialogue choices

PLAN k item 6. All wording is placeholder until the owner approves it.

**The command** (map events and campaign scenes, same JSON in both):

```jsonc
{ "choice": "An old gate blocks the path. What now?",   // the question (not empty)
  "speaker": "mara",            // or "speakerName": "Guide", and "portrait": "id": as for "say" (all optional)
  "store": "gate",              // optional: a game variable that receives the NUMBER of the answer picked
  "answers": [                  // two to four
    { "text": "Open it",  "do": [ { "setSwitch": "gate_open", "to": true } ] },        // "do": any commands legal in this place
    { "text": "Go around" },                                                          // "do" optional (nothing happens)
    { "text": "Ask Tobin first", "if": [ { "alive": "tobin" } ],                      // "if": optional, hides the answer
      "do": [ { "choice": "Trust him?", "answers": [ { "text": "Yes" }, { "text": "No" } ] } ] } ] }   // a nested question
```

Number = the answer's position **as written** (1 = the first), also when earlier answers are hidden; 0 = never asked. It is
an ordinary variable, so everything that reads variables works: `{ "variable": "gate", "op": "=", "value": 2 }` in event and
answer conditions and in campaign scenes, and it is saved and carried between chapters like any variable (so it needs no
save format of its own). A new choice id was not invented: the maker's id is the variable's id (game.json `variables`).

**Branching the campaign.** Two ways, both on what exists: an answer sets the variable and a later `if` branches (`{ "if":
[ { "variable": "road", "value": 2 } ], "then": [ { "goto": "ch3" } ] }`), or the answer holds `{ "goto": "<chapter id>" }`
itself. `goto` works in every scene command list, answers included. In an **after** scene it picks the next chapter (the last
`goto` that ran wins, as before). In a **before** scene (new) it skips this chapter's battle: the chapter it names becomes
current in phase `Before`, so its own before scene runs next, and from there the order is normal (its `next`, else the
following chapter). That is how a first scene asks "Play the tutorial?" (a chapter whose before scene asks and whose map is
never played). A before scene may not name its own chapter (loader error: it would run for ever); longer cycles are the maker's
to avoid.

**Hidden answers.** `if` uses the engine's conditions (map: all kinds; scene: switch, variable, alive, dead, support). At run
time: two or more offered = asked; exactly one offered = taken at once (stored, its commands run, nothing shown); none offered =
the question is skipped (nothing stored). The loader cannot know which conditions hold, so it only refuses fewer than two or
more than four answers written.

**Loader refusals** (plain English, naming the map / chapter, the event or scene and `command N (choice)`): a question that is
not text or is empty; fewer than two / more than four answers; an answer with empty or missing `text`; an answer with the same
`text` as an earlier one; an unknown key (in the command or an answer); an unknown command or a bad command inside an answer
(the message names `answer N: "do": command M`); an unknown `store` variable, unknown switch / unit in a condition; `goto` to
a chapter that does not exist (also inside answers, nested questions and `if`s); a `goto` in a before scene to its own chapter;
and **two questions of one map (or of one scene) with the same `store`** (almost always a copy that was not edited; reuse across
different scenes / maps is fine, the later answer overwrites). The question wording and the answers' `text` are wording for
the save hash; `store`, the answer count and what the answers do are rules.

**Runtime in a battle** (`Battle`, `core/battle/story_choice.cpp`). `Battle::setAskChoices(false)` (default; `ote-sim`, AI-only
runs, `--autoplay`, the editor's Play): the first offered answer is taken on the spot and a `Dialogue` entry with the question
leaves the stream, so every existing consumer works. With `setAskChoices(true)` (the player, a person at the screen)
`apply` / `start` stop at the first unanswered question: the entries up to and including a `Choice` entry are returned
(a = speaker unit, b = question text, c = free speaker name, d = portrait id, `data` = the offered answers' texts), the Battle
is the state exactly at the question (what the view shows matches the entries), `choiceWaiting()` is true and every
`apply` is refused. `answerChoice(pick, events)` (pick = position among the offered answers) goes back to the state before
the action, runs the same action again with all answers given so far (deterministic, so it reaches the same question) and
returns the entries after the question, up to the next unanswered question if there is one. The rest of the action (a fight, the
turn) therefore happens after the answer in the stream and in the state, exactly as if the answer's commands had been written
in place: a test checks the final state against the plain commands. Cost: one copy of the Battle per action while asking is
on and the map has a choice (maps without one are untouched and byte-identical). `Choice` is the last `EventType`.

**Saves.** The answer is the variable, saved with the battle and the playthrough as always; a save made after the choice
(also in the middle of a battle) restores it, and the question is not asked again. A save made **while a question is on screen
is impossible**: the player is busy then (no menu, no autosave), and the save writers refuse a Battle with a waiting question.
The previous Continue save stays (a battle question: the playthrough before the battle, or the last turn's save; a scene
question: the state before the scene), so Continue asks the question again.

**Scenes** (`runScene`): `runScene(db, pt, which, lines)` takes the first offered answer (a `Say` line with the question).
`runScene(db, pt, which, lines, answers)` stops at the first unanswered question: its `SceneLine` (kind `Choice`, `text`,
`answers`, speaker) is the last line, `waiting` is true, the playthrough is as the scene left it and the phase has not moved.
To go on the caller runs the scene again from a copy of the playthrough as it was before it with the answers plus the new one:
the same lines come back (answered questions as `Choice` lines with `waiting` false; the player skips what it has shown), then
the rest.

**Simulator.** `ote-sim` (single maps and `--campaign`) always takes the first offered answer (documented default; no option, the
numbers of the demo games do not change).

**Player.** The question reads like any line in the dialogue box (same speaker / portrait handling, pages, typing). When its last
page is complete the answers come up in a panel directly above the box: Up / Down move the highlight (they wrap), Confirm
picks; a pointer over an answer highlights it, a tap on the highlighted answer picks it (a first tap on a touch screen only
highlights, so a slip of the finger does not decide the story). Answers wrap to the box's width; when four long answers would not
fit above the box on a small screen each gets fewer lines and the last line is cut with "..". There is no way out: Cancel, Menu,
the Skip tab, holding Cancel / Menu and the `skip` script token only show the whole question at once; skipping dialogue (in
a battle or a scene) stops at the next question. Hint line while the answers are up: "Up/Down + Z: answer   tap: pick, tap again:
confirm". Test scripts: the player logs `choice: ... N answers: 0@x,y 1@x,y` (row centres, logical pixels) and `choice: battle|scene
answer N`; `tests/choice_player_test.py` and the game `tests/fixtures/choice-story` use them.

## Strategy-RPG additions

A unit placed as a character is known to events and objectives by the character's id, so conditions and commands that name a unit work
for characters without change. New commands and conditions (promote, teach or forget a skill, "has skill", "is promoted"), a `support`
trigger and support points as a campaign effect, summons and promotion in the event stream the player animates, and campaign roster
behaviour for characters are specified in docs/SRPG.md (the "Story" part of each section, "Events (what the player animates)").

## Editor

- Map editor: unit **id** field; an **Events** panel (list; per event the
  trigger, conditions and a command list with add / reorder / delete, each
  command a small form); **Objectives**.
- Game screen: switches and variables (names).
- Art: portraits.
- **Campaign** screen: chapter list in order, each with its map and the
  scenes before and after.
- Dialogue choices: the command **Ask a question** (docs/EDITOR_UI.md).
- Problems: unknown ids, events that can never fire, a chapter without a
  map.

## Not in this version

Events outside battles other than chapter scenes (no
walking around towns); common events shared between maps; a preparations
screen; world maps; cut-scene unit animation beyond moving; Lua event
commands; voice.

## Build order

1. Core: named units, switches / variables, events, objectives, saves,
   tests; `ote-sim` runs them. **Done** (engine/core/data/events.h and
   loader_events.cpp / loader_story_*.cpp; engine/core/battle/story*.cpp,
   objectives.cpp, save_story.cpp; tests/test_story_*.cpp).
2. Player: dialogue box, Talk, objective display. **Done** (see "Dialogue
   (player)"; tests `test_dialogue_text.cpp`, `test_dialogue_box.cpp`,
   `test_portraits.cpp`).
3. Editor: events, objectives, switches / variables, portraits. **Done**
   (docs/EDITOR_API.md "Story data"; `editor/web/src/screens/maps/editor/story/`).
4. Campaign: core roster and progress, saves (**done**: engine/core/campaign/, tests/test_campaign_*.cpp,
   `ote-sim --campaign`), and the player (game page, scenes, chapter flow, campaign saves; see "Campaign (player)").
5. Editor: Campaign screen (not built yet); demo content **done**
   (`demo-srpg`: opening scene, reinforcements, a recruit by Talk, a fort
   reward, boss lines, and the campaign "The Ford War" with a second
   chapter, `maps/north_road.json`, `data/campaign.json`).
