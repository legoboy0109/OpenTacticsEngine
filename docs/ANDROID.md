# Android player

A debug APK of the SDL3 player. What a port needs in general is in docs/PORTING.md; this is the Android specifics.

## Build

```
tools/android/build_apk.sh        # prints the APK path and size (android/app/build/outputs/apk/debug/app-debug.apk)
```

The script sets `ANDROID_HOME` (default `~/Android/Sdk`) and `JAVA_HOME` (JDK 21 or 17; the default `java` may be 1.8), downloads
the pinned SDL source if missing (`tools/android/fetch_sdl.sh`), and runs `./gradlew :app:assembleDebug`. Needs: NDK 28.2,
platform 36, CMake 3.31.5 from the SDK, internet for Gradle's first run (Gradle 8.13, Android Gradle Plugin 8.11.1).
Debug signing only (Gradle's own debug keystore). There is no release build, keystore or signing setup; `*.keystore`,
`*.jks`, `local.properties` and all build outputs are git-ignored.

## Layout

- `android/` Gradle project: `app/build.gradle` (application id `org.opentacticsengine.player`, minSdk 24, targetSdk 36,
  ABIs `arm64-v8a` and `x86_64`), `AndroidManifest.xml` (SDL's `SDLActivity`, sensor-landscape, SDL's `configChanges`).
- **SDL** (3.4.20, same version as the desktop build) is not committed: `fetch_sdl.sh` downloads the official release tarball,
  checks its sha256 and unpacks it to `android/third_party/` (ignored). Gradle compiles SDL's own Java glue straight from
  that tree and CMake builds `libSDL3.so` from it (zlib licence, `LICENSE.txt` is in the tree).
- Native: Gradle's `externalNativeBuild` points at the repository's root `CMakeLists.txt` with tests and editor off. On Android
  `engine/player/CMakeLists.txt` makes the player a shared library `libmain.so` (the library SDLActivity loads) and builds SDL
  from `OTE_ANDROID_SDL_SOURCE`. Debug builds use `-O2` (the software 3D renderer is CPU-bound).
- Android code in the engine: `engine/platform/sdl3/sdl_android.cpp` (content extraction, extra games folder, logcat sink),
  the `#ifdef __ANDROID__` branches in `engine/platform/common/roots.cpp`, `sdl_app.cpp` (hints, fullscreen) and `sdl_gl3d.cpp`, `sdl_gl3d_android.cpp` (3D context), and the intent extras in `sdl_android.cpp` / `player/main.cpp`.
- App name "OpenTactics Player", application id and the missing launcher icon are placeholders: the owner decides.

## Content

APK assets cannot be read with `fopen` or listed. `stageContent` (in `app/build.gradle`) copies, at build time, from the
repository (no second copy is committed): `assets/`, `games/index.json` and the explicit allow-list `demo-srpg`,
`demo-economy`, `demo-hex` (edit `packagedGames`; private games are never packaged), and writes `ote-manifest.txt`: line 1
`version <versionCode>-<hash of all packaged bytes>`, then one path per line. On start the player
(`androidContentRoot()`) compares line 1 with `files/content/.ote-content-version`; if different (first run, new versionCode
or changed content) it reads each asset with `SDL_LoadFile` and writes it to `files/content.new`, then swaps it in
(about 150 ms for 233 files). `contentRoot()` = `<internal files>/content`, `userRoot()` = SDL's pref path (the internal
files dir: `settings.json`, `saves/`), `extraContentRoot()` = `Android/data/<id>/files/games` (created at start; put
`<game>/game.json` folders there, no permission needed).

## Behaviour

- Logs go to logcat, tag `OTE` (`adb logcat -s OTE`); SDL's own lines use tag `SDL`.
- Touch is SDL's synthesized left-button mouse; the launcher, game page and battle (select, move, menu) work by tap.
- Back is trapped (`SDL_HINT_ANDROID_TRAP_BACK_BUTTON=1`) and arrives as `AC_BACK` = Cancel. On the launcher's first screen
  back leaves the app at once (`launcherBack()` = `Exit`, platform.h; Xbox asks first, desktop does nothing).
- Fullscreen (immersive); Android shows its one-time "Viewing full screen" notice on first launch.
- Home / return: `background` and `foreground` reach the app; a battle in progress writes its Continue save on Home.

## 3D

