# The editor's tutorial ("Learn")

Contract for the lessons that teach a new game maker how to use the editor.
Owner decisions (2026-11-15):

- It teaches **the editor** (tools for tutorials inside games are a
  different, later topic).
- **Hands-on steps**: a side panel gives one small task at a time, points at
  the control to use, and ticks the step off when the maker has really done
  it. A lesson ends with something that works.
- First version: one beginner lesson **"Your first game"** plus short
  **topic lessons** picked from a list (structures, items, story events,
  economy, sound, art, hexagons). Lessons can be added over time.
- The first time the editor opens it **offers** the beginner lesson (with
  "Not now"); afterwards everything is behind a **Learn** button. Progress
  is remembered.

Status: step 1 of the build order is built (framework + lesson 1 "Your first game"; how it is built: docs/EDITOR_UI.md "Learn", the
state file: docs/EDITOR_API.md). Topic lessons 2 to 8 are not written yet; "How to write a lesson" at the end is for them.

## How a lesson works

- A lesson is **data** (`editor/web/src/learn/lessons/<id>.ts`): id, title,
  one-line description, rough length in minutes, the template it starts
  from, and an ordered list of steps.
- **Practice game.** Starting a lesson makes a new game from the lesson's
  template through the normal "new game" call, named after the lesson
  ("Practice: Structures"; a number is added if the name is taken). The
  maker's own games are never touched. The practice game is an ordinary
  game: it stays in the list and can be kept, edited or deleted. Resuming a
  lesson reopens its practice game; if that game is gone the lesson offers
  to start again.
- **A step** has: a short title, one to three plain sentences (what to do
  and why), optionally where it happens (a screen of the editor; the panel
  offers "Take me there" when the maker is elsewhere), optionally the
  control to point at, and a **done test**.
- **Done tests** look at real state, never at clicks: the game document in
  the store (for example "a map has a road terrain on at least 3 cells",
  "a unit of side blue stands on the map", "structure kind X has
  capturePoints > 0"), the current screen, or a named editor signal for
  things that leave no trace in the document (`played`: the Play panel
  reported the player ready; `saved`). A step that is already true when it
  is reached is ticked at once and the lesson moves on. Tests are pure
  functions of `(document, screen, signals)` so they can be unit tested.
- Steps are done **in order**; only the current step is checked. Undoing
  work after a step was ticked does not untick it.
- **Pointing**: the control named by a step gets a highlight ring and the
  panel says where it is. Controls are named by a stable attribute
  (`data-learn="<name>"`, added where needed; never by text or position).
  If the control is not on screen the highlight is simply absent; nothing
  breaks.
- The maker is never blocked: every control of the editor keeps working
  during a lesson, the panel can be collapsed, and a lesson can be left
  ("Stop lesson") and resumed later. "Skip this step" exists for a step the
  maker cannot or does not want to do; a lesson finished with skipped steps
  still counts as finished.
- At the end: a short "what you made" summary, a button to press Play if the
  lesson has something to play, and "Next lesson" suggestions.

## Where it lives in the editor

- **Learn** button in the top bar of every screen and on the game list. It
  opens the lesson list: each lesson with its title, description, length
  and state (not started / step 3 of 9 / finished), with Start, Continue or
  Start again.
- **Lesson panel** while a lesson runs: docked at the side on wide windows,
  a bottom sheet on phones (the editor is used on phones too); shows
  lesson title, progress, the current step, the ticked steps collapsed
  above it, and Take me there / Skip this step / Stop lesson.
- **First run**: when the editor has no saved Learn state at all, the game
  list shows a card offering "Your first game" with Start and Not now.
  Either choice is remembered; it is never offered again by itself.

## Remembered state

Kept by the editor server so it follows the maker to another browser or
device: one JSON file next to the editor's other user data (never inside a
game folder, never in the repository), read and written through
`GET /api/learn` and `PUT /api/learn` (docs/EDITOR_API.md). It holds, per
lesson: state, current step, skipped steps, the practice game's id; plus
"the first-run offer was answered". A missing or damaged file means "no
state" and is never an error. Tests point the server at a temporary file.

## Lessons of the first version

1. **Your first game** (template: blank): name the game; paint terrain on
   the first map; make a unit type; put units of two sides on the map;
   give a unit a weapon; press Play and move a unit; look at "No problems".
2. **Structures**: make a structure kind that can be captured and earns
   funds; place it; give one to a side; Play.
