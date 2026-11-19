// File-system helpers of the SDL2 backend (see platform.h "Files").
// SDL2 has no directory API, so listing uses the OS directly: POSIX
// (Linux, macOS, Emscripten's virtual FS) or the Win32 find calls. Any other
// platform (console ports) returns false; the player then relies on
// games/index.json to know which games exist.
#include <SDL.h>

#include <cstdio>
#include <string>
#include <vector>

#include "core/base/fileio.h"
#include "core/base/log.h"
#include "platform/platform.h"
#ifdef OTE_UWP
#include "platform/common/uwp.h"
#endif

#if defined(_WIN32)
#include <windows.h>
#elif defined(__unix__) || defined(__APPLE__) || defined(__EMSCRIPTEN__)
#include <dirent.h>
#include <sys/stat.h>
#define OTE_POSIX_DIRS 1
#endif

namespace ote {

std::string userDataDir() {
#ifdef OTE_UWP
  return uwpLocalStateDir();  // LocalState itself (SDL_GetPrefPath would nest <org>/<app> below it)
#else
  char* p = SDL_GetPrefPath("OpenTacticsEngine", "player");  // created if missing, ends with a separator
  if (!p) {
    OTE_WARN("no user data folder: %s", SDL_GetError());
    return "";
  }
  std::string dir = p;
  SDL_free(p);
  for (char& c : dir)
    if (c == '\\') c = '/';
  while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
  return dir;
#endif
}

bool listSubdirs(const std::string& path, std::vector<std::string>& names) {
  names.clear();
#if defined(_WIN32)
  // The "W" calls with UTF-16 paths: the engine's paths are UTF-8 and the ANSI calls would misread non-ASCII names.
  // FindFirstFileExW (not FindFirstFileW) because it is also in the UWP API subset.
  WIN32_FIND_DATAW fd;
  HANDLE h = FindFirstFileExW(utf8ToWide(path + "/*").c_str(), FindExInfoBasic, &fd, FindExSearchNameMatch, nullptr, 0);
  if (h == INVALID_HANDLE_VALUE) return false;
  do {
    std::string name = wideToUtf8(fd.cFileName);
    if (name.empty() || name[0] == '.') continue;  // ".", "..", hidden
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) names.push_back(name);
  } while (FindNextFileW(h, &fd));
  FindClose(h);
  return true;
#elif defined(OTE_POSIX_DIRS)
  DIR* d = opendir(path.c_str());
  if (!d) return false;
  while (dirent* e = readdir(d)) {
    std::string name = e->d_name;
    if (name.empty() || name[0] == '.') continue;  // ".", "..", hidden
    struct stat st;
    if (stat((path + "/" + name).c_str(), &st) == 0 && S_ISDIR(st.st_mode)) names.push_back(name);
  }
  closedir(d);
  return true;
#else
  return false;
#endif
}

bool makeDir(const std::string& path) {
#if defined(_WIN32)
  if (CreateDirectoryW(utf8ToWide(path).c_str(), nullptr)) return true;
  return GetLastError() == ERROR_ALREADY_EXISTS;
#elif defined(OTE_POSIX_DIRS)
  if (mkdir(path.c_str(), 0755) == 0) return true;
  struct stat st;
  return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);  // already there
#else
  return false;
#endif
}

bool removeFile(const std::string& path) {
#if defined(_WIN32)
  if (DeleteFileW(utf8ToWide(path).c_str())) return true;
  return GetFileAttributesW(utf8ToWide(path).c_str()) == INVALID_FILE_ATTRIBUTES;  // failed because it was not there?
#else
  if (std::remove(path.c_str()) == 0) return true;
  FILE* f = std::fopen(path.c_str(), "rb");  // failed because it was not there?
  if (!f) return true;
  std::fclose(f);
  return false;
#endif
}

bool replaceFile(const std::string& from, const std::string& to) {
#if defined(_WIN32)
  return MoveFileExW(utf8ToWide(from).c_str(), utf8ToWide(to).c_str(), MOVEFILE_REPLACE_EXISTING) != 0;
#else
  return std::rename(from.c_str(), to.c_str()) == 0;  // replaces an existing target on POSIX
#endif
}

std::string executableDir() {
#if defined(__EMSCRIPTEN__) || defined(__ANDROID__)
  return "";
#else
  char* b = SDL_GetBasePath();  // allocated, ends with a separator; may be null
  if (!b) return "";
  std::string dir = b;
  SDL_free(b);
  for (char& c : dir)
    if (c == '\\') c = '/';
  while (dir.size() > 1 && dir.back() == '/') dir.pop_back();
  return dir;
#endif
}

}  // namespace ote
