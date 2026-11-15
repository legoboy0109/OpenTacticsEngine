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

- Story events, build steps 1 and 2 (docs/EVENTS.md): the rules core is done
  and tested: unit ids, switches / variables, events, objectives, Talk, saves
  and hash, `ote-sim` output. The player shows them inside one battle:
  dialogue box (letter by letter, pages, portrait, skip; `--input` token
  `skip`), Focus / add / remove / relocate / side-change effects, Talk in the
  action menu, objective banner and "Objective" in the battle menu, portraits
  in `data/graphics.json` (docs/GRAPHICS.md). Not done: editor, web
  build check of the new code. No shipped game has events yet (so nothing
  looks different in the demos; checked byte for byte).
- Campaign, build step 4 (docs/EVENTS.md "Campaign" and "Campaign (player)", docs/SAVES.md "Campaign saves"): done in
  the core and in the player. Core: `data/campaign.json`, `engine/core/campaign/`, tests `tests/test_campaign_*.cpp`,
  `ote-sim <game> --campaign`. Player: game page (Continue / New game / Load / Free battles), title card + scenes in the
  battle's dialogue box, chapter flow with retry / end screens, campaign saves in Continue and the slots,
  `--campaign`, `--autoplay` through a story (`engine/player/app_campaign.cpp`, `campaign/`, `battle/battle_campaign.cpp`);
  tests `tests/test_campaign_player.cpp`, `tests/campaign_player_test.py` (windowless playthroughs on a generated
  three-chapter game, also the generator for the web test), `tools/webtest/suites/campaign.mjs`. No shipped game has a
  campaign yet and the editor has no Campaign screen (step 5), so nothing looks different in the demos (launcher and
  game pages compared pixel for pixel with the pre-campaign build). Decisions the owner can change: a free battle of a
  campaign game never writes the Continue file ("not saved automatically"; by hand into a slot is fine); the title card
  stays 2.5 s unless confirmed; Game options and Back sit above the Free battles list; the first chapter's before scene
  cannot name roster units (the roster is empty then: use `speakerName`); removeUnit counts as fallen; a unit that starts on
  a computer side and is recruited by an event is dressed from the roster only from the next chapter on; carried storage
  has no size limit; Retry starts the battle with a new seed.
- Rules core (`engine/core`): JSON, loader, grid, battle, Lua formulas, AI;
  `ote-sim` plays both demos AI-vs-AI. `ote_tests`: 588 green (SDL3 and
  SDL2 builds).
