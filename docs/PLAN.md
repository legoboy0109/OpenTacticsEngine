# Plan

Living roadmap. Update the status column as work lands; keep finished
detail short. Architecture and rules: docs/ARCHITECTURE.md. Data format:
docs/GAME_FORMAT.md.

## Decisions made (by the project owner)

- Own engine, not built on Godot or any other engine. C++17 + SDL3
  (platform plumbing only) + Lua 5.4.
- Targets: PC, web, phones, consoles, retro handhelds; later homebrew ports
  to old consoles (Xbox, 360, PS3, Wii U).
- Genres: economy tactics up to "Advance Wars + light 4X" (several
  resources, tech tree, upgradable cities; not full Civilization) and
  strategy RPGs (Fire Emblem, FFT).
- Grids: square top-down first; hex, isometric, 3D later.
- Audience: mixed. Editor for common things, Lua for unusual ones.
- Open source under GPL-3.0 (owner, 2026-11-14), on GitHub. Known limits of
  that choice: not compatible with Apple's App Store or official (non-homebrew)
  console releases.
- OVERCLOCK (~/Projects/UntitledTBT) is separate: ideas only, never edit it.

## Milestone 1: both genres playable from data, desktop + web

| # | Work package | Status |
|---|---|---|
| 1 | Scaffold, contracts, format spec | done |
| 2 | JSON + game/map loader + two demo games | done |
| 3 | Battle core: movement, combat, turns, capture, production, progression, Lua formulas | done (36 tests) |
| 4 | AI opponent (`core/ai`) + `ote-sim` headless runner | done |
| 5 | SDL3 platform backend + player (menus, battle UI, automation flags) | done |
| 6 | Web build (Emscripten): standalone page + embeddable player, Chromium and Firefox tested | done |
| 6a | Real fonts (stb_truetype, shared by all backends), `--font` option, SDL2 backend | done |
| 6b | Launcher front end inside the player: pick/load a game, edit options (font, size, display...) before starting; settings persist across restarts | done |
| 7 | README, how-to-run, cleanup | todo |

Done means: `ote_tests` green, `ote-sim` finishes both demo maps, the
player plays both demos on Linux and in a browser, verified by screenshots.

## After milestone 1 (owner priorities, 2026-11-13)

Focus on engine functionality and the editor. Demo game balance is NOT a
goal (demos only exercise features).

1. DONE: launcher front end + settings (work package 6b).
2. DONE (2026-11-13): editor first slice: browser editor for novices (see
   docs/EDITOR.md, docs/EDITOR_UI.md): project picker, Game, Rules, Units,
   Weapons, Terrain, Maps (full map editor) and Play (embedded web player).
   Awaiting the owner's verdict on wording/layout. Left out on purpose:
   heights editing, visual-style preview on the map canvas, a scripts
   screen, inline field validation (Problems panel only).
