// The few platform.h functions the player's save-file module calls, for the
// unit tests (which do not link a backend). POSIX only: tests run on desktop.
#include <sys/stat.h>

#include <cstdio>

#include "platform/platform.h"

namespace ote {

bool makeDir(const std::string& path) {
  if (mkdir(path.c_str(), 0755) == 0) return true;
  struct stat st;
  return stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

bool removeFile(const std::string& path) {
  if (std::remove(path.c_str()) == 0) return true;
  std::FILE* f = std::fopen(path.c_str(), "rb");
  if (!f) return true;
  std::fclose(f);
  return false;
}

void commitUserData() {}

}  // namespace ote
