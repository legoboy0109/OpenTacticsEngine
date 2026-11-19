// The few platform.h functions the player's save-file module calls, for the
// unit tests (which do not link a backend). Desktop only (POSIX or MSVC, see posix_compat.h).
#include "posix_compat.h"

#include <cstdio>

#include "platform/platform.h"

namespace ote {

bool makeDir(const std::string& path) {
  if (mkdir(path.c_str(), 0755) == 0) return true;
  return testIsDirectory(path.c_str());
}

bool removeFile(const std::string& path) {
  if (std::remove(path.c_str()) == 0) return true;
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return true;
  std::fclose(f);
  return false;
}

bool replaceFile(const std::string& from, const std::string& to) {
  std::remove(to.c_str());  // std::rename does not replace an existing file on Windows
  return std::rename(from.c_str(), to.c_str()) == 0;
}

void commitUserData() {}

}  // namespace ote
