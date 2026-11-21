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
editor/schema/        hand-written schemas of the data files (milestones 2, 3 and 3b); also compiled into the program
                    _common.json, _skills.json, _map.json, _story.json, _scene.json, _battle.json: shared definitions only ("defs",
                    pulled in with "use"): the story language (conditions, triggers, commands) is _story.json, a campaign scene's is
                    _scene.json, the battle scene's art (graphics.json "battle") is _battle.json
editor/native/
  names.h           every user-visible name (product, window title, settings identity)
  model/            QtCore/QtGui only, no widgets
    json_document   one file as an ote::Json tree; load/serialize, comments flag, dirty flag, change signals
    json_commands   QUndoCommands: set / insert / erase / move at a path
    project         the folder: documents, shared QUndoStack, atomic save + conflict check, QFileSystemWatcher
    problems_runner validateProject on a worker thread (one at a time, newest request wins)
    schema          the schema types and loader (editor/schema/*.json), typeOk / sample values
    id_source       the live id lists ("data/units.json", "game.json#stats") the forms' combo boxes use
    story_ids       ids computed from a whole map ("@storyUnits", "@teams") and the one resolver of every id source
    cell_text, tsv, table_shape, play_command, comment_scan   small pure helpers
  ui/               item models (table, tree), editors, dialogs, main window, prompter, settings;
                    form_context (the one way a form edits), form_fields (leaf widgets), form_view (lifecycle, sync, focus),
                    form_build (objects, blocks, paged lists, maps, variants; lazy containers), form_other (the Other keys box),
                    form_drag (the grip of a list item, dragging, moving items with the form's memory kept in step),
                    variant_picker_dialog (the "which kind?" chooser of a list's Add button)
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
  lists have add / remove / move up / move down / **drag by the grip** (below); maps keyed by ids (stats, damage types, resources) show one row per id; a small
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
| `alt` | `"string"`, `"number"`, `"bool"` or `"null"`: another JSON kind the loader also takes as it is; shown as raw JSON without a warning (a `variant` gives a real form instead; `null` is used by the battle scene's animation slots, where `"attack": null` means none) |
| `sample` | a valid value for the conformance test when the generated one would not do (also what a variant writes when picked) |
| `enable` | exactly what a new value of this node starts with: a block's Enable switch, a new list item (`rangeStep`: the engine needs one of two keys), a required list that must not start empty (`frames`: `[[0, 0]]`) |
| `tuple` | on a `list`: a fixed list whose items differ by position, `[column, row, "sheet"]` as `[int, int, ref]`; `length` is its size and `of` stays unset (read item nodes with `itemNode(i)`) |
| `when` | `{ "kind": ["usable", "staff"] }`: the field applies only while the sibling key `kind` (or its `default`) has one of the words |
| `type: "variant"` + `variants` | one of several shapes, see below |
| `free` | a `map` whose keys are the user's own names (no `ref`) |
| `needs` | `{ "ref": "game.json#resources", "hint": "..." }` on a `block`: Enable is offered only while those ids exist; otherwise the hint is shown |
| `excludes` | `["until"]`: setting this key removes those siblings in the same undo step (`turns` / `until`) |
| `min` on a `map` | at least that many entries (a research bonus needs one stat) |
| `use` | pull in a named definition (below) |
| `collapsed` | on a `list`: the items start collapsed, one line each (a command list) |

**Variants.** `"variants": [ { "name": "hp", "key": "hp", "node": {...} }, { "name": "plain id", "kind": "string", "node": {...} },
{ "name": "sides", "value": "sides" } ]`. A variant matches an object that has the key `key`, or a value of JSON kind `kind`
(`string`, `number`, `bool`, `array`, `object`; an `array` may be narrowed by `length` (exactly that many items) and `first` (the JSON kind
of its first item): a cell `[0, 0]` against a list of cells `[[0, 0], [1, 0]]`, `[c, r]` against `[c, r, "sheet"]`), or exactly `value` (no `node`; `null` is a value too). A constant beats the rest;
otherwise **the first variant in the list that fits wins**, so a catch-all `kind` goes after the `key` variants. The picker lists the
`name`s; a variant's `sample` is what picking it writes.

*Commands.* A story command is an object named by one key (`{ "say": ... }`, `{ "changeSide": {...}, "side": "blue" }`): each command
kind is a variant with that `key`, and the other keys of the entry are fields of its node. Two kinds may share a key, and a command
may carry another command's key as an option, so a variant can say more: `"with": ["level"]` (the object must also have all of these:
`setVariable` that reads a unit's level) and `"unless": ["focus", "moveUnit"]` (and none of these: the `wait` command is not the
`wait` option of `focus`). `"help"` on a variant is the one line the Add picker shows next to its name; `"summary"` is the template
of a collapsed list item: `{say}` is the value at that key, `{a.b}` a nested one, `{a|b}` the first of two that has a text, and
`[ ... ]` a part that is left out when a placeholder in it has no text (`[{speaker|speakerName}: ]"{say}"`). Values read short: a
one-key object such as `{ "unit": "gorm" }` as `gorm`, `{}` as "any unit", long texts are cut.

**Definitions.** A schema file may hold `"defs": { "name": node }`; any node may be `{ "use": "name" }` (same file) or
`{ "use": "_common#name" }` (another file; a file with only defs describes no data file). The node's own keys (help, required ...)
are laid over the definition; `"moreVariants": [...]` / `"moreFields": {...}` add to a def's variants / fields (skills and
status conditions share their effects and each adds a few). A definition that contains itself (a command list inside the `if` command) is expanded twice
when the schema is loaded; the third level is a placeholder (`SchemaNode::lazy`) that a form expands the moment it opens it, so scripts
nest as deep as they like. Tools that list keys (the conformance test) skip the placeholders: they are the same keys one level up.

**Id sources** (`ref`): a file (`data/units.json`), a key path (`game.json#damageTypes`, `data/ranks.json#ranks`), the file being
edited (`@sides`, `@structures`: a list of this file whose items have an `id`), the free words of a key (`tags:data/units.json`,
`field:group:data/weapons.json`), the entry names of an object (`keys:data/audio.json#music`, `keys:game.json#rules.ai.weights`;
`keys:@changes` for an object of the file being edited), and two computed lists of a map: `@storyUnits` (the ids a story may name a
unit by: the map's units, their characters and every unit an event adds, at any depth) and `@teams` (team names, then side ids). An
array of sources is allowed (unit ids and tags). One resolver (`resolveSources`) serves forms, tests and the conformance samples. A
`ref` may list `""` among its `values`: the file then accepts the empty text on purpose (no music, no sound) and the form shows it as
"(none, on purpose)".

`json` fields are shown compactly with "Edit JSON...".

**Long lists** are built one page of 50 items at a time: a bar above and below shows "Items 51 to 100 of 300" with previous / next, the
page is remembered, and focusing a key on another page (a problem that names it, "Show in form") switches to it. Every list item
of an object (not a plain value) has move up / down, **duplicate** (the copy goes right after it) and remove, all single undo steps.
**Reordering by drag and drop.** Every list item has a grip (`≡`) at its left. Press it and move: the item dims, a copy of its title floats
with the pointer and a line shows where it would land (near the top or bottom edge the form scrolls by itself); release to drop. The
drag is made of plain mouse events, not `QDrag`, so tests can press, look and release. An item moves within its list, or into another
list of the same file whose items are of the same kind (the same command variants, the same object keys): from one event's commands
into another's, out of or into an `if` branch or a choice's answers, even into an empty list; never into itself or into something inside
it. Plain value lists (ids, numbers) reorder only. One drop is ONE undo step (a single move, or a macro of insert and erase); the items
that were open stay open (the form's per-path memory is moved with them), the shown page of a long list follows the item, and the keyboard
focus goes with it: to its field, or back to the grip or the up / down button that moved it. Keyboard: Tab to a grip, Alt+Up / Alt+Down
(or the up / down buttons); Escape cancels a drag. Code: `form_drag.*`, `FormContext::transferItem`, `FormView::moveListItem`.

A list whose items are one of several shapes (commands, conditions) asks **which kind** when you press Add: a filterable list of the
variant names with their help lines; the new item starts as that kind and is opened for filling in. A collapsed item shows its
number, its kind in bold and its summary on one line.

### Adding a key when the engine gains one
1. Add the key to the schema file of its section (right object, right type, range, help line).
2. If a new data file appears, add `editor/schema/<name>.json` (it is picked up by CMake and compiled in automatically; re-run CMake).
3. Run `native_test_conformance`. It names every key that no context tests ("keys no context tests"), every key the loader
   refuses with a valid sample, every enum word or range end the loader refuses, and every key in a `tests/fixtures` game
   that the schema does not know. Add the key to a context in `editor/native/tests/test_conformance.cpp` (`contexts()`: which
   fixture, which prerequisite edits, which key prefixes it claims, and `overrides` for keys that need a special sample).
   A key that really cannot be sampled goes into `skipped()` / `extraSkips()` with the reason. To iterate on one section run the test
   with `OTE_CONFORMANCE_ONLY=graphics:battle` (only the keys that start so are tried).

### One schema engine, shared with `ote-mcp`
The Qt-free core of the schema logic lives in `editor/core` (`schema_engine.*`: the node types, the loader of the schema files,
`SchemaSet`; `schema_values.cpp`: value checks, defaults, samples, summaries; `schema_ids.*`: id sources and the computed
`@storyUnits` / `@teams` lists), written with `ote::Json` and std types only. `editor/native/model/schema.*`, `id_source.*` and
`story_ids.*` are thin wrappers that add the `QString` / `QStringList` face the forms use (and the Qt resource folder of the embedded
copy). The MCP server for AI assistants (`ote-mcp`, docs/MCP.md) uses the same engine, so a key added to a schema file is known to
the forms and to that server at once. `ote_editor_core_tests` (ctest) tests the engine without Qt; the native tests keep running
on the wrapper.

## Schema vs loader test (`native_test_conformance`)
For every key of every schema, nested ones included, the test copies a fixture into a temp folder, applies the few edits the key
depends on (e.g. the speed fixture for `chargeSpeed`), puts a sample value at the key's place and requires `validateProject` (the
engine's own check, what the Problems dock shows) to report nothing. Enum keys are tried with every word, numbers at both ends
of their range; then one invalid value per enum / range / reference is set and the check must complain about **that file**. The
sample must actually land in the file (a sample that does not is a failure, never a silent pass). The reverse check walks every
`tests/fixtures` file that has a schema and fails on any key the schema does not know, except where a schema is partial on
purpose (top level of `game.json`, keys directly under `rules`, older map unit keys, top level of the core data files); the
reasons are in the test. Cross-key rules (a power costs at most the meter's max, ranks must rise ...) are listed with the reason.

The battle scene's keys (`conformance_battle.cpp`) are tried in copies of demo-srpg with a small valid `battle` section; `tests/fixtures/battle-art`
holds every key for the reverse check. A key that only makes sense next to another (`speed` needs a travel effect and a fixed stage, `order` a
screen layer, `farRight` a `far`) is claimed by a context whose base has it.

Milestone 3b additions: contexts may claim by the start of a variant's name (`maps:events[].do[]{setVariable`), and the
test's id lookup reads the very file it is building (`@sides`, `@storyUnits`, `keys:@music` ...), so a sample names a side, unit or
track that exists. The tables are `conformance_maps.cpp` (units, objectives, prep ...), `conformance_events.cpp` (triggers, conditions
and every command, one context per family of commands with the fixture that has what it points at), `conformance_story.cpp`
(campaign, audio, graphics). Where the language is recursive only the first level of a nested list is tried and the nested copy
of the commands is skipped with that reason; extra skips may be written `*{tag}.tag = "zz_no_such_id"` (a leading `*` matches the end
of the key). A fixture can be a demo game (`demo-economy`, `demo-srpg`), for what the small fixtures lack (audio files, art).

Milestone 3 additions: the tables (which fixture, which edits, which sample for which key) live in `tests/conformance_*.cpp`, split by
family; a key names its place as `schema:path` with `[]` for list items, `*` for map entries and `{name}` for a variant's shape
(`skills:blocks[].do[]{hurt}.hurt{percent}.percent`); the most specific claim wins. Skill blocks are probed with an extra skill
whose block has the `when` an effect or condition belongs to. **Every optional block is also "enabled alone"** (what the Enable
switch writes) and must be accepted; those that cannot be valid alone carry a reason in `blockExcuses()`. The reverse check now
reads the demo games too and also fails on a value shape that no variant describes. Keys excused in the reverse check are only the
raw parts of a map's tile grid (`tiles`, `tileLayer`, `heights`), listed with reasons in `excusedKey()`.

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
| maps: `name`, `grid`, `fog`, `fogVision`, `aiSeesAll`, `sides[]` (+ `storage`), `units[]` (all keys: placement, `character`, `job` / `jobs` / `learned` / `equipped`, `xp`, `merit`, `ranks`, `goesFor`, `behaviour`, `skills`, `difficulty`), `structures[]`, `legend`, `changes`, `prep`, `objectives`, `music` / `playerTurnMusic` / `enemyTurnMusic` / `prepMusic` / `phaseMusic` / `fewEnemies` | covered | raw: `tiles`, `tileLayer`, `heights` (a map painter is a later milestone) |
| maps: `events[]` | covered | 9 triggers, 16 conditions, 62 command shapes (54 command keys) as variants; see below |
| `data/campaign.json` | covered | rules, free battles, parties, chapters; a scene is a list of 25 command kinds (setVariable / addVariable in 4 shapes each) or `{ music, commands }`, with 9 conditions |
| `data/audio.json` | covered | music, sounds, the standard set (`use`, the 24 sound cues, jingles, phase music, few enemies, promotion sounds); the `sounds` of weapons, units, items and move types are real objects whose values are sound ids of this file |
| `data/graphics.json`, `art/*.style.json` | covered | sheets, `unitMarker`, backgrounds, tilesets (+ animations), terrain / unit / structure looks (cell, variants, frames, join, walk, sides, models, standing things), portraits, `styles` (flat and 3D), and the battle scene's art `battle` (`_battle.json`: sets with the twelve animation slots, effects with timing / hide / fade / flash / shake / range steps, assignments, backgrounds, platforms, the fixed stage with range bands and camera pans, scene timing). `join` and a standing thing's `variants` are variants (one block / animation; cells / model files). Raw: a `null` animation slot is shown as `null` (the loader's "none"). The style files are the same shape as graphics.json (the engine never reads them: a tool merges them). `battleBackground` is a key of a map file, not of graphics.json (the loader refuses it there) |

### Story commands
Every command the loader knows has a shape (`native_test_form_story` checks the list against the loader's tables): say, narrate,
addUnit, removeUnit, moveUnit, changeSide, giveItem, takeItem, giveResource, grantResearch, setSwitch, setVariable and addVariable (a
number, or a unit's level / hit points / most hit points / one stat), objective, win, lose, if, endTurn, focus, playMusic, stopMusic,
playSound, setPhaseMusic, setTerrain, applyChange, revertChange, setFog, setVision, openDoor, closeDoor, lockDoor, unlockDoor,
openChest, setBehaviour, wakeGroup, addCondition, removeCondition, promote, teachSkill, forgetSkill, addSupport, choice, fight,
storeItems, showUnit, background, fade, wait, shake, leave, clearStage, enter, move, weather. `wait` and `fade` come last in the list:
`moveUnit` / `focus` also have a `wait` key and the music commands a `fade` key (the `unless` lists). The commands inside `if` and a
choice's answers are real nested forms (collapsed by default, built when opened).

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
- `native_test_form_demo`: the scripted run over a copy of `games/demo-srpg`: every file of demo-srpg, demo-economy and demo-hex opened, every row (first 50) selected
  and fully expanded, every event of every map expanded with each command shown as a form, no warning mark on untouched valid data, nothing under "Other keys" except what a map keeps raw by design
  (and the check is shown to fail on a wrong value and an unknown key); Save all without edits rewrites nothing, an edit rewrites
  only its file, and that file differs from the original only by the edit and the dropped `//` comments.
- `native_test_form_story`: a command is recognised by its key (changeSide with `side`, the `wait` / `fade` keys two kinds share,
  setVariable in five shapes), every command / condition / trigger of the loader has a shape, summaries read like the story, a script
  nested three levels deep (one past the loaded schema tree) opens and edits through undo, a 120-command script is paged and
  duplicate / move / delete / undo work, Add asks which kind (every kind has a help line), and a 300-command event is shown in
  ~40 ms, opened in ~5 ms and paged to its last page in ~45 ms (bound: 3 s together).
- `native_test_form_battle`: on `tests/fixtures/battle-art` (every key of the loader's battle section): the form shows all of it without a
  warning mark and with nothing under "Other keys" (the one `null` slot is the only raw value), numbers / words / yes-no / points write exact
  JSON, blocks, frame times (one number or a list), frames of another sheet, assignments, `effects` / `byType`, `join` and standing-thing
  variants, a wrong value is kept and marked.
- `native_test_form_drag`: the drag simulated with mouse events on a shown form: one undo step per drop, no write for a drop that changes
  nothing, open items stay open (and shift with their index), focus follows the item (field, grip, button), a command moves to another
  event / into an `if` branch / into an empty branch and out again, never into itself or a list of another kind, Escape cancels, the
  buttons and Alt+Up / Alt+Down still work, a plain value list reorders, a long list scrolls while dragging and its page follows the item.
- `native_test_form_screenshots_battle`: the battle forms (sets, effects, effect lists, stage) and a command list mid-drag and after the
  drop, at 1280x720.
- `native_test_form_screenshots3b`: an event with a long command list collapsed (and paged), one command opened, a `choice` with its
  answers, objectives, the preparation screen, the campaign, the audio file and a style file, at 1280x720 and 1920x1080.
- `native_test_form_screenshots3`: skills, an item with kind-specific fields, characters, the campaign and a map's events (still the
  generic tree), a map's sides and units, the General page of `game.json`, and the three view states, at 1280x720 and 1920x1080.
- `native_test_form_screenshots`: units with form, weapons with area and damage type, the Rules page, factions, jobs,
  commanders, a mismatch warning and the Other keys area, at 1280x720 and 1920x1080, light and dark palette.
- `native_test_screenshots`: the main window with the units table, the tree on game.json, the problems dock with an error, the
  nested-JSON dialog, a refused edit, and a dark palette, at 1280x720 and 1920x1080 where it matters.

## Not there yet
- A map painter for the tile grid (`tiles`, `tileLayer`, `heights` stay raw JSON), pickers that show the sound or the picture
  (the battle scene's frames are cell numbers, not a preview), and the table-level help of a column beyond its tooltip.
- Drag and drop moves list items inside one file; it does not carry an item to another file, and it does not drag keys of a map (a
  free map's entries have no order).
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
