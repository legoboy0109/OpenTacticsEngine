# Notes for agents

Every session starts here, whichever tool or model runs it. Keep this file short: it points at the
real documents instead of repeating them.

## Catch up first

1. `HANDOFF.md`, the section marked "START HERE": the current state, what is merged, what waits.
2. `docs/PLAN.md` (roadmap and the owner's decisions), `docs/ARCHITECTURE.md` (structure and the
   binding portability rules), `docs/GAME_FORMAT.md`.
3. `.agent/notes.md` (git-ignored, lives only in the main checkout): debugging notes, gotchas and
   useful commands. Search it before investigating a symptom that feels familiar. From a worktree,
   read and write the main checkout's copy (`git worktree list` shows the main checkout first).

When work stops for the day or moves to another session, update the "START HERE" section of
`HANDOFF.md`: state, decisions made, checks run, the next concrete step.

## Who decides

The owner makes design, naming, wording and direction calls; technical calls are ours. New names
and wording an agent invents are placeholders: mark them as such in the docs and list them in the
report, so the owner can keep or change them.

## Working here

- One session leads on `main` in the main checkout. Parallel jobs each get their own worktree and
  branch; the lead reviews, merges and runs the checks. Never work on another job's branch.
- In a fresh worktree `editor/web/node_modules` and `tools/webtest/node_modules` are missing, and
  without them the full check skips its web parts. Link them from the main checkout and never
  commit the links (T3 Code's "Worktree setup" action does both).
- Build and test commands: `HANDOFF.md` "Build and test", `docs/TESTING.md`. The full check is
  `nice tools/check_all.sh` (about 45 minutes). Run it under `nice` and keep heavy jobs few: this
  is also the owner's desktop.
- The full check does not build the native editor. After a change to `editor/` or a loader, also
  run `QT_QPA_PLATFORM=offscreen ctest --test-dir build-native -R native`.
- Under load the browser tests time out at random. Re-run a failure alone before believing it; a
  failure that repeats alone is real.
- Tests are silent and windowless: the player runs `--headless --no-audio`, test browsers are
  muted. Never add a test that opens a window or plays sound by default.
- No GPU-heavy test (emulators included) while something else of the owner's is using the GPU.

## Limits

- `games/` holds the owner's own untracked projects next to the tracked `demo-*` games: never
  edit, move or count them. Tests use a temporary copy.
- Games converted from commercial titles live in their own private folders outside this
  repository. None of their data, art or names goes into a commit here.
- Pushing `main` to `origin` after a clean full check is agreed. Never force-push. Anything else
  public (releases, posts, other sites) needs the owner's explicit yes.
- Commits use the account's GitHub no-reply address (already in this repository's git config).
  Message style: read `git log`.
