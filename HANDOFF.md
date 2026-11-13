# Handoff

Current state of the build, for whoever (or whichever model) continues.
Read docs/PLAN.md (roadmap + owner decisions), docs/ARCHITECTURE.md
(structure + binding portability rules) and docs/GAME_FORMAT.md first.

## State (2026-11-13)

Everything below is merged on `main` (local repo only, nothing pushed).

- Rules core (`engine/core`): JSON, loader, grid, battle, Lua formulas, AI;
  `ote-sim` plays both demos AI-vs-AI. `ote_tests`: 55 green.
- Player (`ote-player`): launcher, options with per-game overrides, real
  fonts; SDL3 and SDL2 backends; web build (Emscripten).
- Editor first slice complete: helper `ote-editor` (API test 134 green) and
  the browser UI with every screen including the map editor and Play
  (137 unit tests, 85 e2e in Chromium + Firefox smoke).
- Waiting on the owner: verdict on editor wording/layout, launcher layout,
  font picks, licence choice. Not started: README.

## Editor helper

`ote-editor` (editor/server) serves the browser editor and the HTTP API in
docs/EDITOR_API.md, using the real loader for validation. Starter projects
live in editor/templates (strategy-rpg, economy-tactics, blank). Build and
run (does not need the player target):

```
cmake -S . -B build -G Ninja -DOTE_BUILD_PLAYER=OFF && cmake --build build
./build/ote-editor            # from the repo root; http://127.0.0.1:8765/
python3 tests/editor_api_test.py ./build/ote-editor .   # also ctest "editor_api"
```

Notes: all /api handlers run under one mutex (engine core is not
thread-safe); `/` shows a built-in placeholder page until the web UI is built
(editor/web/dist). Creating a project does not touch
games/index.json. The helper is built with exceptions on (httplib, v0.26.0 in
third_party/httplib); the engine itself stays exception-free.
`ote_tests` writes a settings file into `<repo>/build/`, so that folder must
exist when running it from another build dir name.

## Editor web UI

`editor/web/` is the browser editor (TypeScript, React 18, Vite; runtime deps: react, react-dom only). All screens are
done: project picker, Game, Rules, Units, Weapons, Terrain, **Maps (full map editor)** and **Play** (embedded web player).
Layout: `src/api` (the only fetch calls), `src/model` (pure doc edits, no React; `map*.ts` = the map grid model),
`src/store` (undo, autosave, conflict), `src/ui` (components), `src/screens/<section>` (`maps/editor/` = canvas, tools,
panels), `src/play` (Play button logic + panel), `src/shell` (top bar, sidebar). Spec and decisions: docs/EDITOR_UI.md.

Run the whole editor end to end from a clean checkout (needs CMake, Ninja, Node 20+, Emscripten for Play):

```
cmake -S . -B build -G Ninja -DOTE_BUILD_PLAYER=OFF && cmake --build build            # the helper -> build/ote-editor
PATH="$HOME/.local/share/emsdk:$HOME/.local/share/emsdk/upstream/emscripten:$PATH" EM_CONFIG="$HOME/.local/share/emsdk/.emscripten" \
  emcmake cmake -S . -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-web   # web player (Play), ~1 min
(cd editor/web && npm install && npm run build)                                        # the UI -> editor/web/dist
./build/ote-editor            # from the repo root; open http://127.0.0.1:8765/
```
The helper's defaults (`--web editor/web/dist`, `--player build-web/engine/player`, `--games games`) match this layout.
Without the web player build everything but Play works (Play explains how to build it).

