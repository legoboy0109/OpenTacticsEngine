# Editor design (draft)

Owner decisions (2026-11-13):
- Two tools on one game format:
  1. **Browser editor** for novices who never want to script. Primary tool.
  2. **Native developer app** for advanced users (scripting, debugging,
     power tools). Comes after the browser editor's first slice.
- The engine is a tool for making tactics games; demo games exist only to
  exercise features. Do not spend effort on demo balance.
- Build order: fonts/SDL2 → launcher screen in the player → editor first
  slice. From then on engine features ship together with their editor
  screens, so nothing exists that creators cannot reach.

Layout, wording and workflow of both tools are the owner's design
decisions: show options/mock-ups and let them choose.

## Shared foundation (technical decisions)

- Both editors read/write the same game folders (docs/GAME_FORMAT.md). The
  C++ loader in `engine/core/data` is the single source of truth for what
  is valid; editors never re-implement validation rules.
- **`ote-editor` local program** (C++, links `ote_core`): serves the
  browser UI on its own local port and exposes project operations
  (list/open/save files, validate via the real loader, list fonts/assets).
  A local helper is required because Firefox has no file-system API, and
  it lets the editor be used from a phone on the same network. Desktop
  tool only, so the engine's console portability rules do not apply to it
  (it may use threads/exceptions/third-party libs), but it must not leak
  such code into `engine/core`, `engine/platform` or `engine/player`.
- **Play-testing**: the real player compiled to WebAssembly, embedded in
  the editor page, fed the project files from the helper. This is why the
  web build is part of the editor's first slice.
- Files are written atomically (temp file + rename) with stable key order
  and formatting so projects diff cleanly in version control. Undo/redo
  and autosave live in the editor UI.
- "No scripting" for novices means every Lua expression in the format
  needs a non-code alternative in the browser editor: named rule presets
  (e.g. "Emblem-style damage", "Wars-style damage chart"), game templates
  to start from, and form controls that generate the expression. The raw
  expression stays visible only in an "advanced" disclosure / the native
  developer app.

## Browser editor, first slice

1. Project picker: open an existing game folder or create one from a
   template (strategy RPG / economy tactics / blank).
2. Database screens (forms + tables): game settings and rule switches,
   stats, resources, move types, terrain, weapons, unit types. Live
   validation messages from the real loader, shown next to the field.
3. Map editor: paint terrain, place/edit units, set owners and sides,
   resize, per-map settings.
4. Play button: runs the current project in the embedded player.

Decided by the owner: **sidebar layout** — a left sidebar lists the
sections (Game, Rules, Units, Weapons, Terrain, Maps), the main area shows
the selected section, and the project name, problem count and Play button
stay in a top bar.

Technical choices: helper API in docs/EDITOR_API.md (C++ `ote-editor`
using the vendored cpp-httplib); web UI in `editor/web/` built with
TypeScript + React + Vite; the whole project is one document in the UI
(GET/PUT), with undo/redo and autosave on top. Formula presets are UI
concerns: the UI writes the generated Lua expression to the game file and
remembers the chosen preset under an `"editor"` key in `game.json` (the
engine ignores unknown keys).

The editor also teaches itself: a **Learn** button (top bar and game list) opens short hands-on lessons (docs/TUTORIAL.md). Their
progress is remembered by the helper in `learn.json` in the user's data folder (`~/.local/share/OpenTacticsEngine/editor/` on Linux),
outside every game and the repository. `ote-editor --learn-file FILE` puts it elsewhere and `--learn-file ""` keeps it in memory
only; every test starts the helper with a temporary `--learn-file` (docs/EDITOR_API.md).

Still open for the owner (show the first version, then adjust):
overall layout (RPG Maker-like tabs vs sidebar), naming of things shown to
novices ("unit type" vs "class", "terrain" vs "tile"...), how presets are
presented, visual identity.

## Native developer app (later)

Dear ImGui on the engine's SDL3 backend is the expected route (fast to
build, standard for engine tooling). Adds what novices do not need: Lua
script editing, formula console, battle state inspector, event log, AI
debug overlays, `ote-sim` batch runs with charts, replay stepping.
