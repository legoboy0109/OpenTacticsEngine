# Testing

How the engine, the editor and the web player are checked, and how to run it all. The per-feature test lists are in the
docs of each feature (docs/SRPG.md, docs/HEX.md, docs/SOUND.md ...); this file is the how-to.

## The full check: `tools/check_all.sh`

One command that builds and runs everything, from any checkout, and ends with a PASS / FAIL / SKIPPED line per part:

```
tools/check_all.sh [--jobs N] [--work DIR] [--only a,b] [--skip a,b] [--skip-e2e] [--skip-web]
                   [--with-audio-test] [--emsdk DIR] [--sims-baseline FILE] [--update-baseline] [--strict]
```

| Part | What it does |
|---|---|
| `sdl3` | builds `build/` (SDL3) and runs `ote_tests` |
| `sdl2` | builds `build-sdl2/` (`-DOTE_PLATFORM=sdl2`) and runs `ote_tests` |
| `editor-api` | `tests/editor_api_test.py` against `build/ote-editor` |
| `ctest` | every ctest entry except `audio_player` (the one that opens the sound device; `--with-audio-test` includes it) |
| `sims` | the demo games' simulators (30 runs each, the campaign 20), compared with the previous run's output |
| `webtest` | `tools/webtest` browser checks of the web player, Chromium then Firefox |
| `vitest` | `editor/web`: typecheck, production build, vitest |
| `e2e` | `editor/web` Playwright, Chromium (Firefox runs the `@smoke` tests); reports the minutes it took |

- **Where it builds.** By default in the checkout's own `build/`, `build-sdl2/`, `build-web/`. With `--work DIR` it first
  copies the **tracked** files (the working tree's contents, so uncommitted edits count; untracked and ignored files, the
  owner's own games included, are never copied) to `DIR/src` and builds and runs there, so the checkout can keep changing
  while the check runs. `DIR` must be new or empty (or an earlier `--work` folder). `node_modules` folders of the checkout, if
  present, are copied along; nothing is ever installed.
- **Logs.** Each part's full output is `<logs>/<part>.log` (`build/check-all/` or `DIR/logs/`). The end of the run repeats the
  greppable result lines: `N test(s) run, M failed` (ote_tests), `N passed, M failed` (editor API), ctest's
  `100% tests passed, 0 tests failed out of N`, `SIMS_IDENTICAL` (or `SIMS_DIFFER`, or `SIMS_BASELINE_SAVED` on the first
  run), the vitest `Tests` line, Playwright's `N passed` totals and `e2e minutes`.
- **Sims baseline.** The previous output lives in `build/check-all/sims_baseline.txt` of the checkout (or `--sims-baseline
  FILE`); `--update-baseline` replaces it after an intended rules change. Timings are ignored in the comparison.
- **SKIPPED, not silently passed.** A part whose prerequisite is missing says why: no Emscripten (`--emsdk DIR` or `$EMSDK`
  or `emcmake` on `PATH`; needed for the web build, `webtest` and `e2e`), no `editor/web/node_modules` or
  `tools/webtest/node_modules`, no Playwright browsers, no SDL2. `--strict` turns any SKIPPED into a failure.
- **Exit code** is 0 unless a part FAILED.
- **Safety.** It never uses port 8765 (the helper and test servers pick free ports), runs the player only with `--headless
  --no-audio` (inside the tests), runs the test browsers muted, uses temp copies of `games/`, and does not run `audio_player`
  unless `--with-audio-test`.

Running the parts by hand: `ctest --test-dir build -E audio_player -j4`, `./build/ote_tests [name-filter]`,
`python3 tests/editor_api_test.py ./build/ote-editor .`; the web commands are in HANDOFF.md.

## Sanitizer build

`-DOTE_SANITIZE=<list>` builds every target of the project (the vendored Lua included) with the compiler's run-time
checkers, for compile and link. Values: `address`, `undefined`, `address,undefined` (the usual pair), `thread` (not
together with `address`). Off by default. GCC and Clang only; on MSVC, Emscripten, Android and UWP the option is ignored
and CMake says so. The flags add no compiler warnings, so the warning sets are unaffected.

```
cmake -S . -B build-asan -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DOTE_SANITIZE=address,undefined
cmake --build build-asan -j4
./build-asan/ote_tests
./build-asan/ote-sim games/demo-srpg bridge --runs 5
```

Findings are printed with a stack (file:line) and the program carries on, so one run lists them all. Use
`UBSAN_OPTIONS=print_stacktrace=1` to get stacks for undefined-behaviour reports. Expect it to be several times slower than
a normal build. GCC's `-Wmaybe-uninitialized` may print a few warnings from inside libstdc++'s `std::regex` in a sanitizer
build only; they are in a system header and not from our code.

## clang-tidy

`.clang-tidy` at the repo root holds a small bug-finding set (`bugprone-*` minus the noisy ones, a few `performance-*`; no
style, naming or modernize checks). It reports only our own folders (`engine/`, `editor/server`, `tools/`, `tests/`), never
`third_party/`. It is a tool for looking, not a gate; nothing in the build runs it.

```
cmake -S . -B build-tidy -G Ninja -DCMAKE_EXPORT_COMPILE_COMMANDS=ON && clang-tidy -p build-tidy engine/audio/mixer.cpp
```

## Scratch folders

Everything a test writes goes under one root, `<build>/test-tmp/`, in a folder of its own: `test-tmp/ote_tests/` for the
C++ unit tests (`ote_test::tempDir()` in `tests/test.h`), `test-tmp/<script name>/` for each Python script. A folder is
removed when its test passes and kept after a failure to look at; the next run wipes a leftover first. Nothing is written
inside the source tree (CMake also sets `PYTHONDONTWRITEBYTECODE`, so no `__pycache__` appears next to the scripts).

- CMake gives every test `OTE_TEST_TMP=<build>/test-tmp`. Run by hand without it, the Python scripts use
  `<system temp>/ote-test/` and `ote_tests` uses `<build>/test-tmp/` (the build folder it was compiled in).
- Python: `tests/ote_testkit.py` has the one function, `scratch_dir(name=None)` (name defaults to the script's own, cut to
  24 characters so Windows' 260-character limit is no problem). It can also be used as `with scratch_dir() as tmp:`. Importing
  the kit also points `TMPDIR` at the root, so the programs a script starts (the editor helper, generators under `tools/`)
  that make their own temp folder land there as well. New scripts should call `scratch_dir()` and never `tempfile.mkdtemp`.
- `tests/audio_player_test.py` (the one test that plays sound) is deliberately left as it was.
