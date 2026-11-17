# Browser editor UI spec (first slice)

Audience: novices who never want to script. Plain language everywhere; the
words "JSON", "id", "Lua", "formula" appear only inside "Advanced"
disclosures. Layout and wording are the owner's to change — keep text in
one place per screen so it is easy to adjust.

Tech: `editor/web/` — TypeScript, React, Vite. Minimal dependencies
(react, react-dom; dev: vite, typescript, vitest, playwright). No UI
component library; small own components + plain CSS with CSS variables
(light/dark from `prefers-color-scheme`). Talks to the helper only through
docs/EDITOR_API.md. Built output in `editor/web/dist` (not committed).

## Document model

The whole project is one in-memory document, exactly the GET shape
(`game`, `terrain`, `weapons`, `units`, `maps`, `scripts`). All edits are
pure functions `doc -> doc` so undo/redo is a snapshot stack (cap 200).

- **Autosave**: 800 ms after the last change, PUT with `baseRevision`;
  top bar shows "Saved" / "Saving…" / "Unsaved changes" / "Could not save".
  Also save before Play and on page hide. Never lose edits: if a save
  fails keep retrying with backoff and keep the edits in memory.
- **Conflict (409)**: banner "This game changed on disk (another program
  or device edited it)" with **Reload** (drop local changes) and **Keep
  mine** (re-read the revision, save again).
- **Undo/redo**: Ctrl/Cmd+Z, Ctrl/Cmd+Shift+Z and top-bar buttons. Typing
  in one field coalesces into one undo step.
- **Problems**: the list returned by every save; badge in the top bar,
  panel listing them; each problem links to the section that owns its file
  (data/units.json → Units, maps/x.json → that map, game.json → Game/Rules).
- **Ids**: generated once from the name when a thing is created (slug,
  unique within its list: "Iron Sword" → `iron_sword`, then `iron_sword_2`).
  Renaming a thing changes only its display name. Ids are shown (read-only)
  under Advanced. Deleting a thing that is used elsewhere asks first,
  listing where it is used, and removes those references in the same undo
  step (unit removed from terrain "produces", weapon lists, versus tables,
  map units; terrain replaced by the first remaining terrain on maps; a
  stat removed from every unit, etc.). Never leave dangling references.
- **Editor-only data** lives under `game.editor` (ignored by the engine):
  chosen formula presets and their parameters.

## Screens

**Project picker** (`/`): a card per game (name, description, "N maps",
a warning mark if it has problems) and **New game**: name + choose a
starting point (the templates, as cards with their descriptions) → creates
the project (id = slug of the name) and opens it.
Each card of the maker's own games has **Delete** (owner, 2026-11-15: delete
for good, demos protected): a dialog says what goes and that it cannot be
undone, and the button works only once the game's name is typed (capitals
forgiven). The demos that come with the engine have no Delete. Files:
`screens/picker/DeleteGameDialog.tsx`, helper `project_delete.cpp`.

**Shell** (`/p/<id>/<section>`): top bar = game name (back to picker),
save status, undo/redo, problems badge, **Play**. Left sidebar = Game,
Rules, Units, Weapons, Terrain, Maps. Below 720 px the sidebar becomes a
compact section switcher and forms go single-column. Every list screen is
"list on the left, form for the selected item on the right" with **New**,
**Duplicate**, **Delete** and drag-or-buttons reordering.

**Game**: name, description, author. Text font (built-in fonts from
`/api/fonts`, size, smooth toggle; "Default pixel font" = no font key).
Visual styles (list: name, background colour, show terrain letters,
per-terrain colour overrides with colour pickers). Advanced: screen size
and tile size.

**Rules** (grouped cards, each with one sentence of help):
- *Stats*: list of stats (name); "Which stat is health?" and "Which stat
  is movement?" selectors. *Resources* (e.g. Funds). *Movement types*
  (e.g. Foot, Wheels).
- *Battle*: counterattacks on/off; how a side loses (checkboxes: "all its
  units are defeated", "its leader is defeated", "its headquarters is
  captured").
- *Features* (toggles revealing their settings): Income from buildings;
  Building units (new units can act immediately on/off); Capturing
  buildings (capture speed preset); Experience and levels (XP per level,
  maximum level, XP rewards).
- *Combat maths* — presets instead of formulas. Each preset is a named
  choice with a one-line explanation and a few selectors; the UI generates
  the expression, shows a plain-language summary ("Damage = attacker's
  Strength + weapon Might − defender's Defense − terrain defense") and
  stores preset + parameters in `game.editor.formulas`. A formula in the
  file that matches no stored preset shows as "Custom" with the raw text
  under Advanced (never silently overwritten).

| Formula | Presets (parameters) → generated expression |
|---|---|
| damage | **Attack minus defense** (attack stat A, defense stat D, weapon bonus on/off, effectiveness on/off, terrain defense on/off) → `(a.A + (w.might or 0) * vs) - b.D - (tb.defense or 0)` with the optional parts dropped when off (`vs` multiplies the weapon bonus; without weapon bonus it multiplies `a.A`). **Damage chart** (terrain reduces damage on/off) → `vs * (a.hp / a.maxhp) * (1 - 0.1 * (tb.defense or 0))`; explains that each weapon's "Effectiveness" table then holds the damage percent against each unit. **Fixed amount** (N) → `N`. **Custom**. |
| hit | **Always hits** → key removed. **Weapon accuracy** → `(w.hit or 100)`. **Accuracy against evasion** (skill stat S, speed stat P, terrain avoid on/off) → `(w.hit or 100) + a.S * 2 - b.P * 2 - (tb.avoid or 0)`. **Custom**. |
| crit | **No critical hits** → key removed. **Fixed chance** (N%) → `N`. **Based on a stat** (stat S, divisor) → `a.S / K`. Plus "critical hits multiply damage by" (critMultiplier). **Custom**. |
| strikes | **One attack** → key removed. **Attack twice when faster** (speed stat P, lead needed N) → `(a.P - b.P >= N) and 2 or 1`. **Custom**. |
| progression xpHit / xpKill | **Fixed amounts** (hit N, kill M) → `N` / `M`. **More for stronger enemies** (base, per level) → `B + math.max(0, b.level - a.level) * K`. **Custom**. |
| capture amount | **Instant** → `9999`. **Faster with more health** (divisor K) → `a.hp / K`. **Fixed points per turn** (N) → `N`. **Custom**. |

**Units**: name, map letter (one character), movement type, a stats table
(value per stat; a growth % column when Experience is on), weapons
(multi-select), cost per resource (when Building units is on), "can
capture buildings" (when Capturing is on), tags (free chips; explained as
"labels weapons can be strong or weak against").

**Weapons**: name, range (min–max), "can strike back when attacked", "can
attack after moving", numbers used by combat maths (a small key/number
table; offers `might` and `hit` when the chosen presets use them),
effectiveness: rows of (unit or tag → number) plus "against everything
else" (on → number; off → cannot target others). Label and help text adapt
to the damage preset ("multiplier" vs "damage percent"). Advanced:
per-weapon damage override.

**Terrain**: name, colour, letter, movement cost per movement type (number
or "cannot enter"), numbers used by combat maths (`defense`, `avoid`),
"can be captured" (points), income per resource, "units can be built
here" (multi-select), "counts as headquarters" (the `hq` tag), other tags
under Advanced.

**Maps**: map list (new, rename, duplicate, delete) + the map editor:
- Canvas grid drawn like the player's placeholder look (terrain colour +
  letter, owner border, unit block with letter in side colour, leader
  mark). Zoom and pan (wheel/pinch, drag with middle button or two
  fingers), fits to the available space initially.
- Tools: **Terrain** (palette; click/drag paints; fill tool; pick with
  Alt-click), **Units** (choose side + unit type; click places; clicking
  an existing unit selects it → name, level, leader, stat overrides;
  drag moves; eraser removes), **Owners** (choose side or neutral; click a
  capturable/owned tile), **Select** (inspect a tile).
- Map settings: name, size (resize with an anchor choice; warns before
  cropping units), sides (name, colour, played by Human/Computer, team,
  starting resources; add/remove with at least two).
- Every paint stroke is one undo step.

**Play**: saves, then opens a panel with the embedded player
(`/player/…` in an iframe) running the current project on the map being
edited (or the first map), with **Restart** and **Close**. If the project
has problems, Play explains them instead of starting.

## Quality bar

Keyboard accessible, labelled inputs, visible focus, works at 360 px
wide, no layout shift while typing, no data loss on reload (state is the
saved project; unsaved edits are flushed on page hide). Unit tests
(vitest) for the pure logic: slugs/ids, preset → expression and
expression → preset detection, reference clean-up on delete, document
edit functions, map resize. End-to-end tests (Playwright, headless
Chromium) against a real `ote-editor` on a temp copy of the games folder.

## Implementation notes

Deviations and additions made while building the first slice:

- **Preset recognition is broader than "stored preset only".** Besides the preset stored in
  `game.editor.formulas`, any expression that matches a preset's generated shape (ignoring spaces and the
  `or 0` / `or 100` defaults) is recognised, so the templates' hand-written formulas show as presets rather
  than "Custom". The stored preset wins when two would give the same text.
- **Removed stat in a preset**: that choice is blanked, the expression uses `0` in its place (the game still
  loads), and the Rules screen shows a warning until a stat is picked again. Hand-written (custom) formulas
  that mention the stat are only listed in the delete confirmation.
- Deleting the last unit, terrain, map or movement type is refused (the loader requires at least one).
- Autosave sends only the sections that changed since the last save; "Keep mine" sends everything loadable.
  Sections that failed to load (null) are never sent. Page-hide saves use `fetch` keepalive and fall back to a
  normal request above 60 KB.
- Stat ids avoid the names the engine reserves in formulas (`hp`, `maxhp`, `level`, `xp`, `type`, `name`, `tags`).
- Per-terrain "numbers used by combat maths" and weapon numbers offer `defense`/`avoid` and `might`/`hit` based
  on the chosen presets; capture, income and build lists stay visible with a hint when their feature is off.
- The first effectiveness row added to a weapon that could hit anything also sets "everything else", so the weapon
  does not suddenly lose targets.

### Map editor and Play (second slice)

Code: `editor/web/src/screens/maps/editor/` (canvas, tools, panels), pure edits in `src/model/map*.ts`,
Play in `src/play/`. Decisions and deviations:

