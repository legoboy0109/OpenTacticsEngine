// memoryStatus() and platformLaunchArgs(): see platform.h. Shared by every backend.
#include <cstdio>
#include <string>
#include <vector>

#include "platform/platform.h"

#if defined(OTE_UWP)
#include "platform/common/uwp.h"
#endif
#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#define PSAPI_VERSION 2  // GetProcessMemoryInfo from kernel32 (K32...), the form UWP has too: no psapi.lib
#include <psapi.h>
#elif defined(__linux__)
#include <unistd.h>
#endif

namespace ote {

std::string memoryStatus() {
  char buf[96];
#if defined(_WIN32)
  PROCESS_MEMORY_COUNTERS pmc;
  if (!GetProcessMemoryInfo(GetCurrentProcess(), &pmc, sizeof(pmc))) pmc.WorkingSetSize = 0;
  std::string out;
  if (pmc.WorkingSetSize) {
    std::snprintf(buf, sizeof(buf), "working set %.0f MB", (double)pmc.WorkingSetSize / (1024.0 * 1024.0));
    out = buf;
  }
#if defined(OTE_UWP)
  std::string app = uwpMemoryStatus();  // the app's budget (about 1 GB on Xbox for an app, not a game)
  if (!app.empty()) out += (out.empty() ? "" : ", ") + app;
#endif
  return out;
#elif defined(__linux__)
  // /proc/self/statm: total pages, resident pages, ...
  std::FILE* f = std::fopen("/proc/self/statm", "r");
  if (!f) return "";
  long total = 0, resident = 0;
  int got = std::fscanf(f, "%ld %ld", &total, &resident);
  std::fclose(f);
  if (got != 2) return "";
  std::snprintf(buf, sizeof(buf), "resident %.0f MB", (double)resident * (double)sysconf(_SC_PAGESIZE) / (1024.0 * 1024.0));
  return buf;
#else
  (void)buf;
  return "";
#endif
}

std::vector<std::string> platformLaunchArgs() {
#if defined(OTE_UWP)
  return uwpLaunchArgs();
#else
  return std::vector<std::string>();
#endif
}

}  // namespace ote
