// attachParentConsole(): see platform.h. The player is built for the Windows GUI subsystem (no console window on a
// double-click); this gives it its terminal back when it was started from one.
#include "platform/platform.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

#include <cstdio>
#endif

namespace ote {

#if defined(_WIN32) && defined(WINAPI_FAMILY) && defined(WINAPI_PARTITION_DESKTOP)
#if !WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
#define OTE_NO_CONSOLE_API 1  // UWP: there is no console
#endif
#endif

#if defined(_WIN32) && !defined(OTE_NO_CONSOLE_API)
namespace {
// A standard handle that points at something (a file, a pipe, a console) needs nothing from us.
bool hasHandle(DWORD which) {
  HANDLE h = GetStdHandle(which);
  return h != nullptr && h != INVALID_HANDLE_VALUE;
}
}  // namespace

void attachParentConsole() {
  bool needOut = !hasHandle(STD_OUTPUT_HANDLE);
  bool needErr = !hasHandle(STD_ERROR_HANDLE);
  if (!needOut && !needErr) return;  // redirected (the tests, `> log.txt`) or already attached
  // Double-click: there is no parent console, AttachConsole fails and the program stays windowless and silent.
  if (!AttachConsole(ATTACH_PARENT_PROCESS)) return;
  std::FILE* f = nullptr;
  if (needOut && freopen_s(&f, "CONOUT$", "w", stdout) == 0) setvbuf(stdout, nullptr, _IONBF, 0);
  if (needErr && freopen_s(&f, "CONOUT$", "w", stderr) == 0) setvbuf(stderr, nullptr, _IONBF, 0);
}
#else
void attachParentConsole() {}
#endif

}  // namespace ote
