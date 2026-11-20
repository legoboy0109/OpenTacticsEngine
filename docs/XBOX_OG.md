# Original Xbox (2001) player

The 2D player for the first Xbox, built with the open-source [nxdk](https://github.com/XboxDev/nxdk) toolchain. What a port
needs in general is in docs/PORTING.md; the modern-console (UWP) port is docs/XBOX.md. This one is for the 733 MHz Pentium III
console with 64 MB of RAM, and it is meant to **run from the console's hard disk**.

**Status.** Everything below was run in the xemu emulator only (not cycle-accurate: its speed says nothing exact about a real
console), never on a real console. Not verified at all: a real controller, sound (see "Sound"), a real TV (overscan, 480i /
480p / 720p), real disk speed. The 3D views run on a native NV2A backend (section "3D"), also in the emulator only. Planned next: the owner's
real console.

What works: the launcher, all three demo games (Story Tactics, Wars, Hex Wars) with battles, fights, menus, Options, saving,
Continue, Exit to the dashboard, a user-added game folder, 640x480 and 1280x720, 4:3 and 16:9, and the 3D styles (3D isometric,
full 3D) drawn by the console's graphics chip.

## Building

You need a built nxdk (`git clone --recursive https://github.com/XboxDev/nxdk`, then its README: toolchain, libraries,
`make tools`; it needs clang, lld and a few more) and nothing else (no Microsoft file of any kind).

```
tools/xbox-og/build.sh --nxdk /path/to/nxdk [--jobs 4] [--out DIR]
```

Result: `<out>/OpenTacticsEngine/` (default `build-xbox-og/`), about 22 MB: `default.xbe` (about 3.9 MB), `assets/`,
`games/` (`index.json` and the three demo games), `licenses/`. About 450 files compile in a minute or two.

Only the demo games go in (the same rule as `tools/windows/package.ps1` and `tools/uwp/build.ps1`): the script stops when
`games/index.json` lists anything else, ships a fixed list (`demo-srpg demo-economy demo-hex`), and checks the staged folder
for private games. Everything staged is also checked against the disk's file-name rules (next section) and the build fails
with a list of offending paths.

`tools/xbox-og/build.sh --check-game DIR` checks one game folder without building, for people preparing a game.

How it is put together: `tools/xbox-og/Makefile` (nxdk's own makefile does the linking and the XBE; our compile rules
replace nxdk's so that sources outside the build folder work), `tools/xbox-og/nx_compat.{h,c}` and `nx_printf.c` (libc
replacements, "nxdk traps"), `engine/platform/nxdk/` (the platform code: `nxdk_platform.cpp`, `nxdk_overlay.cpp`,
`video_pick.*`, `nxdk_hw3d.cpp` + `hw3d_*` + `nxdk_present.*` = the 3D backend), the SDL2 backend (`engine/platform/sdl2/`, with `OTE_NXDK` switches) on
nxdk's SDL2 2.0.9. Every Xbox change is behind `OTE_NXDK` or lives in Xbox-only files; generic helpers (`readChunk()`) are plain
`fread` everywhere else.

## Install and start

Copy the `OpenTacticsEngine` folder to the console's hard disk with FTP (any FTP-capable dashboard): for example
`E:\Apps\OpenTacticsEngine\` or `F:\Apps\...`. Start `default.xbe` from the dashboard's app list or file browser. It works from
**any folder on any partition**: nxdk maps the folder the XBE was started from to `D:`, the program reads its content there
(`D:\games`, `D:\assets`) and never uses a hard-coded drive letter.

**Your own games:** copy a game folder (with its `game.json`) into `<app folder>\games\`. The launcher scans the folder (a FATX
directory listing), so a game does not need to be in `index.json`.

**Where settings, saves and the log go.** In `<app folder>\userdata\`: `settings.json`, `saves\<game id>\continue.json` and
`slot1..5.json`, `progress.json`, `log.txt` (this run) and `log-previous.txt`. The log is written straight to the disk and
flushed (every line for the first 200, warnings and errors at once, otherwise twice a second), so it survives a crash. To keep
saves when updating, copy the new files over the old folder and leave `userdata\` alone.

*Safety net (not a supported way to run):* from a read-only disc the program still starts; it finds that `<launch
folder>\userdata` cannot be written (it tries to make a file there) and uses `E:\Apps\OpenTacticsEngine\userdata\` instead, with
one warning in the log (`the launch folder is not writable (a disc?) ...`).

**Leaving.** B on the launcher's first screen asks "Exit the player?" (Yes / No, No selected); Yes writes the settings and
returns to the console's dashboard (`XLaunchXBEEx` with no path: a quick reboot into the BIOS, which starts the dashboard),
exactly the question the UWP build asks. If the owner prefers no way out, change `launcherBack()`
in `platform/common/process_info.cpp`.

## Controls

The Xbox controller through SDL's game-controller API with nxdk SDL's built-in mapping for its Xbox driver, and the same button
meanings as every other build (docs/PORTING.md "Input"): d-pad or left stick move, A confirm, B cancel, Start menu, White =
previous (left shoulder), Black = next (right shoulder), Back = show / hide the enemies' reach in a battle, X / Y turn the camera, right stick and triggers the free camera (3D
only). Four ports, hot-plug: pads present at start and ones plugged in later are opened (`SDL_CONTROLLERDEVICEADDED`) and all
act as one input. **Not tested with a real pad** (xemu gets no input in these runs); the scripted-input path (below) stands in.

## Picture

* **Mode.** At start the player lists the modes the console offers (`XVideoListModes`; 720p / 1080i appear only when the
  dashboard's video settings allow HDTV) and sets **640x480** (`video_pick.*`). `OTE_VIDEO=720p` in `args.txt` asks for
  1280x720 when offered. 720p is not the default because every frame is made by the CPU and copied through video memory: in
  xemu 55 to 67 ms a frame at 720p against 17 to 33 ms at 480.
* **Fit.** The game's logical canvas (480x270 for all demos) is scaled by the largest factor that fits the **safe area** and
  centred (`platform/common/screen_fit.*`, `SdlRenderer::setFixedScreen`): 576x324 inside 640x480 at 90% (x1.2), 1216x684 inside
  1280x720 at 95% (x2.53). Fractional scale on purpose (a whole-number scale would use only 75% of the screen); text stays
  readable because the pixel font is drawn through the same scale and smooth fonts are rasterised at the real size.
* **Safe area.** An Options row **Screen margin** (Xbox builds only; every other build has the same rows as before):
  Automatic, then 100% down to 76% in steps of 2. The picture resizes as you change it, so the edges can be judged on the
  spot. Stored in `settings.json` as `safeArea` (global, or per game like any option). Automatic = **90% on 480 modes, 95% on
  720p**. Why: CRTs hide part of every edge; the rule of thumb of the CRT era was "action safe" 90%
  (5% per edge) and "title safe" 80% (10% per edge); the 2009 revision for flat panels is 93% / 90%. 90% keeps the UI inside
  the old action-safe area without wasting a fifth of a 640x480 picture (quoted from memory, not re-checked here); HD sets overscan
  less and many show the picture 1:1. Owner may change both numbers (`defaultSafePercent()`).
* **16:9.** The dashboard's widescreen flag (`VIDEO_WIDESCREEN` in `XVideoGetEncoderSettings()`) makes a 480 mode anamorphic:
  the console stretches 640x480 to 16:9, so the canvas is drawn narrower in x than in y (x1.2 by x1.6) and looks right on the TV.
  Verified in xemu with the flag set (the screenshot looks tall because xemu shows 640x480 at 4:3).

## Memory and speed

Measured in xemu, `used` = kernel pages in use of 64 MB (the log line `memory after ...`, and the overlay):

| Where | Used |
|---|---|
| Launcher | 7 to 9 MB |
| Wars (economy) game, battle | 9 to 10 MB |
| Hex Wars, battle (flat / isometric) | 15 to 17 MB (art 4 MB with the isometric sheet) |
| Story Tactics, battle | 18 to 21 MB |
| Story Tactics, fight on the map (battle scene mode) | 21 MB |
| Story Tactics at 720p | 23 to 25 MB (two 3.7 MB frames) |

**Art budget: 20 MB** (`artBudgetMB()`, docs/GRAPHICS.md "Art memory"; `OTE_ART_BUDGET_MB=N` overrides): about 25 MB stay free.
The biggest sheets are `demo-srpg/art/detailed_tiles.png` (2048x1024, 8 MB decoded; only the "Detailed 3D" style uses it, which is
off here) and `demo-hex/art/iso_tiles.png` (4.6 MB, isometric style: used, art 4 MB in the log). Both are far below the
budget; a sheet that does not fit simply loads and drops on demand.

Frame time in a battle at 480 (the `frame time` debug line; measured with the first version, whose `work` left out SDL's drawing, see "3D" > Speed; xemu is a PC running the CPU through an emulator, a real 733 MHz
Pentium III will differ): work 1 to 3 ms, interval 17 ms (60 Hz, the player waits for the vertical blank) when the host was idle
and 33 ms (30 Hz) under load. 720p: work 1.2 to 3 ms, interval 55 to 67 ms. Start to launcher: 2.1 s after the XBE starts.
What a real console costs is unknown: the picture is made by SDL's CPU renderer and converted by `SDL_ConvertPixels` into the
frame buffer (write-combined memory) every frame.

## 3D (the NV2A)

The 3D view kinds (3D isometric, full 3D) are on (`viewSupported()` in `engine/player/views.h` answers true for every kind), drawn by
the console's graphics chip, the NV2A, through nxdk's pbkit (raw push buffers). The backend is `Renderer3D` like the others
(docs/RENDER3D.md "Original Xbox (NV2A) backend" has the design and the arithmetic); this section is what matters on the console.
**Everything here was run in xemu only.** xemu's NV2A is an emulation (it turns the chip's commands into OpenGL), not the
chip: see "What an emulator cannot tell" below.

**Picture and composition.** The 3D picture is drawn straight into the screen at the screen's own pixels (576x324 in the 480
mode, 1216x684 at 720p), in the rectangle the 2D layer's fit gives the canvas (safe area and anamorphic 16:9 included), under
whatever the 2D renderer draws afterwards (HUD, menus, text). The 2D layer is SDL's CPU renderer, which on nxdk draws into an
ARGB surface of our own (the window's surface has no alpha). Until the first 3D backend is made the surface is copied to the
frame buffer as before (pure 2D is unchanged). The first 3D use starts pbkit, which takes the screen over; from then on the chip
composes every frame: the surface (2D drawn so far) is copied into a texture and drawn as one rectangle, the 3D picture is drawn
over it, then the surface is cleared to transparent, what the 2D renderer draws next lands in it, and it is drawn over the picture
as a second rectangle, blended (premultiplied: SDL's own blending leaves premultiplied colour on a transparent surface). A
frame without 3D costs one upload and one rectangle, the same copy of 1.2 MB (3.7 MB at 720p) the CPU path made. A clear colour of
alpha 0 leaves the 2D backdrop showing, as everywhere. Why this design: SDL 2.0.9 has no GPU renderer for the NV2A, and drawing
the 3D into a CPU picture and reading it back would be slow on a real console (the frame buffer is slow to read); composing on the
chip needs no read-back and keeps the 2D layer as it is.

**Memory** (kernel pages of the 64 MB, `used` in the overlay and the log). The chip's own cost, once 3D starts: three frame
buffers and a depth buffer (4 x 1.2 MB at 640x480, 4 x 3.7 MB at 1280x720), the 2 MB push buffer, 1 MB for triangles cut at the
camera, two 2D-layer textures (2 x 1.2 MB, 2 x 3.7 MB at 720p): about 10 MB at 480, 25 MB at 720p. On top of that the 3D layer has
its **own explicit budgets**: textures 6 MB (4 MB at 720p), meshes 2 MB; an allocation over a budget is refused with a log line. A
texture that would take more than 3 MB (with its mipmaps) or more than what is left of the budget is held at half the size, and again,
until it fits (texture positions are fractions, nothing above notices; the log says `held at`). Measured scenes: the test scene 16 MB used at start (the chip's 9 MB included; textures 12 KB, meshes 72 KB); Wars (economy) 3D at 480: 24 MB (11 MB in 2D) with textures 0.6 MB, meshes 0.1 MB; Hex Wars 3D at 480: 24 MB (2.2 MB textures); Hex Wars 3D at 720p: 43 MB (16 MB in 2D); Story Tactics 3D isometric / full 3D: textures 0.5 MB, meshes 0.5 MB (1.2 MB in Detailed 3D; its total use is in the next paragraph).
`Caps3D`: textures up to 1024 a side (powers of two), 65,535 vertices a mesh, `fullResolution` true.

**Story Tactics "Detailed 3D".** Its art sheet is 2048x1024 (8 MB decoded). The sheet loader (`sheets3d.cpp`) now shrinks a sheet
that is over the backend's size to it (a box filter weighted by alpha) instead of refusing it: 1024x512 here, 2.7 MB with
mipmaps; the style's six model textures (1024x1024 each) are held at 512x512. **That style does not fit in the memory**: with its
meshes and the 2D art the console reaches 61 of 64 MB and some later model textures fail to decode (`outofmem` in the log; what uses
them keeps the placeholder look, the battle runs; 8 model pictures failed in the 140-frame test and the picture still differs from the software one in only 2.0% of its pixels). Owner's decision: keep it like this, or leave the style off this console
(a per-style switch would be needed; `viewSupported()` knows kinds, not styles), or shrink the style's art in the game.

**Debug switches** (words in `args.txt`): `OTE_3D_DEBUG=N` (1 no back-face culling, 2/3 the other winding, 4 front faces culled,
5 no depth test, `cycle` = N changes every 120 pictures and is logged) and `OTE_SHOT_EVERY=N` (saves every Nth frame's composed
picture as `userdata\dbgNNNN.bmp`). The 3D rows of Options ("3D graphics", "3D resolution") are not shown on this console: the
chip always draws at the screen's resolution and software 3D is too slow to offer.

**Testing in xemu.** The scene set is `tools/xbox-og/scenes/*.txt`: the player arguments of the backend conformance test
(`tests/render3d_backends_test.py`: the test scene flat and in perspective, each demo's 3D styles, fog, a unit falling) with
`--fixed-dt`, as `args.txt` files (`build.sh --test-iso --test-args FILE`). `tools/xbox-og/host_scene.py PLAYER SCENE OUT.bmp`
renders the same scene on the host with the software backend, `tools/xbox-og/compare_shot.py` compares it with the console's
`--screenshot` (`userdata\shot.bmp`, read back with `fatx-read.py`). Results (the 3D rectangle of the console picture against the
software picture scaled to it; "after a 2 px blur" removes edge and scaling noise): test scene flat 0.4%, in perspective 0.3%; Story Tactics 3D isometric 1.4%, full 3D 1.8%, full 3D turned and tilted 1.8%, an autoplayed battle with models 0.5%; Wars 3D isometric 1.9%, full 3D 2.5%; Hex Wars 3D isometric 1.3%, full 3D with fog 1.2% (percent of pixels off by more than 24 of 255 after the blur; mean difference 1 to 3 of 255 per channel). Picture names in `OpenTacticsEngine-OGXbox/shots/`: `3d-*.png` (the console's picture of each scene) and `perf2-*.png`.
Differences that remain: triangle edges (the chip's fill rule and 8-bit colour steps), nearest-pixel sampling at the 1.2x scale,
and units caught at slightly different animation moments.

**Speed** (xemu on a loaded PC, `frame time` log line: `work` = what the player spends in a frame before the picture is sent,
`interval` = frame to frame; 60 Hz is 16.7 ms): 2D Wars battle at 480: interval 33 ms; 3D isometric (Hex Wars) 34 to 36 ms, full 3D 33 ms; at 720p 2D 67 ms, 3D 72 to 77 ms (60 Hz steps: 17, 33, 50, 67 ms). The `work` figure now includes the 2D layer's drawing (SDL's CPU renderer draws while the player records, it used to run in `SDL_RenderPresent`, after the timer): 11 to 14 ms at 480 in 2D, 16 to 18 ms in 3D isometric, 20 to 44 ms in full 3D (the first window includes loading); the first version's pure-2D `work` of 2.3 ms was only the recording of commands, and its interval was the same 33 ms.
These say little about a real console: xemu runs the 733 MHz CPU through an emulator and the NV2A through OpenGL, and the host was
shared with other work (load average 20 to 30). What is certain is the structure: per frame one 1.2 MB upload (two with a 3D
picture), vertex memory written once per mesh and once per frame for a CPU-posed character, indices written into the push buffer
(about 290 KB of the 2 MB in a busy 89-draw frame).

**Limits and traps** (found the hard way):

* **The chip's depth buffer was a W-buffer.** pbkit's surface setup turns on "Z perspective" (`NV097_SET_CONTROL0` bit 16); with our
  vertex program, which divides z by w itself, an orthographic camera (w = 1 everywhere) then gives every pixel the same depth and
  the isometric view drew far faces over near ones. Every frame clears that bit.
* **Vertex programs read at most one constant and one input per instruction**, and the texture attribute `v[8]` is hardware
  attribute 9 (vp20compiler maps it).
* **Fog through the secondary colour's alpha did not work** (every pixel came out fully fogged); the fog share is carried in the
  secondary colour's RGB and added after the texture instead (`hw3d_vp.vsh`).
* **Culling.** The chip judges the winding as seen on screen: counter-clockwise faces are front (`NV097_SET_FRONT_FACE` CCW).
* **Nothing between the camera and the near plane, and nothing far outside the screen, may reach the chip**: after the vertex
  program's divide a vertex behind the camera is mirrored and a very near one lands thousands of pixels away. Meshes not wholly inside
  are cut on the CPU (`hw3d_clip.*`, unit-tested on the host).
* **The push buffer holds the whole frame** (indices are sent inline, 16 bit): 2 MB, about 100,000 triangles; a draw that does not
  fit is dropped with a warning in the log.
* Vertex and texture memory must not change while the chip may still read it: `updateMesh`, `destroy*` and `create*` wait for the
  chip to be idle first (cheap: it is idle by then, the player waits a frame for the vertical blank).
* A frame buffer is read back only for screenshots; on a real console its tiled layout may scramble that picture (the picture on the
  TV is not affected).

**What an emulator cannot tell** (the features most likely to differ on a real console): the chip's rasteriser edge rules and
texture filtering (xemu uses OpenGL's), the exact 24-bit depth behaviour (depth compression and the z-cull unit, which xemu does not
model), the guard band (how far outside the screen a vertex may lie: 8 half-screens or at most about 1800 pixels is assumed), the
write-combined memory timing (a missing flush shows only on hardware), vertex-program corner cases (division by zero in the distance
for fog is guarded, other garbage-in cases are not), and the real speed, which decides whether 720p is usable at all.


## Sound

`openAudioOut()` opens SDL's Xbox audio driver (the log says `audio: SDL driver 'xbox', device opened at 44100 Hz stereo`); the
mixer (`engine/audio`, music streamed from OGG through stb_vorbis, effects) feeds it through `SDL_QueueAudio`. **Not verified:
nothing was heard and nothing could be.** In xemu with the host's audio muted (`SDL_AUDIODRIVER=dummy`) the sound chip never
reports a finished buffer, so SDL's audio thread never consumes the queue (the log at exit: `3528 frames pushed, 3528 still
queued (so 0 were played)`; 3528 frames is the 80 ms the player keeps queued). Whether a real console plays is open. The
CPU cost of the Ogg decoding on this CPU is not measured either. Because SDL's audio thread would then also never exit, the
player does not close the device when leaving: it silences it and goes straight to the dashboard.

## Testing without a controller (scripted runs in xemu)

The player's scripted input (`--input`, `--frames`, `--screenshot`, `--game`, `--map`, `--autoplay`, `--continue`) is available
because `platformLaunchArgs()` reads a file: **`D:\args.txt`**, then `<app folder>\userdata\args.txt` (the second replaces the
first). Words are player arguments; `NAME=value` words go into the environment (`OTE_LOG_LEVEL=debug`, `OTE_XBOX_OVERLAY=1`,
`OTE_VIDEO=720p`, `OTE_ART_BUDGET_MB=..`); `#` starts a comment line; double quotes keep spaces. Example:

```
OTE_LOG_LEVEL=debug
OTE_XBOX_OVERLAY=1
--game demo-economy --map misty_ford
--input wait:30,next,confirm,up,confirm,wait:20,background
```

A script that ends quits the player (to the dashboard). `wait:N` is N input intervals of 6 frames; with the player waiting for
the vertical blank a frame is 1/60 s, so timing follows the frame count, but xemu's speed is not the console's.

`OTE_XBOX_OVERLAY=1` draws frame time, memory and the newest log lines over the picture. `--screenshot D:/userdata/shot.bmp`
(with `--frames N`) writes the whole 640x480 output, margins included, to the disk; read it back with a FATX reader.

**Running it in xemu.** xemu starts discs, not folders, and there is no FATX writer on Linux, so `build.sh --test-iso` also builds
a tiny installer disc (`tools/xbox-og/installer/`, a test vehicle, not how users install): it copies the app folder from the disc
to `E:\Apps\OpenTacticsEngine` (keeping `userdata\`) and starts it from there with `XLaunchXBE`; `ote-launcher.iso` only starts
what is installed; `ote-dash.iso` only tests the exit path. `--test-args FILE` puts a script on the disc as `args.txt`,
`--test-extra DIR` merges more files (a user game) into the test disc's copy. xemu needs your own console's boot ROM and BIOS
(see below). A small read-only FATX reader for the emulator's disk image (`fatx-read.py`, not part of the repo) reads the log and
the saves back without booting.

## Running in xemu with your own dump

**The boot ROM and BIOS are not provided and must be dumped from your own console** (and so is the hard disk image if you want
your console's dashboard). Nothing of Microsoft's is in this repository. Point xemu at your own files; the program needs a
bootable dashboard-less setup only for the installer disc above. In xemu's Machine settings turn on the HDTV video flags to see
720p (the EEPROM xemu creates has none), and 16:9 for the anamorphic mode.

## File name limits (FATX)

The console's hard disk file system (FATX) is stricter than any desktop one. Rules used here (`engine/core/base/fatx_names.h`,
unit-tested; `tools/xbox-og/check_names.py` repeats them for the build): a name is 1 to **42 characters**; allowed are ASCII
letters, digits, space and `! # $ % & ' ( ) - . @ [ ] ^ _ ` { } ~` (the set the open-source FATX tools accept; from fatx 1.17's
name table); anything else (`+ , ; = " * / : < > ? | \`, non-ASCII, control characters) is refused by the kernel; names are
compared without regard to case but keep their case, so two names in one folder differing only by case are the same file. The kernel's
path limit is about 250 characters; the build check allows 240 counting from `E:\Apps\OpenTacticsEngine\`.

**For people preparing a game:** keep every file and folder name inside those limits, including the files `game.json`, graphics
and audio files name (`"file": "art/hero.png"`). A name the disk cannot hold cannot be copied to the console (FTP clients
refuse or rename it) and the player says so in the log when such a file cannot be opened (`cannot open a file whose name the Xbox disk
does not allow: ...`). `build.sh --check-game DIR` lists the offenders.

**What the engine creates itself** is safe: `settings.json` (+ `.tmp`, `.bad`), `log.txt`, `log-previous.txt`, `args.txt`,
`saves\<id>\continue.json`, `slotN.json`, `progress.json` (+ `.tmp`) all fit (longest 21 characters). The one name built from user
data is the folder named after the **game id**: `platformFileName()` (platform.h) encodes it with `fatxSafeName()` on the Xbox
(characters FATX refuses and `~` become `~XX` hex, a name over 42 characters is cut and ends in `~` plus eight hash digits;
a name that is already fine is unchanged, so every demo and every existing game keeps its name) and is the identity on every
other platform. Map ids and the like never become file names.

**Found in our own files:** all 237 files of the three demos and `assets/` pass: the longest name is 32 characters
(`AtkinsonHyperlegible-Regular.ttf`), the longest path 73 characters, the only punctuation used is `-`, `.`, `_`.

## nxdk traps (found the hard way)

* `fread` until end of file sets `ferror` on files whose size is not a multiple of the disc's 2 KB sector (XISO): `readChunk()`
  (`core/base/fileio.h`) never reads past the end (plain `fread` elsewhere).
* libc stubs that `assert(0)`: `strtod`, `lround`, `llround`, `lrint`, `llrint` (and `log2`, `exp2`, `cbrt`, `log1p`, `expm1`,
  `scalbn`, `fdim` we expect to hit): replaced in `nx_compat.c`; `nx_compat.h` is force-included into every unit.
* **`printf` has no `%f` / `%e` / `%g`** (prints nothing): the JSON writer wrote `"saveVersion":,` and every Continue save was
  unreadable until `nx_printf.c` replaced `snprintf` / `vsnprintf` (checked against the host's printf over 80,000 cases,
  `tools/xbox-og/nx_printf_test.c`; `std::to_string(double)` is not covered).
* Default stack is 64 KB (the engine keeps 64 KB read buffers on the stack): linked with `-stack:1048576`.
* stb_image needs `-DSTBI_NO_SIMD` (the CPU has SSE, not SSE2).
* No `getenv` data and no `putenv`: a small table (`ote_getenv` / `ote_setenv`) carries the args file's environment words.
* No `thread_local` destructors (link error `___tlregdtor`), no `gmtime_s` / `localtime_s`: switches in the engine.
* The retail kernel's DbgPrint goes nowhere; diagnostics go to the log file and the overlay. FATX keeps writes in the kernel
  cache until the file is closed or flushed: the log calls `NtFlushBuffersFile` (without it a crash leaves an empty file).
* Paths: the kernel wants backslashes (`openFile`, `makeDir`, `listSubdirs` convert); nxdk's `rename()` / `MoveFileA` neither
  converts `/` nor replaces an existing file, `CopyFileA(..., FALSE)` cannot overwrite on FATX, and a file copied from a disc keeps
  its read-only attribute: `nxReplaceFile()` / `nxRemoveFile()` (`core/base/fileio.cpp`) do it with the kernel calls.
* `XLaunchXBE` wants the kernel path (`\Device\Harddisk0\Partition1\...`), not a drive letter mounted with `nxMountDrive`;
  `XLaunchXBEEx(NULL, NULL)` is "go to the dashboard".
* SDL 2.0.9 (nxdk): software renderer only, no vsync (`XVideoWaitForVBlank()` paces the loop), no `SDL_GetTicks64`, and the
  window surface takes the size you ask for (ask for the whole screen, then place the picture yourself: `SDL_RenderSetLogicalSize`
  on a small window draws unscaled in the top-left corner).
* 1280x720 is listed only when the console's video flags allow HDTV; asking for it otherwise must fall back to 480.
* SDL's Xbox audio thread waits for the sound chip; closing the device waits for the thread (see "Sound").

## Diagnostic program

`tools/xbox-og/diag/` is the first feasibility program, kept as a small target: loads `demo-srpg` from the disc, plays an
AI-vs-AI battle with the rules core and prints the result and a hash (the same hash as on Linux: the core gives identical
results), draws the final map, and times a software 3D scene at 640x480 and 1280x720 (about 300 ms a frame). Build:
`NXDK_DIR=... PATH=$NXDK_DIR/bin:$PATH make -C <build dir> -f tools/xbox-og/diag/Makefile` (the host twins
`battle_host.cpp` and `bench3d_host.cpp` give the Linux numbers).

## Known limits and open items

* Never run on a real console; sound, controller, real TV and disk speed unverified.
* Video mode is chosen at start only (`OTE_VIDEO` in `args.txt`); there is no Options row for 480 / 720p.
* 3D: see "Limits" in the 3D section; the "Detailed 3D" style of Story Tactics does not fit the memory (owner's choice there).
* Settings per game exist for the Screen margin row because every option row has them; a global value is the sensible use.
* A real console's dashboard may show the app folder without an icon (no `TitleImage.xbx` / `default.xbe` logo is made).
* The XBE has nxdk's fixed title ID `0xFFFF0002` (cxbe offers no option); nothing here depends on it because data lives in the
  app folder, not under `E:\UDATA\<title id>`.
