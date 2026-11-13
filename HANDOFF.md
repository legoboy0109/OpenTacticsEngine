# Handoff

Current state of the build, for whoever (or whichever model) continues.
Read docs/PLAN.md (roadmap + owner decisions), docs/ARCHITECTURE.md
(structure + binding portability rules) and docs/GAME_FORMAT.md first.

## State (2026-11-13)

- Rules core complete for milestone 1 and tested: `engine/core` (JSON,
  loader, grid, battle, Lua formulas). 36 tests green.
- Two demo games in `games/` load and start battles.
- In progress (delegated, each in its own git worktree/branch, to be merged
  and verified): real AI + `ote-sim`; SDL3 backend + `ote-player`.
- Text/fonts: shared text engine (TrueType via stb_truetype) with SDL3 and
  SDL2 backends; the player lays out everything from measured text.
- Web build done (see "Web build" below). Not started: README.

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
thread-safe); the web UI folder (editor/web/dist) does not exist yet, so `/`
shows a built-in placeholder page. Creating a project does not touch
games/index.json. The helper is built with exceptions on (httplib, v0.26.0 in
third_party/httplib); the engine itself stays exception-free.
`ote_tests` writes a settings file into `<repo>/build/`, so that folder must
exist when running it from another build dir name.

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

Hook the editor's Play button to `/player/embed.html` (docs/ARCHITECTURE.md "Web"); README.
