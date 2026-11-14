# Handoff

Current state of the build, for whoever (or whichever model) continues.
Read docs/PLAN.md (roadmap + owner decisions), docs/ARCHITECTURE.md
(structure + binding portability rules) and docs/GAME_FORMAT.md first.

## State (2026-11-14)

Everything below is merged on `main`, which is also on GitHub:
https://github.com/legoboy0109/OpenTacticsEngine (public, remote `origin`;
owner said yes to uploading on 2026-11-14). Keep it up to date as work
lands; never force-push. Commits use the account's GitHub no-reply address
(set in this repo's git config), never a personal email.

- Rules core (`engine/core`): JSON, loader, grid, battle, Lua formulas, AI;
  `ote-sim` plays both demos AI-vs-AI. `ote_tests`: 105 green (SDL3 and
  SDL2 builds).
- Player (`ote-player`): launcher, options with per-game overrides, real
  fonts; SDL3 and SDL2 backends; web build (Emscripten). Webtest:
  Chromium 86, Firefox 19.
- Editor: helper `ote-editor` (API test 250 green) and the browser UI with
  every screen incl. map editor, Play, Art and Structures (258 unit tests,
  171 e2e in Chromium + Firefox smoke, 15 screenshot specs skipped unless
  `SHOTS=1`).
- Art (docs/GRAPHICS.md): `data/graphics.json` + `art/*.png`; joining
  terrain can be animated (`"join"` as a list of 4x4 blocks) and can join
  with "everything except" (`joinExcept`, used by bridges so they reach
  land). Starter art comes from `python3 tools/art/make_starter_art.py`.
- Structures (docs/STRUCTURES.md): their own layer on top of terrain, can
  cover several squares; capture, income, building units and headquarters
  live there. Old-format games are refused by the loader with a message;
  the editor offers Convert (one undo step). An empty map `"owners": []`
  is still accepted (older editor versions always wrote one).
- Saving (docs/SAVES.md): automatic Continue save + manual slots, per-game
  setting `game.saving` (`any`/`continue`/`off`, Rules screen "Saving"
  card), "Leave battle" in the battle menu. A save stays valid when a game
  changes only cosmetically (names, colours, letters, fonts, styles, art);
  rule or map changes make it "made with a different version". A Move
  alone does not write the Continue save (owner: wanted).
- Waiting on the owner: verdict on editor wording/layout (incl. Art,
  Structures, Convert, Saving card, picture dialog's animated joining),
  starter art, launcher layout, font picks. README is a first short version.

## Editor helper

`ote-editor` (editor/server) serves the browser editor and the HTTP API in
docs/EDITOR_API.md, using the real loader for validation. Starter projects
live in editor/templates (strategy-rpg, economy-tactics, blank). Build and
run (does not need the player target):

```
cmake -S . -B build -G Ninja -DOTE_BUILD_PLAYER=OFF && cmake --build build
./build/ote-editor            # from the repo root; http://127.0.0.1:8765/
python3 tests/editor_api_test.py ./build/ote-editor .   # also ctest "editor_api"
python3 tests/render3d_backends_test.py ./build/engine/player/ote-player .   # OpenGL vs software pictures
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
done: project picker, Game, Rules, Units, Weapons, Terrain, **Structures** (cities, forts, factories; the Terrain form points to it),
**Art** (images, pictures, picker), **Maps (full map editor with the Structures tool, drawn with the art)** and **Play** (embedded web
player). Projects made before structures existed get a one-click, one-undo-step **Convert** offer (a banner on every screen).
Layout: `src/api` (the only fetch calls), `src/model` (pure doc edits, no React; `map*.ts` = the map grid model,
`graphics*.ts` + `pickCell.ts` = art, `structures*.ts` + `mapStructures.ts` = structures incl. `structuresEffective.ts` (the engine's
terrain + structure combination rule) and `structuresConvert.ts` (old projects)), `src/store` (undo, autosave, conflict; `art.ts` = the image files),
`src/art` (drawing art, shared by canvas/palettes/previews), `src/ui` (components; `picture/` = Picture card, picker, piece grid),
`src/screens/<section>` (`maps/editor/` = canvas, tools, panels, `drawStructures.ts`; `art/` = the Art section; `structures/`), `src/play` (Play button logic + panel),
`src/shell` (top bar, sidebar). Spec and decisions: docs/EDITOR_UI.md (the art notes explain the upload/autosave handling, the structures notes the Convert flow).

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
npm test               # vitest unit tests (model incl. map grid, presets, art, the C++-pinned pickCell hash, store, autosave, uploads,
                       #   structures: effective numbers pinned to tests/test_structures.cpp, placing, cascades, the Convert flow)
OTE_EDITOR_BIN=../../build/ote-editor OTE_PLAYER_DIR=../../build-web/engine/player npm run e2e
npm run screenshots    # regenerate editor/web/screenshots (same env vars), then: python3 tools/compress_screenshots.py screenshots
```
Notes: Vite's asset folder is `app/` because the helper owns `/assets/`. e2e needs `npm run build` first, a built
`ote-editor` (`OTE_EDITOR_BIN`, default `../../build/ote-editor`) and, for the Play tests, the web player build
(`OTE_PLAYER_DIR`, default `../../build-web/engine/player`; the Play tests FAIL with the build command when it is missing).
e2e starts the real helper on a temp copy of games/ and editor/templates (never the repo's own) plus a second helper without
a player for the "player not built" test. One structures test also runs the native simulator on the saved game (`OTE_SIM_BIN`, default
`../../build/ote-sim`); the Convert tests write a project in the pre-structures format into the temp game folder (`tests/e2e/oldkit.ts`,
fixture `tests/unit/oldProject.ts`). Firefox runs the `@smoke` tests only; the phone tests (360 x 740, touch) are
Chromium only. Play tests read the player's picture from screenshots (WebGL), so they need the player's intro to settle.
The templates carry starter art now: tests that need an art-free game use `bareGame` / `seedArt` (`tests/e2e/artkit.ts`, which also
makes PNGs on the fly); the new art screenshots are `tests/e2e/screenshots-art.spec.ts` and the structures ones (53 to 72)
`tests/e2e/screenshots-structures.spec.ts` (generate into a temp folder with `SHOTS_DIR`, compress, then copy only the new or changed files, so
the older screenshots are not re-quantised).

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
- Licence: GPL-3.0 (owner decision 2026-11-14); a game's own data is its
  maker's. Pushing to `origin` is agreed; anything else public (releases,
  posts, other sites) still needs the owner's explicit yes.

## Kinds of view (2026-11-14)

Owner decisions and the contract: docs/VIEWS.md (also docs/PLAN.md item
3d). Build order: d1 2D isometric (DONE, on main), d2 the 3D drawing layer
(must be portable to the console ports), d3 3D isometric, d4 full 3D.

d1: core `engine/core/view/iso.*` + `StyleDef::view`; player
`battle/map_proj.*`, `iso_tiles.*`, `iso_draw.cpp`, `views.h`; isometric
starter art `tools/art/art_iso.py`; webtest suite `views`; editor: kind of
view per style (Game screen), Heights tool in the map editor (key H), Play
style choice, both real templates ship an Isometric style. Verified:
`ote_tests` 109 (SDL3 + SDL2, isometric screenshot pixel-identical between
them), API 250, editor unit 274, e2e 185 (+18 skipped screenshot specs),
webtest Chromium 92 / Firefox 25.

## The 3D drawing layer (2026-11-14)

Contract: docs/RENDER3D.md. `engine/gfx3d/` (library `ote_gfx3d`):
`render3d.h` (the interface), `math3d.h`, `soft3d.*` (software backend,
the reference), `shapes.*`, `model*.{h,cpp}` (glTF loader). Player:
`PlayerApp::renderer3d()` owns the backend, `show3d.*` puts the picture on
screen (`Renderer::updateTexture`), `model_view.*` uploads and draws a
model, `scene_test3d.*` is the test scene (`--test-3d`). Tests:
`tests/test_soft3d.cpp`, `tests/test_model.cpp`, webtest suite `render3d`.
Nothing in a game uses it yet: `viewSupported` (views.h) still offers only
top-down and isometric.

## 3D isometric view (2026-11-14)

Contract: docs/VIEWS.md "3D isometric". Core `engine/core/view/iso3d.*`
(`Iso3DGeometry`: projection, pick, hidden; tests/test_views3d.cpp). Player:
`MapProj` has a third mode (`iso3d()`, `setTurn`), `battle/map3d*.cpp`
(`Map3D`, owned by BattleScene, draws through the 3D layer), `sheets3d.*`
(art sheets as 3D textures), `batch3d.*` (per-frame meshes per material),
`Button::TurnLeft/TurnRight`, on-screen turn buttons
(`BattleScene::turnButtonBox`). Both demos have a "3D Isometric" style
(written by tools/art/make_starter_art.py: top-down tiles on block tops,
iso unit sprites, top-down structure pictures). The editor offers the kind.
Webtest suite `views` covers it.

## 3D models and animation (2026-11-14)

Contract: docs/GRAPHICS.md "Models", docs/RENDER3D.md "Models". Loader
`engine/gfx3d/model*.cpp` reads skins and animation clips; `poseModel` /
`skinPart` (model_pose.cpp) pose on the CPU. Art file: `UnitLook::model`
(`ModelLook`) for units and structures in styles of the 3D kinds. Player:
`battle/models3d.*` (one `ModelAsset` per file, one `ModelActor` per thing),
`battle/map3d_models.cpp` (animation from what BattleView does: sliding =
Move, lunging = Attack, fading = Die; facing). Demo models are generated by
`tools/art/make_unit_models.py` into `games/demo-srpg/models/`. Editor:
model upload endpoints and a picture-or-model choice on the Units and
Structures screens.

## Full 3D view (2026-11-14)

Contract: docs/VIEWS.md "Full 3D". Core `engine/core/view/free3d.*`
(`Free3DGeometry`: perspective camera circling a focus, project, pick,
hidden, shiftIntoView; tests/test_free3d.cpp). Player: `MapProj` has a
fourth mode (`free3d()`, `cam3()`) and hides which camera looks at the 3D
space (`project3`, `pixelsAt`, `artPixels`, ...), so `Map3D` draws both 3D
kinds; `battle/free_cam.*` (`FreeCam`: turn / tilt / zoom, following the
cursor). Platform: `Button::TiltUp/TiltDown/ZoomIn/ZoomOut`,
`InputEvent::CameraDrag`, mouse wheel, camera stick and triggers (SDL3 and
SDL2). Both demos have a "Full 3D" style (same art as 3D isometric). Editor:
"Full 3D" kind on the Game screen, models allowed in such styles.
Player option "3D graphics" (Automatic / OpenGL / Software): settings key
`draw3d`, docs/RENDER3D.md "Choosing".
Not verified by hand: gamepad stick/triggers (no gamepad in tests); mouse
wheel and right-drag are checked in the browser suite only.

## Items (2026-11-14)

Contract: docs/ITEMS.md. Core: `engine/core/data/loader_items.cpp`,
`engine/core/battle/items.cpp`, `unit_stats.h`, item actions / events /
queries on `Battle`, carried weapons in `combat.cpp`, AI healing rule
`engine/core/ai/ai_items.cpp`; saves carry inventories and storage; the
save hash mixes `data/items.json` only when the file exists (games
without items keep their old hash). Player: `battle/battle_items.cpp`
(state machine), `battle/item_text.*`, `ui/item_panel.*`,
`ui/trade_panel.*`; action menu entries Items / Trade / Storage / Shop.
Editor: Items screen (`editor/web/src/screens/items/`), Items card on
Rules, item fields on Weapons, Units, Structures and the map editor;
server reads/writes `data/items.json` (docs/EDITOR_API.md).
`demo-srpg` uses carried weapons, gold, a shop at the fort, storage next
to the leader and two enemy drops (its saves from before are refused).
All wording is placeholder until the owner approves it. Open owner
questions: should Trade / Storage / Shop stay open for several actions;
show Storage when empty; id renaming for items.

## Next concrete step

Owner's order (docs/PLAN.md item 3): a structures, b animated joining
terrain and c saving are done; of d (kinds of view) 2D isometric, the 3D
layer (software backend and the OpenGL ES 3 / WebGL 2 / OpenGL 3.3 one,
docs/RENDER3D.md "OpenGL backend": own GL context, picture read back, so
the 2D renderer is untouched; `--3d auto|software|hardware`), glTF models
with animation and 3D isometric are done. The full 3D view's map part
is done too (close-up battle scenes later, by owner decision). Nothing in game data may name a
graphics API or carry shader code (console ports must be able to
implement the interface). Items are done. In progress (2026-11-14):
drawing 3D at the display's resolution, smooth textures, then a
high-resolution 3D style in `demo-srpg` from the CC0 KayKit Adventurers
models (github.com/KayKit-Game-Assets/KayKit-Character-Pack-Adventures-1.0)
and CC0 ground textures. Then story events and dialogue including a
campaign (owner decisions in docs/PLAN.md 3f; write docs/EVENTS.md
first), economy options, sound and music. Close-up battle scenes: low
priority.
Small leftovers: README; heights editing in the map editor; arrow-key
navigation inside the editor's option groups; "+N Funds" toast overlap;
night style does not darken unit sprites; editor paint stroke with art
~45 ms on 64 x 64; starter-art limits (water corners that touch only
diagonally are square, a sand strip shows between squares of a bridge
longer than one square); no pixel-level check of animated water on the
editor canvas; the editor's map canvas and picture previews do not draw
isometric (Play does); isometric starter art has no joining pieces and
the buildings are the top-down pictures on plinths; a capture was never clicked through in the embedded player
(the embed page has no scripted input; `ote-sim` covers captures).
