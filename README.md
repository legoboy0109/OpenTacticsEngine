# OpenTacticsEngine

An approachable engine for turn-based tactics games, in the spirit of
RPG Maker: make a game by filling in data and drawing maps, without
writing code, and play it on many kinds of machines.

It covers both economy tactics (income, building units, capturing) and
strategy RPGs (named heroes, experience, levels), as rules a game switches
on or off. Work in progress.

## What is here

- **Player** (`engine/`): runs a game, with a launcher, options, saving and a computer opponent. One game can ship
  several visual styles and the player switches between them: top-down 2D, 2D isometric, 3D isometric and full 3D
  (`docs/VIEWS.md`), on square or hexagon maps (`docs/HEX.md`).
- **Web editor** (`editor/server`, `editor/web`): a browser-based editor for people who never script: units, weapons,
  terrain, structures, art, maps, story events, and Play (`docs/EDITOR.md`).
- **Native editor** (`editor/native`, preview): a desktop editor (Qt 6) for bigger projects: tables and forms for every
  data file, undo, a live list of problems, and Play. Optional to build (`docs/NATIVE_EDITOR.md`).
- **MCP server** (`ote-mcp`, `editor/mcp`): lets an AI assistant build and edit games through the editor's own rules
  (schemas, the engine's project check, dry runs) over the standard Model Context Protocol, with any client and any
  model. Built with the editor (`./build/ote-mcp --root games`). How to use it: `docs/MCP.md`.
- **Games** (`games/`): three small demos (a story RPG, an economy game, one on hexagons), all with sound and
  music (the demo-srpg set is CC0, credited in its `CREDITS.md`). A game is a folder of
  JSON files, pictures, optional sound (`docs/SOUND.md`) and optional Lua
  formulas (`docs/GAME_FORMAT.md`). New projects start from templates (blank, economy tactics, strategy RPG,
  two hexagon layouts) and come with the generated retro sound set (`tools/audio/`).

## Rules a game can switch on

Everything here is optional data in the game's files; a game that leaves a rule out plays as if it did not exist.

- Economy: income, building and capturing, structures, research trees, factions (`docs/ECONOMY.md`,
  `docs/STRUCTURES.md`).
- Strategy RPG: named characters, experience and levels, items, supports, skills, jobs and promotion, ranks
  (`docs/SRPG.md`, `docs/ITEMS.md`, `docs/JOBS.md`).
- Combat: damage types and resistances, line and area fire, height, facing, action points, fog of war.
- Turn order by side or by unit speed, with charged actions (`docs/TURN_ORDER.md`).
- Commanders with a meter and powers (`docs/COMMANDERS.md`).
- Story: events, dialogue, choices, objectives, a campaign across maps (`docs/EVENTS.md`), difficulty levels
  (`docs/DIFFICULTY.md`), close-up battle scenes (`docs/BATTLE_SCENE.md`).

The engine checks a game's files strictly and says what is wrong and where, including a "did you mean" for a
misspelled key (`./build/ote-editor --validate <game folder>`).

## Where it runs

| System | State | Notes |
|---|---|---|
| Linux desktop (SDL3) | main development system | SDL2 backend kept working too |
| Browser (WebAssembly) | working | `HANDOFF.md` for the build |
| Windows desktop | working, built in a VM | `docs/WINDOWS.md` |
| Android | working in the Android emulator; not yet tried on a real device | `docs/ANDROID.md` |
| Xbox One / Series (Dev Mode, UWP) | working, Direct3D 11 | `docs/XBOX.md` |
| Original Xbox (homebrew, nxdk) | working in the xemu emulator; not yet tried on a real console | `docs/XBOX_OG.md` |

How a port is put together: `docs/PORTING.md`, `docs/ARCHITECTURE.md`, `docs/RENDER3D.md`.

## Building

Needs CMake, Ninja, a C++17 compiler and SDL3 (or SDL2).

```
cmake -S . -B build -G Ninja && cmake --build build
./build/engine/player/ote-player          # the player
./build/ote-editor                         # the web editor, then open http://127.0.0.1:8765
./build/ote-mcp --root games               # the MCP server (started by an AI client, not by hand)
```

The native editor also needs Qt 6 (qt6-base):

```
cmake -S . -B build-native -G Ninja -DOTE_BUILD_NATIVE_EDITOR=ON && cmake --build build-native
./build-native/editor/native/ote-editor-native games/demo-srpg
```

Run the whole test suite with `tools/check_all.sh` (how, and the single parts: `docs/TESTING.md`).
A first game step by step: `docs/TUTORIAL.md`. More (SDL2 build, web build): `HANDOFF.md`. Roadmap: `docs/PLAN.md`.

## Licence

The engine, editor and tools are under the GNU General Public License
v3.0 (`LICENSE`). Bundled third-party code and fonts keep their own
licences (`third_party/`, `assets/fonts/`).

A game made for the engine (its data files, pictures, scripts and maps) is
its maker's own work and is not covered by the engine's licence. Anyone who
passes on the engine itself together with a game, changed or not, must
offer the engine's source under the GPL.
