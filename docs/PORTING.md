# Porting the player

What a new platform (Android, UWP / Xbox Dev Mode, a console SDK) has to provide, and what the shared groundwork
already does. The layer rules are in docs/ARCHITECTURE.md; everything here lives in `engine/platform/`.

## Roots (where files are)

`platform.h` "Where things live":

| Function | Meaning | Desktop today |
|---|---|---|
| `contentRoot()` | folder holding shipped, read-only `games/` and `assets/` | the program's own folder when `games/` sits in it (an unpacked download), else empty = the working directory |
| `extraContentRoot()` | optional second folder of games the user added (`<id>/game.json`) | none |
| `userRoot()` | folder the app may write (settings, saves, progress) | `userDataDir()`: `SDL_GetPrefPath("OpenTacticsEngine", "player")` |

Implemented in `platform/common/roots.cpp` (shared by both SDL backends); the program's folder comes from `executableDir()`
(`SDL_GetBasePath`; empty on the web and Android). A port changes the answers there, behind its own
`#if`: Android returns the folder the APK's assets were extracted to on first run (SDL's `SDL_GetAndroidInternalStoragePath`;
APK assets cannot be read with `fopen`). UWP (docs/XBOX.md, `OTE_UWP`) reads content straight from the read-only install
folder (the "`games/` next to the program" rule, `SDL_GetBasePath`), `userRoot()` is the LocalState folder itself and
`extraContentRoot()` is `LocalState/games`. Nothing else in the player builds a content or write path:
`engine/player/content_roots.cpp` turns the roots into `Args::gamesDir` / `assetsDir` / `extraGames`, and every read of a
game goes through `GameEntry::dir` (or `SceneHost::gameDir`).

Precedence (highest first): `--games`, `--assets`, `--extra-games` > `--content-root DIR` (and the environment variables
`OTE_CONTENT_ROOT`, `OTE_EXTRA_GAMES`) > the platform's `contentRoot()`. `--settings` / `--saves` / `--no-settings` /
`--no-saving` are unchanged and beat `userRoot()`. The game list merges the built-in folder and the extra folder; a game in
the extra folder with the id of a built-in one replaces it. Saves of a game are keyed by id, so the two share a save folder.
Tested by `tests/roots_player_test.py` (player started from an empty working directory).

Not under these roots: `--screenshot FILE`, `--audio-log FILE`, `--audio-render FILE` (the caller names them), and the
web build's `/userdata` (IDBFS, `userDataDir()`).

## Life cycle

`platform.h` `Lifecycle` (`WillEnterBackground`, `DidEnterForeground`, `LowMemory`, `Terminating`) reaches
`App::onLifecycle`. Both SDL backends register an SDL event watch (SDL3 `SDL_EVENT_WILL_ENTER_BACKGROUND`, ...; SDL2
`SDL_APP_WILLENTERBACKGROUND`, ...) so the notice is handled at once, not from the queue: the OS may freeze the process
right after the callback returns. The watch and the frame are serialized by one (recursive) SDL mutex, because on Android
the notice arrives on the UI thread while the main loop runs. Not registered on the web (no such events there). On UWP the notices arrive from SDL's WinRT port (suspend =
`WillEnterBackground`, resume = `DidEnterForeground`) on the thread that runs the loop; there is no terminate notice
(the system may end a suspended app silently), so the suspend is where the Continue save is written; the budget is about
5 seconds.

The player (`app_lifecycle.cpp`) on background or terminating: pauses the music and stops mixing (`AudioService::suspend`),
stops the simulation clock (`PlayerApp::update` returns before the scene update), writes the settings, and asks the scene to
write what leaving would write (`Scene::onSuspend`). Only the battle scene does anything: it writes the **Continue save**,
exactly what "Leave battle" writes (the very latest position, including a unit that has only moved; no new file format).
It respects `--no-settings` / `--no-saving` (no path = nothing written). Foreground resumes music and clock. Low memory
writes settings only (nothing is cheap to drop yet).

