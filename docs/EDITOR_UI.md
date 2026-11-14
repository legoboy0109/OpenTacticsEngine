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
