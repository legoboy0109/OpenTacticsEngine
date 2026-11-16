// Deleting a whole game. The one operation of the helper that cannot be taken back, so it is narrow on purpose:
// the caller repeats the id (`confirm`), the demos that come with the engine are refused, and only a real folder directly
// inside the games folder is removed (a link is refused rather than followed or unlinked).
#include "projects.h"

#include "log.h"

namespace ote_editor {

bool protectedProject(const std::string& id) {
  return id == "demo-srpg" || id == "demo-economy" || id == "demo-hex";
}

ApiResult deleteProject(const Config& cfg, const std::string& id, const std::string& confirm) {
  fs::path dir;
  ApiResult err;
  if (!locateProject(cfg, id, dir, err)) return err;
  if (protectedProject(id)) return apiError(403, "this demo game comes with the editor and cannot be deleted");
  if (confirm != id) return apiError(400, "the delete was not confirmed (confirm must repeat the project id)");
  std::error_code ec;
  if (fs::is_symlink(fs::symlink_status(dir, ec)))
    return apiError(409, "this game's folder is a link to somewhere else; delete it by hand");
  // remove_all never follows links inside the folder: a link is removed, its target is not.
  std::uintmax_t n = fs::remove_all(dir, ec);
  if (ec) {
    elog("DELETE %s: %s", id.c_str(), ec.message().c_str());
    return apiError(500, "could not delete the game's folder: " + ec.message());
  }
  elog("DELETE %s: removed %llu files and folders", id.c_str(), (unsigned long long)n);
  ote::Json body;
  body.set("deleted", ote::Json::makeString(id));
  return apiOk(body);
}

}  // namespace ote_editor