3. **Items**: a usable item, give it to a unit, a shop or drop.
4. **Story events**: an opening dialogue, an event when a unit reaches a
   place, an objective.
5. **Economy**: building units at a structure, an upgrade or a research
   topic, fog of war on a map.
6. **Sound**: map music and replacing one sound.
7. **Art**: give a terrain and a unit a picture; a joined road.
8. **Hexagons**: start from a hexagon template, paint, place a
   seven-hexagon structure.

Every lesson is short (about 5 to 10 steps) and its text is plain language
for people who do not program. Lesson texts are reviewed by the owner.

## Tests

- Unit: every done test of every lesson against small documents (true and
  false cases); the lesson runner (order, auto-tick, skip, resume, practice
  game gone); the state file round trip.
- End to end: each lesson is played through by a script that does what the
  steps ask, in the real editor, asserting each step ticks only after its
  action, and that the finished practice game loads in the real engine.
  First-run offer (Start, Not now, never again), resume after reload, on a
  phone-sized window.

## Build order

1. **Framework + "Your first game"**: lesson data format, runner, panel,
   pointing, Learn list, first-run offer, server state, tests, and a short
   "how to write a lesson" section at the end of this file.
2. **Topic lessons** 2 to 8, written on the framework (can be split between
   people once step 1 is merged).

## How to write a lesson

You need no knowledge of the framework's code. A lesson is one TypeScript file of data plus a unit test and an e2e test.

### 1. The file

`editor/web/src/learn/lessons/<id>.ts` exports a `Lesson`; add it to the list in `lessons/index.ts` (the Learn list shows lessons in that
order, and only those). The model to copy is `lessons/first-game.ts`.

```ts
import { addedCount, structuresOnMap } from '../helpers'
import type { Lesson } from '../types'

const START = { structures: [] as string[] }     // what the template begins with (see "Done tests")

export const structures: Lesson = {
  id: 'structures',                               // stable; used in the saved state
  title: 'Structures',                            // the practice game is named "Practice: Structures"
  description: 'Make a building that can be captured and earns funds, and put it on the map.',
  minutes: 6,                                     // rough, shown in the list
  template: 'economy-tactics',                    // a template id: blank, strategy-rpg, economy-tactics, hex-tactics-pointy, hex-tactics-flat
  playable: true,                                 // the end offers a Play button
  summary: ['A building kind that earns funds.'], // "What you made", one short sentence each
  steps: [
    {
      id: 'new-kind',                             // stable within the lesson; "skipped" is remembered by it
      title: 'Make a building',                   // short; shown in the panel and on the folded bar
      text: 'Buildings stand on the map. On the Structures screen, click New structure and give it a name.',
      where: { section: 'structures' },           // the screen it happens on (optional)
      point: 'new-item',                          // data-learn name of the control to ring (optional)
      done: (doc) => addedCount(doc.structures, START.structures) >= 1,
    },
    // ...
  ],
}
```

Step fields: `id`, `title`, `text` (one to three plain sentences: what to do and why), `where` (`{ section }`, optionally `item: (doc) => id` to
open one entry of that screen: a map, a unit; used by "Take me there"), `point`, `done` and `settleMs` (below). 5 to 10 steps per lesson, one
small task each. The lesson must end with a game that loads with no problems and, if `playable`, one that Play starts.

Where things go: the panel says "This happens on the X screen." from `where`; **Take me there** opens it; while the maker is on another
screen of the practice game the ring points at the sidebar link of `where.section`, and on that screen at `point`. A step with no `where`
(Play, the Problems badge) works from any screen. When the control is inside something collapsed, only the text can say so ("open the Map tab"):
the ring is simply absent when its control is not on screen.

### 2. Done tests

`done: (doc, screen, signals) => boolean` is a **pure function** of the game document (`ProjectDoc`, `editor/web/src/model/types.ts`), the current
screen (`'games'` or a section name) and the editor signals seen so far in this lesson. It looks at real state, never at clicks. The runner calls
it after every change; only the current step is tested; a step that is already true when it is reached ticks at once; nothing is ever unticked
(undoing later changes nothing).

Rules that keep ticks honest:
- **Nothing may be true at the start.** Write down what the template begins with (`START` above: the ids of its terrain, unit types, weapons...)
  and test for something *beyond* it ("a unit type that is not `soldier`"). The unit test checks this for every step.
- **Make it hard to tick by accident**: "pick a tool" is not the work, "a unit of the new type stands on the map for side Blue" is.
- **Do not depend on how the maker did it**: any way of getting the document into the right state counts.
- **Typing**: a step done by typing a name should set `settleMs` (for example 1200): the test must stay true that long before it ticks, so
  the lesson does not move on at the first letter.
