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
- Open source (license not chosen yet; nothing gets published without the
  owner's say-so).
- OVERCLOCK (~/Projects/UntitledTBT) is separate: ideas only, never edit it.

## Milestone 1: both genres playable from data, desktop + web

| # | Work package | Status |
|---|---|---|
| 1 | Scaffold, contracts, format spec | done |
| 2 | JSON + game/map loader + two demo games | in progress |
| 3 | Battle core: movement, combat, turns, capture, production, progression, Lua formulas | written, tests pending |
| 4 | AI opponent (`core/ai`) + `ote-sim` headless runner | todo |
| 5 | SDL3 platform backend + player (menus, battle UI, automation flags) | todo |
| 6 | Web build (Emscripten) | todo |
| 7 | README, how-to-run, cleanup | todo |

Done means: `ote_tests` green, `ote-sim` finishes both demo maps, the
player plays both demos on Linux and in a browser, verified by screenshots.

## After milestone 1 (rough order, to be confirmed with the owner)

1. Save/resume (battle state to JSON), settings persistence.
2. Sprites and tilesets (image loading, animation), sound.
3. Speed-based turn order (FFT style), move-after-attack option, height rules.
4. Event system (story triggers, dialogue, win/lose conditions in data),
   Lua event hooks, campaign layer (chapters, persistent army).
5. Economy depth: multiple resources in UI, research/tech tree, upgradable
   cities, fog of war, repair/supply.
6. Hex grid; isometric view.
7. The editor (its look and workflow are the owner's design decisions).
8. Ports: Android, console/handheld backends, GLES3/SDL_GPU renderer, 3D grids.

## Known gaps / notes

- `versus` + per-weapon damage formulas cover AW damage charts and FE
  effectiveness; weapon triangle needs the defender's weapon exposed to
  formulas (not yet).
- A defeated side's buildings become neutral (AW gives them to the victor;
  make this a rule option when it matters).
- Unit ids index `Battle::units` and are never reused; dead units stay in
  the vector with `alive=false`.
