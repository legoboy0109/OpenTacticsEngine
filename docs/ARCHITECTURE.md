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
engine/platform/       platform.h interface + one folder per backend (sdl3/)
engine/player/         the game-running app: scenes, UI, input -> actions
tools/sim/             headless AI-vs-AI runner (testing, balance)
tests/                 unit tests for the core
third_party/lua/       Lua 5.4 (vendored)
```

Dependency rule: `core` depends on nothing but Lua and the C/C++ standard
library. `player` depends on `core` and `platform.h`. Only
`platform/<backend>/` may touch SDL or any device API.

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
  The first backend uses SDL3's 2D renderer; a GLES3/SDL_GPU backend for
  effects and 3D grids is a later, optional addition.
- Fixed small logical resolution (default 480x270) scaled to the display.
- Keep memory modest (original Xbox has 64 MB).

## Testing

`ote_tests` (unit tests) and `ote-sim` (plays whole battles AI vs AI,
headless). The player has automation flags (headless, scripted input,
screenshot) so UI flows can be verified without a human.