What a suspend does **not** save: a story scene or dialogue in progress (docs/SAVES.md: such a state is not saved; Continue
restarts the chapter's scene), a half-typed anything (no text entry exists), and a battle that has just been decided.

Not handled: a **device reset**. Neither backend reacts to `SDL_EVENT_RENDER_DEVICE_RESET` / `SDL_EVENT_RENDER_TARGETS_RESET`
(SDL2: `SDL_RENDER_DEVICE_RESET`; the Android hardware 3D context is the exception, it is rebuilt, docs/ANDROID.md "3D"), so the textures `Renderer::createTexture` made (art sheets, the 3D picture, the text atlas)
are assumed to survive. On Android (GL context loss) and UWP (D3D device removal) they may not; the fix is to rebuild
`Sprites`, `IsoTiles`, `Show3D` and the `TextEngine` atlas, from a handler that does not exist yet (`Sprites` is the easy part now: `trimUnused()` drops every texture and they come back on demand).

**Art memory budget.** `artBudgetMB()` (platform.h, implemented once in `platform/common/process_info.cpp`, so a new backend gets
it for free) says how many MB of art textures the player may hold: web 384, UWP 256, Android 192, original Xbox 20, others 768; the environment
variable `OTE_ART_BUDGET_MB` overrides it. Beyond it the player drops the textures used longest ago and makes them again when
drawn (docs/GRAPHICS.md "Art memory"). A port with less memory than these numbers assume changes the numbers there. A port needs
nothing else for this, except that `Renderer::destroyTexture` must really free the memory. On low memory `Sprites::trimUnused()`
frees every art texture.

Tests: the `--input` tokens `background`, `foreground`, `lowmemory`, `terminate` inject these notices
(`tests/lifecycle_player_test.py`).

## Input

The platform back key (`SDL_SCANCODE_AC_BACK`: Android back, and Xbox B on UWP when the app sets
`SDL_HINT_WINRT_HANDLE_BACK_BUTTON`) is `Button::Cancel` in both backends. What Cancel does on the launcher's first
screen is the platform's answer, `launcherBack()` in `platform.h` (`common/process_info.cpp`), so the player has no platform
`#ifdef` there: `Exit` on Android (leaves at once, docs/ANDROID.md), `ConfirmExit` on UWP / Xbox (asks "Exit the player?",
Yes / No with No selected; Yes quits, docs/XBOX.md; the original Xbox asks too and Yes returns to the dashboard, docs/XBOX_OG.md), `Nothing` everywhere else, so a stray Esc cannot close the player.
A new port picks its answer there. `OTE_LAUNCHER_BACK=nothing|exit|confirm` overrides it (`tests/launcher_back_player_test.py`).

One press must give one ButtonDown / ButtonUp: `platform/common/press_dedupe.h` drops the second delivery when the same logical
button comes from another source (back key vs pad button, or a pad listed twice by the OS) while the first holds it or just
after; both SDL backends use it, a keyboard-only or single-pad user is unaffected (docs/XBOX.md "One B press").

Every screen is operable with a controller alone (d-pad or left stick, A confirm, B cancel, Start = Menu, shoulders =
Prev/Next, X/Y = turn camera, right stick and triggers = free camera): all screens take their input as `Button`s; pointer
handlers are additions that call the same actions. No screen has text entry. Skipping a dialogue is "hold Cancel or Menu".

## Build

- **Entry point.** `engine/player/main.cpp` includes `platform/platform_main.h`, which pulls in `SDL_main.h` of the
  configured backend (CMake defines `OTE_PLATFORM_SDL3` / `OTE_PLATFORM_SDL2`). Nothing changes on Linux/macOS; on
  Windows, UWP and Android SDL renames `main`. SDL2 also links `SDL2::SDL2main` on WIN32/ANDROID. The web build skips it.
- **MSVC.** `if(MSVC)` in the root `CMakeLists.txt`: `/W4 /permissive- /utf-8 /EHsc /GR-`, `NOMINMAX`,
  `_CRT_SECURE_NO_WARNINGS`. Built and tested with Visual Studio 2022 Build Tools (MSVC 14.44) on Windows 11: no warnings in
  our code, all tests pass (docs/WINDOWS.md). `/GR-` holds for everything including the editor helper (nothing uses RTTI);
  `/EHsc` is on for every target (the engine throws nothing; the STL and the editor need it declared). The C/C++ runtime is
  static (`/MT`) by default and `-DOTE_MSVC_STATIC_RUNTIME=OFF` switches every target to `/MD` (UWP needs that).
- **Windows differences live in the platform layer.** `core/base/fileio.h` `openFile()` is the only way the engine opens a
  file (wide-path `_wfopen` on Windows, `fopen` elsewhere: engine paths are UTF-8); `platform.h` `replaceFile()`
  (a rename that replaces: `std::rename` does not on Windows and reads the ANSI code page), `executableDir()` and
  `attachParentConsole()` (the player is a GUI-subsystem program; started from a terminal it attaches to that terminal so
  `--help` and the log show up). The SDL2 backend's Windows folder calls use the `W` functions with UTF-16 paths.
- **Texture format (SDL2).** `-DOTE_SDL2_UPLOAD_ARGB8888=ON` makes every SDL2 texture ARGB8888 and swizzles the bytes in the
  backend (`sdl2/sdl2_pixels.h`) in one tight loop, because UWP's Direct3D 11 renderer has no RGBA32 and SDL would convert
  on every upload (every frame for the 3D picture). Default OFF, pictures byte-identical either way (checked with headless
  screenshots). The conversion is in the backend, not at the 3D renderer's output: a software 3D drawer that wrote the
  native format directly would save the loop, but that changes `Renderer3D`'s contract and was left alone.

- **UWP.** `-DCMAKE_SYSTEM_NAME=WindowsStore -DCMAKE_SYSTEM_VERSION=10.0 -DOTE_PLATFORM=sdl2 -DOTE_MSVC_STATIC_RUNTIME=OFF`
  with Ninja in the x64 UWP compiler environment (`tools/uwp/build.ps1`, docs/XBOX.md). CMake defines `OTE_UWP` and
  `WINAPI_FAMILY=WINAPI_FAMILY_APP` (only the UWP part of the Windows API is declared). SDL2 comes from its own
  `VisualC-WinRT` project (`OTE_UWP_SDL2_SOURCE`, `OTE_UWP_SDL2_LIBDIR`). Two files are C++/CX and compiled with `/ZW`: SDL's
  WinRT entry point and `sdl2_uwp_view.cpp`. The pieces that differ live in `platform/sdl2/sdl2_uwp*.cpp`,
  `platform/common/uwp.h` and `platform/common/process_info.cpp` (`memoryStatus()`, `artBudgetMB()`, `platformLaunchArgs()`: a UWP app has
  no command line, so `LocalState/args.txt` stands in). This is also the first MSVC build of the SDL2 backend.

- **Original Xbox (nxdk).** `tools/xbox-og/build.sh --nxdk DIR` builds the player with the open-source nxdk toolchain on its SDL2 2.0.9
  (the SDL2 backend again, plus `engine/platform/nxdk/`; `OTE_NXDK` is defined by `tools/xbox-og/Makefile`) and stages an app folder to
  copy to the console's hard disk. The pieces that differ: roots (content = the folder the XBE was started from, `D:`; user
  data = `<that folder>\userdata`), a fixed 640x480 / 1280x720 screen with a title-safe rectangle instead of a window
  (`SdlRenderer::setFixedScreen`, `Renderer::setSafeArea`, an Xbox-only "Screen margin" option row), `platformLaunchArgs()` from
  `D:\args.txt` (test scripts in the emulator), a flushed log file, `platformFileName()` (FATX-safe folder names), the dashboard
  exit, no 3D views (`viewSupported()`), and libc replacements for what nxdk's C library lacks (`%f` in printf among them).
  Everything is in docs/XBOX_OG.md.

