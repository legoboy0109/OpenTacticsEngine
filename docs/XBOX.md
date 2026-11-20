# Xbox / UWP player

(This is the modern-console UWP port. The original 2001 Xbox, built with nxdk, is docs/XBOX_OG.md.)

The player as a UWP app (Universal Windows Platform), x64, for sideloading on an Xbox Series X|S or Xbox One in Developer
Mode, and for installing on any Windows 10/11 PC. What a port needs in general is in docs/PORTING.md; the Windows desktop
build it grew out of is in docs/WINDOWS.md; the previous port, with the same app-model decisions, is docs/ANDROID.md.

**Status: builds and runs as a UWP app on Windows 11; never run on a console yet.** Built, packaged, installed with
`Add-AppxPackage` and played in the Windows 11 build VM (no GPU, no sound card, so Direct3D 11 runs on WARP, Microsoft's
software renderer). The Windows App Certification Kit (the Store's own checker, including its "supported APIs" test)
passes with no warnings. Nothing here has touched an Xbox: "Not verified" at the end lists what that leaves open.

Hardware 3D is Direct3D 11 on the device of SDL's own 2D renderer (section "3D" below), with the software drawer as the
fallback; only ever run on WARP, Microsoft's software rasteriser, never on a graphics chip or a console. SDL3 has no UWP
support, so this build uses the SDL2 backend (`engine/platform/sdl2`) with SDL 2.32.10's own WinRT port.

## Build

In the Windows 11 build VM (or any Windows with Visual Studio 2022 Build Tools: MSVC v143, the "UWP" C++ build tools, the
Windows SDK 10.0.26100, `curl` and `tar`):

```
powershell -ExecutionPolicy Bypass -File tools\uwp\build.ps1          # work folder C:\uwp, from nothing
```

One command: 9 minutes from an empty `C:\uwp` in the VM (SDL2 about 3, the engine the rest), a couple of minutes for a rebuild
(`-SkipSdl` reuses the SDL2 build).
It: downloads the SDL2 2.32.10 release zip (checks the sha256 pinned in the script against the release's published digest),
turns on `SDL_RenderGetD3D11Device` for WinRT (two small edits to SDL's source, see "3D"), builds `SDL2.dll` with SDL's own `VisualC-WinRT\SDL-UWP.vcxproj` (MSBuild, retargeted to v143 and SDK 10.0.26100 on the
command line, `/MD`), configures the engine with CMake/Ninja, builds it, stages the package folder, makes the logos, makes
the test certificate if there is none, packs with `makeappx`, signs with `signtool`, and copies the dependency package and the
public certificate next to the result. Output, in `C:\uwp\out\`:

| File | What |
|---|---|
| `OpenTacticsPlayer_<version>_x64.appx` | the app, about 13.5 MB; version `0.1.MMdd.HHmm` of the build |
| `Microsoft.VCLibs.x64.14.00.appx` | the Visual C++ runtime for UWP apps (the framework package the app depends on), copied from the SDK |
| `ote-test.cer` | the public half of the test certificate, for PCs that need to trust the package (not a secret) |

Why this packaging. Two ways were possible: let CMake's Visual Studio generator make the app package project
(`CMAKE_SYSTEM_NAME=WindowsStore` with `-G "Visual Studio 17 2022"`), or build with the same Ninja setup as the desktop and
write the package by hand. The second is used: the VS generator needs a manifest and package project anyway, hides the
packaging steps in MSBuild targets, and cannot run the engine's other tools the same way; here the whole path is one
readable script, the build is the same CMake/Ninja one as everywhere else (`-DCMAKE_SYSTEM_NAME=WindowsStore`
`-DCMAKE_SYSTEM_VERSION=10.0 -DOTE_PLATFORM=sdl2 -DOTE_MSVC_STATIC_RUNTIME=OFF`), and a layout folder plus `makeappx` is all a
UWP package is. The manifest is `tools/uwp/AppxManifest.xml.in`.

What lands in the package: `ote-player.exe`, `SDL2.dll`, `assets\`, `games\index.json` and the games it lists (`demo-srpg`,
`demo-economy`, `demo-hex`; the script refuses to run when `games\index.json` and its own list disagree, and checks the
staged folder for `vision-quest` / `practice-`), `licenses\`, the logos. No private game can get in.

Linux does not build this target; on Linux the SDL2 backend still builds and tests as before (`-DOTE_PLATFORM=sdl2`).

## Install on a Windows 10/11 PC (to try it without a console)

```
powershell -ExecutionPolicy Bypass -File tools\uwp\install.ps1        # administrator, from the logged-in desktop
```

It turns on sideloading (`HKLM\...\AppModelUnlock`, what Settings > For developers > Developer Mode sets), trusts the test
certificate (`LocalMachine\TrustedPeople`: it can vouch for packages signed with it and nothing else) and runs
`Add-AppxPackage` (the VCLibs package is added only if the PC has none that is new enough, which Windows normally does).
Start it from the Start menu ("OpenTactics Player"). It has to run from the desktop session: Windows refuses to install
packages from a remote (ssh) session with `0x80070005, Failed to initialize PLM`. In the build VM this was done once, with
the registry switch above, on that VM's own Windows. A rebuild has a newer version number and updates in place, keeping the
saves; the same version number with other contents is refused by Windows.

## Install on an Xbox (Dev Mode), step by step

Not tried on a console: follow it, and tell me where it differs.

1. **One time: put the console in Developer Mode.** On the console install the Store app "Dev Mode Activation" and follow it;
   it needs a Microsoft developer account (Partner Center). **That registration can cost money** (a one-time fee for
   individuals; check the current price before you start). The console restarts into "Dev Home"; "Leave Dev Mode" there
   goes back to normal. Dev Mode is a separate mode: retail games do not run in it.
2. In Dev Home open the remote-access settings, **enable Windows Device Portal**, set a user name and password, and note the
   console's IP address that Dev Home shows.
3. On a PC on the same network open `https://<console-ip>:11443` (accept the certificate warning: the console made its own),
   log in.
4. Take these three files from the build (on the Linux side: `OpenTacticsEngine-WindowsVM/builds/uwp/`):
   `Microsoft.VCLibs.x64.14.00.appx`, `OpenTacticsPlayer_<version>_x64.appx` and `ote-test.cer`.
5. In the portal choose **My games & apps > Add** (Install app). Pick the `OpenTacticsPlayer_...appx`; in the same dialog add
   `Microsoft.VCLibs.x64.14.00.appx` as the dependency (the portal's "optional/dependent packages" field), then Install.
   If the portal refuses the package because of its signature, add `ote-test.cer` where the dialog offers a certificate
   field, or install the VCLibs package first on its own, then the app. (Unverified which of these the Xbox portal wants; the
   package is a normal test-signed `.appx`, the same kind Visual Studio's "remote machine" deploy sends.)
6. Start "OpenTactics Player" from **My games & apps** (it may be under "Apps"). The first start makes
   `LocalState\log.txt`.

An update: build again (new version number), upload the new `.appx` the same way; saves stay.

**Extra games.** The app lists the built-in demo games plus whatever is in its `LocalState\games\` folder (one folder per
game, each with a `game.json`, exactly as in `games\`). Put them there with the portal's file explorer: **File explorer >
LocalAppData > OpenTacticsEngine.Player_<id> > LocalState > games**; create the game's folder, upload its files into it
(subfolders as in the game). Restart the app. `LocalState\games` is created at the first start.

**Logs and saves** are in the same `LocalState` folder: `log.txt` (this run), `log-previous.txt`, `settings.json`, `saves\`.
Download `log.txt` from the file explorer when something is wrong.

**Debug launch.** A UWP app cannot be given a command line, so a file `LocalState\args.txt` stands in: its words are player
arguments (`--game demo-srpg --map bridge --style full-3d`), and a word like `OTE_LOG_LEVEL=debug` goes into the environment.
Delete the file to go back to the normal start. Debug level also logs the frame time every 120 frames.

## Decisions

| Topic | What it does | Why |
|---|---|---|
| Backend | SDL2 2.32.10 (WinRT port); SDL's `direct3d11` 2D renderer (asked for by name); 3D: Direct3D 11 on SDL's device, else software | SDL3 has no UWP support. SDL's WinRT port renders with Direct3D 11 only; no OpenGL on UWP |
| Runtime | `/MD`, the VCLibs framework package | UWP does not allow the static runtime for the Store; the package depends on `Microsoft.VCLibs.140.00` |
| Entry point | SDL's `SDL_winrt_main_NonXAML.cpp` (WinMain, `SDL_WinRTRunApp`, then the player's `main`) | The one place C++/CX (`/ZW`) is needed; the player's `main.cpp` is unchanged. Our own other C++/CX file is `sdl2_uwp_view.cpp` (view setup, memory budget). Both are compiled with `/ZW /GR /permissive` (C++/CX does not parse under `/permissive-`), everything else is plain C++ |
| Content | Read from the app's install folder (read-only: `ote-player.exe`'s folder holds `games\` and `assets\`, the same rule as the Windows zip). Nothing is copied: UWP can `fopen` the install folder, unlike APK assets | Android must extract; here no need |
| `userRoot()` | The app's `LocalState` folder itself (not `LocalState\OpenTacticsEngine\player` as `SDL_GetPrefPath` would give) | Easier to find in the Device Portal file explorer |
| `extraContentRoot()` | `LocalState\games` (made at start) | A place to put more games without repackaging |
| Log | `LocalState\log.txt` and `OutputDebugString`; stdout/stderr go to the same file | UWP has no console. `uwpInstallLog()` runs first thing in `main()` through `attachParentConsole()`, so no player code changed |
| Life cycle | SDL turns suspend, resume into `SDL_APP_WILLENTERBACKGROUND` / `DIDENTERFOREGROUND`, which the engine's existing `Lifecycle` events handle: the Continue save is written at the suspend notice, before the system freezes the process (UWP gives about 5 seconds; Xbox suspends when you go Home). There is no terminate notice on UWP: after a suspend the system may end the app silently, which is why the save is written at suspend | Same as Android (docs/PORTING.md "Life cycle") |
| Back / B | `SDL_HINT_WINRT_HANDLE_BACK_BUTTON` is set before SDL starts, so B (the system back request) reaches the game as `AC_BACK` = `Button::Cancel` and does not close the app. **On the launcher's first screen B asks "Exit the player?" (No selected; Yes ends the app)**: `launcherBack()` answers `ConfirmExit` on UWP (docs/PORTING.md "Input"). Yes calls the player's normal quit (settings flushed, the main loop ends, `main` returns, SDL's WinRT run loop returns and the process ends; `log.txt` ends with `quit requested after N frames`). B (or No) closes the question. For a test on a PC, `OTE_LAUNCHER_BACK=exit` or `=nothing` as a line in `LocalState\args.txt` changes it | The hint is the supported way; the owner chose "exit after a confirmation" for Xbox, immediately for Android, nothing on desktop |
| Controller | SDL2's WinRT joystick driver (Windows.Gaming.Input) with SDL's built-in game-controller mapping for such devices: d-pad / left stick move, A confirm, B cancel, Start menu, shoulders prev/next, X/Y turn the camera, right stick and triggers free camera. Every screen already works with a controller alone (docs/PORTING.md "Input") | Not redesigned. Not tested with a real pad: the VM has none |
| Display | `uwpPrepareView()`: the view uses the whole core window (no safe-area margin) and Xbox's 200% layout scaling is switched off, so the window is the screen's pixel size. The game picture is scaled to the window by `SDL_RenderSetLogicalSize`, keeping the aspect ratio, the same as a desktop window (480x270 into 1920x1080 is exactly 4x). On a PC both calls do nothing (`layout scaling disabled: no`) | The Xbox defaults would leave a black frame and render at 960x540 |
| Sound | If the system lists no audio output the player runs without sound instead of opening a device | SDL 2.32's WinRT audio crashes (in `ReleaseWasapiDevice`, SDL_wasapi.c) when asked to open a device on a machine with no sound endpoint: found in the VM. A console always has an endpoint |
| Texture format | `OTE_SDL2_UPLOAD_ARGB8888` is on | Direct3D 11 has no RGBA32 texture format (docs/PORTING.md "Texture format") |
| Lua | Built with `LUA_USE_C89` | Drops `popen` and `LoadLibrary`, which UWP does not have; game scripts never get `io`, `os` or `package` anyway |
| Manifest | Identity `OpenTacticsEngine.Player`, display name "OpenTactics Player", publisher `CN=OpenTacticsEngine Test`; `TargetDeviceFamily` `Windows.Universal`, min version 10.0.17763 (1809), tested 10.0.26100; **no capabilities** (no network, no libraries) | One package for PC and Xbox. See "Names" |
| Logos | `tools/uwp/make-logos.ps1`: a plain dark-blue tile with a small diamond of four floor tiles, drawn by the script | Placeholders; no borrowed art |
| Signing | A self-signed test certificate (`CN=OpenTacticsEngine Test`, code signing, 3 years) made by `build.ps1` on first run into `<work>\cert\` (`ote-test.pfx` and a generated password file). Never committed: `.gitignore` covers `*.pfx`, `*.cer`, `*.appx`, `*.msix`, `*.pem` | A test certificate only proves the package was not changed; it is not a Store signature |
| Memory | The player logs `memory: working set X MB, app memory X MB of limit Y MB` at start and after every scene change (`memoryStatus()`, `Windows.System.MemoryManager`; UWP only, so other platforms' logs do not change) | An Xbox UWP app (not a game) gets about 1 GB |

## Measured (Windows 11 VM, no GPU: Direct3D 11 on WARP, 800x600 window; software 3D)

Taken before the window-size rule (docs/WINDOWS.md "Decisions"): the app now asks for 960x540 on that 1280x800 screen.

- Start to launcher: 0.24 s on an ordinary start (the first start after an install took 8 s, the new files being scanned).
  Working set at the launcher 36 MB (app memory 12 MB); after loading the story demo's battle 79 MB (app memory 56 MB). The
  VM's limit is 12 GB, so this says nothing about Xbox's roughly 1 GB budget except that the player is far below it.
- 2D battle: work per frame 0.1 ms, vsync-limited (interval 17 to 19 ms, about 58 frames per second).
- Software 3D, full-3D view of the demo map: at the game's own 480x270 resolution, work per frame 3.1 ms (interval 17.2 ms,
  vsync); at "display" resolution (the 800x600 window) 6 to 7 ms. These are a desktop CPU in a VM; an Xbox's CPU cores differ.
- Suspend: minimising the window sent the background notice, and "Continue save written" followed 1.2 s later; the foreground
  notice came back when the window was restored. After killing the process and starting it again, the launcher offered
  Continue ("Skirmish at the Ford - Turn 1") and it restored the unit at its moved position.
- Windows App Certification Kit (`appcert.exe test -apptype windowsstoreapp`): 27 of 27 tests passed, including supported
  APIs, binary analysis, manifest and launch; run again on the package with Direct3D 11 3D and the edited SDL2
  (0.1.1119.1342): overall PASS. (The kit uninstalls the app when it cleans up: install it again afterwards.)

## 3D

The player's "3D graphics" option works as on the other platforms: Automatic and "OpenGL" (the label means "the graphics
chip") use Direct3D 11, "Software" the processor; `--3d` in `LocalState\args.txt` beats it. The log says which:
`3D drawing: Direct3D 11 (feature level 11_1; Microsoft Basic Render Driver, ...; drawn on SDL's render target; ...)`, or
`3D drawing: no hardware backend (Direct3D 11: ...); using the software one` then `3D drawing: software`.

How it works (`engine/platform/sdl2/sdl2_d3d11_3d.cpp`, docs/RENDER3D.md "Direct3D 11 backend"): the 3D backend draws with
the Direct3D 11 device SDL's 2D renderer already has, into a picture of its own, and puts that picture onto the render
target SDL is drawing to (its back buffer) as one quad, under the 2D things drawn afterwards: no second device and no copy
through memory. Around every use it lets SDL finish its queued drawing and puts SDL's pipeline state back exactly.

What SDL2 forced:

- **No device accessor on WinRT.** SDL 2.32 compiles `SDL_RenderGetD3D11Device` for desktop Windows only, although its
  WinRT renderer is the same Direct3D 11 one. `tools/uwp/build.ps1` widens that one `#if` in `SDL_render_d3d11.c` and
  declares the function for WinRT in `SDL_system.h` (marked "OpenTacticsEngine"), after the zip's hash check, and checks
  that the built `SDL2.dll` exports it. Without it the only way would be a second device and a copy through memory every
  frame (the "read-back" mode, still there as the fallback: `OTE_3D_D3D11_OWN=1`).
- **No swap chain.** SDL3 hands out its swap chain; SDL2 does not, so the picture goes onto the render target SDL has bound
  after the flush, at SDL's bound viewport (the letterbox) times `SDL_RenderGetScale`. SDL2's renderer binds both only when
  it draws, and the flip-model present unbinds the target, so the glue first queues one point of alpha 0 (blended: no pixel
  changes) to make SDL bind them.
- **Its own environment.** SDL2's WinRT `SDL_getenv` never sees the process environment, so the glue reads its debug
  switches with the C runtime's `getenv`, where `args.txt` puts them.
- **Suspend.** SDL 2.32 already calls `IDXGIDevice3::Trim` on its renderer's device when the app is suspended
  (`SDL_winrtapp_direct3d.cpp`, in its suspend task right after `SDL_APP_DIDENTERBACKGROUND`); that is the device the 3D
  drawing shares, so the glue does not trim it a second time and only logs `3D (Direct3D 11): suspending: the device is
  SDL's, which SDL trims now` (from the same task, just before SDL's Trim, which itself logs nothing). An own device is
  trimmed by the glue (`ClearState`, `Trim`) on `SDL_APP_WILLENTERBACKGROUND`: `suspending: own device trimmed`.
- **Device reset.** SDL2 makes itself a new device after a removal and sends `SDL_RENDER_DEVICE_RESET`; the 3D side
  notices the new device pointer (or the removal reason) and rebuilds through `ResilientRenderer3D`, or falls back to
  software. SDL's own 2D textures are not rebuilt after a real reset (docs/PORTING.md "Life cycle"), as on every platform.

Debug words for `args.txt`: `OTE_3D_D3D11_OWN=1` (own device, picture read back), `OTE_3D_D3D11_LEVEL=10_1` (with an own
device: cap the feature level, what an Xbox One gives), `OTE_3D_SIMULATE_LOSS=N`, `OTE_3D_SIMULATE_NO_REBUILD=1`.

Checked in the Windows 11 VM (no GPU: WARP, "Microsoft Basic Render Driver"), demo-srpg `bridge` in full 3D, by
screenshots of the app window compared pixel by pixel (differences = any channel off by more than 16; the units' idle
animation and the blinking cursor account for most of them, the shots being taken seconds apart):

| Run | Log | Picture |
|---|---|---|
| Direct3D 11 on SDL's device, 960x540 window | `feature level 11_1; ... drawn on SDL's render target` | 3D under the HUD and dialogue, right way up, colours as software: 0.36% of the 3D area differs from software 3D in the app, 0.14% from the desktop (SDL3) Direct3D 11 player |
| Same, window maximised (1280x720) | no warning | 0.026% differs from software 3D maximised |
| Own device capped to 10_1 | `feature level 10_1; ... own device, picture read back` | 0.24% from the shared-device picture |
| `OTE_3D_SIMULATE_LOSS=200` | `device was lost ...; rebuilding it`, `device rebuilt, 26 meshes and 3 textures uploaded again` | 0.12% from the first picture |
| Loss, rebuild refused | `cannot rebuild the device ...; using the software 3D renderer` | the software picture |
| Minimise (suspend) and back | `lifecycle: background`, `suspending: the device is SDL's, which SDL trims now`, `lifecycle: foreground` | drawn correctly after resume (0.18%); own device: `own device trimmed`, also correct after resume |

The 2D drawn after the 3D picture (HUD, dialogue text) stayed intact over 2,300 frames. Frame times on WARP (software
device, not representative): work 1.9 ms, interval 17.3 ms (vsync), at the game's resolution.

## One B press (found on the first real console run)

Symptom: one press of B acted as several presses of "back". Reading SDL 2.32.10's WinRT sources (`src/core/winrt/SDL_winrtapp_direct3d.cpp`,
`src/video/winrt/SDL_winrtkeyboard.cpp`, `src/joystick/windows/SDL_windows_gaming_input.c`):

- **System back request.** With `SDL_HINT_WINRT_HANDLE_BACK_BUTTON=1`, `WINRT_OnBackButtonPressed()` (called for
  `SystemNavigationManager::BackRequested`, which Xbox raises for B) sends `SDL_SendKeyboardKey` press and release of
  `SDL_SCANCODE_AC_BACK` at once. The pad's own B button still arrives as `SDL_CONTROLLERBUTTONDOWN` / `UP`. So B = two
  deliveries, both `Button::Cancel`. A, the d-pad, Start and the rest have no such second path: the system raises no other
  request for them.
- **Gamepad virtual keys.** `WINRT_TranslateKeycode()` only knows VirtualKey codes up to 172 (`WinRT_Official_Keycodes`) plus a
  few OEM codes; the gamepad keys (GamepadA ... 195 and up) fall through to `SDL_SCANCODE_UNKNOWN` (their native scan code is
  not in `windows_scancode_table`), and our `keyButton()` ignores unknown keys. So A / D-pad do not double through the keyboard
  (by reading; not seen on a console).
- **Pad listed several times.** The real console's log showed "gamepad connected: Xbox One Game Controller" three times within
  1 ms for one pad. `WGI_JoystickInit()` registers `RawGameControllerAdded` and then walks `RawGameControllers`, and both go
  through `InvokeAdded()`, which has no check for a controller it already knows (no identity compare at all); each call makes
  a new joystick instance with the same GUID. The backend opens every `SDL_CONTROLLERDEVICEADDED`, so each copy reports every
  button and every button of the pad (A, d-pad too) can arrive up to three times. (What exactly makes it three, not two, is
  not visible in the source; the log now prints device index, instance id and GUID per copy.) There is no way to tell from
  SDL's data that two copies are one physical pad (same GUID, no path, no serial).

Fix: `engine/platform/common/press_dedupe.{h,cpp}` (tests `tests/test_press_dedupe.cpp`), used by `SdlInput::translate()`. Each
event is tagged with its source: all keyboard keys, the system back request (AC_BACK), or one controller instance. When the
same logical button goes down from a second source while the first holds it, or within 400 ms of the first source's press, that
press and its release are dropped; the first line is logged once per run (`input: one press arrived from two sources ...`).
Keyboard-only use (one source, Escape + Backspace included) and one pad listed once are never touched, and neither is
direction auto-repeat (same source). Two real pads are merged into one input anyway (the game has one cursor), so merging
their duplicate presses costs nothing. The hint stays (without it B closes the app). Sticks and triggers are not de-duplicated:
they are level-based and copies report the same value.

## What the Windows Store API partition changed

`WINAPI_FAMILY=WINAPI_FAMILY_APP` makes the headers declare only the UWP part of the Windows API; anything else is a compile
error, and the app certification repeats it. The effect on the engine (all in `engine/platform`, with `OTE_UWP` defined by
CMake for the UWP target):

- `sdl2_files.cpp`: `CreateDirectoryW`, `DeleteFileW`, `MoveFileExW`, `GetFileAttributesW` and `FindFirstFileExW` are all in
  the UWP subset, so the existing Windows branch compiles and runs unchanged. `userDataDir()` returns LocalState.
- `console.cpp`: no console; `attachParentConsole()` installs the file log instead.
- `roots.cpp`: `extraContentRoot()` = `LocalState\games`; the content root is the install folder through the existing
  "`games\` next to the program" rule.
- `getenv`, `_putenv_s`, `_wfsopen`, `_dup2`, `K32GetProcessMemoryInfo` are in the allowed set; no change needed.
- New: `process_info.cpp` (`memoryStatus()`, `platformLaunchArgs()`), `uwp.h`, `sdl2_uwp.cpp`, `sdl2_uwp_view.cpp`.
- The SDL2 backend itself needed nothing beyond the hint, the view call and the no-audio-device guard; this was also the
  first time the SDL2 backend was built with MSVC (see docs/PORTING.md).
- Third party: Lua with `LUA_USE_C89` (above). Nothing else changed.

## Open questions for the owner

1. **Wording of the exit question.** B on the launcher asks "Exit the player?" with "Yes" / "No" (a placeholder, the
   words of the player's other Yes / No questions). Change it in `engine/player/scene_home.cpp`.
2. **Names.** Package identity (`OpenTacticsEngine.Player`), display name ("OpenTactics Player"), publisher
   (`CN=OpenTacticsEngine Test`) and the logos are placeholders. Changing the identity or the publisher later makes Windows
   see a different app (saves would not carry over), so decide before anyone relies on the saves. The Store, if ever,
   needs its own identity from Partner Center.
3. **Dev Mode registration** may cost money (step 1 above).

## Not verified

- Everything on a real Xbox: Dev Mode install through the Device Portal (and which signature route the portal wants), the
  1 GB budget, the 5-second suspend, B handling, a real controller through Windows.Gaming.Input, full-screen fit and the
  layout-scaling switch on a TV (on a PC it is a no-op), audio.
- Hardware 3D on a graphics chip: Direct3D 11 ran on WARP only (feature level 11_1, and 10_1 when capped). An Xbox's
  GPU, its real 10_1 (Xbox One) or 11_0+ (Series) device and its speed are untried.
- A device reset of Direct3D (docs/PORTING.md "Not handled").
- Sound on any machine (the VM has no audio device; the player ran without it).
- Windows on ARM or x86: x64 only.
