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
  the `#ifdef __ANDROID__` branches in `engine/platform/common/roots.cpp`, `sdl_app.cpp` (hints, fullscreen) and `sdl_gl3d.cpp`.
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
  Cancel does nothing, so back does not leave the app there (Home does).
- Fullscreen (immersive); Android shows its one-time "Viewing full screen" notice on first launch.
- Home / return: `background` and `foreground` reach the app; a battle in progress writes its Continue save on Home.

## Known not to work yet

- **3D**: the hardware 3D backend needs a second hidden GL window, which SDL on Android does not have, so only the software
  3D renderer is used (slow on a phone-sized 3D picture; not measured on a real device).
- Device reset: GL context loss (rotation is handled by `configChanges`, but memory pressure can still lose textures) is not
  handled (docs/PORTING.md "Not handled").
- Safe-area / notch insets, an app icon, a release build, AAB, and a way to leave the app from the launcher with back.
- Audio: SDL3 opens its device and the game's tracks load; nothing was listened to (the emulator ran without sound).
- Only tested on an x86_64 emulator (API 36, software GL). arm64 libraries build but have not run.
