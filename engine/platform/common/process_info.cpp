// memoryStatus() and platformLaunchArgs(): see platform.h. Shared by every backend.
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "platform/platform.h"

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

std::vector<std::string> platformLaunchArgs() {
#if defined(OTE_NXDK)
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
#elif defined(OTE_UWP) || defined(OTE_NXDK)
  // owner's decision: B on the Xbox launcher exits after a Yes / No question (UWP: ends the app; original Xbox: back to the dashboard)
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

}  // namespace ote
