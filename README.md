# OpenTacticsEngine

An approachable engine for turn-based tactics games, in the spirit of
RPG Maker: make a game by filling in data and drawing maps, without
writing code, and play it on many kinds of machines.

It covers both economy tactics (income, building units, capturing) and
strategy RPGs (named heroes, experience, levels), as rules a game switches
on or off. Work in progress.

## What is here

- **Player** (`engine/`): runs games on desktop (SDL3, with an SDL2 backend
  kept working for older and homebrew systems) and in the browser. Includes
  a launcher, options, saving, and switchable visual styles: top-down and
  isometric today, 3D kinds planned.
- **Editor** (`editor/`): a browser-based editor for people who never
  script: units, weapons, terrain, structures, art, maps, and Play.
- **Games** (`games/`): two small demos, both with sound and music (the
  demo-srpg set is CC0, credited in its `CREDITS.md`). A game is a folder of
  JSON files, pictures, optional sound (`docs/SOUND.md`) and optional Lua
  formulas (`docs/GAME_FORMAT.md`). New projects from the editor's templates
  come with the generated retro sound set (`tools/audio/`).

## Building

Needs CMake, Ninja, a C++17 compiler and SDL3 (or SDL2).

```
cmake -S . -B build -G Ninja && cmake --build build
./build/engine/player/ote-player          # the player
./build/ote-editor                         # the editor, then open http://127.0.0.1:8765
```

More (SDL2 build, web build, tests): `HANDOFF.md`. Roadmap: `docs/PLAN.md`.

## Licence

The engine, editor and tools are under the GNU General Public License
v3.0 (`LICENSE`). Bundled third-party code and fonts keep their own
licences (`third_party/`, `assets/fonts/`).

A game made for the engine (its data files, pictures, scripts and maps) is
its maker's own work and is not covered by the engine's licence. Anyone who
passes on the engine itself together with a game, changed or not, must
offer the engine's source under the GPL.
