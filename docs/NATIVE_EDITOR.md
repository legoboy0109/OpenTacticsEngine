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
editor/native/
  names.h           every user-visible name (product, window title, settings identity)
  model/            QtCore/QtGui only, no widgets
    json_document   one file as an ote::Json tree; load/serialize, comments flag, dirty flag, change signals
    json_commands   QUndoCommands: set / insert / erase / move at a path
    project         the folder: documents, shared QUndoStack, atomic save + conflict check, QFileSystemWatcher
    problems_runner validateProject on a worker thread (one at a time, newest request wins)
    cell_text, tsv, table_shape, play_command, comment_scan   small pure helpers
  ui/               item models (table, tree), editors, dialogs, main window, prompter, settings
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
- `native_test_screenshots`: the main window with the units table, the tree on game.json, the problems dock with an error, the
  nested-JSON dialog, a refused edit, and a dark palette, at 1280x720 and 1920x1080 where it matters.

## Not there yet
- Schema-aware forms (typed fields per game format section, pickers for ids and sounds). Everything is generic JSON for now.
- Lua tooling (editing scripts, formula checks inline, debugging).
- Live validation of unsaved edits (the problems check runs on saved files only) and jumping to the exact node of a problem.
- In-process preview of a map or battle (Play starts the separate player on the saved files).
- Non-JSON files (Lua scripts, art, audio) in the file list; creating, renaming and deleting files.
- Keeping `//` comments.
- A Windows build and installer (the code avoids platform-specific calls; not yet tried).
- A settings dialog beyond the player path; translations.
