# Xbox 360 player (phase 1)

The 2D player for the Xbox 360, built with [xsdk](https://github.com/xenosworks/xsdk) (MIT; GCC 15 for `powerpc-xsdk-elf`, newlib,
XEX2 packing, its own Xenos GPU layer) and [SDL-xenon](https://github.com/xenosworks/SDL-xenon) (branch `xenon`: SDL 3.4 with
audio, controllers, paths, threads and timers for xsdk, **no video driver**). Not libxenon, never the Microsoft XDK. What a port
needs in general is docs/PORTING.md; the original-Xbox port this one follows is docs/XBOX_OG.md.

**Status.** Everything below was checked on a PC: the engine on a big-endian PowerPC CPU under qemu-user, and the finished `.xex`
in the **Xenia Canary emulator**. **Nothing has run on a real console.** Emulator results say little about speed, the real
display, controllers, sound or the console's kernel; Xenia has no display controller at all, so the picture was checked off-screen
(see "What is verified").

## Building

You need (all outside this repository; the sizes are what the 2026-11 build took):

1. **xsdk's toolchain**, about 1.5 GB installed (7 GB while building, deletable): `git clone https://github.com/xenosworks/xsdk`, then
   `PREFIX=<tc> WORK=<work> JOBS=8 tools/toolchain/build.sh` (9 minutes with many cores; needs bison, flex, texinfo, GMP/MPFR/MPC,
   Python 3.11+). Then `make -j8 TOOLCHAIN=<tc>/bin/powerpc-xsdk-elf-` inside the xsdk checkout (builds `build/libxsdk.a`).
2. **glslang-xenon** (xsdk's `docs/GETTING_STARTED.md` section 4), its `glslang` binary copied into `<tc>/bin/`: the one screen
   shader pair is compiled from GLSL at build time.
3. **SDL-xenon** checkout. `build.sh` builds its `build-xsdk/` when missing.

```
tools/xbox360/build.sh --xsdk DIR --sdl DIR --toolchain DIR [--jobs 8] [--out DIR]
```

Result: `<out>/OpenTacticsEngine/` (default `build-xbox360/`), about 27 MB: `OpenTacticsEngine.xex` (about 10 MB), `assets/`, `games/`
(`index.json` and the three demo games), `licenses/`. It is a plain CMake build (xsdk's toolchain file, `if(XSDK)` branches in
`CMakeLists.txt` and `engine/player/CMakeLists.txt`, `OTE_XBOX360` defined for the player); a full build takes a few minutes.

Only the demo games go in (the rule of `tools/xbox-og/build.sh`, `tools/windows/package.ps1`): the script stops when
`games/index.json` lists anything else, ships a fixed list, and checks the staged folder for private games.

## Running on a console (not yet done)

Needs an RGH or JTAG console whose dashboard (e.g. Aurora) launches unsigned `.xex` files and has FTP. Copy the whole
`OpenTacticsEngine/` folder to the hard disk (e.g. `Hdd1/Apps/OpenTacticsEngine/`), start `OpenTacticsEngine.xex` from the
dashboard's file browser. The title writes `OpenTacticsEngine.log` beside the `.xex` (readable while it runs over FTP; xsdk's
`net_log.txt` sends it live to a PC). Settings and saves go to `userdata/` beside it (`SDL_GetPrefPath`).
An optional `args.txt` beside the `.xex` is read as the command line (`platformLaunchArgs`, same format as the Xbox ports;
`NAME=value` words go into the environment, e.g. `OTE_LOG_LEVEL=debug`). The launcher's B asks "Exit?" and Yes returns to the dashboard.

## How it works

- **Backend.** The SDL3 backend unchanged (`engine/platform/sdl3/`), on SDL-xenon: gamepads (up to four), audio, threads, paths.
  `SDL_GetBasePath` is `game:\`, so the content-root rule ("`games/` next to the program") finds the demo games.
- **Picture.** SDL-xenon has no video driver, so `sdl_app.cpp` (`#ifdef OTE_XBOX360`) uses SDL's dummy video driver and the **software
  renderer** into the dummy window's surface (1280x720, the logical frame letterboxed in it as on any desktop), and
  `platform/xbox360/xbox360_present.cpp` uploads that surface each frame as one tiled A8R8G8B8 texture and draws it as a
  full-screen quad (two GLSL shaders, `present.vert` / `present.frag`) through xsdk's GPU layer into a 720p front buffer
  (2 buffers, one flip per vblank). `xsdk_present_begin` waits for a free buffer, so the loop is paced by the display. On a 4:3 screen xsdk
  letterboxes the 720p frame itself.
