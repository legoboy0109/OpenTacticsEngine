// contentRoot() / extraContentRoot() / userRoot(): see platform.h "Where things live". Shared by the desktop
// backends. A port with different storage (Android app storage, UWP InstalledLocation/LocalState) replaces
// the defaults here, behind its own #if, and leaves every caller alone.
#include <cstdlib>
#include <vector>

#include "platform/common/android_roots.h"
#include "platform/platform.h"
#ifdef OTE_UWP
#include "platform/common/uwp.h"
#endif

namespace ote {
namespace {
std::string& contentOverride() { static std::string s; return s; }
std::string& extraOverride() { static std::string s; return s; }

std::string trimmed(std::string dir) {
  while (dir.size() > 1 && (dir.back() == '/' || dir.back() == '\\')) dir.pop_back();
  return dir;
}
std::string fromEnv(const char* name) {
  const char* v = std::getenv(name);
  return v ? v : "";
}
}  // namespace

void setContentRootOverride(const std::string& dir) { contentOverride() = dir; }
void setExtraContentRootOverride(const std::string& dir) { extraOverride() = dir; }

std::string contentRoot() {
  if (!contentOverride().empty()) return trimmed(contentOverride());
  std::string env = trimmed(fromEnv("OTE_CONTENT_ROOT"));
#ifdef __ANDROID__
  // APK assets cannot be opened with fopen: the first run extracts them to app storage (sdl_android.cpp).
  if (env.empty()) return androidContentRoot();
#endif
  if (!env.empty()) return env;
  // Desktop default (UWP too: the install folder is the program's folder and holds games/ and assets/, read-only):
  // a download unpacked anywhere runs by double-click, because games/ sits next to the program
  // (docs/WINDOWS.md). Otherwise empty = the working directory (a source checkout run from its root).
  std::string exe = executableDir();
  std::vector<std::string> names;
  if (!exe.empty() && listSubdirs(exe, names))
    for (const std::string& n : names)
      if (n == "games") return exe;
  return "";
}

std::string extraContentRoot() {
  if (!extraOverride().empty()) return trimmed(extraOverride());
  std::string env = trimmed(fromEnv("OTE_EXTRA_GAMES"));
#ifdef __ANDROID__
  if (env.empty()) return androidExtraGamesRoot();  // Android/data/<id>/files/games
#endif
#ifdef OTE_UWP
  // LocalState/games: the owner puts extra games here (Xbox Device Portal file explorer; docs/XBOX.md). Always made.
  if (env.empty()) {
    std::string dir = uwpLocalStateDir();
    if (dir.empty()) return "";
    dir += "/games";
    makeDir(dir);
    return dir;
  }
#endif
  return env;
}

std::string userRoot() { return userDataDir(); }

}  // namespace ote