```
cd editor/web
npm run dev            # Vite on :5173, proxies /api /assets /player to 127.0.0.1:8765 (a helper must be running)
npm run typecheck      # tsc, zero errors expected
npm test               # vitest unit tests (model incl. map grid, presets, store, autosave, history)
OTE_EDITOR_BIN=../../build/ote-editor OTE_PLAYER_DIR=../../build-web/engine/player npm run e2e
npm run screenshots    # regenerate editor/web/screenshots (same env vars), then: python3 tools/compress_screenshots.py screenshots
```
Notes: Vite's asset folder is `app/` because the helper owns `/assets/`. e2e needs `npm run build` first, a built
`ote-editor` (`OTE_EDITOR_BIN`, default `../../build/ote-editor`) and, for the Play tests, the web player build
(`OTE_PLAYER_DIR`, default `../../build-web/engine/player`; the Play tests FAIL with the build command when it is missing).
e2e starts the real helper on a temp copy of games/ and editor/templates (never the repo's own) plus a second helper without
a player for the "player not built" test. Firefox runs the `@smoke` tests only; the phone tests (360 x 740, touch) are
Chromium only. Play tests read the player's picture from screenshots (WebGL), so they need the player's intro to settle.

## Build and test

```
cmake -S . -B build -G Ninja && cmake --build build
./build/ote_tests            # all unit tests
./build/ote_tests battle     # subset by name
```

Player backend (SDL3 default; SDL2 for old ports, same sources):

```
cmake -S . -B build -G Ninja && cmake --build build                       # SDL3
cmake -S . -B build-sdl2 -G Ninja -DOTE_PLATFORM=sdl2 && cmake --build build-sdl2
./build/engine/player/ote-player --game demo-srpg --font sans             # fonts: pixel pixel-ttf arcade sans serif mono
./build/engine/player/ote-player --headless --game demo-srpg --map bridge \
    --input "wait:14,confirm,wait:2" --screenshot /tmp/x.bmp               # --font-size N, --font-smooth 0|1, --assets DIR
```

Launcher (default when no `--game` is given): Home -> game page -> map select
-> battle. Options (font, text size, UI scale, visual style) are global with
optional per-game overrides and live in `settings.json` in the user data
folder (Linux: `~/.local/share/OpenTacticsEngine/player/`). Tests and
automation should pass `--settings /tmp/x.json`; `--headless` without it uses
no settings file at all. Flags `--font --font-size --font-smooth --style
--ui-scale` override the stored options for one run.

```
./build/engine/player/ote-player                                    # the launcher
./build/engine/player/ote-player --game demo-srpg --map bridge      # straight into a battle
./build/engine/player/ote-player --settings /tmp/s.json             # use another settings file
./build/engine/player/ote-player --headless --settings /tmp/s.json --input "down,down,down,confirm,wait:2" --screenshot /tmp/x.bmp
```

## Web build

Emscripten 7.0.0 (`~/.local/share/emsdk`; `source ~/.local/share/emsdk/emsdk_env.sh`
puts `emcmake` on PATH). SDL3 backend only (SDL2's Emscripten branch is untested).

```
emcmake cmake -S . -B build-web -G Ninja -DCMAKE_BUILD_TYPE=Release && cmake --build build-web
python3 -m http.server -d build-web/engine/player 8000     # any static server works
# http://localhost:8000/index.html  (standalone launcher)
# http://localhost:8000/index.html?args=--game+demo-srpg+--map+bridge+--autoplay+--frames+300   (debug args)
# embed.html?project=<id>&map=<id> needs the editor helper's API (docs/EDITOR_API.md)
```

`build-web/engine/player/` is self-contained: `ote-player.js/.wasm` (factory
`createOtePlayer`, main is not auto-run), `index.html`, `embed.html`,
`player.js/.css`, copies of `games/` + `assets/` and `games.manifest.json`.
Sources of the pages: `engine/platform/web/`. Details: docs/ARCHITECTURE.md "Web".
Size: wasm ~1.5 MB (490 KB gzip), js 163 KB (41 KB gzip); first frame ~0.1 s locally.

Browser checks (Playwright; Chromium + Firefox), screenshots land in `tools/webtest/out/`:

```
cd tools/webtest && npm install && npx playwright install chromium firefox   # once
node run.mjs                       # standalone, pointer/touch, resize, embed (mock API), perf: Chromium
node run.mjs --browser firefox     # smoke test in Firefox
node run.mjs --only embed --build /path/to/web/output
```

## Working agreements

- The owner makes design/naming/direction calls; technical calls are ours.
- Heavy model: architecture, contracts (headers), core rules. Lighter
  models: well-specified work packages against those contracts, verified
  by the delegating session before merging.
- Commit style: `feat(core): ...`, `test: ...`, `content: ...`, `docs: ...`.
- Nothing is published/pushed anywhere without the owner's explicit yes.
  No license chosen yet (owner decision, needed before publishing).

## Next concrete step

Get the owner's feedback on the editor, then roadmap item 3 in docs/PLAN.md
(each engine feature lands with its editor screen; sprites/tilesets + visual
styles is first in the list, the owner picks the order). Small leftovers:
README; heights editing and visual-style preview in the map editor; arrow-key
navigation inside the editor's option groups; the "+N Funds" toast overlap.
