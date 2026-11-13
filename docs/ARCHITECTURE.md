# Architecture

OpenTacticsEngine is a standalone tactics-game engine in the spirit of RPG
Maker: creators describe a game with data files (and optional Lua), and a
small player program runs that game on each platform.

## Goals that shape everything

1. **Runs everywhere**: PC, web, phones, current consoles, retro handhelds,
   and (via homebrew) old consoles such as Xbox, Xbox 360, PS3, Wii U.
2. **Two genres, one engine**: economy tactics (Advance Wars, light 4X) and
   strategy RPGs (Fire Emblem, FFT) are combinations of rule switches.
3. **Any grid**: square top-down first; hex, isometric and 3D later.
4. **Approachable**: common things are data/editor only; Lua for the rest.

## Layers

```
games/<game>/          data files + Lua          (docs/GAME_FORMAT.md)
engine/core/           rules core: pure logic, no graphics, no platform code
  base/                json, log, rng, shared types
  grid/                grid topology (the only code that knows cell layout)
  data/                Database + loader (game folder -> structs)
  script/              Lua host (formulas, later event hooks)
  battle/              Battle state, actions, events, combat, turns, modules
  ai/                  computer opponent (uses only Battle's public API)
engine/platform/       platform.h interface; common/ (shared text engine, fonts); one folder per backend (sdl3/, sdl2/)
engine/player/         the game-running app: scenes, UI, input -> actions
tools/sim/             headless AI-vs-AI runner (testing, balance)
tests/                 unit tests for the core
third_party/lua/       Lua 5.4 (vendored)
third_party/stb/       stb_truetype (public domain, the only font rasterizer)
```

Dependency rule: `core` depends on nothing but Lua and the C/C++ standard
library. `player` depends on `core` and `platform.h`. Only
`platform/<backend>/` may touch SDL or any device API.

## Platform layer and text

`platform.h` is the whole device interface (`Renderer`, `App`, `runApp`).
Backends: `platform/sdl3/` (default) and `platform/sdl2/` (plain SDL 2.0.x
calls, for old ports). Pick one at configure time:
`cmake -S . -B build [-DOTE_PLATFORM=sdl3|sdl2]`; the player sources are
identical for both.

Everything text-related is backend independent and lives in
`platform/common/`: `TextEngine` (UTF-8, measuring, glyph atlas, the
built-in 8x8 font from `bitmap_font` and TrueType via stb_truetype),
`font_registry` (built-in font list from `assets/fonts/fonts.json`, and
game/command-line font choice -> `FontRequest`) and `StickMapper`.
`smooth` fonts are rasterized at the real display resolution and drawn 1:1
in output pixels; `textWidth`/`lineHeight` are always logical pixels, so
layout never depends on the window size or the font (the player measures
everything, it never assumes a glyph size).

A new backend (another SDL, a console SDK) implements `Renderer` +
`runApp` and gives `TextEngine` a `TextTarget` (create/update/destroy an
RGBA texture, draw a tinted sub-rectangle in logical or output pixels,
report the logical->output transform). Copy `sdl2/`, replace the SDL calls.

## Core ideas

- **Battle is a copyable value; changes go through `apply(Action)`**, which
  appends `Event`s. The player animates events; the AI and tests call the
  same API. This gives undo (copy), replays (seed + actions), and later
  network play, for free. See the header comment in `core/battle/battle.h`.
- **Grid topology is isolated** in `core/grid`. Rules call
  `neighbors()`/`distance()` only, so a new layout touches `grid` and the
  renderer, not the rules.
- **Rule modules are switches** in `RulesConfig` (economy, production,
  capture, progression, defeat conditions, later fog/research/turn order).
  Each lives in its own file under `core/battle/`.
- **Formulas are Lua expressions in data** (`"damage": "a.str - b.def"`),
  compiled once. Game scripts can define functions the formulas call.
- **Determinism**: all randomness comes from `Battle::rng` (PCG32). No
  wall-clock, no `rand()`, no Lua `math.random` in rules.

## Portability rules (do not break these)

- C++17, **no exceptions, no RTTI**, no threads, no `<filesystem>`,
  `<regex>`, iostream in engine code. No `std::stoi`-style throwing calls.
- No third-party libraries in `core` except vendored Lua.
- Never assume little-endian: binary files (future saves/asset packs) must
  be written byte by byte. Text formats are preferred.
- Rendering goes through `platform.h`: logical-pixel 2D primitives only.
  The backends use SDL3's / SDL2's 2D renderer; a GLES3/SDL_GPU backend for
  effects and 3D grids is a later, optional addition. Text goes through the
  shared `TextEngine` (no SDL_ttf/FreeType).
- Fixed small logical resolution (default 480x270) scaled to the display.
- Keep memory modest (original Xbox has 64 MB).

## Launcher and options