## Still missing

Android (native SDL3 APK): built and run on an emulator, see docs/ANDROID.md. Still missing there: a device-reset handler for SDL's 2D textures;
safe-area / notch insets; an icon
and release build.

Windows desktop (SDL3, MSVC): done, see docs/WINDOWS.md. Hardware 3D is Direct3D 11, verified on WARP only (the build VM has no
GPU); unverified: sound (no audio device there); a real PC.

UWP (SDL2 2.32, Xbox Dev Mode): done as far as a PC can show, see docs/XBOX.md: builds and runs as a UWP app on Windows 11
(package, install, launcher, battles, software and Direct3D 11 3D, suspend / Continue; the Windows App Certification Kit passes); never run
on a console yet. Hardware 3D is Direct3D 11 on SDL2's device ("Direct3D 11 3D on UWP" below), run on WARP only. Still
missing: a run on a real Xbox (Device Portal install, the 1 GB budget, B and a real controller, the TV fit, a real GPU),
D3D11 device removal handling for SDL's 2D textures (same gap as the other platforms, "Not handled" above), a decision
on B on the launcher, final names and logos.

Original Xbox (nxdk, SDL2 2.0.9, 2D only): built and run in xemu only, see docs/XBOX_OG.md: launcher, the three demos, saves, Continue,
exit to the dashboard, 4:3 / 16:9 and 480 / 720p. Never run on a real console; sound, a real controller and a real TV unverified; 3D
off until a native NV2A backend exists.