3. DONE (2026-11-13): sprites and tilesets, engine + editor. Decisions and
   contract: docs/GRAPHICS.md (sheets + single images, 16-piece joining
   tiles, colour marker + optional per-side art, original generated starter
   art, simple looping animation). Editor: Art section, Picture cards,
   map canvas draws with art (docs/EDITOR_UI.md "Art"). Awaiting the
   owner's verdict on wording/layout and the starter art. Not in this
   step: 47-piece blending, walking/attack animation, sheet margins/gaps,
   animated joining tiles, unit art in the player's side panel/forecast.
   From here on, each engine feature lands with its editor screen.
   Owner's order (2026-11-13), one after the other:
   a. DONE (2026-11-14): structures, a separate layer placed on top of
      terrain squares, several squares allowed (docs/STRUCTURES.md).
   b. DONE (2026-11-14): animated terrain that also joins with its
      neighbours; bridges reach land (docs/GRAPHICS.md).
   c. DONE (2026-11-14): saving: Continue + slots, per-game setting
      (docs/SAVES.md).
   d. Alternative visual styles. Owner decisions (2026-11-14): a style is
      made separately by the game's maker and has a kind of view; the
      player switches between the styles a game ships that the platform
      can draw. Kinds: the current top-down 2D, 2D isometric, 3D isometric,
      full 3D (GameCube/Wii Fire Emblem-like). Isometric shows map heights
      as raised blocks. 2D isometric has one fixed angle; 3D isometric
      rotates. In 3D isometric the maker chooses per game between 2D
      sprites and 3D models for units. Full 3D: 3D map, free camera,
      fights on the map first; separate close-up battle scenes later.
      The 3D renderer must be portable to the console ports: an
      engine-owned 3D interface with swappable backends (OpenGL ES 3 /
      WebGL 2 first), no graphics-API or shader code in game data, fixed
      built-in materials, one directional light + ambient, modest
      power-of-two textures, CPU skinning, glTF models.
      Build order: d1 2D isometric (DONE 2026-11-14, contract:
      docs/VIEWS.md, with a Heights tool in the map editor); d2 the 3D
      drawing layer (first part DONE 2026-11-14, contract:
      docs/RENDER3D.md: interface, software backend that runs everywhere,
      glTF loading, test scene `--test-3d`; the OpenGL ES 3 / WebGL 2 /
      OpenGL 3.3 backend DONE 2026-11-14); d3 3D isometric (first part
      DONE 2026-11-14: blocks, sprites, turning camera, editor; still to
      do: nothing; units and structures can be animated glTF models, DONE
      2026-11-14, docs/GRAPHICS.md "Models"); d4 full 3D (map part DONE
      2026-11-14, docs/VIEWS.md "Full 3D": free camera with perspective
      over the block map, both demos have a "Full 3D" style; still to do:
      close-up battle scenes, later by owner decision).
   e. Items (DONE 2026-11-14, docs/ITEMS.md: usable items, carried
      weapons with wear, accessories, unit inventories plus a shared
      storage per side, trading, shops, drops; `demo-srpg` uses them;
      not yet: selling, chests, stealing, scripted effects, carrying
      items between maps, AI that trades or shops).
   f. Story events and dialogue (triggers, win/lose conditions in data,
      campaign layer).
   g. More economy options (research, upgradable cities, fog,
      repair/supply).
   h. Sound and music.
   Not yet ordered: speed-based turns (FFT), height rules, hex grid,
   isometric view.
   Editor wording decided: "style" (never "look"); terrain lists keep the
   flat colour chip.
4. Native developer app (Dear ImGui) for advanced users.
5. Ports: Android, console/handheld backends, GLES3/SDL_GPU renderer, 3D
   grids, homebrew old-console ports via the SDL2 backend.

## Potential future features (not scheduled; owner to decide)

- **Obsidian vault projects** (idea, 2026-11-13). A project could be stored
  as an Obsidian vault instead of plain JSON files: one note per unit /
  weapon / terrain with stats in the note's Properties, `[[links]]` for
  references, design notes in the body, maps as a text block, Bases tables
  for side-by-side balancing. This is how OVERCLOCK's `game-data/` vault
  works (read it for the pattern; never edit it). Constraints found:
  the player should keep loading JSON (vault = authoring format, JSON
  generated on save/Play/export); Obsidian Properties are flat, so nested
  data becomes flat keys (`stat_hp`, `growth_hp`, `cost_funds`); needs a
  one-way (vault is the source) vs two-way (browser editor also edits the
  notes, preserving the body) decision. Recommended shape if built:
  two-way, as a second storage option inside `ote-editor` (the helper's
  on-disk revision check already detects Obsidian's edits).

## Settings model (owner decision, 2026-11-13)

Options are global: they apply to every game by default. A game can carry
a per-game setting that overrides one particular option for that game only;
everything not overridden keeps following the global value. Layers, lowest
to highest priority: engine default < game's own default in game.json
(e.g. its `font`) < user's global option (when not left on "game default")
< per-game override. (Confirm the middle ordering with the owner when the
options screen is shown.)

First options: font, text size, UI scale, visual style. "Visual style"
means a game can ship several switchable graphics styles (e.g. tilesets /
palettes) and the option picks one; design the sprite/tileset system with
named styles in mind. More options get fleshed out later.

## Known gaps / notes

- `versus` + per-weapon damage formulas cover AW damage charts and FE
  effectiveness; weapon triangle needs the defender's weapon exposed to
  formulas (not yet).
- A defeated side's buildings become neutral (AW gives them to the victor;
  make this a rule option when it matters).
- Unit ids index `Battle::units` and are never reused; dead units stay in
  the vector with `alive=false`.
- Player: the "+N Funds" toast is drawn over the side panel's terrain line
  at the start of a turn (seen in editor/web/screenshots/27). Cosmetic.
- Editor screenshots are lossily compressed (20-64 colour palettes), so
  colours in editor/web/screenshots can be off (24 shows forest as slate);
  judge colours in the running editor, not the PNGs.
