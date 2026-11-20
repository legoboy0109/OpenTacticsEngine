# Testing

How the engine, the editor and the web player are checked, and how to run it all. The per-feature test lists are in the
docs of each feature (docs/SRPG.md, docs/HEX.md, docs/SOUND.md ...); this file is the how-to.

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