- Player (`ote-player`): launcher, options with per-game overrides, real
  fonts; SDL3 and SDL2 backends; web build (Emscripten). Webtest:
  Chromium 126, Firefox 59.
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
3D at the display's real resolution (docs/RENDER3D.md "Resolution"): option
"3D resolution" (key `res3d`), `--3d-res`, headless `--3d-scale N` (screenshot
written at N times the size), `Renderer::displaySize()`, OpenGL 4x
multisampling. Graphics sheets take `"smooth": true` (3D views only: linear +
mipmaps; the editor's Art screen has the checkbox); models follow the glTF
sampler. Not verified by hand: a real window, the web build's canvas size.
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

## Upgradable structures and research, rules core (2026-11-14)

docs/ECONOMY.md build order step 1, done in `engine/core` only (no player UI, no editor screens, no demo content:
steps 4 and 5 are open; both demos play exactly as before, checked: `ote-sim` bridge Blue 17 / Red 13, twin_rivers
Red 30, pinned hashes and save bytes unchanged). Where things are: data `data/loader_research.cpp`,
`loader_upgrades.cpp` (`Database::research`, `StructureDef::upgrades` / `research`); battle `battle/upgrades.cpp`,
`research.cpp`, bonuses in `unit_stats.h` (`unitStat(db, unit, stat, &sides)`; the formula host gets the sides
through `FormulaCtx::sides`), saves `save_research.*`, AI `ai/ai_economy.cpp`, campaign `CarriedSide::research`.
Tests: `tests/test_upgrades.cpp`, `test_research*.cpp`, `research_fixture.h`, `json_edit.h`.

What the player and the editor need (all on `Battle` / `Database`):
- Actions: `Action::upgrade(structureIndex, toKind)` (`UpgradeStructure`) and `Action::research(techIndex)`
  (`StartResearch`, for the active side); both free of any unit, no unit order is needed or consumed.
- Menus: `upgradeOptions(structure, out)` (for the active side's own structure; each entry: `to`, `def->cost`,
  `def->needs`, `ok`, `why`), `researchOptions(side, out)` (`tech`, `ok`, `why`; finished and under-way ones are not
  listed), `researching(side)` / `researchTurnsLeft(side)` (the panel line), `hasTechnology(side, tech)`,
  `canResearch(side)` (does the side own a `research` structure: show "paused" otherwise),
  `buildOptions(cell, out, includeLocked)` + `unlockedBy(unitType, out)` for a greyed "needs Armor or Forge" line,
  `canBuild(cell, unitType, &why)` (the whole check). Technology data: `db->research[i]` (id, name, description,
  cost, turns, needs, unlocksUnits, bonusTypes, bonus per stat).
- Events (after the old ones): `StructureUpgraded` (a structure, b old kind, c new kind), `ResearchStarted` (a side,
  b technology, c turns; 0 = at once), `ResearchFinished` (a side, b technology). A payment arrives first as
  `ResourceChanged`. The state is already updated when the events are animated, so the structure is drawn in its new
  kind from the start of the animation (the player just ignores the three events today, `battle_view.cpp`).
- A stat shown to a player: use `Battle::stat` (includes accessories and research); `Unit::stats` is the unit's own value.
- Editor: new keys `upgrades` / `research` on structure kinds and the new file `data/research.json`. The helper
  validates through the real loader, which reads them now (messages name the file, the technology or structure and
  the entry, plain sentences, safe to show); the helper's project file list and the web model have not been taught
  about `data/research.json` or the two keys (nothing in the editor was touched).
- Campaign scenes did not get `researched` / `grantResearch` (scenes have no map sides).
Open for the owner: wording of every message (placeholder); whether a free upgrade should exist at all (the AI never
takes one); whether research under way should carry between chapters (today it is lost).

## Repair and supply, rules core (2026-11-14)

docs/ECONOMY.md build order step 2, done in `engine/core` only (no player UI, no editor screens, no demo content).
Both demos play exactly as before (`ote-sim` bridge Blue 17 / Red 13, twin_rivers Red 30, campaign finished 5; pinned
hashes and save bytes unchanged). Where things are: data `data/loader_supply.cpp` (`rules.supply`, `repairs`, `fuel`,
`fuelPerTurn`, `supplies`, `ammo`); battle `battle/repair.cpp`, `fuel.cpp`, `ammo.cpp`, `supply.cpp` (turn start order in
`turn.cpp`, reach limit in `movement.cpp`, weapon choice in `items.cpp` `weaponsOf` and `combat.cpp`), saves
`save_supply.*`, AI `ai/ai_supply.cpp`. Tests: `test_supply_loader.cpp`, `test_repair.cpp`, `test_fuel.cpp`,
`test_ammo.cpp`, `test_supply.cpp`, `test_supply_save.cpp`, `test_supply_ai.cpp`, `supply_fixture.h`.

What the player and the editor need (all on `Battle` / `Database`; the rules are in docs/ECONOMY.md):
- Action: `Action::supply(unit)` (`ActionType::Supply`): a supplier unit that has not acted, next to a unit of its side
  that lacks something; may move first; ends the unit's turn. Offer it in the unit's action menu when `canSupply(unit)`.
  `supplyTargetsFrom(unit, cell, out)` lists whom a supplier standing on `cell` would fill (for a preview).
- Unit state: `Unit::fuel` (-1 = none) and `Unit::ammo` (one entry per entry of the type's `weapons`, -1 = unlimited, empty =
  nothing tracked) or `fuel(unit)`, `maxFuel(unit)`, `ammo(unit, weapon)`, `maxAmmo(unit, weapon)` (-1 = none / unlimited);
  `needsSupply(unit)`. Show them in the info panel only when not -1. `weaponsOf` already leaves an empty weapon out;
  to grey it out, walk `typeOf(u).weapons` and ask `ammo()`.
- Repair: `repairPreview(unit, plan)` (what the next turn start would do for the unit where it stands, from the side's
  funds now; false = nothing) and `repairPreviewAt(unit, cell, plan)`; `RepairPlan { heal, cost per resource index }`.
- Events (after the old ones): `UnitRepaired` (a unit, b HP restored, c HP now, data cost per resource index, empty =
  free), `UnitResupplied` (a unit). The payment of a repair arrives first as `ResourceChanged`. The player today draws
  nothing for them and just sets the displayed HP when a repair event ends (`battle_view.cpp`); `ote-sim` prints both.
- Editor: new keys `rules.supply` (`repair.amount` / `repair.costShare` / `structures`), structures `repairs`, units `fuel` /
  `fuelPerTurn` / `supplies`, weapons `ammo`; the helper validates through the real loader (messages name the file and the
  entry); the helper's model and the web model were not taught the new keys.
Decisions the owner can change: the first turn of the battle already burns `fuelPerTurn` (a plane that starts with 20 and
burns 4 has 16 at its first move); `fuelPerTurn` equal to or above `fuel` is refused; repair prices are rounded up per
resource; resupply by structures needs no `repairs` match and goes to every unit of the owner; suppliers fill only their
own side (not allies of another side); fuel and ammunition are not carried between chapters (full at each start); a unit
running dry is destroyed by `fuelPerTurn` only (others just cannot move); the AI's thresholds (40% HP, a quarter of the
tank, out of every usable weapon) are constants in `ai_supply.cpp`; wording of every loader message is placeholder.