- **The legend is invisible.** Maps are edited as a grid of terrain ids; `legend` + `tiles` are rebuilt on every edit
  (`model/legend.ts`, `model/mapGrid.ts`). Characters already in the file are kept; a terrain new to the map gets its own
  glyph/letter if free, else the first free character of a fixed pool (87 symbols; never space, `"` or `\`). Legend entries
  for terrain no longer used on the map are dropped on the next edit. Deleting a terrain rewrites the maps through the same
  code, so no two symbols ever mean the same terrain. Unknown keys on maps, sides, units and owners survive every edit.
- **Damaged maps** (wrong row count/length, unknown symbol, legend naming a missing terrain, bad heights) show the exact
  problems and a **Repair this map** button instead of the editor; the repair is one undo step.
- **Impassable terrain under a unit**: painting is allowed and the unit is flagged (red corner on the canvas, a line in the
  checks list, the helper reports it as a problem); nothing is deleted. Placing/dragging a unit onto terrain it cannot enter,
  and changing a unit's type to one that cannot stand there, is refused with a one-line explanation. Placing on an occupied
  square selects the unit there instead.
- **Selection is a square, not a list index**, so it survives undo. Stat overrides are stored only when they differ from the
  unit type's value; typing the base value (or clearing the box) removes the key; changing the unit type drops overrides that
  equal the new type's values. Name, level and leader are stored only when meaningful.
- **Undo**: one drag (paint, owners) = one history step through a `stroke:<n>` coalescing key, which merges however long
  the pointer rests (ordinary typing keys still merge only within 1.5 s). Fill, place, move, remove, resize are single steps.
- **Touch**: the first action of a touch is held until it is clear whether it is a tap, a drag, a long press or the start
  of a two-finger pan/pinch (no timer is involved except the 500 ms long press, which picks terrain with the Terrain tool).
  Mouse acts immediately. Canvas uses `touch-action: none`, so the page never scrolls while drawing.
- **Keyboard**: the canvas has a cursor (arrows, Shift+arrows = 5 squares, Enter or Space applies the tool, Delete removes
  a unit, Esc deselects). Space pans only while the mouse is over the canvas; with the keyboard alone it applies the tool.
  Tool/palette buttons are individually tabbable `radio` buttons (no arrow-key roving inside a group).
- **Layout**: the zoom buttons live in the toolbar (floating buttons covered squares on phones). Below 680 px of column
  width the key hints are dropped from the tool buttons (the line under the map lists them). The map list column starts
  hidden below 1500 px window width (a "Show map list" button brings it back; the choice is remembered) so the canvas gets
  the width. On phones the canvas and tools stay pinned at the top while the options sheet scrolls; the sheet can be folded.
- **Remembered per browser** (localStorage, optional): chosen tool, fill mode, last terrain/unit type/side, options sheet
  open/closed, map list shown/hidden.
- **Checks** (`model/mapChecks.ts`) are friendly hints, separate from the helper's problems. Square coordinates in them
  and in the status line count from 1; the helper's own messages count from 0.
- **Rendering** reproduces the game's placeholder look but ignores Visual styles (always the terrain's own colour/letter);
  heights are preserved and resized but not drawn or edited.
- **Play**: Play flushes the save, refuses with the Problems panel ("Fix these before playing.") when the saved game has
  problems, then does a HEAD request for `/player/ote-player.wasm` (404 = player not built: a dialog shows the build
  command), then opens a full-size dialog with the embedded player. It runs the map being edited, else the first map; the
  header map picker and Restart both save first. The panel listens only to messages from its own iframe, shows a loading
  veil until `ote-player-ready`, and a readable error with **Back to editing** on `ote-player-error`. Esc closes the
  panel only while focus is outside the iframe (inside it, Esc belongs to the game).
- **Phone top bar** is one row: icon-only Undo/Redo/Play, the problem count as a badge on the problems button, the save
  state as a dot (the words stay for screen readers).

### Art (third slice)

Code: pure edits in `editor/web/src/model/graphics*.ts` (+ `pickCell.ts`, `gridGuess.ts`, `artFiles.ts`), the image files and
the upload-safe saver in `src/store/art.ts` + `autosave.ts`, drawing in `src/art/` (shared by the map canvas, the palettes and
every preview), the Art screen in `src/screens/art/`, the Picture card and picker in `src/ui/picture/`. Wording: `screens/art/text.ts`
and `ui/picture/text.ts`. Decisions and deviations:

- **Uploads never cause a false conflict.** Uploading, replacing or deleting an image changes the project revision outside a save.
  These calls run through `Saver.withDiskChange`: it waits for a save in flight, no save starts while it runs (autosaves wait,
  then go on with the new revision), a cheap check (a PUT with no sections) tells a real outside change from our own, and the
  revision the call returns is adopted. If the check (or an earlier save) found a real outside change, the file is still
  uploaded but the revision is NOT adopted and the usual conflict banner stays. `flush()` (Play, leaving) waits for a running
  upload. Limitation: a change made elsewhere in the few milliseconds between the check and the upload is not detected.
- **Undo past the first image.** Back at "no art" after art was saved, the saver writes `{ "sheets": [] }` (a `null` section is never
  sent, so otherwise the old images would stay on disk while the screen showed none). Files are never touched by undo.
- **A damaged `data/graphics.json`** (helper reports `null` plus a problem on that file) blocks the Art section with an explanation,
  so a first edit cannot overwrite the file.
- **Adding images.** The file chooser and drag-and-drop accept any file the browser can open (SVG, JPEG, GIF = first frame, ...);
  PNGs are uploaded byte for byte, others are drawn on a canvas and sent as PNG. The 2048 x 2048 limit and 8 MB are checked before
  uploading; the helper's own errors are shown in plain words. The image's display name is the original file name without its
  extension; the file name on disk is its slug (made unique); the internal name is a slug of the display name.
- **Division.** "One picture" = the piece covers the whole image; switching to a grid starts from the tile size when it divides the
  image, else 16, 32, 24, 48 (same rule as the first guess). Pieces keep their positions when the grid changes; a banner names the
  terrain and units whose pieces would fall outside (also before **Replace file…**, which asks for confirmation because the old
  file is gone for good). Quick sizes are square 16 / 24 / 32 / 48.
- **Removing an image** asks only when something uses it; the file moves to "Unused files". There each file can be **Use again**
  (added back as an image) or deleted for good (confirmation; the editor saves first, because the helper refuses while the saved
  game still uses it, and a refusal is explained).
- **Per-side pictures** need the unit's own picture first (the file format has no side pictures without one); changing the unit's
  own picture keeps its side pictures; removing it removes them. Side ids that no map has any more but that still have a picture are
  listed (marked) so they can be removed. A look's entry for a unit replaces the whole entry, side pictures included, like the engine.
- **Looks.** The picker and card say "look"; the Game screen says "style" (its existing wording). The map's Look selector is shown
  with two or more styles; the canvas always shows the first style until another is picked (what the player starts with), so a
  game with one style now shows that style's colours, background and letters too. The choice is remembered per game.
- **Drawing.** The map canvas, the palette chips, the card samples and the picker preview all use `drawMap` (previews are tiny
  generated maps), so they cannot differ. Join pieces, random mixes (`cellHash`, ported with `Math.imul`, pinned by 16 values printed
  by the C++ function in `tests/unit/pickCell.test.ts`) and animation follow `engine/core/data/graphics.*`. The unit base follows
  the player's code (`engine/player/battle/sprites.cpp`: a sixth of the square of fill, one-pixel dark outline, one pixel inset),
  scaled with the zoom. The HP bar and the "already acted" darkening of the player do not exist in the editor; the leader pip keeps
  its top-left place on the square (not on a tall sprite). Units are sorted by row only (ties keep file order). With art on the map
  the faint square outline is drawn per square, only for squares without a picture; a map without any art keeps the old
  single-pass path (same speed as before).
- **Animation** runs on the page's own clock (about 20 redraws a second, only while the tab is in front and only when an animation
  is on screen); under "reduce motion" every animation shows its first frame.
- **Piece grid** (`ui/picture/PieceGrid.tsx`): one canvas with zoom buttons (25 % to 1600 %), scrolling inside its box, keyboard
  cursor (arrows + Enter/Space). It starts at the whole image across the box; in the picker, pieces smaller than 32 px start zoomed
  in (up to 4x) so they can be tapped. The guide image for joining pieces is generated in the browser at the image's piece size.
- **Perf.** A 200-square stroke on a 64 x 64 map: about 32 ms without art (unchanged code path), about 41 ms with art on every
  square type (random mix, joins, animation, sprites), measured in headless Chromium with `tests/e2e/art-perf.spec.ts`.
- **Tests start from art-free games** (`bareGame`/`seedArt` in `tests/e2e/artkit.ts`) because the templates now carry starter art;
  the Play tests use them too, since animated starter art keeps the picture from "settling".

### Structures (fourth slice)

Code: pure edits in `editor/web/src/model/structures*.ts` (kinds, `structuresEffective.ts` = the engine's combination rule,
`structuresRefs.ts` = cascades, `structuresConvert.ts` = old projects) and `model/mapStructures.ts` (placing on maps); the screen in
`src/screens/structures/`; the map tool in `src/screens/maps/editor/` (`tools.ts`, `panels/StructureTool.tsx`, `panels/Inspect.tsx`,
`drawStructures.ts`); the offer to convert in `src/shell/ConvertOffer.tsx`. Wording: `screens/structures/text.ts`, the map editor's
`text.ts` and `shell/convertText.ts`. Decisions and deviations from the spec above:

- **Structure kinds.** A new kind gets the next palette colour and its first letter; the size boxes write `width` / `height` only above 1
  (1 is the default). Making a kind bigger leaves placed ones at their top-left corner; the map checks then say "does not fit there any more"
  (outside the map or overlapping) because the game would refuse to load it. Movement: "Same as the ground" removes the key, "A cost of its own"
  needs at least 1, "Cannot enter" is 0. Numbers: an empty box means the ground's number; the numbers offered are the ones the damage / hit
  formulas use (like Terrain) plus free-form ones.
- **Settings of rules that are off.** The capture, headquarters, income and build-list settings are shown when their rule is on OR the structure
  already has that setting (then with the old "switched off, so no effect yet" hint, so nothing is hidden that is in the file). With the rule off
  and nothing set they are not shown; one line names what is hidden and links to Rules. Headquarters follows "its headquarters is captured" being
  a way to lose.
- **Owners tool replaced, key O kept.** The Structures tool has the owner choice (Nobody or a side) for the structures about to be placed. A click on
  an empty square places with the top-left corner there; a click on a placed structure selects it (its whole footprint gets the selection ring) and
  a drag moves it, keeping where inside the footprint it was grabbed. Owner, kind and Remove of the selected one are in the panel (changing an
  owner never needs a different tool). Delete removes the structure under the Structures tool and the unit under every other tool.
- **Refusals.** Beside the loader's rules (inside the map, no overlap) the editor also refuses a structure that would strand a unit (the unit's
  movement type cannot enter once the structure's costs apply) and, the other way round, a unit on a square the structure makes impassable. Each
  refusal is a plain sentence under the map; nothing is written. (Painting terrain under a structure is still allowed and flags a stranded unit,
  like before.)
- **The ghost.** The footprint follows the mouse pointer (and the keyboard cursor) and is red with a cross where the structure would not fit; it
  is not shown over an existing structure (a click there selects). Touch has no hover: a tap places or says why not, a drag from a placed
  structure shows the ghost while it moves. The canvas exposes the ghost as `data-ghost="kind,x,y,ok|bad"` for tests.
- **Drawing** follows `docs/STRUCTURES.md` and `engine/player/battle/structure_draw.cpp`: one pass of structures and units sorted by the row of the
  bottom edge (structures first on a tie, a unit on a structure square sorts with that structure's bottom edge, as the engine does), a picture
  across the footprint's width standing on its bottom edge (the owner's own picture when it has one), the block without a picture (inset one pixel
  of a 24 px square, soft top highlight, letter centred at 0.45 x the footprint's smaller side) and the owner frame around the whole footprint.
  Differences from the player: lengths scale with the zoom (the player works in whole pixels of its own tile); the letter is hidden at very small
  zoom like terrain letters; the move/attack highlight does not exist in the editor.
- **Styles.** `styles.<id>.structureColors` colours the placeholder block; the Game screen lists every structure per style next to the terrain colours.
  The map canvas and the palette chips use it. Per-style pictures for structures work like the other kinds ("No picture in this style" included).
- **Pictures.** The picker offers "One piece" and "Animation" for structures (the file format has no mix or join for them). "A different picture
  for an owner" works like a unit's per-side pictures. The card's preview shows the structure at its real footprint on the first terrain, once with
  and once without an owner frame, with up to four rows above for a tall picture. The Art screen lists structures among the users of an image.
- **Old projects (Convert).** The offer is a banner above every screen while the project has old terrain settings (`capture` or `capturePoints`,
  `income`, `produces`, tag `hq`) or a non-empty `owners` list; **See what will change...** opens the plain-language explanation with the real numbers
  (kinds, squares, owner marks). Converting is one edit, so one Undo restores the old project exactly (Undo past a first structure list saves an empty
  `structures` list, never leaves stale data). Details the spec left open: the structure keeps the terrain's id when free (else `id_2`...); one 1 x 1
  structure per square; owner marks on squares that do not become a structure (they never did anything) and marks naming an unknown side are left out
  and counted in the explanation; the terrain stays under the structure and keeps its movement costs, numbers and other tags; a style's colour for the
  terrain is COPIED to `structureColors` (the terrain colour stays); a single piece or animation picture of the terrain is copied to the structure, a
  random mix copies its first piece, a joining set is not copied. A damaged `data/structures.json` blocks the offer (the Structures section explains).
  Terrain that kept `capture` etc. is not rewritten any other way: until converted the engine reports its usual problem.
- **Saving.** `structures` is a section like `terrain`; a map's EMPTY `owners` list (older editors wrote one into every map) is left out whenever the map
  is written; a non-empty one is never dropped except by Convert.
- **Cascades.** Deleting a kind lists the maps with counts, removes the placed ones, its pictures and style colours in one step; deleting a unit,
  resource or movement type cleans the structures' build lists, income and movement rules; removing a side on a map makes its structures there
  nobody's (the confirmation says how many). Duplicating copies the pictures.
- **Select tool and checks.** "Ground", "Structure", "Owner" and "Unit" of a square, then the movement costs and numbers as the game will use them, each
  marked "(the structure)" where the structure sets it; the checks judge units with the same rule, flag structures that no longer fit, and look for
  an owned structure that builds units / the headquarters.
- **Phone.** With eight sections the switcher is two rows of four (one row clipped "Structures").
- **Other.** A tool remembered by an older editor under the name "owners" falls back to the Terrain tool. `MasterDetail` got an optional `note`
  (the one-line explanation under the Structures list). `LookKind` is `'terrain' | 'units' | 'structures'`.
- **Perf.** A 200-square stroke on a 64 x 64 map with 84 structures on it: 34 to 37 ms with placeholder blocks and 43 to 48 ms with pictures on
  terrain, units and structures (four runs of `tests/e2e/structures-perf.spec.ts` in headless Chromium; the same drag without structures: about 32 to 45 ms
  art-free and 41 to 49 ms with art, so structures cost nothing measurable).
- **Tests.** Unit: `structures*.test.ts`, `mapStructures.test.ts`, `drawStructures.test.ts`, the effective numbers pinned to cases from
  `tests/test_structures.cpp`, the conversion on an old-format fixture (`tests/unit/oldProject.ts`). e2e: `structures*.spec.ts` (kinds, map tool incl.
  pixel checks of the drawing rules, pictures, the conversion on a project written to disk in the old format, phone, perf, Play and `ote-sim` on the saved game).

## Art (third slice): images, tiles and sprites

Contract for the data and drawing rules: docs/GRAPHICS.md. Helper calls:
docs/EDITOR_API.md (`graphics` section, `/art` endpoints). Wording here is
a starting point; the owner decides final names and layout.

Words: the UI says **image** (a file), **piece** (one cell of an image),
**picture** (what a terrain or unit looks like). It never says "sheet",
"cell", "sprite" or "tileset" outside Advanced.

### Document model additions
- `graphics` joins the document (the GET/PUT section). `null` from the
  helper means "no art yet"; the first edit creates `{ "sheets": [] }`.
  All looks edits are pure `doc -> doc` functions and undoable.
- Image files are NOT in the document; they live on disk in `art/`.
  Uploading returns a new `revision`: the store adopts it without treating
  it as a conflict. Undo never deletes or restores files; it only changes
  which images the game uses. Files no longer used by any image entry show
  up under "Unused files" where they can be deleted for good (confirm).
- Image entries may carry `"name"` (display name; the engine ignores it).
- Deleting a terrain or unit removes its pictures (base and every style
  override, and its id from other terrains' `joinWith`) in the same undo
  step. Duplicating copies its pictures. Deleting a visual style removes
  that style's overrides.
- Problems in `data/graphics.json` link to the Art section.

### Art section (sidebar, between Terrain and Maps)
List on the left (thumbnail + name), details on the right.
- **Add image**: file picker (multiple) and drag-and-drop anywhere on the
  section. Any image the browser can open is accepted; anything that is not
  a PNG is converted to PNG in the browser first. File name on disk = slug
  of the original name, made unique. Errors in plain words ("This image is
  3000 x 1200 pixels. The largest that works on every device is 2048 x
  2048.").
- First guess for how the image is divided: if it is at most twice the
  game's tile size in both directions → **One picture**; otherwise **A grid
  of pieces** using the tile size if it divides the image evenly, else 16,
  32, 24, 48 (first that divides both sides), else the tile size.
- Details: name; "How is this image divided?" (**One picture** / **A grid
  of pieces** with piece width and height, quick buttons 16 / 24 / 32 /
  48); a zoomable preview on a checkerboard with the grid drawn over it and
  "N across, M down"; a note when pixels are left over at the right or
  bottom; **Used by** (links to the terrains and units using it);
  **Replace file…** (same name, `?overwrite=1`; warns that pieces keep
  their positions); **Remove** (asks first when used, listing users; clears
  those pictures in the same undo step; the file moves to "Unused files").
- Section setting: "Show a coloured base under units so sides can be told
  apart" (`unitMarker`).

### Picture card (on each Terrain and each Unit)
Shows a live preview and **Choose picture…** / **Remove picture**.
- Terrain preview: a small sample patch drawn with the real rules (for
  joining pictures a shape with a bend, a T, a cross and a dead end; for
  random mixes a 5 x 5 field). Unit preview: the unit on its coloured base
  in two side colours. Animations play (first frame only when the system
  asks for reduced motion).
- The picker dialog: choose an image, then click pieces on a zoomable grid.
  - Terrain kinds: **One piece**; **Random mix** (click several; numbered
    badges; click again to remove); **Animation** (click frames in order;
    speed Slow / Medium / Fast = 700 / 400 / 200 ms, exact number under
    Advanced); **Joins with neighbours** (hovering shows the 4 x 4 block
    that will be used, click to place it; a diagram explains which of the
    16 pieces goes where and **Download a guide image** saves a template
    PNG at the image's piece size, generated in the browser; "Also joins
    with" multi-select of other terrains; "Joins at the edge of the map").
  - Unit kinds: **One piece**, **Animation**.
- Units: "A different picture for a side" lists every side id used by the
  game's maps (with its name and colour); each can have its own picture.
- When the game has more than one visual style, the card has a "For style:"
  selector: **All styles** (the base picture) or one style. In a style:
  **Same as the others** (no override), a picture of its own, or **No
  picture in this style** (`null`).

### Map editor
- The canvas, the terrain palette and the unit palette draw with the art,
  following "Drawing rules" in docs/GRAPHICS.md exactly (stretch to the
  square, placeholder colour under transparent pixels, joining, random mix
  by position with the SAME hash as the engine, marker base, per-side
  pictures, units sorted top row first). Port `pickCell`'s hash to
  TypeScript and unit-test it against values taken from the C++ function.
- Animated pictures animate on the canvas only while the tab is visible and
  not under reduced motion.
- A **Style** selector in the map toolbar (only when the game has more than
  one visual style) previews a style: its art overrides AND its colours,
  background and letter setting (this closes the earlier gap "the canvas
  ignores visual styles").
- Things without a picture keep the placeholder look.

### Tests and screenshots
Unit tests for every model function (pictures per kind, cascade deletes,
used-by, first-guess division, the hash/pickCell port, the join mask). e2e
against the real helper: add a PNG made in the test, convert a non-PNG,
too-large image error, set the grid, assign each kind to a terrain and a
unit, per-side and per-look pictures, undo/redo, remove with users, unused
file delete, the canvas shows the art (pixel checks), Play shows the art in
the real player, phone layout. New screenshots continue the numbering.

## Structures (fourth slice)

Contract for data and rules: docs/STRUCTURES.md. Helper: the `structures`
section of GET/PUT project (docs/EDITOR_API.md). Wording is a starting
point; the owner decides final names and layout.

Words: the UI says **structure** (the kind: City, Fort) and, on a map,
"a City" / "this structure". Help text explains it once: "Structures stand
on top of the ground: cities, forts, factories, gates. The ground under
them stays what it is."

### Document model
- `structures` joins the document (list, like `terrain`). `null` from the
  helper = none yet; the first edit creates the list.
- Maps gain `structures: [{ type, x, y, side? }]`; the `owners` list is
  gone. The map grid model owns placing, moving, removing, overlap and
  bounds checks, and resize (structures that no longer fit are listed in
  the crop warning, like units, and removed on confirm).
- Deleting a structure kind asks first when maps use it (lists the maps and
  counts) and removes the placed ones, its pictures and style overrides in
  the same undo step. Deleting a unit removes it from structures' "can
  build" lists; deleting a movement type or a resource removes it from
  structures' movement overrides / income; deleting a side on a map makes
  that side's structures there neutral. Duplicating copies its pictures.
- Problems in `data/structures.json` link to the Structures section.
- **Old projects**: when a project is opened whose terrain still has the
  moved settings (`capturePoints`, `income`, `produces`, tag `hq`) or whose
  maps still have `owners`, the editor offers a one-click **Convert**
  (explained in plain words, one undo step): each such terrain kind gets a
  structure kind with the same name, colour, letter and those settings;
  every square of that terrain on every map gets that structure (owner
  taken from `owners`); the terrain keeps its ground rules but loses the
  moved settings. Until converted, the engine reports the problem as usual.

### Structures section (sidebar, between Terrain and Art)
List + form, same pattern as Terrain.
- Name, colour, map letter.
- **Size**: width and height in squares (1 to 8), with a small footprint
  preview.
- **Moving through it**: per movement type a choice "Same as the ground" /
  a cost / "Cannot enter".
- **Numbers used by combat maths**: per number (the same keys Terrain
  offers) "Same as the ground" or a value.
- "Can be captured" (points), income per resource, "units can be built
  here", "counts as headquarters"; each shown only when the matching rule
  feature is on, as on the old Terrain form. Other tags under Advanced.
- **Picture** card (the existing component): kinds One piece / Animation;
  "A different picture for an owner" per side id; per-style overrides. The
  preview shows the structure on a patch of the first terrain at its real
  footprint, with an owner frame, including how far it sticks out above.

### Terrain form
Loses "can be captured", income, "units can be built here" and "counts as
headquarters", with one line pointing to Structures.

### Map editor
- The **Owners** tool is replaced by **Structures** (key O stays): a
  palette of structure kinds plus an owner choice (Nobody or a side).
  Click places the chosen kind with its top-left corner on the square (a
  ghost of the footprint follows the pointer and turns red where it would
  not fit or would overlap); clicking a placed structure selects it
  (panel: kind, owner, position, Remove); dragging moves it; Delete removes
  it. Placing never changes the terrain underneath.
- Painting terrain under a structure is allowed and leaves the structure.
- The canvas draws structures per "Drawing" in docs/STRUCTURES.md
  (placeholder block or picture, owner frame around the footprint, combined
  row sorting with units, tall pictures).
- Map checks ("things to check"): a unit standing where it cannot enter is
  judged with the structure's movement costs; a side with the lost-HQ rule
  on and no headquarters; structures of a kind that can build units but
  belong to nobody are fine (neutral).
- The Select tool shows the ground AND the structure of a square, with the
  effective movement and numbers.

### Tests and screenshots
Unit tests for every model function (placing rules, overlap, bounds,
resize, cascades, conversion of old projects, effective numbers). e2e
against the real helper: create a structure kind of each size, set every
field, place/move/remove/own on a map (mouse and touch), overlap refused,
resize crop warning, cascade deletes, the conversion flow on a project
seeded in the old format, pictures for structures, the canvas matches the
drawing rules (pixel checks), Play shows structures and capturing one works
in the real player, phone layout. Screenshots continue the numbering.

## Animated join, joinExcept and the Saving card (implementation notes)

Catches the editor up with engine features documented in GRAPHICS.md and SAVES.md.

- **Model.** `Look.join` is `CellRef | CellRef[]`; `Look.joinExcept?: string[]`; `Game.saving?: 'any' | 'continue' | 'off'`.
  `model/pickCell.ts` ports the C++ `pickCell` (time picks the block with `frameMs`, default 400; the mask picks the piece) and
  has `joinBlocks`/`lookCells`/`lookShape` for both join shapes. Nothing is rewritten unless the user presses "Use this picture",
  so untouched looks round-trip byte for byte.
- **Writing.** `lookFromPick`: one block stays `"join": [c, r]`; two to 16 blocks write `"join": [[c, r], ...]` plus `frameMs`
  (always written for several blocks, like the Animation kind). "Everything except" writes `joinExcept` and never `joinWith`.
- **Canvas.** `art/scene.ts` builds each joining terrain's "same terrain" set from `joinWith`, or, for `joinExcept`, every terrain
  but the listed ones. A join with several blocks keeps the animation clock running (canvases and previews animate).
- **Cleaning.** Deleting a terrain also removes its id from `joinExcept` lists. A list that becomes empty is kept as `[]`
  (dropping it would turn "everything except" into "only itself").
- **Picture dialog (Joins with neighbours).** Clicking a block adds it (up to 16, numbered on the image when more than one);
  "Remove last" and "Clear" undo. Deviation: a second click no longer replaces the single block, press Clear first.
  With several blocks the same Speed control as Animation appears. A "Joins with" choice switches between "Only itself, and
  the terrain I choose" (the existing list) and "Everything except the terrain I choose" (the new list).
  The engine's rule that `joinExcept` cannot list its own terrain is met because the list never offers the terrain itself.
- **Convert** (`structuresConvert.ts`) still does not copy a joining set to a structure; it now also leaves out `joinExcept`.
- **Saving card** (Rules screen, after Experience): three choices, "Players can save anywhere" (writes no key), "Only the
  automatic Continue save" (`continue`), "No saving" (`off`). Unknown values read as "anywhere" until the user picks.
  The helper passes `saving` through unchanged (checked by the e2e test against the real loader).
- **Tests.** Unit: `pickCell.test.ts`, `joinAnimated.test.ts`. E2E: `art-join-animated.spec.ts` (both `@smoke`).

## Kinds of view and heights (implementation notes)

Contract: `docs/VIEWS.md`. The engine loader (`loadStyles`, `loader_graphics.cpp`) stays the source of truth.

- **Kind of view per style** (Game screen, `screens/game/ViewFields.tsx`, model `model/styleView.ts`). A "Kind of view" choice,
  Top-down or Isometric, sits under the style's name. Top-down writes no `view` key and removes the style's `isometric` object
  (so switching to Isometric and back leaves the file as it was). For Isometric, the four sizes (`tileWidth`, `tileHeight`,
  `heightStep`, `unitWidth`) are in a collapsed "Advanced" area. An empty box writes no key; the placeholder is the default
  computed as in VIEWS.md from the sizes around it (2 x the game's tile size, then halves), so it follows what the maker types.
  Ranges are the loader's (even for the two diamond sizes); `NumberInput` got an `even` option for that.
  A style whose file says `isometric-3d` or `3d` shows only a note ("not available yet"): no control, so nothing in it can change
  (its other settings, such as the background, still edit). The kind cannot be chosen for other styles.
- **Art mirrors the loader.** `effectiveLook(..., inherit)` ignores the general pictures for a style whose kind is not top-down
  (`inheritsGeneralArt`); `buildArtScene` uses it. The picture card for such a style hides "Same as the others", says why in one
  sentence, and "No picture in this style" removes the style's entry instead of writing `null` (both mean the same there).
  Its preview is the chosen piece itself (`PreviewSpec.piece`, drawn as it is in its image, animations play), not a top-down
  composition; with no picture it falls back to the plain placeholder preview.
- **Map editor.** The canvas always draws top-down. For a non-top-down style in the "Style" preview choice it uses that style's
  colours and background but the general pictures (`MapEditor.tsx`). A line under the map says so. Deviation: none.
- **Heights tool** (key H, `panels/HeightsTool.tsx`, `model/mapHeights.ts`, stroke in `tools.ts`). Raise / Lower / Set to a number,
  -8..32 (clamped). A drag changes each square once (a square crossed again is not raised twice) and is one undo step. The tool
  settings are remembered with the other tool settings (older saved choices default to Raise, 1). With the tool active, squares
  not at 0 are tinted and show their number (`drawHeights` in `drawMap.ts`; hidden below ~10 px squares, the tint stays).
  The Select tool's panel always lists the square's height. Saving: `heights` is written only when some square is not 0; a map
  that never had heights stays byte-identical, and bringing every square back to 0 removes the key. Resize already kept heights
  with their squares (new squares 0). A heights grid that does not match the size is reported by the existing repair banner.
- **Play** (`play/PlayPanel.tsx`). The embedded player already took `style=<id>`, so a "Style" choice is offered when the game
  has more than one style the player can draw (`isometric-3d` and `3d` are left out, as the player does). The choice is remembered
  per game. The address only carries `style` when it is not the first style, so the default stays `embed.html?project=..&map=..`.
- **Templates.** Strategy RPG and Economy Tactics start with a Classic and an Isometric style (generated isometric art by
  `tools/art/make_starter_art.py`); Blank has none.
- **Tests.** Unit: `tests/unit/views.test.ts`. E2E: `views.spec.ts` (styles, templates, picture cards, Heights, Play; the style
  round trip, the template check and the Heights drag/undo are `@smoke`), `views-mobile.spec.ts`. Screenshots 73-75
  (`screenshots-views.spec.ts`).

## Standing terrain (docs/GRAPHICS.md "Standing terrain")

A terrain in a style of a 3D kind can have something standing on its ground. Code: pure edits in `editor/web/src/model/stand.ts`
(`standKind`, `setStandPicture`, `setStandModels` / `addStandModel` / `removeStandModel`, `setStandWidth` / `Height` / `Turn` /
`Scatter`), the section in `src/ui/picture/StandEditor.tsx` (wording `standText.ts`), the type `Look.stand` / `Stand` in `model/types.ts`.
The loader stays the source of truth for the ranges and the refusals; the helper reports them as problems on `data/graphics.json`.

- **Where.** On the Terrain screen, in the Picture card, after the style's picture controls, when "For style:" names a style whose kind
  of view is 3D (Isometric 3D or Full 3D). It is not there for "All styles" or for the top-down and 2D isometric styles (they refuse a
  standing part). The section is named **Standing part**, with the line "In 3D styles a terrain can have something standing on its
  ground, like trees or a peak. The ground picture stays underneath." A terrain with no picture of its own in that style only shows
  "First give this terrain a picture of its own in this style. The standing part stands on it."
- **Choice** ("What stands here"): *Nothing* (the key is removed), *An upright picture*, *A 3D model*. Choosing a kind writes nothing
  until a picture or a file is chosen; switching between the two kinds removes the other kind's settings (the file format does not mix them).
- **A picture**: "Choose picture…" opens the usual piece picker (Choose a picture for Forest, standing part) with *One piece*, *Random mix*
  and *Animation* (no joining pieces); the preview there and beside the button is the chosen piece as it is in its image
  (`standPreview`, like any picture of a style that is not top-down). A random mix is how a forest gets variants. "Picture width (pixels)"
  (empty = one square). One piece is written as `cell`, a mix as `variants`, an animation as `frames` + `frameMs`.
- **A 3D model**: "Model files" lists the files (each with Remove), "Add a model file…" picks another file of the game, "Upload a model…" is
  the unit models' upload (`.glb`, up to 8 MB, name made from the file's, shared `ModelLibrary`). One file is written as `model`, several as
  `variants`. "Model height (squares)" (empty = 1) and "Model turn (degrees)". No preview (the editor shows none for models: Play does).
- **Vary position and turn** (`scatter`) for both. Unknown keys of a stand survive every edit; changing the ground picture keeps the
  standing part (`setLook` carries `stand` like a unit's per-side pictures); removing an image removes only a standing picture drawn
  from it (the ground stays) and a ground picture's removal takes the whole entry; a standing picture counts as using its image in
  "Used by" and in the check for pieces outside a smaller image. Deleting a terrain or a style removes its standing part with its pictures.
- **Models in use.** `GET .../models` lists a terrain's stand under `usedBy` with `kind: "terrain"`; deleting a model a saved stand names is
  refused with the same `409`.
- **Map editor and canvas** stay top-down and show no standing part; Play shows the real thing.
- **Tests.** Unit: `tests/unit/stand.test.ts`. E2E: `terrain-stand.spec.ts` (a picture mix with width and scatter, models with upload and
  Play, hidden for 2D styles, unknown keys). API: `tests/editor_api_stand_checks.py`.

## Economy options: Research, upgrades, repair, fuel, ammunition, supply

Contract: docs/ECONOMY.md. Model `editor/web/src/model/research*.ts`, `structuresEconomy.ts`, `supplyRules.ts`, `supplyUnits.ts`,
`economyChecks.ts`; screens `screens/research/` (new sidebar entry "Research", after Structures; list + form like Items), plus cards on
Structures (research here, "Can be upgraded to", Repairs), Units (fuel, loses fuel every turn, resupplies), Weapons (ammunition) and
Rules ("Repair and supply"). Story: condition "A side has researched a technology" and command "Give a technology" (map events only).

- `data/research.json` is read and written like Items (missing = no list, no file until there is a technology; a broken file blocks only
  the Research screen). An empty list removes the file, because the loader refuses an empty one.
- Prerequisites: options that would make a loop (itself, or a technology that already needs this one) are disabled with a reason.
- Repairs show on a structure only while the game has a repair rule (otherwise the "hidden because the rule is off" line names them).
  No key = all units; "Only these" = a list of unit types and tags (empty = repairs nothing, said on screen).
- Bonus: "Makes units stronger" starts as +1 of a non-role stat; no unit type ticked = all unit types (un-ticking the last one goes back to all).
- Clean-up: `reconcileEconomy` (run by the store's `edit`, like `reconcileCampaign`) takes a deleted technology, unit type, structure kind,
  resource or stat out of research lists, upgrades, repairs and events in the same undo step. Ids never change, so renaming a technology
  only changes its name. A bonus whose unit types are all deleted is removed (an empty list would mean "all").
- Checks (amber banners, `EconomyChecks`) mirror loader_research / loader_upgrades / loader_supply on the screen that holds the setting.
- Not part of this: fog of war keys. Campaign scenes get neither the condition nor the command (the scene loader refuses them).

## Fog of war

Map tab: "Fog of war" (`fog: true`; off removes the key; disabled with the reason above 32 sides) and, while on, "Show what a side sees" (a drawing aid, nothing saved). Units: "Sight" (`vision`, 0 to 20, default 2 is not written). Terrain and Structures: "Extra sight for a unit standing here" (`visionBonus`, 0 to 10, 0 not written), "Who gets the extra sight" ("For all units" writes the plain number; "Only for these" writes `{ "amount": N, "units": [...] }` and shows the unit types and labels to tick, the same picking as a structure's Repairs; nothing ticked is flagged because the loader refuses an empty list; a deleted unit type is taken off the lists, and a list that empties removes the bonus, in the same undo step) and "Hides" (`hides`). The map aid "Show what a side sees" applies the limit. Events: condition "A team sees a unit" (`sees` + `team`), map events only. Play panel: "Show everything" (`showall=1`) when the played map (or any chapter's map) has fog.

## Sound (docs/SOUND.md step 4)

Sidebar entry "Sound" after Art. Three parts on one page: **Music** (tracks: file, play / stop, volume, Repeats, Repeat from with
"use the playing position"), **Sounds** (effects: one file or variants, play picks one at random, volume) and **When things
happen** (the standard set as labelled pickers with play buttons: music slots, then Menus / Moving and fighting / Economy / Story),
then **Sound files** (every file with length, rate, what uses it, "Not used yet", use as music or a sound, delete) and "Remove all
sound". No sound yet: an explanation and "Add music" / "Add a sound". Adding = choose a file (checked here: 8 MB, first bytes), upload,
name it. Small "Sounds" groups on Weapons / Items (cards) and Units (folded), "Music" on the map's Map tab, "Music for this scene"
on the campaign scenes, and Play music / Stop the music / Play a sound among the story commands (map events and scenes); all shown only
when the game has sounds or music. "Same as the game" = no key, "Silent" / "No music" = "". Delete and rename follow everywhere in one
undo step; deleting a file that is used asks first. Amber checks mirror the player's loader (`model/audioChecks.ts`). Wording:
`screens/sound/text.ts` and `model/audioLabels.ts`.

## Hexagon games (editor step of docs/HEX.md)

Contract: docs/HEX.md. A game's grid is chosen when it is made (the template decides) and cannot be changed in the editor. Square games
behave exactly as before; everything below only happens when `game.json` has `"grid": { "type": "hex", ... }`.

- **Model.** `model/hex.ts` is the one place for hexagon geometry (neighbour tables in the contract's order, distance, within, ring cells,
  box positions, map size, corners, picking = nearest middle, straight lines, join mask). `model/gridSpec.ts` (`GridSpec`, `gridOfGame`,
  `neighboursOf`, `distanceOf`, `withinOf`, `lineOf`) answers the same questions for squares, so fill, paint, fog and the checks do not branch.
  `MapCtx` carries the `grid`; a structure's cells come from ONE function, `footprint` (`model/mapStructures.ts`: squares row by row, hexagons the
  middle first then its neighbours), used by placing, overlap, `misfits`, resize, the sight aid and drawing. `model/mapGeo.ts` (`makeGeo`) puts
  cells on the picture in **tiles** (1 tile = the game's tile size in pixels; a square is one tile, a hexagon has the box of docs/HEX.md): the
  canvas, the input, the previews and the tools all ask it (`box`, `at`, `range`, `outline`), so the tools are shared. New maps in a hexagon
  game are hexagon maps (`blankMap`).
- **Canvas.** `drawMap.ts` keeps the square drawing untouched and hands hexagon maps to `drawHex.ts`; both read the same `Scene` / `DrawState`
  (`drawTypes.ts`), units share `drawUnits.ts`. Terrain: a filled hexagon (also stroked one pixel in its colour, so no seam of the backdrop
  shows between neighbours), or the sheet cell in the hexagon's box (join pieces from the 8 x 8 block with the six-bit mask, `pickCell(..., 8)`; animated
  joins as on squares). Faint grid lines are one path of hexagon outlines. Units: their square of one tile centred on the hexagon's middle.
  Structures: one hexagon = the picture in the box; with rings the picture across the box around all seven (width of the box, height from the piece's
  proportions, standing on the bottom edge); placeholder = the covered hexagons filled in the colour with the letter on the middle one; the
  owner frame, the selection ring and the story areas are outlines around the set of covered hexagons (`outline`, drawn inside the set). Drawing
  order is by the bottom edge of the box. Highlights (hover, cursor, selection, ghost, fog dimming, heights) are hexagon shaped.
  `data-view` is `scale,x,y` with scale = pixels per tile.
- **Tools.** Paint: the hexagons under the pointer (a fast drag is joined by a straight line of hexagons), one drag = one undo step. Fill: through the six
  neighbours. Eyedropper, units, erasing and keyboard (arrows by column and row) are unchanged. Structures are placed by their **middle**; the ghost shows
  the whole footprint (red with a cross where it does not fit); refusals: "A Headquarters covers a hexagon and the six around it, and would stick
  out of the map there." / "A Headquarters would overlap the City that is already there." Dragging a structure keeps the place it was grabbed at
  (the same hexagon vector is applied to the middle, not the column difference, which changes meaning between odd and even rows). Map resize keeps
  contents by column and row; a structure whose seven hexagons no longer all fit is listed and removed on confirm. Story areas (`reach`...) stay
  rectangles of columns and rows (picked by dragging between two hexagons) and are drawn as the set of hexagons they contain. "Show what a side sees"
  uses hexagon steps (`withinOf`).
- **Screens.** Game: a "Grid" card shows "Squares", "Hexagons, pointy-top" or "Hexagons, flat-top" (read only) with one sentence; a style's Kind of
  view lists only the kinds the engine draws for hexagons (all four today; the helper says which: `GET /api/info` `hexViews`, asked once
  at start, `api/hexViews.ts`); a style that somehow has another kind keeps it in the list, greyed with "not for hexagon games yet". The
  isometric settings of a hexagon game have no "Square height" (the angle is fixed), "Tile width" instead of "Square width", and the Height step
  placeholder is a quarter of the width. Structures: the size control is "Covers" with "1 hexagon" /
  "7 hexagons (a hexagon and its ring)" (writes `rings`, 0 is not written), width and height are hidden, the preview is little hexagons; upgrades may
  only target kinds with the same `rings`. Art: a new image is divided into pieces of the hexagon's box (24 x 28 pointy, 28 x 24 flat at tile size
  24) with a "Hexagon size" quick button; the join picker uses 8 x 8 blocks (checked against the image, a guide with 64 hexagon diagrams, download at
  the piece size); palettes and previews draw hexagons through the same `drawMap`. New game: each template card shows its grid (read from the template's
  `game.json` by `GET /api/templates`). Wording that says "square" reads "hexagon" on a hexagon game (`ui/cellWords.ts`).
- **Play.** The panel has a "Sound" checkbox (on by default, remembered per game like Style / Show everything) that adds `noaudio=1` to the
  embed address when off; `embed.html` turns it into `--no-audio`.
- **Tests.** Unit: `hex.test.ts` (tables pinned against the contract's text, distance = BFS steps, boxes, picking, join mask, fill), `hexMap.test.ts`
  (on the engine's fixtures). API: `tests/editor_api_hex_checks.py`. e2e: `hex-map`, `hex-tools`, `hex-screens`, `hex-play` specs (both layouts, from
  copies of `tests/fixtures/hex-pointy` / `hex-flat`, pixel checks from the contract's formulas) and `screenshots-hex.spec.ts`.

## Learn (the tutorial; contract docs/TUTORIAL.md)

Code: `editor/web/src/learn/`. Wording: `learn/text.ts` (the framework) and `learn/lessons/*.ts` (the lessons).
- **Learn button**: `learn/LearnButton.tsx`, in the top bar (before the problems badge; icon only on phones) and in the game list's header. It
  opens the **Learn list** (`LearnList.tsx`, a dialog): every lesson in `lessons/index.ts` with title, one line, "About N minutes", its state
  (Not started / In progress: step 3 of 10 / Finished) and Start, Continue or Start again. Lessons that do not exist are not listed.
- **First-run card** (`FirstRunCard.tsx`) on the game list while the helper has no saved Learn state at all (and could be read): Start (makes the
  practice game and starts) or Not now. Either answer is saved as `offerAnswered`; it never comes back by itself.
- **Practice game**: `LearnController.start` lists the games, names the new one "Practice: <lesson title>" (a number is added while the folder name
  is taken: `practiceName`), creates it from the lesson's template through `POST /api/projects` and opens it. Resume reopens it at the current step; a
  game that was deleted is found by `checkPractice` (page load, game list, Learn list, a game that cannot be opened) and the panel / list say so and
  offer Start again (a new game; the progress starts over).
- **Panel** (`LessonPanel.tsx`): while a lesson runs, above every screen (mounted by `App`, so it stays across screens, the game list and reloads).
  From **1800 px** wide it is docked at the right edge (340 px, the whole page gives up that room by a margin on `#root`); below that, and on phones, it is
  a bottom sheet (padding on `#root` and on `.app`, because `#root` is exactly one window high; sidebar / lists / the map's options end above it, `scroll-padding-bottom`). Both fold into a slim bar showing
  the current step's title (remembered in `ote.learnCollapsed`). Why 1800: the map editor needs about 1200 px beside the sidebar and its own
  options; `.map-main` got `grid-template-columns: minmax(0, 1fr)` so its canvas follows the room it has (before, it kept its size when the room shrank).
  Content: lesson title, "Step 3 of 10", a meter, the ticked steps folded ("Done so far (n)", skipped ones marked), the current step (title, text,
  "This happens on the X screen.", Take me there when elsewhere, Skip this step) and Stop lesson. At the end: "You finished the lesson", "What you made", Play,
  Done and "Next lessons" (other lessons not finished). `data-testid`s: `learn-step-title`, `learn-done`, `learn-end`, `learn-gone`, `learn-offer`.
- **Pointing**: `Highlight.tsx` draws a ring around the first visible element with `data-learn="<name>"`. It checks the element every frame (so it follows
  scroll, resize and layout changes), is hidden when there is no such element or it is off screen (a control found off screen is scrolled into view
  once, and tried again a few times while the layout settles), uses `pointer-events: none`, and pulses only under `prefers-reduced-motion: no-preference`.
  While a modal dialog is open the ring becomes a manual popover (top layer) so it shows above the dialog (the picture chooser); otherwise it is an
  ordinary fixed box under the panel. On phones a control hidden under the bottom sheet counts as off screen (it is scrolled above the sheet). On the step's own screen it rings the step's
  `point`; elsewhere it rings the sidebar link (`nav-<section>`) that gets there; on the game list, while folded or at the end, nothing.
- **Names in use** (`data-learn`): `nav-<section>` (sidebar), `new-item` (the New ... button of a list screen), `game-name`, `unit-weapons`,
  `tool-<tool>` (map toolbar), `terrain-picker`, `play`, `problems`, `learn`; the topic lessons added `structure-capture`, `structure-income`,
  `structure-produces`, `structure-upgrades`, `structure-covers` (hexagon games), `structure-picker`, `owner-picker`, `tech-unlocks`, `map-tab-<tab>`,
  `fog-setting`, `picture-choose`, `picker-kind`, `picker-grid`, `picker-use`. A lesson that needs another control adds the attribute there
  (`Check`, `Card`, `KeyNumberTable`, `SidePicker`, `PieceGrid` take a `learn` prop for it).
- **Runner** (`runner.ts`, `controller.ts`): done tests run on every change of the open practice game's document, its screen or the signals; only the
  current step is tested, a step already true is ticked at once, nothing is ever unticked. `settleMs` makes a typing step wait for a pause.
- **Signals** (`signals.ts`): `played` (PlayPanel's `ote-player-ready`), `saved` (the game's revision changed on disk), `problemsChecked` (the Problems
  list opened), `pictureChooser` (the "Choose picture" dialog opened). Remembered with the lesson's progress.
  list opened). Remembered with the lesson's progress.
- **Topic lessons items, story, sound** (`learn/lessons/{items,story,sound}.ts`, helpers in `learn/helpers/`): more `data-learn` names (`item-heal`,
  `unit-start-items`, `map-tab-<tab>`, `add-event`, `events-back`, `event-open`, `event-when`, `pick-area`, `add-command`, `say-text`, `objective-text`,
  `add-win`, `sound-play`, `track-volume`, `map-music`, `sound-event-<event>`, `own-sound-<event>`, `list-item`); the signal `soundPreviewed` (a play
  button of the Sound screen was pressed); and **alternatives** in a step's `point`: `'add-event|map-tab-events'` rings the first name that is on screen
  (the tab until it is open, then the button in it). While the lesson is a bottom sheet the ring treats a control under the sheet as hidden and scrolls it
  up (tried again a few times). Known limit on phones and narrow windows: the map editor's own options sheet and the end of a long page can stay under
  the lesson sheet; folding the lesson ("Hide lesson") shows them.
- **Lesson "Characters and skills"** (`learn/lessons/characters.ts`, helpers in `learn/helpers/characters.ts`; step T1 of docs/SRPG.md, which holds the
  contract and the "T1 as built" note): starts from the blank template (nothing of the strategy-RPG parts exists there, so every step is real work) and
  takes the maker through the Rules switches, a character, the map's Characters tab, the starter skills, a personal and a learned skill, and a skill of
  their own. New `data-learn` names: `srpg-<feature>` and `progression` (the Rules switches; `Toggle` now takes `learn`), `character-stats`,
  `unit-characters-tab`, `starter-open`, `<card>-add` / `<card>-learns` (the Skills card, e.g. `character-skills-add`), `skill-rule` (the When box of
  the first rule of a skill that is not in the starter set).
- **State**: `GET`/`PUT /api/learn` (docs/EDITOR_API.md), every change is sent at once; `learn/state.ts` reads it tolerantly.
- **Tests.** Unit: `learnHelpers`, `learnRunner` (runner, state, practice names), `learnController` (fake helper), `learnFirstGame`, `learnStructures`, `learnEconomy`, `learnArt`, `learnHexagons`, `learnCharacters` (every done test of
  a lesson, true and false; `learnTopicKit` has the shared checks). API: `tests/editor_api_learn_checks.py`. e2e: `learn-lesson`, `learn-structures`, `learn-economy`, `learn-art`, `learn-hexagons`, `learn-characters` (the whole lesson, step by step, ends with the real
  engine loading the game; helpers in `learntopic.ts`), `learn-ring-room` (the ring above a dialog, the map options above the phone sheet), `learn-state` (offer, resume, stop and continue, skip, deleted game, numbering, no lesson = as before), `learn-layout`
  (phone, laptop, wide window, the ring). Every Learn spec starts its own helper with an empty temporary `--learn-file` (`tests/e2e/learnkit.ts`);
  the shared helper starts with "offer answered" so the game list looks as before for every other spec.

## Strategy-RPG skeleton (editor step E0 of docs/SRPG.md)

Contract: docs/SRPG.md "Editor". E0 only lays the shared places; the screens and cards are filled by E1 (characters), E2 (skills),
E3 (abilities, promotion, carrying) and E4 (supports), each in its own files, so the steps can run side by side.

- **Document**: `ProjectDoc` gains `characters`, `skills`, `abilities` (lists) and `supports` (object), each `null` when the game has no file
  (`model/srpgTypes.ts`; helpers `withCharacters` / `withSkills` / `withAbilities` / `withSupports` in `model/doc.ts`). Unit types, weapons, items and
  game rules gain the optional keys of the contract in `model/types.ts`; map units gain `character`. **Saving** (`store/autosave.ts`): a list that
  changed is sent; a feature that is switched on and still empty is sent as its empty list (or `{ "pairs": [] }`) and the helper writes that file;
  `false` removes the file (the feature was switched off). A game that never switches a feature on never gets a file.
- **Switches** (`model/srpgFeatures.ts`, Rules card "Strategy-RPG features" in `screens/rules/srpg/FeatureSwitchesCard.tsx`): a feature is on when its
  list or file exists in the document. Switching on creates it empty and the screen appears in the sidebar after Units; switching off asks first when
  something is in it, and removes the feature's items from everything that names them (`switchOffFeature`, in one undo step). Supports need characters.
  The switch sticks (owner, 2026-11-15): the empty file is what remembers it across reloads and devices. The engine treats a file that holds nothing
  as no file (`engine/core/battle/save_hash.cpp` `holdsNothing`), so switching a feature on never makes a save invalid.
- **Sidebar and routes**: `Section` gains `characters`, `skills`, `abilities`, `supports` (`/p/<id>/characters/<item>` etc.), listed only when the
  feature is on or the screen is the open one (`srpgSectionShown`). A problem in one of the four files goes to its screen's dot (`model/problems.ts`).
  The screens are empty frames (`screens/srpg/SrpgScreen.tsx`): switched off (link to Rules), unreadable file (the loader's words), or the screen
  itself, which E1 to E4 put in the frame (`screens/<name>/<Name>Screen.tsx`).
- **Where later steps plug in** (each a file of its own that currently renders nothing): Rules `screens/rules/srpg/CarryingCard.tsx`,
  `PromotionRulesCard.tsx` (E3); unit type `screens/units/srpg/UnitSkillsCard.tsx` (E2), `UnitAbilitiesCard.tsx`, `UnitPromotionCard.tsx`,
  `UnitPairBonusCard.tsx` (E3); weapon `screens/weapons/srpg/WeaponSkillsCard.tsx` (E2); item `screens/items/srpg/ItemSkillsCard.tsx` (E2),
  `ItemPromotionCard.tsx` (E3). The forms mount only the group file (`UnitSrpgCards`, `WeaponSrpgCards`, `ItemSrpgCards`, `SrpgRulesCards`).
  Shared wording is in `screens/srpg/text.ts`; each screen adds its own `text.ts`.
- **References** (`model/srpgRefs.ts`): `newCharacterId` (also avoids the story ids of map units), `newSkillId`, `newAbilityId`;
  `characterUsageLines` / `skillUsageLines` / `abilityUsageLines` for the delete question; `deleteCharacter` / `deleteSkill` / `deleteAbility` clean
  every reference in the same edit. Story commands that name these (teach / forget a skill, promote, support points) are added to `storyRefs.ts` by
  the step that adds the form. Deleting a unit type also deletes the characters of that type (E1); promotions into it are E3's.
- **Problems**: the loader's refusals for the four files reach the Problems panel as they are. The editor-side checks of the contract (unused skill,
  promotion item nobody hands out, ...) belong to the steps that own the data (E1 to E4).
- **Tests**: unit `tests/unit/srpg.test.ts`; API `tests/editor_api_srpg_checks.py`; e2e `tests/e2e/srpg-skeleton.spec.ts`.

## Characters (editor step E1 of docs/SRPG.md)

- **Screen** `screens/characters/`: list + form (like Units) inside the E0 frame. Cards: name / class / "Joins at level" (progression on) /
  description; Portrait (Art's picker, keyed by the character id); Stats and Growth tables with three columns *class | Personal | Total*
  (`table.three-col`, fixed layout so it fits 360 px); "Starts with its own items"; slots `CharacterSkillsCard` (E2), `CharacterAbilitiesCard` (E3).
  Checks (`model/characterChecks.ts`): "Not used on any map." in a banner with links.
- **Class wording**: `classWord(doc)` is "class" once the game has a character, else "unit type"; used on the Characters screen and the map editor's
  Units picker / unit form. Other screens keep "unit type" for now.
- **Map editor**: with the feature on, the Units tool shows tabs *Classes | Characters* (`panels/UnitPicker.tsx`; `ToolState.unitTab`,
  `unitCharacter`, remembered per game). A character already on the map is greyed ("On the map"). A placed character is
  `{ character, side, x, y }`, `MapUnit.type` is optional; `model/characterUnits.ts` (`typeIdOf`, `storyIdOf`, `withResolvedTypes`) is the one
  place that resolves it, and `MapCtx.characters` carries the list. The unit form shows the character with "Open character", hides the story name,
  and the class select stores `type` only when it differs from the character's.
- **Tests**: unit `tests/unit/characters.test.ts`; API `tests/editor_api_characters_checks.py`; e2e `characters.spec.ts`, `characters-phone.spec.ts`
  (360 px), screenshots `screenshots-characters.spec.ts` (`SHOTS=1`).

## Strategy-RPG: abilities, promotion, carrying (editor step E3 of docs/SRPG.md)

Contract and wording: docs/SRPG.md "Editor" and "E3 as built". No helper change: the files are the ones of docs/EDITOR_API.md.

- **Abilities screen** (`screens/abilities/`): list + form like Items. Pickers only offer what the loader accepts (`model/abilities.ts` `TARGETS_FOR`):
  changing the effect repairs the target and the range; a default is never written (cost "action", afterMove true, summon limit 1 ...); unknown keys stay.
  The summon has its own card. "Only if" rows (`abilityConditions.ts`) cover hit points, units nearby, type or tag, carrying and round; others are kept and shown.
  The sentence under the form is `abilityText.ts`. A banner lists `abilityChecks.ts` (nobody has it; the loader's refusals in plain words).
- **Unit type**: cards Abilities (order = menu order, shown when the game has abilities), Promotion (rows; checks inside the card), Gives when behind
  (pair-up games). **Item**: "Promotion item" switch on usable items. **Rules**: cards Carrying and Promotion. Mode changes that drop settings ask first.
- **Delete clean-up** (`promotionRefs.ts`, called from `references.ts` and `itemRefs.ts`): unit type (promotions into it; abilities that summon it are deleted),
  item (promotions drop it), stat (changes, bonuses, carrying rows), all in one undo step.
- **Story**: command "Promote a unit" and trigger "A unit is promoted" (`forms/PromoteForm.tsx`, `TriggerForm.tsx`); `storyWalk.ts` knows their references.
- **Tests**: unit `abilities`, `promotion`, `rescue`; API `tests/editor_api_abilities_checks.py`; e2e `srpg-abilities.spec.ts`, `srpg-promotion.spec.ts`.

## Supports (editor step E4 of docs/SRPG.md)

- **Screen** `screens/supports/`, inside the E0 frame: a **Settings** card (`SupportRulesCard`: level names, the three point sources as "empty = off", how
  close is near, most levels, where conversations are played), a banner of checks (`model/supportChecks.ts`), then the pairs as list + form. A pair's form
  (`PairForm`): its two characters (a slot never offers the other character or a partner that makes an existing pair), starting points, and a `LevelCard`
  per level: points needed, bonus rows (`KeyNumberTable`: the combat numbers and the game's stats except HP and move), and the conversation.
- **Conversation**: `screens/campaign/SceneCommands.tsx` is the command list of a scene, extracted from `SceneEditor` so another source can use it: a
  `SceneSource` says how to read and write the commands, which kinds are offered (`only: ['say']`) and which map the forms see (`talkBase`: every character
  as a named unit). Support levels use it; a chapter's Before / After scenes use it as before.
- **Story**: command `addSupport`, condition `support`, trigger `support` (`storyTypes`, `storyDefaults`, `storySummary`, `forms/SupportForms.tsx`). A pair
  in a command is `[a, b]`; `storyWalk` names its characters with ref kind `character` and the pair itself with `pair` (`pairId`: alphabetical, dot-joined),
  so a deleted character or pair removes what was about it (maps, and scenes through `reconcileCampaign`) and a retargeted pair is followed.
- **Edits** (`model/supports.ts`) keep the file loadable: levels always rise, names cover every level, defaults are not written. Delete / retarget with story
  follow-up are in `srpgRefs.ts` (`deletePairClean`, `retargetPair`, `pairUsageLines`).
- **Tests**: unit `tests/unit/supports.test.ts`; API `tests/editor_api_supports_checks.py`; e2e `srpg-supports.spec.ts`.
## Weapon ranks and stat caps (editor step of docs/SRPG.md section 7)

- **Rules** (`screens/rules/srpg/`): two more switches in the Strategy-RPG features card (`RankSwitches`; ranks are disabled with a reason until weapons are
  carried), then the cards **Weapon ranks** (`RanksCard`: growth, the ladder in `LadderRows`, "Use the default ranks" while the ladder is empty, "Worth a look")
  and **Stat caps** (`CapsCard`: one optional number per stat). A switch that is on stays on after a reload: the empty file is written.
- **Units** (classes): `UnitRanksCard` (a rank per wielded group, "None" removes the entry) and `UnitCapsCard` (empty = the game's cap, shown as the placeholder).
  **Characters**: `CharacterRanksCard` and `CharacterCapsCard` (cap | adjust | result). **Weapons**: `WeaponRankCard` (needed rank; experience per use only when growth
  is "use"). **Map**: `UnitRankOverride` in the placed unit's panel. Shared: `screens/ranks/` (`RankMapTable`, `CapRows`, `text.ts`, `ranks.css`).
- **Model**: `model/ranks.ts` (ladder and picker edits), `rankRefs.ts` (clean-up, renames, "used by" lines), `rankChecks.ts`. `setWeaponGroup` in `weapons.ts` is
  where a group is typed (see docs/SRPG.md section 7 "Editor: as built" for what follows what). A stat delete also clears caps (`cleanStat`).
- **Tests**: unit `tests/unit/ranks.test.ts`; API `tests/editor_api_ranks_checks.py`; e2e `srpg-ranks.spec.ts`, `srpg-ranks-screens.spec.ts` (with the 360 px check).

## Character-locked weapons (editor step of docs/SRPG.md "Character-locked weapons")

- **Weapons**: `WeaponLockCard` (`screens/weapons/srpg/`, words in `lockText.ts`): "Only these characters can wield it", a `MultiSelect` checkbox list of the characters, a
  one-line summary and "Worth a look" checks. Shown only when weapons are carried and the game has characters; written only when non-empty; a hidden card leaves the key alone.
- **Characters**: `CharacterLockedWeapons`, a read-only card with links to the weapons locked to the character (nothing when none). **Rules**: the confirmation for switching
  Characters off adds a line when weapons are locked.
- **Model**: `model/locks.ts` (edit, clean-up on delete / rename / switch-off, "used by" line) and `lockChecks.ts`; `cleanCharacter` drops the character from the lists.
- **Tests**: unit `tests/unit/locks.test.ts`; e2e `srpg-locks.spec.ts` (with Play and the 360 px check).

## Staves (editor step of docs/SRPG.md section 8)

A staff is an item of kind "staff". Contract: docs/SRPG.md section 8 ("Staves: the editor, as built"). No helper change (stage two included): `items` carries every staff key.

- **Items screen**: kind "Staff" in the kind choice. `screens/items/staff/`: `StaffUseCard` (group with the game's groups suggested, who it can be used on, reach as "up to N squares" /
  "from N to M squares" / "a formula", area as one unit / within N squares of the unit picked / everyone in reach), `StaffEffectCard` (healing as a number / a percentage / all HP / a
  formula, "It can miss" with a chance box, experience for the user), a numbers table (`props`, the weapons' `KeyNumberTable`), then after the uses and price: `WeaponRankCard` and
  `WeaponLockCard` with `owner="item"` (the weapons' own cards, same conditions and checks), the abilities' `AbilityConditions` card (given `title` / `help`, fed the staff's `if`), and a
  "Worth a look" banner. Words in `screens/items/staff/text.ts`.
- **Units / Characters**: `UnitForm` offers every group of a weapon or a staff under "Can wield groups" (shown when weapons are carried **or** the game has a staff); starting and personal ranks and
  the map override list groups through `groupsOf`, which now covers staves. The Characters "locked" card (`CharacterLockedWeapons`) lists staves too, linking to the Items screen.
- **Model**: `model/gear.ts` (weapons and staves as one "gear" list: `allGear`, `patchGear`, `editGear`), used by `ranks.ts`, `rankRefs.ts`, `locks.ts`, `rankChecks.ts`, `lockChecks.ts`, so rank id
  rename / removal, ranks off, character delete / rename, characters off and the "last of a group" clean-up cover both. `model/staff.ts` (edits and limits: reach 1-20, radius 1-10, xp 0-1000, heal 1-1000000),
  `staffChecks.ts` (loader mirror, `blocks: true` = shown in the Items screen's "will not load" banner via `itemChecks`; the rest, "nobody can use it yet", on the staff's form). `setItemKind` (`items.ts`)
  gives a new staff a group, reach 2 and a heal of 10, and removes the staff keys when leaving (one Undo step). Keys the form does not show (stage-2/3 effects, `zzz`) are kept.
- **Stage two** (warp, pull, swap, repair, light, an item's `repairable`; docs/SRPG.md "Staves: stage two, as built" and "the editor, stage two"): in the form. "What it does" is a set of parts
  (`StaffEffectCard`: "It heals" with its kinds; `StaffOtherParts`: "Moving a unit" none / warp (with its reach: up to / from-to / formula) / pull / swap, "It repairs an item" all uses or a number,
  "It lights up an area" with a radius and turns). Each part is found by its kind, so the file's effect order never matters; an effect the form does not know is kept and said so (`staff-more-effects`).
  Choices the loader refuses are **greyed out with the reason beside them** (move with repair, pull / swap with "Only the user", a wider area with a move or repair, an enemy with healing, light with an
  unknown effect) and the model refuses them too (`model/staffEffects.ts`). "It lights up an area" replaces the other effects and removes target, area, chance of missing and conditions in the same step
  (one Undo), the way "Only the user" removes reach and area; the form then hides those. Items screen: the uses card has "A repair staff can mend it" (`repairable`; key absent = on, off = `false`),
  shown only for an item with uses when the game has a repair staff or the item already has the key. The conditions box says "The unit using the staff" (`AbilityConditions` takes `userLabel`).
  Checks (`staffChecks.ts`, `itemChecks.ts`) mirror every stage-two refusal; "worth a look": a light staff when no map has fog, a repair staff when no item can be repaired. Model files:
  `staffFx.ts` (reading the effects list), `staffEffects.ts` (the parts), `staff.ts` (reach, target, area, hit, xp).
- **Tests**: unit `tests/unit/staves.test.ts` (on `tests/fixtures/srpg-staves`) and `staves2.test.ts` (on `srpg-staves2`); API `tests/editor_api_staves_checks.py`; e2e `srpg-staves.spec.ts` and
  `srpg-staves2.spec.ts` (each with Play and the 360 px check).

## Conditions (docs/SRPG.md section 9): no forms yet

The editor has **no forms for status conditions**. Nothing in the editor breaks: the helper (`ote-editor`) does not know `data/conditions.json`, so GET never
returns it, PUT never writes or removes it, and it stays byte for byte as it is; every key that names a condition (a staff's `inflict` / `cure` effects, a
usable item's `cure` / `inflict`, a weapon's `inflict`, a skill's `immune` / `inflict` blocks and the `condition` test) travels through GET and PUT untouched
(`tests/editor_api_conditions_checks.py`), and the real loader's refusals about them reach Problems in the engine's words. What a maker must **hand-edit**:

- `data/conditions.json` (the whole file; new games made from the strategy-RPG template already have the six starter conditions in it, with a comment
  header explaining the keys);
- on a weapon: `"inflict": { "condition": "poison", "chance": 30 }`; on a usable item: `"cure"` / `"inflict"`; in a staff's `"effects"`: `{ "inflict": ... }` and
  `{ "cure": ... }` (a staff whose first effect is not a heal shows as "not editable" in the Items form, as the stage-two effects do, and is kept as it is);
- in a skill: the `immune` and `inflict` effects and the `condition` test (the Skills form keeps blocks and effects it does not know).

The Problems list shows an unknown condition id anywhere it is named, in plain words (`names unknown condition "poison" (data/conditions.json)`).

## Strategy-RPG: skills (editor step E2 of docs/SRPG.md)

Contract and wording: docs/SRPG.md "Editor" and "E2 as built". No helper change: the `skills` section of docs/EDITOR_API.md carries everything.

- **Skills screen** (`screens/skills/`): list + form like Abilities, plus a bar with "Add starter skills…" above the list. A skill is a column of rule cards
  (`BlockCard`): When, Only if (`ConditionRows` / `ConditionFields`), Chance and Reaches (`BlockOptions`, shown only where the contract allows them), Then
  (`EffectRows` / `EffectFields` / `StatRows`), and the sentence of the rule (`model/skillText.ts`). The tables of section 3 are data in `model/skillRules.ts`
  and the only thing the pickers read, so a refused choice is never offered. Changing When repairs the rule (`skillBlocks.ts` `setWhen`): what the new point does
  not allow is removed (Undo brings it back). A rule the file holds in a refused shape shows "Not valid yet" with plain messages (`skillChecks.ts` `ruleIssues`).
- **Installer** (`StarterDialog.tsx`, data in `model/starterSkills.ts`, the same list as `editor/templates/strategy-rpg/data/skills.json`; a unit test compares them).
- **Cards**: unit type and character (Skills + Learns at a level), accessory item (only the accessory kind), weapon: all through `screens/skills/SkillsCard.tsx`.
  An accessory needs a bonus or a skill (the check and help text say so). Changing an item away from accessory drops its skills.
- **Story**: commands "Teach a skill" / "Make units forget a skill" (`forms/SkillForms.tsx`), condition "A unit has a skill"; `storyWalk.ts` has the `skill` reference kind, so
  deleting a skill removes those commands and conditions (listed in the delete question, one undo step).
- **Tests**: unit `skills`, `skillsStarter`; API `tests/editor_api_skills_checks.py`; e2e `srpg-skills.spec.ts`, `srpg-skills-cards.spec.ts` (with the 360 px check).

## Reading a unit's numbers and taking an item away (docs/EVENTS.md)

All wording below is placeholder. No helper (API) change: the loader already checks maps and the campaign.

- **"Set a variable" / "Add to a variable"** (`forms/VariableForms.tsx`): below "Variable" a choice **"Take the number from"**: "A number I type" (the old
  box, "Set it to" / "Add"), "A unit's level", "A unit's hit points now", "A unit's most hit points", "A stat of a unit". Any reading shows "Which unit" (the
  named units of the map; in a campaign scene the roster ids the scene can name) and, for a stat, "Which stat" (the game's stats first); a line of help says what
  counts as 0. Choosing another source replaces the keys (`level` / `hp` / `maxHp` / `stat` + `of`, and `to` / `by`) in one undo step. A scene does not
  offer "hit points now". The command list reads "Sets the variable “Bar” to the level of Edda".
- **"Take an item away"** (group Give, after "Give an item"; `TakeItemForm` in `forms/GiveForms.tsx`): "Item", "Who loses it" (the usual unit picker; in a scene "Which character loses it"),
  and the checkbox "Take every copy". Offered in map events and in campaign scenes; greyed with "(no items yet)" in a game without items.
- **Problems** (`model/readingChecks.ts`, used by `storyChecks.ts` and `campaignChecks.ts`): both a number and a reading; more than one reading; a stat reading without
  its unit (or a unit key with a stat); "hit points now" in a scene; plus the ordinary missing-name checks (a unit the map does not name, an unknown stat or item, an
  empty pick).
- **References**: a reading names a unit (`level` / `hp` / `maxHp` / `of`: ref kind `unit`) and maybe a stat (new ref kind `stat`); `takeItem` names an item and a
  unit filter or roster id. They go through `storyWalk.ts`, so a unit rename follows, deleting a unit / item / stat removes the command (listed in the delete
  question: the stat and item "used by" lines now include story commands and scenes, `references.ts` / `itemRefs.ts`; deleting a stat runs `scrubDoc`, scenes follow
  through `reconcileCampaign`, whose `goneRefs` now also tracks stats and items).
- **Tests**: unit `unitReading`, `takeItem`; e2e `unitreading.spec.ts` (with the 360 px check; the written JSON is loaded by the real engine through the helper's problems list).

## Dialogue choices (docs/EVENTS.md "Dialogue choices")

All wording below is placeholder. No helper (API) change: the loader already checks maps and the campaign.

- **Command "Ask a question"** (group Dialogue, map events and campaign scenes; `forms/ChoiceForm.tsx`). Form: the speaker and portrait fields shared with
  "Say something" (`SpeakerFields` in `forms/SayForm.tsx`), "The question", "Remember the answer in" (a game variable or "Do not remember it"), then
  "Answers": per answer "Words of the answer (answer N)", up / down / delete buttons (delete greyed at two answers), "Only offered if (optional)" (the
  same condition list as "Only if"; a scene offers the scene kinds) and "Add an answer" (greyed at four, with a note). A new question starts with
  "Yes" / "No". Reordering moves an answer with its commands.
- **What each answer does** is its own command list, shown under the question like the branches of an If ("Answer 1: “Open it”", add box named
  "Question 1: Answer 1: Add a command…"); any command legal there works, including another question (lists nest, the path in `model/storyEdit.ts` is
  `[index, 'answerN']`, rows are keyed `…a<N>`). `forEachCommand` / `mapCommands` (storyWalk.ts) walk answers, so renaming a unit, deleting a switch,
  variable, item, chapter... cleans them in the same undo step (a hiding condition that cannot stay removes its answer; the Problems then say the question is
  short of answers).
- **Problems** (`model/choiceChecks.ts`, used by `storyChecks.ts` and `campaignChecks.ts`): empty question; fewer than two / more than four answers; empty or
  repeated answer text; two questions of one map / one scene saving in the same variable; plus the usual unknown-name checks inside answers.
- **Campaign**: "Go to a chapter" is now offered in the Before scene too (it skips that chapter's battle; the check refuses the scene's own chapter); the
  flow card shows such a jump (`campaignFlow.ts`).
- **Tests**: unit `choices.test.ts` (+ the changed line in `campaign.test.ts`); e2e `choices.spec.ts` (desktop: a question with a hidden answer, nested
  question, reorder, limits, duplicate store; a scene question that jumps to chapters; 360 px phone with four long answers).

## Campaign timeline (PLAN 6c)

All wording is placeholder (`screens/campaign/timeline/text.ts`). No helper (API) change. The Campaign screen has two views, **List** (the original) and **Timeline**
(tabs; the last one chosen is kept in `localStorage` key `ote.campaignView`, default List).

**What it shows.** A flowchart reading left to right: per chapter its before scene (if it has one), its level, its after scene (if any), then the next chapter.
A scene "exists" when it is a list (even `[]`) or the object form; "Add a scene" writes `[]`, which disappears again when the scene is emptied or removed.
Level boxes show a small picture of the map (the map editor's own `drawMap`, plain look, drawn once on an off-screen canvas and cached per map object;
`mapThumb.ts`; if drawing throws, a one-pixel-per-square terrain-colour miniature), chapter name and map name. Every box shows a problems badge, and
"what changes" signs with counts: units join / leave (`addUnit` / `removeUnit`), items given, switches or variables set, questions asked, and for levels
a shop (a placed structure whose kind sells items). A level's signs add up all its events. Scenes show no summary of what is said. A legend explains the signs.

**How branches are derived** (`model/timelineGraph.ts`, `timelineFlow.ts`). Arrows: *order* (before to level to after, and a chapter's last box to the next
chapter's first box), *next* (the chapter's own "Next chapter" when it is not the following one), *goto* (every `goto` of a scene, bare, in an `if` / `else`, or
in an answer, to the first box of the chapter it names). A goto arrow is labelled with the answer's words (nested answers joined with ›), a short "if ..." /
"otherwise", or nothing. A goto does not end a scene (the last one that ran wins), so the plain order arrow out of a scene is dropped only when every way
through the scene runs a goto (a bare goto, a question whose answers all jump, an `if` with an `else` where both sides jump). A question whose answers do not
jump is only a marker. If a before scene always jumps, the chapter's level and after scene are drawn dashed and dimmed ("never played"). A goto to a chapter that
does not exist draws nothing (the Campaign checks report it). Reach is worked out from chapter 1's first box along all arrows; chapters not reached are
drawn dashed in a separate band below the story with a note.

**Layout** (`timelineLayer.ts`, `timelineLayout.ts`, `timelineRoute.ts`; pure, unit tested). Columns = one after the latest chapter that leads to it, over forward
arrows; a loop (an arrow to a chapter still being walked in a depth-first walk) is left out of the columns and drawn as a wrap-around arrow under the boxes, so loops
never break the picture. Rows: a chapter keeps the row of the chapter that leads to it by the normal order (the main story stays on one line), a chapter reached
only by a goto takes the nearest free row. Arrows that skip columns, loops, and arrows leaving a box that has another box on its right run along the free
band between rows. A long story is folded: columns that would pass 3400 px start a new line below (still left to right; the arrow across the fold runs under the line and
comes in from the left). Only boxes and arrows near the screen are in the page; the view is moved on the element and handed to React once a frame.

**Controls.** Drag empty space, or one finger, to move; wheel scrolls, Ctrl + wheel or pinch zooms; buttons Zoom in / out, Fit all. A chapter's name above its
boxes selects it (shown in the bar: Move earlier / later, Add a chapter before / after, add or remove its before / after scene, Chapter settings, Remove the chapter).
Dragging any box (mouse or pen) moves that chapter (nearest chapter decides, left half = before it); touch uses the buttons. "+" on an order arrow between two
neighbours adds a chapter there; if the chapter before had "Next chapter" set to the one after, it now points at the new one, so the story goes through it.
Removing a chapter uses the usual confirmation with where it is used (`campaignUsagesOf`); removing a scene that has commands asks first. Every change goes through
`edit` (Undo, saving, Problems). Clicking a scene box opens the chapter's page scrolled to that scene; a level box opens that map in the map editor (a level with no
map opens the chapter's settings); the small button on a level opens its events (collapsed by default, remembered per chapter while the editor is open), ordered:
start of battle, turn numbers ascending, every turn, then conditional ones grouped by trigger kind; an event opens in the map editor's Events tab
(`maps/editor/openRequest.ts`). The chapter page has "Back to the timeline"; the view, selection, expanded levels and the box left from are remembered per project in
memory (`timeline/memory.ts`, not saved). Keyboard: arrows move between boxes (Left / Right in play order, Up / Down to the next row), Home / End, Enter opens, Space selects the
chapter, `[` `]` move it, Delete removes it (asks), `e` opens / closes a level's events, `+` `-` `0` zoom / fit; listed under "Keys".

**Problem badges** (`timelineProblems.ts`). Scene box: Campaign checks naming that scene. Level box: the chapter's other checks and the map's event / objective checks (the
ones the Events tab shows). They are live; the helper's list (last save) is used for a box only when the live checks have nothing for it, so nothing is counted twice.
The badge (count, tooltip and accessible name list the messages) opens the Problems list.

**Limits.** No dependency was added. Scene text is never edited inline. The edge labels are nudged apart but can still overlap in very busy pictures; arrows that
share a free band are spread over a few lanes. Tests: unit `timelineGraph.test.ts`, `timelineLayout.test.ts`; e2e `timeline.spec.ts` (+ `timeline-shots.spec.ts`, SHOTS=1).