`ote-player` opens a launcher: Home (games from `games/index.json`, then
any other sub-folder with a `game.json`, alphabetical; plus Options and
Quit) -> a game's page (Play, Game options, Back) -> map select -> battle.
The folder scan is `listSubdirs()` in the platform layer (the SDL2 backend
returns false on platforms without a folder API, where only the index
counts). `userDataDir()` holds `settings.json` (`core/data/settings.*`:
global options, per-game overrides, remembered UI state such as the last
game/map). `--headless` never reads or writes it unless `--settings FILE`.

Where things live (`engine/player/`):
- `options/option_defs.cpp`: THE table of options (key, label, list of
  choices, how a stored value feeds the look). Both options screens
  (`scene_options*.cpp`, global and per-game) are generated from it.
- `options/effective.cpp`: the one function that combines the layers into
  the font request, UI scale and style index. Priority, highest first:
  command-line flags > per-game override > global option > the game's own
  default (`font`, style index 0) > engine default. On launcher screens
  (no game) the game level is skipped. An explicit font size wins, else the
  size belonging to the font in use; `uiScale` then multiplies it.
- `app_look.cpp`: loads/saves settings (saved on every scene switch and on
  exit) and applies the result through `Renderer::setFont` / `setLogicalSize`.
- `scene_home|game|options*.cpp`, `game_library.*` (discovery),
  `ui/chrome.*` and `ui/row_list.*` (shared look, scrolling lists).

Adding an option: add one entry to `optionDefs()`; if it affects the look,
add a field to `LookLayer` (effective.h), read it in `resolveLook()`, and
fill it in the entry's `apply`. The screens, the per-game override, "Use
global", reset and persistence need no further code. Style recoloring
itself is in `battle/battle_draw.*` (`terrainRgb`, background, glyphs).

## Web

`emcmake cmake` builds `ote-player` with Emscripten's SDL3 port into
`build-web/engine/player/` (see HANDOFF.md). The main loop is
`emscripten_set_main_loop_arg` (sdl_app.cpp); the module is MODULARIZE'd
(`createOtePlayer`), `INVOKE_RUN=0`, and exports `FS`, `callMain`, `IDBFS`.
The pages decide the arguments and call `Module.callMain([...])`.

**Files reach the engine through Emscripten's in-memory file system** (the
engine just `fopen`s): `engine/platform/web/player.js` instantiates the
module, the page's `prepare` callback fetches files and writes them with
`FS.writeFile` (creating folders), then `callMain` runs. `index.html` reads
`games.manifest.json` (made by `make_manifest.py` at build time: every file of
`games/` and `assets/` with sizes) and loads `/games/...` and `/assets/...`;
it starts the launcher with no arguments. `?args=...` appends arguments
(spaces as `+`), e.g. `--autoplay --frames 300`; `--input` works too.

**Embed protocol** (`embed.html`, for the editor's Play button, inside an iframe):
`embed.html?project=<id>[&map=<mapId>][&api=<base>][&font=<id>][&style=<id>]`.
It fetches `<api>/api/projects/<id>/files` and each
`<api>/api/projects/<id>/file/<path>` (path segments URL-encoded, 6 requests at
a time) into `/games/<id>/`, plus `<api>/assets/fonts/fonts.json` and every font
it lists into `/assets/fonts/` (`api` defaults to the page's origin), then runs
`ote-player --games /games --assets /assets --no-settings --game <id> [--map ..]`.
Messages to the parent window: `{type:"ote-player-ready"}` once the first frames
are drawn; `{type:"ote-player-error", message}` on any failure (file list not
loadable, a script error, or engine `[ERROR]` lines printed while starting, such
as a broken game.json - the engine's own error screen stays visible in that
case; for load failures the page shows an error panel). From the parent:
`{type:"ote-player-restart"}` reloads the page (re-fetches the project).
The canvas fills the iframe (CSS `!important` size, SDL sizes the backing
store to CSS size x devicePixelRatio via `SDL_WINDOW_HIGH_PIXEL_DENSITY`, so
smooth text is rasterized at real resolution); the engine letterboxes inside.
Arrow keys, space, tab and backspace are `preventDefault`ed so the host page
never scrolls; the canvas takes focus on load and on click.

**Settings persistence.** `userDataDir()` on the web is `/userdata` if it
exists, else empty (settings then live in memory only). The standalone page
mounts IDBFS there and loads it (`FS.syncfs(true)`) before `main`; after every
successful settings save the app calls `commitUserData()` (platform.h; no-op on
desktop) which runs `FS.syncfs(false)`. The embed page mounts nothing and passes
`--no-settings` (new flag: settings are never read or written, always
defaults), so the editor preview always starts from the defaults.

Testing: `tools/webtest` (HANDOFF.md). Known limits: the SDL2 backend's
Emscripten branch is untested; when the main loop ends (`--frames`, end of an
`--input` script) destructors do not run (Emscripten unwinds), the page stays up.

## Testing

`ote_tests` (unit tests) and `ote-sim` (plays whole battles AI vs AI,
headless). The player has automation flags (headless, scripted input,
screenshot) so UI flows can be verified without a human.