The hardware 3D backend (docs/RENDER3D.md "OpenGL backend") runs unchanged. Its GL context is an EGL **pbuffer** context made
directly with EGL (`engine/platform/sdl3/sdl_gl3d_android.cpp`): `eglGetDisplay(EGL_DEFAULT_DISPLAY)` (the display SDL
already initialised; never `eglTerminate`d here), a config with `EGL_OPENGL_ES3_BIT` + `EGL_PBUFFER_BIT`, a 1 x 1 pbuffer,
an ES 3.0 context. `enter()` saves SDL's current display / surfaces / context with `eglGetCurrent*` and `leave()` restores
exactly that. GL function addresses come from `eglGetProcAddress`, then `dlsym` on `libGLESv3` (the library links `EGL` and
`GLESv3`). Everything runs on SDL's native thread, the one that draws the frame. Why not a second `SDL_GL_CreateContext` on
the main window: it would have to use the EGL config SDL chose for its 2D renderer and share SDL's own "current context"
bookkeeping; the pbuffer is independent of both.

- **If no ES 3.0 context can be made** the reason is logged (`OTE` tag, warning "no hardware backend") and the software
  renderer is used, as before. `--3d auto|software|hardware` and the "3D graphics" / "3D resolution" options behave as on the
  desktop (4x multisampling and display resolution work when the context allows).
- **Context loss.** A pbuffer context normally survives Home and return (checked on the emulator); a driver may still lose
  it (`EGL_CONTEXT_LOST`, GPU reset). `GlRenderer3D` has no "recreate everything" path, so on Android it is wrapped in a
  `ResilientRenderer3D` that keeps a CPU copy of every mesh and texture the game created (extra memory: the meshes and
  textures once more). When `eglMakeCurrent` fails the wrapper discards the backend, makes a new context and backend, uploads
  everything again (the game's mesh and texture ids stay valid) and repeats the frame. If a new context cannot be made, the
  same copies go to the software renderer for the rest of the session (logged). Nothing else in the player notices.
  Not covered: SDL's own 2D textures (see above). After a switch to software `caps()` changes (`fullResolution` goes
  false); only the player's own per-frame choices see that, nothing was seen to break in the emulator test.
- Debug hooks (environment, see "Debug launch"): `OTE_3D_SIMULATE_LOSS=N` destroys the 3D context after N frames;
  `OTE_3D_SIMULATE_NO_REBUILD=1` also refuses the rebuild (tests the software fallback).
- Emulator reading (API 36 x86_64, `-gpu swiftshader_indirect`): `GL_VERSION` "OpenGL ES 3.0 (OpenGL ES 3.0 SwiftShader
  4.0.0.1)", `GL_RENDERER` "Android Emulator OpenGL ES Translator (Google SwiftShader)", max samples 4, max texture and
  renderbuffer 8192. The log prints the same on every start (`3D drawing: OpenGL (...)`, `3D (EGL): pbuffer ES 3.0 context;
  max samples ...`).

## Debug launch

Player arguments cannot be typed on a phone, so the launching intent can carry two string extras (read once at start, so
`am force-stop` first; they only exist in the Android build and are meant for testing):

```
adb shell "am start -n org.opentacticsengine.player/org.libsdl.app.SDLActivity \
  --es ote_env 'OTE_LOG_LEVEL=debug' \
  --es ote_args '--game demo-srpg --map bridge --style full-3d --3d hardware --3d-res display'"
```

`ote_args` are ordinary `ote-player` arguments (split at spaces), `ote_env` is `NAME=value` pairs put into the environment
before anything reads it. With `OTE_LOG_LEVEL=debug` the player logs, every 120 frames, `frame time: work X ms, interval Y
ms`: *work* is input + update + drawing (before present and the vsync wait), *interval* the whole frame; vsync pins the
interval to 16.7 ms.

Measured on the emulator (1280 x 720, `-gpu swiftshader_indirect`, host CPU with KVM; indicative only, the emulator's GL is
a CPU renderer, a phone's will differ): full 3D battle on `demo-srpg` / `bridge`, work per frame:

| 3D resolution | software | hardware |
|---|---|---|
| game (480 x 270) | 3.5 ms | 5.7 ms |
| display | 16.2 ms | 10.4 ms |

Pictures were compared with a desktop headless screenshot of the same map and style (`--headless --style full-3d`): Android
software at game resolution differs from it by 0.06 / 255 on average (downscaled to 480 x 270), Android hardware at game
resolution by 0.19 / 255 (a few edge pixels), as the backends are meant to match.

## Known not to work yet

- Device reset of the **2D** side: SDL's own renderer textures (art sheets, text atlas, the 3D picture's texture) are not
  rebuilt if Android loses SDL's GL context (docs/PORTING.md "Not handled"); the 3D backend's own context is (see "3D").
- Safe-area / notch insets, an app icon, a release build, AAB.
- Audio: SDL3 opens its device and the game's tracks load; nothing was listened to (the emulator ran without sound).
- Only tested on an x86_64 emulator (API 36, SwiftShader GL). arm64 libraries build but have not run.
