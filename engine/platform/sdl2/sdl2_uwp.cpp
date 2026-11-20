// UWP: LocalState folder, log file, launch arguments (see platform/common/uwp.h). The rest of the UWP pieces
// (view setup, memory budget) are C++/CX in sdl2_uwp_view.cpp.
#ifdef OTE_UWP
#include <SDL.h>
#include <SDL_system.h>

#include <io.h>
#include <share.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "core/base/fileio.h"
#include "core/base/log.h"
#include "platform/common/launch_args.h"
#include "platform/common/uwp.h"
#include "platform/platform.h"

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

namespace ote {
namespace {
std::FILE* g_log = nullptr;

void fileSink(LogLevel level, const char* message) {
  static const char* names[] = {"debug", "info", "warn", "ERROR"};
  char line[1200];
  std::snprintf(line, sizeof(line), "[%8.3f] [%s] %s\n", SDL_GetTicks() / 1000.0, names[(int)level], message);
  if (g_log) {
    std::fputs(line, g_log);
    std::fflush(g_log);  // the process can be suspended and never resumed: nothing may stay in a buffer
  }
  OutputDebugStringA(line);  // visible in a debugger / DebugView
}
}  // namespace

std::string uwpLocalStateDir() {
  // SDL_GetPrefPath would add "<org>/<app>" below LocalState; the Device Portal file explorer and the docs are simpler
  // when the files are directly in LocalState.
  const char* p = SDL_WinRTGetFSPathUTF8(SDL_WINRT_PATH_LOCAL_FOLDER);
  if (!p) return "";
  std::string dir = p;
  for (char& c : dir)
    if (c == '\\') c = '/';
  while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
  return dir;
}

void uwpInstallLog() {
  std::string dir = uwpLocalStateDir();
  if (dir.empty()) return;
  std::string path = dir + "/log.txt";
  replaceFile(path, dir + "/log-previous.txt");  // false when there was no previous log: fine
  g_log = _wfsopen(utf8ToWide(path).c_str(), L"wb", _SH_DENYNO);  // others may read it while the app runs
  if (!g_log) return;
  setLogSink(fileSink);
  // Anything printed with fprintf(stderr / stdout) (usage errors in main) goes to the same file: the standard handles of
  // a UWP app point nowhere, so redirecting them takes nothing away. dup2 onto the open file (one handle).
  _dup2(_fileno(g_log), _fileno(stderr));
  _dup2(_fileno(g_log), _fileno(stdout));
}

std::vector<std::string> uwpLaunchArgs() {
  std::vector<std::string> out;
  std::string dir = uwpLocalStateDir();
  if (dir.empty()) return out;
  std::FILE* f = openFile(dir + "/args.txt", "rb");
  if (!f) return out;
  std::string text;
  char buf[512];
  size_t n;
  while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) text.append(buf, n);
  std::fclose(f);
  // Words separated by spaces or line ends; NAME=value words (OTE_LOG_LEVEL=debug) are environment variables for the
  // process, everything else is a player argument (platform/common/launch_args.h, shared with the original Xbox build).
  std::vector<std::pair<std::string, std::string>> env;
  out = parseLaunchArgs(text, env);
  for (const auto& e : env) _putenv_s(e.first.c_str(), e.second.c_str());
  return out;
}

}  // namespace ote
#endif  // OTE_UWP
