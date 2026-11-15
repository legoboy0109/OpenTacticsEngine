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

Status: contract written; nothing built yet. Build order at the end.

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
