// Original Xbox (nxdk) pieces of the platform layer, docs/XBOX_OG.md. Only built when OTE_NXDK is defined (tools/xbox-og/);
// called from platform/ files and from the SDL2 backend's main loop, never from the player or the core.
#pragma once
#include <string>
#include <vector>

namespace ote {

class Renderer;

// The folder the XBE was started from, as nxdk's D: drive: content (games/, assets/) is read from here and user data
// goes to a subfolder, so the app works from any folder or partition it is copied to. "D:" (no trailing separator).
std::string nxdkAppDir();

// Where settings, saves and the log are written, decided once: `D:/userdata` when the launch folder is writable (a
// hard-disk install), else `E:/Apps/OpenTacticsEngine/userdata` (a read-only disc, or the folder is on a partition that
// refuses writes). Empty if neither works (the player then runs without saving).
std::string nxdkUserDataDir();
bool nxdkUserDataIsFallback();

// Called first thing in main(): decides the user data folder and sends the log to <user data>/log.txt (the previous
// run's file becomes log-previous.txt), flushed after every line so it survives a crash.
void nxdkInstallLog();

// D:\args.txt, then <user data>\args.txt (the second wins): words are player arguments, NAME=value words go to the
// environment (parseLaunchArgs). Debug / test aid; empty when there are no such files.
std::vector<std::string> nxdkLaunchArgs();

// "used 12 MB of 64 MB" (kernel page counts).
std::string nxdkMemoryStatus();

// What the TV gets. Chosen by nxdkPrepareVideo() before SDL starts.
struct NxdkScreen {
  int w = 640, h = 480;
  bool widescreen = false;   // the dashboard's 16:9 setting
  bool anamorphic = false;   // a 480 mode shown as 16:9: pixels are 4/3 wider than high
  bool highDefinition = false;
};
// Lists the console's video modes, picks one (OTE_VIDEO=auto|480|720p; video_pick.h) and sets it. Logs what it saw.
NxdkScreen nxdkPrepareVideo();
const NxdkScreen& nxdkScreen();

// Moves `from` over `to`, replacing it (FATX rename does not replace by itself). True on success.
bool nxdkReplaceFile(const std::string& from, const std::string& to);

// Leaves the player for the console's dashboard (XLaunchXBEEx with no path: a quick reboot into the BIOS, which starts the
// dashboard). Does not return.
[[noreturn]] void nxdkReturnToDashboard();

// The newest log lines, oldest first (up to `max`; the log keeps its last 12), for the overlay.
std::vector<std::string> nxdkRecentLog(int max);

// OTE_XBOX_OVERLAY=1: the last log lines, frame time and memory drawn over the picture, once per frame, in logical pixels.
// A no-op unless that is set. Call after the app has drawn and before present.
void nxdkDrawOverlay(Renderer& r);

}  // namespace ote