- **Small gaps filled.** `platform/xbox360/xbox360_compat.cpp`: `times()` (newlib's `clock()` needs it; xsdk has none).
  `tools/xbox360/xsdk_compat.h` (force-included): GCC + newlib give strict-mode C++ no `LLONG_MAX`, which Lua checks. `process_info.cpp`:
  `args.txt`, a 128 MB art budget (guess, unmeasured), the launcher Back rule.

## Big-endian proof (the most valuable result of phase 1)

The console is big-endian; the engine's rule is "never assume little-endian" (docs/ARCHITECTURE.md). Independent of xsdk,
`tools/xbox360/bigendian_check.sh --toolchain DIR --qemu FILE` builds the engine core, the player library and `ote_tests` for
**64-bit big-endian PowerPC Linux** (bootlin's `powerpc64-power8` glibc toolchain tarball, static) and runs them under
`qemu-ppc64` user mode (Debian's `qemu-user` package, unpacked with `ar`/`tar`, no root). Result at this commit:

- `ote_tests`: **2748 tests run, 0 failed**.
- The simulators (`ote-sim`: 3 demo games' battles and the demo campaign) print summary lines **identical** to the little-endian
  build's (same wins, rounds, captures, builds), so rules, Lua, JSON, RNG and the AI behave the same byte for byte.
- No endianness bug was found in engine code (earlier work had already made glTF and WAV reads byte-wise).

Not covered by it: the player/SDL layer (pixel formats through SDL, which handles byte order itself), 32- vs 64-bit differences
(the console is 32-bit user mode; ppc64 is 64-bit, `long` and pointers are wider), and xsdk's own newlib.

## What is verified (emulator only)

Xenia Canary 02d2cb5 (Linux build), on a private virtual X display, audio backend `nop`, empty HOME
(`tools/xbox360/xenia_smoke.sh`). With an `args.txt` of `OTE_XBOX360_NO_DISPLAY=1 --no-audio --no-saving --no-settings --game
demo-srpg --map bridge --frames 150 --screenshot cache:/ote.bmp`:

- the `.xex` loads, the engine starts, loads `demo-srpg`, runs a battle with the opening dialogue and exits with status 0;
  the screenshot (software frame) is the correct picture, colours included;
- the picture path's self test (`xbox360PresentSelfTest`: a 1280x720 pattern through the texture, shaders, quad and resolve,
  read back) **passes** on Xenia's GPU emulation: 84 sampled pixels exact.

`OTE_XBOX360_NO_DISPLAY` exists only because **Xenia has no display controller** (`xsdk_present_open` fails there with
0xC00000B5): the player then draws without showing, runs the self test, and carries on. Never set it on a console (it would hide a
real display failure). So the **actual flip to the screen is unverified**, even in the emulator.

## Not verified / missing

- Anything on a real console: the display flip (720p, 4:3 letterbox), real controllers (A/B/Start mapping), **sound** (SDL-xenon's
  xsdk audio path: untested here, the smoke runs use `--no-audio`), thread behaviour, the real kernel, memory (512 MB shared: the 128 MB art
  budget is a guess), speed (the software renderer at 720p on the 3.2 GHz in-order CPU may be slow: the biggest unknown; the next step
  if so is drawing the frame's rectangles and sprites on the GPU instead of one texture).
- Saves/settings writing (`userdata/`) was not exercised (Xenia mounts `game:\` read-only); the code path is SDL's `SDL_GetPrefPath`.
- The XEX's title id is xsdk's default and its stack the default 1 MB; the dashboard shows the name "OpenTacticsEngine", no icon.
- 3D views (software 3D only through the same picture), a native editor / MCP are not part of this port; the 2D launcher and three
  demos are what is expected to work. 32-bit pointer effects on `size_t` / `long` are only partly covered by the 64-bit proof.
- The `.xex` is not signed: it runs on RGH / JTAG consoles only.
