// The few POSIX calls the tests use to build their scratch folders (mkdir, rmdir, is-it-a-folder), for MSVC.
// Tests are desktop-only (docs/PORTING.md); engine code never includes this. On Linux/macOS it is just the system headers.
#pragma once
#if defined(_MSC_VER)
#include <direct.h>
#include <sys/stat.h>

inline int mkdir(const char* path, int /*mode*/) { return _mkdir(path); }
inline int rmdir(const char* path) { return _rmdir(path); }
inline bool testIsDirectory(const char* path) {
  struct _stat st;
  return _stat(path, &st) == 0 && (st.st_mode & _S_IFDIR) != 0;
}
#else
#include <sys/stat.h>
#include <unistd.h>

inline bool testIsDirectory(const char* path) {
  struct stat st;
  return stat(path, &st) == 0 && S_ISDIR(st.st_mode);
}
#endif
