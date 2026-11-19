# Porting the player

What a new platform (Android, UWP / Xbox Dev Mode, a console SDK) has to provide, and what the shared groundwork
already does. The layer rules are in docs/ARCHITECTURE.md; everything here lives in `engine/platform/`.

## Roots (where files are)

`platform.h` "Where things live":

| Function | Meaning | Desktop today |
|---|---|---|
| `contentRoot()` | folder holding shipped, read-only `games/` and `assets/` | empty = the working directory |
| `extraContentRoot()` | optional second folder of games the user added (`<id>/game.json`) | none |
| `userRoot()` | folder the app may write (settings, saves, progress) | `userDataDir()`: `SDL_GetPrefPath("OpenTacticsEngine", "player")` |

Implemented in `platform/common/roots.cpp` (shared by both SDL backends). A port changes the answers there, behind its own
`#if`: Android returns the folder the APK's assets were extracted to on first run (SDL's `SDL_GetAndroidInternalStoragePath`;
APK assets cannot be read with `fopen`), UWP returns the install folder (`SDL_GetBasePath`) for content and
`SDL_GetPrefPath` (LocalState) for `userRoot()`. Nothing else in the player builds a content or write path:
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
the notice arrives on the UI thread while the main loop runs. Not registered on the web (no such events there).

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
`Sprites`, `IsoTiles`, `Show3D` and the `TextEngine` atlas, from a handler that does not exist yet.

Tests: the `--input` tokens `background`, `foreground`, `lowmemory`, `terminate` inject these notices
(`tests/lifecycle_player_test.py`).

## Input

The platform back key (`SDL_SCANCODE_AC_BACK`: Android back, and Xbox B on UWP when the app sets
`SDL_HINT_WINRT_HANDLE_BACK_BUTTON`) is `Button::Cancel` in both backends. On the launcher's first screen Cancel does
nothing, except on Android, where the platform layer leaves the app from there (the owner's decision, docs/ANDROID.md).

Every screen is operable with a controller alone (d-pad or left stick, A confirm, B cancel, Start = Menu, shoulders =
Prev/Next, X/Y = turn camera, right stick and triggers = free camera): all screens take their input as `Button`s; pointer
handlers are additions that call the same actions. No screen has text entry. Skipping a dialogue is "hold Cancel or Menu".

## Build

- **Entry point.** `engine/player/main.cpp` includes `platform/platform_main.h`, which pulls in `SDL_main.h` of the
  configured backend (CMake defines `OTE_PLATFORM_SDL3` / `OTE_PLATFORM_SDL2`). Nothing changes on Linux/macOS; on
  Windows, UWP and Android SDL renames `main`. SDL2 also links `SDL2::SDL2main` on WIN32/ANDROID. The web build skips it.
- **MSVC.** `if(MSVC)` in the root `CMakeLists.txt`: `/W4 /permissive- /utf-8 /EHsc /GR-`, `NOMINMAX`,
  `_CRT_SECURE_NO_WARNINGS`. Untested (no MSVC here); expect a first round of `/W4` warnings.
- **Texture format (SDL2).** `-DOTE_SDL2_UPLOAD_ARGB8888=ON` makes every SDL2 texture ARGB8888 and swizzles the bytes in the
  backend (`sdl2/sdl2_pixels.h`) in one tight loop, because UWP's Direct3D 11 renderer has no RGBA32 and SDL would convert
  on every upload (every frame for the 3D picture). Default OFF, pictures byte-identical either way (checked with headless
  screenshots). The conversion is in the backend, not at the 3D renderer's output: a software 3D drawer that wrote the
  native format directly would save the loop, but that changes `Renderer3D`'s contract and was left alone.

## Still missing

Android (native SDL3 APK): built and run on an emulator, see docs/ANDROID.md. Still missing there: a device-reset handler for SDL's 2D textures;
safe-area / notch insets; an icon
and release build.

UWP (SDL2 2.32, Xbox Dev Mode): the UWP project and manifest; content in the package, `userRoot()` = LocalState; the
back-button hint set before SDL init; D3D11 device removal handling; `sdl2_files.cpp`'s `_WIN32` branch (`listSubdirs`,
`makeDir`) calls `FindFirstFileA` / `CreateDirectoryA`, which the UWP API subset does not offer (it needs the `W`
variants / `FindFirstFileExW`, `CreateDirectory2`), and `_WIN32` is also defined on UWP, so it needs its own `#if` there;
gamepad-only operation was verified by reading the code and a scripted run, not on a device.

Non-portable code noticed (left alone): `tests/*.cpp` and `tests/platform_stub.cpp` use `<sys/stat.h>`/`mkdir` (tests are
desktop-only); `tools/sim/event_print.cpp` uses `__attribute__((format))` unguarded (the sim tool is not part of the ports;
`core/base/log.h` is guarded). `saves.cpp` / `progress.cpp` use `localtime_r` / `gmtime_r` behind `_WIN32` (fine). No
`<filesystem>`, `dirent` or `FindFirstFile` outside `engine/platform/` (inside it, see the UWP note above).
