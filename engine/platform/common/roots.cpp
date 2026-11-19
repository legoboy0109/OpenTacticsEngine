// contentRoot() / extraContentRoot() / userRoot(): see platform.h "Where things live". Shared by the desktop
// backends. A port with different storage (Android app storage, UWP InstalledLocation/LocalState) replaces
// the defaults here, behind its own #if, and leaves every caller alone.
#include <cstdlib>

#include "platform/platform.h"

namespace ote {
namespace {
std::string& contentOverride() { static std::string s; return s; }
std::string& extraOverride() { static std::string s; return s; }

std::string trimmed(std::string dir) {
  while (dir.size() > 1 && (dir.back() == '/' || dir.back() == '\\')) dir.pop_back();
  return dir;
}
std::string fromEnv(const char* name) {
  const char* v = std::getenv(name);
  return v ? v : "";
}
}  // namespace

void setContentRootOverride(const std::string& dir) { contentOverride() = dir; }
void setExtraContentRootOverride(const std::string& dir) { extraOverride() = dir; }

std::string contentRoot() {
  if (!contentOverride().empty()) return trimmed(contentOverride());
  return trimmed(fromEnv("OTE_CONTENT_ROOT"));  // desktop default: empty = the working directory
}

std::string extraContentRoot() {
  if (!extraOverride().empty()) return trimmed(extraOverride());
  return trimmed(fromEnv("OTE_EXTRA_GAMES"));
}

std::string userRoot() { return userDataDir(); }

}  // namespace ote
