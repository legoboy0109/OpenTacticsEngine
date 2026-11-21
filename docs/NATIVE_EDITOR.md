# The native editor (preview)

An advanced editor for game folders, written with Qt 6 Widgets. It sits beside the browser editor (docs/EDITOR.md) on the same
game format and does not touch it. Linux is built and tested; Windows is planned. The product name is not decided: the working
target is `ote-editor-native`, and every user-visible name lives in `editor/native/names.h`.

What milestone 1 does:
- Open a game folder (needs a `game.json`); recent projects and the window layout are remembered.
- One tab per JSON data file. A file that is an array of objects (or an object holding one such array: `units.json`,
  `weapons.json`, `terrain.json`, `items.json` ...) opens as a **table**; every other file (`game.json`, maps ...) as a **tree**.
- Edits are made on the parsed tree itself, so key order and keys the editor does not know about survive. One undo stack for the
  whole project (Undo / Redo / history dock), dirty marks per file, Save / Save all.
- **Problems** dock: the engine's own project check (`validateProject`, the same one `ote-editor` serves) runs on the saved files
  on a worker thread, on open and after every save. Double-clicking a problem opens the file it names.
- **Play** starts `ote-player` for the open folder as a separate process.

## Build

```
cmake -S . -B build-native -DOTE_BUILD_NATIVE_EDITOR=ON
cmake --build build-native --target ote-editor-native
```
`OTE_BUILD_NATIVE_EDITOR` is OFF by default and needs `OTE_BUILD_EDITOR=ON`; nothing else in the repository depends on Qt. It
uses only QtCore, QtGui, QtWidgets and QtTest from qt6-base (6.12 here). Run `build-native/editor/native/ote-editor-native [GAME_FOLDER]
[--settings-file FILE.ini]`. Play looks for `ote-player` next to the editor program; change it under Edit > Player program.
Play passes `--games <parent> --game <folder name>` (the player's `--game` is an id, not a path) and `--assets` when an
`assets/` folder sits next to the player or one level up.

### Exceptions and RTTI
`ote_core` hands `-fno-exceptions -fno-rtti` to every program that links it, but Qt's headers and moc output need both. The top-level
`CMakeLists.txt` builds those two flags as generator expressions that are skipped for a target with the property
`OTE_RTTI_EXCEPTIONS_OK`; every target in `editor/native` sets it (`ote_native_target()`). The engine libraries stay built without
RTTI; only plain non-polymorphic types (`ote::Json`, `ote_editor_core` functions) cross that line.

## Layout
```
editor/core/        ote_editor_core (static): validateProject, audio check, safe paths, revision hash, log. No HTTP.
                    Linked by ote-editor (the HTTP shell, behaviour unchanged) and by the native editor.
editor/schema/        hand-written schemas of the data files (milestones 2 and 3); also compiled into the program
                    _common.json, _skills.json: shared definitions only ("defs", pulled in with "use")
editor/native/
  names.h           every user-visible name (product, window title, settings identity)
  model/            QtCore/QtGui only, no widgets
    json_document   one file as an ote::Json tree; load/serialize, comments flag, dirty flag, change signals
    json_commands   QUndoCommands: set / insert / erase / move at a path
    project         the folder: documents, shared QUndoStack, atomic save + conflict check, QFileSystemWatcher
    problems_runner validateProject on a worker thread (one at a time, newest request wins)
    schema          the schema types and loader (editor/schema/*.json), typeOk / sample values
    id_source       the live id lists ("data/units.json", "game.json#stats") the forms' combo boxes use
    cell_text, tsv, table_shape, play_command, comment_scan   small pure helpers
  ui/               item models (table, tree), editors, dialogs, main window, prompter, settings;
                    form_context (the one way a form edits), form_fields (leaf widgets), form_view (lifecycle, sync, focus),
                    form_build (objects, blocks, lists, maps, variants; lazy containers), form_other (the Other keys box)
  tests/            QtTest programs (below)
```
Design rules worth knowing:
- **Edits never copy the file.** The table and tree models read the document's tree directly; each change is announced by the
  document (`aboutToChange` / `changed`) so Qt's begin/end protocol is exact and views keep selection and expansion.
- **Dirty = version counter.** Commands bump a per-file counter on redo and drop it on undo; dirty means "counter differs from the
  saved one", so undoing back to the saved state is clean again. Reloading a file bumps a generation, which turns commands recorded
  for the old tree into no-ops.
- **Conflicts.** Save compares the file on disk with what was loaded or last written (content hash). A mismatch writes nothing and
  asks: reload from disk, keep mine (overwrite), or cancel. The write itself is `QSaveFile` (temp file + rename). The file watcher
  only marks a file "changed on disk"; it never reloads on its own.
- **Comments.** The JSON reader accepts `//` and `/* */` comments, `ote::Json` does not keep them, so saving drops them. The
  model exposes `hasComments`; the window warns once before the first save of such a file (default until the owner decides).
  `game.json` and most demo data files have comments.
- **Type kept.** A cell that holds a number stays a number: text in it is refused with a red message. Nested objects and arrays are
  shown as compact JSON and edited in a small dialog that will not close on invalid JSON.
- **Sort and filter are view-only**; "Move up/down" is disabled while either is active.
- **Prompter.** Every question (comment loss, conflict, unsaved changes, errors) goes through `Prompter`; tests script it.
- Saved numbers are written by `ote::Json::dump`, i.e. whole numbers plainly and other numbers with 17 significant digits
  (0.1 becomes 0.10000000000000001). Valid, but ugly; fixing it means changing the engine's writer.

## Schema-driven forms (milestone 2)

A file with a **schema** gets a form; a file without one behaves exactly as in milestone 1 (plain table or tree, nested values
as raw JSON). Schemas are small hand-written JSON files in `editor/schema/`. They describe keys for the editor only: the
engine's loader stays the authority on what is valid, and `native_test_conformance` (below) proves the two agree.

What a user sees:
- **Table files** (`units`, `weapons`, `research`, `factions`, `jobs`, `commanders`, `terrain`, `items`): the table on the left,
  the form of the selected row on the right (a splitter; the sizes are the user's to change). Scalar keys are table columns, and
  objects marked `expand` give one real column per member (`stats.hp`, `resist.fire`, `cost.gold`) instead of one JSON cell.
  Editing an absent nested cell creates the parent object in the same undo step.
- **`game.json`** and **maps**: two pages under the tab, "Rules" (the form) and "Tree (all keys)". Every optional block
  (`rules.actionPoints`, `rules.height`, ...) is a frame with an **Enable** checkbox that adds or removes the block (one undo step);
  required keys of a new block are filled in, so enabling is a valid first step where possible.
- A form shows the raw key as the label (required keys get a `*`) and the schema's one-line help under the field. Id fields are
  combo boxes filled from the project's live data (unsaved edits included) and refilled when the referenced file changes;
  lists have add / remove / move up / move down; maps keyed by ids (stats, damage types, resources) show one row per id; a small
  `x` removes a key (the game then uses its default).
- **Nothing is ever lost or blocked.** A key the schema does not know stays in the file untouched and appears under
  **Other keys (n)** at the bottom of its object: read-only JSON, "Edit as JSON..." (one undo step) and "Show in tree view". A value
  of the wrong kind (text where a whole number belongs, a list of three where two are expected) is shown as it is, as raw JSON
  with a red warning, and is never converted; a number outside its range, or a word / id that is not in the list, is kept and
  marked the same way. Typing something invalid into a field is refused with a note under the field and writes nothing.
- **Problems jump to the place**: double-clicking a problem opens the file, selects the row it names (by `id`, or by position such
  as `technologies[2]`) and focuses the key it names; for `game.json` and maps it focuses the block or key (`rules.actionPoints`,
  `"resistRange"`). When nothing can be placed the file is simply opened.
- Every edit goes through the same path-based undo commands as the table and the tree: one user action is one undo step, and
  form, table and tree of the same file always show the same document.

### Where schemas are read from
`--schema-dir FOLDER` (or the environment variable `OTE_SCHEMA_DIR`) > a folder named `schema` next to the program > the copy
compiled into the program (Qt resources, so the program works standalone; the test `embeddedSchemasEqualTheSourceFiles` keeps
the copy equal to `editor/schema/`). A schema file with a mistake is reported in the test, never fatal in the program.

### Schema file format
```jsonc
{
  "title": "Rules",                       // tab title of the form page of a non-table file (default "Form")
  "files": ["data/weapons.json"],         // project-relative files; "maps/*.json" matches every map
  "rows":  { /* node */ },                // table files: the schema of ONE row (an object node)
  "root":  { /* node */ }                 // other files: the schema of the whole document (an object node)
}
```
A **node** (`type` defaults to `object` when `fields` is present, otherwise `json`):

| key | meaning |
|---|---|
| `type` | `string`, `int`, `number`, `bool`, `enum`, `ref`, `list`, `object`, `map`, `json` |
| `help` | one line of developer wording, shown under the field (all wording lives here, never in C++) |
| `required` | the key must be present (marked `*`; cannot be cleared) |
| `default` | what the engine assumes when the key is absent (shown as placeholder / "not set: on") |
| `min`, `max` | range of an `int` / `number` |
| `values` | the words of an `enum`; for a `ref`, extra words allowed beside the ids (`"choose"`) |
| `ref` | where ids come from: `"data/units.json"` (ids of its table rows) or `"game.json#damageTypes"` (the list under that key path; items are objects with an `id`, or plain strings). A `ref` node is an id picker; on a `map` it names the allowed keys; an array of sources is allowed |
| `of` | node of the items of a `list` / values of a `map` |
| `length` | a `list` of exactly that many items (a pair such as `[min, max]`, shown inline) |
| `fields` | `{ "key": node, ... }` of an `object`, in display order |
| `block` | an `object` that may be absent as a whole: shown with an Enable checkbox |
| `expand` | table: one column per member of this object / per id of this map |
| `alt` | `"string"`, `"number"` or `"bool"`: another JSON kind the loader also takes as it is; shown as raw JSON without a warning (no schema uses it any more: a `variant` gives a real form) |
| `sample` | a valid value for the conformance test when the generated one would not do (also what a variant writes when picked) |
| `when` | `{ "kind": ["usable", "staff"] }`: the field applies only while the sibling key `kind` (or its `default`) has one of the words |
| `type: "variant"` + `variants` | one of several shapes, see below |
| `free` | a `map` whose keys are the user's own names (no `ref`) |
| `needs` | `{ "ref": "game.json#resources", "hint": "..." }` on a `block`: Enable is offered only while those ids exist; otherwise the hint is shown |
| `enable` | on a `block`: exactly what Enable writes |
| `excludes` | `["until"]`: setting this key removes those siblings in the same undo step (`turns` / `until`) |
| `min` on a `map` | at least that many entries (a research bonus needs one stat) |
| `use` | pull in a named definition (below) |

**Variants.** `"variants": [ { "name": "hp", "key": "hp", "node": {...} }, { "name": "plain id", "kind": "string", "node": {...} },
{ "name": "sides", "value": "sides" } ]`. A variant matches an object that has the key `key`, or a value of JSON kind `kind`
(`string`, `number`, `bool`, `array`, `object`), or exactly `value` (no `node`). A constant beats a kind. The picker lists the
`name`s; a variant's `sample` is what picking it writes.

**Definitions.** A schema file may hold `"defs": { "name": node }`; any node may be `{ "use": "name" }` (same file) or
`{ "use": "_common#name" }` (another file; a file with only defs describes no data file). The node's own keys (help, required ...)
are laid over the definition; `"moreVariants": [...]` / `"moreFields": {...}` add to a def's variants / fields (skills and
status conditions share their effects and each adds a few). A definition that contains itself is expanded twice, then ends as
raw JSON.

**Id sources** (`ref`): a file (`data/units.json`), a key path (`game.json#damageTypes`, `data/ranks.json#ranks`), the file being
edited (`@sides`, `@units`), the free words of a key (`tags:data/units.json`, `field:group:data/weapons.json`); an array of
sources is allowed (unit ids and tags).

`json` fields are shown compactly with "Edit JSON...". Lists with more than 60 items show the first 60 (the tree shows all).

### Adding a key when the engine gains one
1. Add the key to the schema file of its section (right object, right type, range, help line).
2. If a new data file appears, add `editor/schema/<name>.json` (it is picked up by CMake and compiled in automatically; re-run CMake).
3. Run `native_test_conformance`. It names every key that no context tests ("keys no context tests"), every key the loader
   refuses with a valid sample, every enum word or range end the loader refuses, and every key in a `tests/fixtures` game
   that the schema does not know. Add the key to a context in `editor/native/tests/test_conformance.cpp` (`contexts()`: which
   fixture, which prerequisite edits, which key prefixes it claims, and `overrides` for keys that need a special sample).
   A key that really cannot be sampled goes into `skipped()` / `extraSkips()` with the reason.

## Schema vs loader test (`native_test_conformance`)
For every key of every schema, nested ones included, the test copies a fixture into a temp folder, applies the few edits the key
depends on (e.g. the speed fixture for `chargeSpeed`), puts a sample value at the key's place and requires `validateProject` (the
engine's own check, what the Problems dock shows) to report nothing. Enum keys are tried with every word, numbers at both ends
of their range; then one invalid value per enum / range / reference is set and the check must complain about **that file**. The
sample must actually land in the file (a sample that does not is a failure, never a silent pass). The reverse check walks every
`tests/fixtures` file that has a schema and fails on any key the schema does not know, except where a schema is partial on
purpose (top level of `game.json`, keys directly under `rules`, older map unit keys, top level of the core data files); the
reasons are in the test. Cross-key rules (a power costs at most the meter's max, ranks must rise ...) are listed with the reason.

Milestone 3 additions: the tables (which fixture, which edits, which sample for which key) live in `tests/conformance_*.cpp`, split by
family; a key names its place as `schema:path` with `[]` for list items, `*` for map entries and `{name}` for a variant's shape
(`skills:blocks[].do[]{hurt}.hurt{percent}.percent`); the most specific claim wins. Skill blocks are probed with an extra skill
whose block has the `when` an effect or condition belongs to. **Every optional block is also "enabled alone"** (what the Enable
switch writes) and must be accepted; those that cannot be valid alone carry a reason in `blockExcuses()`. The reverse check now
reads the demo games too and also fails on a value shape that no variant describes. Keys excused in the reverse check are only the
raw parts of a map (tile grid, events ...), listed with reasons in `excusedKey()`.

## What is covered
| Section | State | Note |
|---|---|---|
| `game.json`: top level (`name` ... `author`, `saving`, `battle*`, `menu*`, `attackWeaponChoice`, `grid`, `view`, `font`, `stats`, `roles`, `resources`, `moveTypes`, `switches`, `variables`, `scripts`, `maps`, `formulas`, `damageTypes`, `resistRange`, `difficulties`, `styles`) | covered | `moveTypes[].sounds` is a free map of raw values (needs audio.json) |
| `game.json` `rules.*`: `counterAttacks`, `defeat`, `hitRoll`, `turnOrder`, `economy`, `production`, `capture`, `items` (+`freeBattleStorage`), `supply`, `research`, `fog`, `display`, `actionPoints`, `height`, `facing`, `progression` (+ rank table), `ranks`, `jobs`, `rescue`, `promotion`, `places`, `ai` (+ weight sets) | covered | |
| `data/weapons.json`, `data/units.json`, `data/terrain.json` | covered | `sounds` are free maps of sound ids |
| `data/items.json` | covered | kind-specific keys with `when`; staff `effects`, `range`, `area`, `hit`, `xp` as variants |
| `data/skills.json`, `data/conditions.json` | covered | conditions (21 shapes) and effects (40 shapes) are variants; which effect works in which kind of block is in the help line and the engine's check |
| `data/abilities.json` | covered | `effect` (11 shapes) as a variant |
| `data/structures.json` | covered | `door`, `chest`, `visit`, map weapon keys |
| `data/characters.json`, `data/supports.json`, `data/ranks.json`, `data/caps.json` | covered | quotes and support talk as lists of `say` lines |
| `data/research.json`, `data/factions.json`, `data/jobs.json`, `data/commanders.json` | covered | skill grants and AI weights are real forms now |
| maps: `name`, `grid`, `fog`, `fogVision`, `aiSeesAll`, `sides[]`, `units[]` (placement, name, level, items, stats), `structures[]` | partly | raw for now: `tiles`, `legend`, `tileLayer`, `heights` (a map painter is later), `events`, `changes`, `prep`, `objectives`, a side's `storage`, a unit's `behaviour`, `character`, `job`, `learned`, `equipped`, `xp`, `merit`, `ranks`, `goesFor` |
| story events (triggers, commands), `data/campaign.json`, `data/audio.json`, `data/graphics.json`, `art/*.style.json` | not covered | generic table / tree as in milestone 1: milestone 4 |

## Tests
Registered in ctest as `native_test_*` only when the option is ON. They set `QT_QPA_PLATFORM=offscreen`, so no window ever
appears; settings go to an ini file in a temp folder, scratch data to `<build>/test-tmp`, and the games are copies of
`games/demo-srpg`. Play is only ever checked as a command line (a hook replaces the process start), the folder dialog and all
message boxes are hooks too.
```
cd build-native && ctest -R native_ --output-on-failure
QT_QPA_PLATFORM=offscreen OTE_SHOT_DIR=/some/folder ./editor/native/native_test_screenshots   # PNGs for looking at
```
- `native_test_model`: every operation with undo/redo to byte-identical text, key order and unknown keys, dirty tracking, conflicts,
  comments, file watching, cell rules.
- `native_test_table`, `native_test_tree`: the item models (also under `QAbstractItemModelTester`), edits, row operations, TSV
  copy/paste, nested JSON, the demo game's data files.
- `native_test_table_perf`: 5000 generated rows (load, paint, sort, filter, scroll, edits, paste, undo, save) with generous bounds;
  the measured times are printed.
- `native_test_problems`, `native_test_window`: the problems check and the scripted window (edit, undo, redo, save, problems naming
  the file, conflict, comment warning, Play command line, remembered state, leaving with unsaved work).
- `native_test_schema`: schema parsing, kinds, samples, ids.
- `native_test_form`: one widget per field kind, one undo step per action, refused input, live id combos, lists, maps,
  blocks, unknown keys byte for byte (edit and undo all: identical text), wrong-kind warnings, form / table / tree in sync,
  problem messages placed, schema columns.
- `native_test_form_window`: scripted run on a temp copy of a fixture: new resource, action points enabled in Rules, new
  damage type, a weapon given it through the combo, save, problems empty, undo all -> files as loaded; problem double-click;
  "Show in tree view"; no schema -> milestone 1 behaviour.
- `native_test_conformance`: see above.
- `native_test_form_features`: `when`, variants, free maps, block `needs` / `enable`, lazy building (collapsed containers are not
  instantiated; focusing a key opens the way), two columns, the table | table + form | form views and their per-kind memory, and
  a timing test with the real schemas (Rules page with every block on, a skill with 8 blocks x 6 effects; bound 1.5 s, measured ~30 ms).
- `native_test_form_demo`: the scripted run over a copy of `games/demo-srpg`: every file opened, every row (first 50) selected
  and fully expanded, no warning mark on untouched valid data, nothing under "Other keys" except what a map keeps raw by design
  (and the check is shown to fail on a wrong value and an unknown key); Save all without edits rewrites nothing, an edit rewrites
  only its file, and that file differs from the original only by the edit and the dropped `//` comments.
- `native_test_form_screenshots3`: skills, an item with kind-specific fields, characters, the campaign and a map's events (still the
  generic tree), a map's sides and units, the General page of `game.json`, and the three view states, at 1280x720 and 1920x1080.
- `native_test_form_screenshots`: units with form, weapons with area and damage type, the Rules page, factions, jobs,
  commanders, a mismatch warning and the Other keys area, at 1280x720 and 1920x1080, light and dark palette.
- `native_test_screenshots`: the main window with the units table, the tree on game.json, the problems dock with an error, the
  nested-JSON dialog, a refused edit, and a dark palette, at 1280x720 and 1920x1080 where it matters.

## Not there yet
- Forms for story events (triggers and commands), the campaign, audio, graphics and style files, a map's objectives, preparation
  screen, behaviours and map changes; pickers for sounds and art; a map painter for the tile grid; and the table-level help of a
  column beyond its tooltip.
- Which skill effect or condition is allowed in which kind of block is not filtered in the picker (the help line says, the engine's
  check refuses).
- Lua tooling (editing scripts, formula checks inline, debugging).
- Live validation of unsaved edits (the problems check runs on saved files only); a problem is placed by the names in its
  message, not by an exact node.
- In-process preview of a map or battle (Play starts the separate player on the saved files).
- Non-JSON files (Lua scripts, art, audio) in the file list; creating, renaming and deleting files.
- Keeping `//` comments.
- A Windows build and installer (the code avoids platform-specific calls; not yet tried).
- A settings dialog beyond the player path; translations.
