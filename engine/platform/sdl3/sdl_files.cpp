// File-system helpers of the SDL3 backend (see platform.h "Files").
#include <SDL3/SDL.h>

#include <string>
#include <vector>

#include "core/base/log.h"
#include "platform/platform.h"

namespace ote {

std::string userDataDir() {
#ifdef __EMSCRIPTEN__
  // The page mounts browser storage (IDBFS) here before main() runs
  // (engine/platform/web/player.js). Not mounted (embedded preview) = no folder.
  SDL_PathInfo info;
  if (SDL_GetPathInfo("/userdata", &info) && info.type == SDL_PATHTYPE_DIRECTORY) return "/userdata";
  return "";
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

namespace {
struct ListCtx {
  std::string base;
  std::vector<std::string>* out;
};

SDL_EnumerationResult onEntry(void* user, const char* /*dirname*/, const char* fname) {
  ListCtx* ctx = (ListCtx*)user;
  std::string name = fname;
  if (name.empty() || name[0] == '.') return SDL_ENUM_CONTINUE;  // ".", "..", hidden
  SDL_PathInfo info;
  if (SDL_GetPathInfo((ctx->base + "/" + name).c_str(), &info) && info.type == SDL_PATHTYPE_DIRECTORY)
    ctx->out->push_back(name);
  return SDL_ENUM_CONTINUE;
}
}  // namespace

bool listSubdirs(const std::string& path, std::vector<std::string>& names) {
  names.clear();
  ListCtx ctx{path, &names};
  return SDL_EnumerateDirectory(path.c_str(), onEntry, &ctx);
}

}  // namespace ote