- **Things with no trace in the document** (Play, opening a panel) use a signal: `done: (_d, _s, signals) => signals.has('played')`.

Helpers (`learn/helpers.ts`; maps default to the game's first map):
`nameChanged(doc, notPrefix)`, `addedCount(list, startIds)` (entries beyond what the template began with: `doc.terrain`, `doc.units`, `doc.weapons`,
`doc.structures`, `doc.items`...), `cellsOfTerrain(doc, terrainId, mapId?)`, `cellsOfOtherTerrain(doc, excludeIds, mapId?)`,
`unitsOnMap(doc, { side?, type?, notTypes? }, mapId?)`, `structuresOnMap(doc, { type?, side? }, mapId?)`,
`unitTypesWithWeapon(doc, excludeIds?)`, `mapOf(doc, mapId?)`, `firstMapId(doc)`. Anything else is a few lines over the document; if you write
a helper another lesson would use, put it in `helpers.ts` with a unit test in `tests/unit/learnHelpers.test.ts`.

### 3. Naming a control: `data-learn`

The ring finds a control by `data-learn="<name>"`, never by its text or position. Put the attribute on the control (it renders onto the element;
nothing else changes). The existing names: `nav-<section>` (every sidebar link, e.g. `nav-maps`), `new-item` (the "New ..." button of a list screen),
`game-name`, `unit-weapons`, `tool-<tool>` (the map toolbar: `tool-terrain`, `tool-units`, `tool-structures`, `tool-heights`, `tool-select`),
`terrain-picker`, `play`, `problems`, `learn`.
To add one: pick a short lowercase name for *what the control is* (`structure-picker`), add `data-learn="structure-picker"` on the element
(`TextInput` and `FieldGroup` / `MultiSelect` accept it as a prop: `data-learn` / `learn`; for another component add the same optional prop and pass
it to its root element), then use it as a step's `point`. Names are never renamed afterwards (lessons and tests use them). The first visible element
with the name is ringed, so use a name once per screen.

### 4. Adding a signal

`learn/signals.ts`: add the name to the `SignalName` union and call `emitSignal('name')` where the thing happens (for example `emitSignal('played')`
in `play/PlayPanel.tsx` when the player reports ready). Signals seen while a lesson runs are saved with its progress. That is all.

### 5. Wording

Every text is reviewed by the owner. Plain, short, friendly, second person, for people who do not program: say "click", "screen", "square"
("hexagon" in hexagon lessons); never "tile", "id", "expression", "formula", "script"; call the visual styles "style", never "look". Use the
words the editor itself shows (Terrain, Units, Weapons, "New unit", "Take me there"). A title names the task ("Make a weapon"), a text says what to do and
why in one to three sentences. Do not explain more than the step needs.

### 6. Testing a lesson

**Unit** (`tests/unit/learn<Lesson>.test.ts`; the model is `learnFirstGame.test.ts`): load the template (`loadTemplate('blank')` from
`tests/unit/fixtures.ts`), `structuredClone` it, and for every step check: false at the start (the test loops over all steps), false for the near
misses a maker could make, true once the work is done, and that `where.item` picks the right entry. Also check the lesson's data is well formed
(unique step ids, the template exists, no forbidden words).

**e2e** (`tests/e2e/learn-<lesson>.spec.ts`; the model is `learn-lesson.spec.ts`; helpers in `tests/e2e/learnkit.ts`): import `test` from
`learnkit` (each test gets its own helper process with an empty temporary tutorial state; never the real user data folder) and play the lesson through
doing what each step says: `expectCurrent(page, title)` before the action (the step is not ticked), the action, then
`expectTicked(page, title, nextTitle)`; `expectRingOn(page, name)` where the ring matters. To start in the middle seed the state:
create the practice game through `POST /api/projects` and `PUT /api/learn` (see `seed` in `learn-state.spec.ts`). End with
`expectLoadsInEngine(lh, practiceId)` (the real loader finds no problem and the native player starts the game). Use a 1920 x 1080 window
for a long run (the panel docks at the side; below 1800 px it is a bottom sheet over the lower part of the window).

```
cd editor/web && npm run typecheck && npm run build && npm test
OTE_EDITOR_BIN=../../build/ote-editor OTE_PLAYER_DIR=../../build-web/engine/player npx playwright test learn --project chromium
```
