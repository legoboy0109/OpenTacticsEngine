// File-system helpers of the SDL2 backend (see platform.h "Files").
// SDL2 has no directory API, so listing uses the OS directly: POSIX
// (Linux, macOS, Emscripten's virtual FS) or the Win32 find calls. Any other
// platform (console ports) returns false; the player then relies on
// games/index.json to know which games exist.
#include <SDL.h>

#include <cstdio>
#include <string>
#include <vector>

#include "core/base/log.h"
#include "platform/platform.h"

#if defined(_WIN32)
#include <windows.h>
#elif defined(__unix__) || defined(__APPLE__) || defined(__EMSCRIPTEN__)
#include <dirent.h>
#include <sys/stat.h>
#define OTE_POSIX_DIRS 1
#endif

namespace ote {

std::string userDataDir() {
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
}

bool listSubdirs(const std::string& path, std::vector<std::string>& names) {
  names.clear();
#if defined(_WIN32)
  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA((path + "/*").c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) return false;
  do {
    std::string name = fd.cFileName;
    if (name.empty() || name[0] == '.') continue;  // ".", "..", hidden
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) names.push_back(name);
  } while (FindNextFileA(h, &fd));
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
  if (CreateDirectoryA(path.c_str(), nullptr)) return true;
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
  if (std::remove(path.c_str()) == 0) return true;
  FILE* f = std::fopen(path.c_str(), "rb");  // failed because it was not there?
  if (!f) return true;
  std::fclose(f);
  return false;
}

}  // namespace ote
