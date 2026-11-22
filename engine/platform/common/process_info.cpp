// memoryStatus() and platformLaunchArgs(): see platform.h. Shared by every backend.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "platform/platform.h"

#if defined(OTE_XBOX360)
#include "platform/common/launch_args.h"
#endif

#if defined(OTE_NXDK)
#include "core/base/fatx_names.h"
#include "platform/nxdk/nxdk.h"
#endif

#if defined(OTE_UWP)
#include "platform/common/uwp.h"
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#define PSAPI_VERSION 2  // GetProcessMemoryInfo from kernel32 (K32...), the form UWP has: no psapi.lib
#include <psapi.h>
#endif

namespace ote {

unsigned artBudgetMB() {
  const char* env = std::getenv("OTE_ART_BUDGET_MB");
  if (env && *env) {
    char* end = nullptr;
    const long v = std::strtol(env, &end, 10);
    if (end && *end == 0 && v >= 1 && v <= 1000000) return (unsigned)v;
  }
#if defined(__EMSCRIPTEN__)
  return 384;  // the heap stops at 2 GB, and the page has more to hold than art
#elif defined(OTE_NXDK)
  return 20;  // original Xbox: 64 MB for everything and the video; the program, Lua, game data and the screen take about
              // 10 to 15 MB (measured in xemu), so 20 MB of art leaves about 25 MB spare. docs/XBOX_OG.md "Memory"
#elif defined(OTE_XBOX360)
  return 128;  // 512 MB shared by the program, the video buffers and the system; kept modest until measured on a console
#elif defined(OTE_UWP)
  return 256;  // an app has about 1 GB on Xbox
#elif defined(__ANDROID__)
  return 192;  // phones: the app is killed when the system runs short
#else
  return 768;
#endif
}

std::string memoryStatus() {
#if defined(OTE_NXDK)
  return nxdkMemoryStatus();  // 64 MB shared with video: worth a line in the log
#elif defined(OTE_UWP)
  // Only where the number matters (a hard budget): everywhere else an always-different line in the log would break the
  // tests that compare whole logs of two runs.
  std::string out;
  PROCESS_MEMORY_COUNTERS pmc;
  if (GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) {
    char buf[64];
    std::snprintf(buf, sizeof(buf), "working set %.0f MB", (double)pmc.WorkingSetSize / (1024.0 * 1024.0));
    out = buf;
  }
  std::string app = uwpMemoryStatus();  // the app's budget (about 1 GB on Xbox for an app, not a game)
  if (!app.empty()) out += (out.empty() ? "" : ", ") + app;
  return out;
#else
  return "";
#endif
}

#if defined(OTE_XBOX360)
namespace {
// args.txt beside the .xex (game:\), the way the original Xbox and UWP ports take a command line: the dashboard starts a
// .xex with none. NAME=value words go into the environment (OTE_LOG_LEVEL ...).
std::vector<std::string> xbox360LaunchArgs() {
  std::vector<std::string> out;
  const std::string dir = executableDir();
  FILE* f = std::fopen((dir + "/args.txt").c_str(), "rb");
  if (!f) return out;
  std::string text;
  char buf[512];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) text.append(buf, n);
  std::fclose(f);
  std::vector<std::pair<std::string, std::string>> env;
  out = parseLaunchArgs(text, env);
  for (const auto& e : env) setenv(e.first.c_str(), e.second.c_str(), 1);
  return out;
}
}  // namespace
#endif

std::vector<std::string> platformLaunchArgs() {
#if defined(OTE_XBOX360)
  return xbox360LaunchArgs();
#elif defined(OTE_NXDK)
  return nxdkLaunchArgs();
#elif defined(OTE_UWP)
  return uwpLaunchArgs();
#else
  return std::vector<std::string>();
#endif
}

LauncherBack launcherBack() {
  if (const char* o = std::getenv("OTE_LAUNCHER_BACK")) {
    if (!std::strcmp(o, "nothing")) return LauncherBack::Nothing;
    if (!std::strcmp(o, "exit")) return LauncherBack::Exit;
    if (!std::strcmp(o, "confirm")) return LauncherBack::ConfirmExit;
  }
#if defined(__ANDROID__)
  return LauncherBack::Exit;  // owner's decision: back leaves the app, as users expect on Android
#elif defined(OTE_UWP) || defined(OTE_NXDK) || defined(OTE_XBOX360)
  // owner's decision: B on the Xbox launcher exits after a Yes / No question (UWP: ends the app; original Xbox and 360: back to the dashboard)
  return LauncherBack::ConfirmExit;
#else
  return LauncherBack::Nothing;
#endif
}

std::string platformFileName(const std::string& name) {
#if defined(OTE_NXDK)
  return fatxSafeName(name);  // the Xbox hard disk accepts only a small set of names (core/base/fatx_names.h)
#else
  return name;
#endif
}

InputDevice naturalInputDevice() {
  if (const char* o = std::getenv("OTE_INPUT_DEVICE")) {
    if (!std::strcmp(o, "keyboard")) return InputDevice::Keyboard;
    if (!std::strcmp(o, "controller")) return InputDevice::Controller;
    if (!std::strcmp(o, "touch")) return InputDevice::Touch;
    if (!std::strcmp(o, "mouse")) return InputDevice::Mouse;
  }
#if defined(OTE_UWP) || defined(OTE_NXDK)
  return InputDevice::Controller;  // consoles: a pad is what is in the hands
#elif defined(__ANDROID__)
  return InputDevice::Touch;  // a phone; the first pad button switches to Controller
#else
  return InputDevice::Keyboard;
#endif
}

}  // namespace ote
