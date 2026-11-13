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
- Not started: web build (Emscripten is installed at
  `~/.local/share/emsdk`; run `source ~/.local/share/emsdk/emsdk_env.sh`),
  README.

## Build and test

```
cmake -S . -B build -G Ninja && cmake --build build
./build/ote_tests            # all unit tests
./build/ote_tests battle     # subset by name
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

Merge the AI and player branches, run `ote_tests`, `ote-sim` on both
demos, and the player's headless screenshot flows; then the web build.
