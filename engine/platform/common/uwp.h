// UWP (Universal Windows Platform: Windows 10/11 and Xbox in Dev Mode) pieces of the platform layer. Only built when
// OTE_UWP is defined (docs/XBOX.md); the functions are declared here and called only from platform/ files, so the
// player and the core never see them. Backend: SDL2 (SDL3 has no UWP support).
#pragma once
#include <string>
#include <vector>

namespace ote {

// The app's LocalState folder (read-write, survives updates): UTF-8, forward slashes, no trailing separator.
// userRoot() is this folder itself (settings.json, saves/, log.txt); extraContentRoot() is LocalState/games.
std::string uwpLocalStateDir();

// Sends every log line to LocalState/log.txt (the previous run's file becomes log-previous.txt) and to
// OutputDebugString, and points stdout/stderr at the same file: a UWP app has no console. Called first thing in main().
void uwpInstallLog();

// Arguments for a packaged app that cannot be given a command line: LocalState/args.txt (see sdl2_uwp.cpp).
// Empty when the file is missing.
std::vector<std::string> uwpLaunchArgs();

// Asks the system for the whole screen: the app's view uses the full core window (no TV safe-area margin on Xbox) and
// the layout scaling Xbox applies (200%) is switched off, so the window is the screen's pixel size. C++/CX; its own
// file (sdl2_uwp_view.cpp) because only that file is compiled with /ZW. Call on the UI thread before the window is made.
void uwpPrepareView();

// "app memory X MB of limit Y MB" from Windows.System.MemoryManager (the app's real budget on Xbox).
std::string uwpMemoryStatus();

}  // namespace ote
