# Windows (desktop)

The player for 64-bit Windows 10/11, built with Microsoft's compiler (Visual Studio 2022 Build Tools, MSVC 14.44, x64) on
the SDL3 backend. It is the first MSVC build of the engine and the base for the later UWP / Xbox port (docs/PORTING.md).
First built and tested in a Windows 11 VM (no GPU, no sound card) in November 2026.

## Run it

Unpack `OpenTacticsEngine-windows-x64.zip` anywhere and double-click `ote-player.exe`. Nothing to install (the C++ runtime is
linked statically). The zip holds the player, `SDL3.dll`, `assets\`, `games\` (the three demo games and `index.json`),
`licenses\` and a short `README.txt`. How it is made: "Packaging" below.

## Decisions

| Topic | What it does | Why |
|---|---|---|
| Content root | The folder of `ote-player.exe` when it has a `games\` folder next to it, else the working directory (as on Linux) | An unpacked zip runs from anywhere, by double-click or from a shortcut. `--content-root`, `OTE_CONTENT_ROOT` and `--games` / `--assets` still win (docs/PORTING.md "Roots") |
| User folder | `%APPDATA%\OpenTacticsEngine\player\` (what `SDL_GetPrefPath("OpenTacticsEngine", "player")` gives), holding `settings.json` and `saves\` | Same call and same layout as Linux (`~/.local/share/OpenTacticsEngine/player/`). Roaming profile, so it follows the user on a domain; saves are small |
| Console | `ote-player.exe` is a GUI-subsystem program (CMake `WIN32_EXECUTABLE`, SDL's `WinMain` calls `main`): a double-click opens no console window. Started from `cmd` / PowerShell without redirection, `attachParentConsole()` (platform/common/console.cpp) attaches to that terminal, so `--help` and the log lines print there. Redirected to a file or pipe (the tests) the handles already work and nothing is attached | Both wishes at once. From a terminal the prompt returns at once while the game runs: use `start /wait ote-player.exe ...` to wait for it |
| File paths | All engine paths are UTF-8. `openFile()` (core/base/fileio.h) opens through `_wfopen` on Windows; SDL3's folder/rename/delete calls are UTF-8 already; the SDL2 backend uses the `W` calls | A user name or game folder with non-ASCII letters must work; `fopen` would read the ANSI code page |
| Replacing saves | `replaceFile()` (SDL3: `SDL_RenamePath`, replaces an existing file) instead of `std::rename`, which refuses to replace on Windows | Saves are written to `x.tmp` and moved over `x` |
| C++ runtime | Static, `/MT` (`OTE_MSVC_STATIC_RUNTIME=ON`, the default) | The zip needs no Visual C++ redistributable on a clean PC. `-DOTE_MSVC_STATIC_RUNTIME=OFF` gives `/MD` for every target; UWP must use that. SDL3.dll has its own runtime use; no CRT objects (memory, FILE*) cross that boundary |
| Flags | `/W4 /permissive- /utf-8 /EHsc /GR-` | `/GR-`: nothing in the engine or the editor uses RTTI (all of it builds). `/EHsc` on every target: the engine throws nothing, but the STL and `ote-editor` (exceptions on purpose) need it declared |
| 3D | The software 3D drawer is the default and what the tests use; hardware OpenGL is requested through SDL's hidden window as on Linux | See "Not verified" |

## Build

Needs Visual Studio 2022 or its Build Tools with the "Desktop development with C++" workload (MSVC v143, CMake and Ninja come
with it), and the SDL3 development package for Visual C++ of the same version as the Linux builds
(`SDL3-devel-3.4.20-VC.zip` from github.com/libsdl-org/SDL/releases, unpacked anywhere). Python 3 for the tests.

```
:: x64 Native Tools prompt (or: call "...\VC\Auxiliary\Build\vcvars64.bat")
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DSDL3_DIR=C:\path\to\SDL3-3.4.20\cmake
cmake --build build
```

That builds `ote-player.exe` (with `SDL3.dll` copied beside it), `ote-sim.exe`, `ote_tests.exe` and `ote-editor.exe` in `build\`.
A clean build has no warnings at `/W4` in our code; third-party C (`stb_vorbis`, the 8x8 font, `stb_truetype`) is compiled with `/w`, which
MSVC answers with the harmless note `D9025: overriding '/W4' with '/w'`.

Tests, from the same prompt, in `build\`:

```
ote_tests.exe
ctest -E audio_player -j4 --output-on-failure
```

`audio_player` plays through the real sound device and is never run by the agents. Besides Python, the Python tests need
Pillow (`pip install pillow`) and `ffmpeg` on `PATH` (`sound_player`, `starter_audio` encode Ogg Vorbis with it). Set `TEMP` to a
folder you do not mind filling. The suite is far slower than on Linux (`ote_tests.exe` 3 to 4 minutes against 4 seconds, the whole `ctest` about
10 minutes against 6 on Linux), probably Windows Defender scanning the many small files the tests create (not measured);
excluding the build and temp folders may help.

## Packaging

```
powershell -ExecutionPolicy Bypass -File tools\windows\package.ps1 -Sdl3Dir C:\path\to\SDL3-3.4.20
```

Builds `ote-player` in Release in `build-windows-release\` and writes `build-windows-release\package\OpenTacticsEngine-windows-x64.zip`
(and the folder it was made from). Contents: `ote-player.exe`, `SDL3.dll`, `assets\`, `games\index.json` and the games listed there
(`demo-srpg`, `demo-economy`, `demo-hex`: the script refuses to run if `games\index.json` and its own list disagree, so a private game
can never slip in), `licenses\` (GPL-3.0, SDL zlib, Lua MIT, notices for stb / font8x8 / the OFL fonts, the demo credits),
`README.txt`. `-DynamicRuntime` makes the `/MD` variant under another name. The zip is not committed. No installer, no code signing,
no icon yet (Windows SmartScreen may warn about an unsigned download).

## Known gaps

- **Not verified: hardware 3D.** The VM has only Microsoft's basic display adapter. `--3d hardware` there gets Microsoft's old GL 1.1
  software driver, which lacks `glBlendFuncSeparate`, and the player logs `3D drawing: no hardware backend (the GL driver lacks
  glBlendFuncSeparate); using the software one` and draws the 3D test scene correctly in software. So the fallback is verified; the
  OpenGL drawer itself has never run on Windows. (In the VM that probe left a black window for several seconds before the first
  picture; real drivers should answer at once.)
- **Not verified: sound.** The VM has no audio device. The player logs `audio: no sound device: No default audio device available;
  the game runs without sound` and runs on, as designed. Nothing was heard; `audio_player` (the real-device test) was not run.
- **Not verified: a real PC** (window, DPI scaling, full screen, controller, display of mixed refresh rates).
- `ote-editor` builds and its API test passes (it needed two fixes: a busy port must be an error, which on Windows takes
  `SO_EXCLUSIVEADDRUSE`, and the test's default-folder check now expects `%APPDATA%`). Its file code (`std::ifstream`, `<filesystem>`)
  takes UTF-8 path strings that Windows reads in the ANSI code page: started on a `games` folder under a non-ASCII path
  (`...\éü-test\games`) it lists the projects but reads none of them (0 maps, problems reported). Not fixed: it is
  the desktop helper for the frozen web editor, it is not packaged, and the web editor UI (`editor\web`) was not part of this work.
- The default window is 3 times the 480x270 game picture, 1440x810 pixels: bigger than a 1024x768 or 1280x720 screen (it is placed
  partly off-screen there). `--scale N` changes it for one run, the same on Linux; a smarter default is a design question, not
  done here.
- The SDL2 backend has not been built with MSVC (only the SDL3 one); on Linux both build.
- The VM's clock was 7 hours behind (QEMU gave the guest local time while Windows reads UTC), which made files installed earlier look
  newer than new builds, so Ninja looped; setting the clock fixed it. Not an engine matter, but expect it after every VM start.

## What the UWP / Xbox port still needs on top

The compiler, flags, tests and file/path handling are shared; what is left is in docs/PORTING.md "Still missing": the package
project and manifest (SDL2 2.32 for UWP; the `-VC` SDL3 package is desktop-only), `/MD`, the UWP subset of the Win32 calls in
`sdl2_files.cpp`, the SDL2 backend under MSVC, content from the install folder with `userRoot()` = LocalState, the Xbox back-button
hint, Direct3D device-lost handling, and a run on a real Xbox in Dev Mode.