Non-portable code noticed: the tests are desktop-only and reach `mkdir`/`rmdir`/`chmod` through `tests/posix_compat.h`
(MSVC shims). `saves.cpp` / `progress.cpp` use `localtime_r` / `gmtime_r` behind `_WIN32` (fine). No `<filesystem>`, `dirent`
or `FindFirstFile` outside `engine/platform/` (inside it, see the UWP note above). The editor helper (`ote-editor`, desktop tool)
uses `<filesystem>` and `std::ifstream` with UTF-8 path strings, which on Windows are read in the ANSI code page: project
folders with non-ASCII names do not work there yet.

## Direct3D 11 3D on UWP

Done (`platform/sdl2/sdl2_d3d11_3d.cpp`; details in docs/RENDER3D.md "Direct3D 11 backend" and docs/XBOX.md "3D"). The
backend in `engine/gfx3d/d3d11/` is the desktop's; only the glue is SDL2's:

- **Host**: SDL's device (`SDL_RenderGetD3D11Device`, which SDL 2.32 builds for desktop Windows only: `tools/uwp/build.ps1`
  turns it on for WinRT with two small source edits). `enter()` queues one invisible point (so SDL binds its target and
  viewport), calls `SDL_RenderFlush` and saves the pipeline state with `D3D11StateBackup`; `leave()` puts it back
  exactly (SDL2 does not forget its cached state on a flush).
- **Picture**: one quad onto the render target SDL has bound (its back buffer; SDL2 exposes no swap chain), at the
  frame's rectangle mapped through SDL's bound viewport and `SDL_RenderGetScale`. No read-back. Read-back stays the
  fallback for an own device (`OTE_3D_D3D11_OWN=1`) or a texture render target.
- **Device loss**: `ResilientRenderer3D` with a keeper like the SDL3 glue's: lost = removed, simulated, or SDL's device
  pointer changed (SDL2 makes a new one and sends `SDL_RENDER_DEVICE_RESET`).
- **Suspend**: SDL 2.32's WinRT layer trims its own (shared) device in its suspend task; the glue trims only an own
  device (`ClearState` + `IDXGIDevice3::Trim` on `SDL_APP_WILLENTERBACKGROUND`).

Feature level: 10_0 or better is accepted (an Xbox One gives UWP apps 10_1, Series 11_0+); SDL2 itself may make a 9_x
device on old hardware, which the backend refuses with a log line (then software). No shader is compiled at run time.
The SDL2 backend's `OTE_SDL2_UPLOAD_ARGB8888` matters only for the software 3D picture and read-back mode.