## Economy in the player: upgrades, research, repair, supply (2026-11-14)

Build order step 4 of docs/ECONOMY.md, the part for upgrades, research, repair and supply (fog drawing is separate).
Files: `battle/battle_economy.cpp` (structure menu, Upgrade list, research panel, Supply highlight, `[info]` line for a
hovered unit with fuel / ammunition, upgrade flash), `battle/economy_text.*` (ALL wording of this part: `EconomyWords`
plus the text builders, like `item_text` for items), `battle/battle_view_economy.cpp` (events), small edits in
`battle_scene.*`, `battle_menus.cpp`, `battle_input.cpp`, `panel.cpp`, `ui/menu.h` (`selectedId`), `ui/item_panel.*`
(`setDetailLines`, `setMaxWidth`) and four one-line hooks in `battle_view.cpp`. Test: `python3 tests/economy_player_test.py
./build/engine/player/ote-player .` (24 checks on a generated game; `--shots DIR` keeps its screenshots; `--make-games
DIR` only writes the game). It reads the `[info]` lines the player logs: `structure menu:`, `build menu:`, `upgrade menu:`,
`research panel:` (every row with price / turns / `[greyed]`), `info:` (hovered unit's fuel and ammunition), `upgrade:`,
`research started:`, `research finished:`, `repair:`, `resupply:`.
Behaviour: a structure that offers only one thing (a plain factory) opens that screen at once, as before; two or more open
the structure menu (Build / Upgrade / Research); Cancel in a sub-screen goes back to it. A unit that can still act and stands
on the structure takes the Confirm (select it first, or let it act). Greyed entries show their reason where the price is
(a greyed row cannot be selected, so a hint line would never show it); the research panel shows the reason in its detail
lines. Build menu shows locked unit types greyed with "needs <tech>". Supply is in the action menu; while it is the
highlighted entry the units it would fill are tinted (the move-range overlay, so all four kinds of view draw it). Info panel:
a "Fuel" row (red at a quarter or less), fixed weapons show "3/6 r1" and a weapon out of ammunition shows "EMPTY" in red
(it is not in `weaponsOf`, so the panel walks the type's weapons); the side's research shows under its resources ("Armor   1",
"paused" when no research structure). The hint line says "Low fuel: move limited to N" when the tank is the limit of a move.
Events: banner "Blue Army: Armor researched" (+ what it gives), started banner, "+N" heal number for a repair, "Resupplied"
float, white flash on the squares of an upgraded structure; 1.3 s for the viewed side, 0.5 s / 0.1 s for others.
Not checked: a real window, gamepad, touch (hit testing is the shared Menu / ItemPanel / RowList code). The old comments in
`battle_view.cpp` ("no player UI yet") on the event list are stale; left alone to keep the merge with the fog branch clean.

## Story events in the editor (2026-11-14)

Build order step 3 of docs/EVENTS.md (inside one map; the Campaign screen is step 5). Model: `editor/web/src/model/story*.ts`
(`storyWalk.ts` = the ONE walk over every id an event names: rename, clean-up, "used by" lines and checks all use it;
`storyChecks.ts` mirrors the loader; `storyUnits.ts` = unit / side deletes as one undo step), `portraits.ts`. UI:
`screens/maps/editor/story/` (Events and Objectives tabs, one file per form group), unit "Story name" in `panels/UnitProps.tsx`,
switches / variables on Rules (`screens/rules/StoryListsCards.tsx`), Portraits card on Art (`screens/art/Portraits*.tsx`).
Picking a square / area on the map: `MapViewState.picking` + `story/pickStroke.ts`; open event areas are drawn by `drawStory.ts`.
The helper passes the new keys through unchanged (docs/EDITOR_API.md "Story data"). Squares are shown 1-based, stored 0-based.
Tests: `tests/unit/story.test.ts`, `tests/e2e/story*.spec.ts`, `tests/editor_api_events_checks.py`. All wording is placeholder.

## Next concrete step

Owner's order (docs/PLAN.md item 3): a structures, b animated joining
terrain and c saving are done; of d (kinds of view) 2D isometric, the 3D
layer (software backend and the OpenGL ES 3 / WebGL 2 / OpenGL 3.3 one,
docs/RENDER3D.md "OpenGL backend": own GL context, picture read back, so
the 2D renderer is untouched; `--3d auto|software|hardware`), glTF models
with animation and 3D isometric are done. The full 3D view's map part
is done too (close-up battle scenes later, by owner decision). Nothing in game data may name a
graphics API or carry shader code (console ports must be able to
implement the interface). Items are done, and so is drawing 3D at the display's
resolution with smooth textures (docs/RENDER3D.md "Resolution"; option
"3D resolution", key `res3d`; `--3d-res`, `--3d-scale N` for headless
screenshots at N times the size) plus `demo-srpg`'s "Detailed 3D" style
from CC0 packs (`games/demo-srpg/CREDITS.md`,
`tools/art/make_detailed_style.py`). Not checked in a real window.
Story events, dialogue and the campaign are built (docs/EVENTS.md) and
`demo-srpg` has a story and the two-chapter campaign "The Ford War"
(`data/campaign.json`, `maps/north_road.json`; `ote-sim games/demo-srpg
--campaign`). The editor's Campaign screen is built (`editor/web/src/screens/campaign/`, model
`model/campaign*.ts`; scenes reuse the story command list through a made-up "scene map"; the store's `edit` runs
`reconcileCampaign` so deletes of switches / sides / maps / story names clean the scenes in the same undo step;
`"campaign": false` in the PUT removes the file). Not done: a Play-from-chapter-1 button (needs a `campaign` query
parameter in `engine/platform/web/embed.html`). Next:
economy options (owner decisions in docs/PLAN.md 3g, contract in
docs/ECONOMY.md, build order at its end), then sound and music.
Close-up battle scenes: low priority.
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
