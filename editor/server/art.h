// Art endpoints (docs/GRAPHICS.md, docs/EDITOR_API.md): list, upload and
// delete the PNG files in a project's art/ folder. Callers hold the API mutex.
#pragma once
#include <string>

#include "config.h"

namespace ote_editor {

// Largest accepted upload, in bytes.
constexpr size_t kMaxArtBytes = 8u * 1024 * 1024;

ApiResult listArt(const Config& cfg, const std::string& id);  // GET .../art
// PUT .../art/<name>: `name` includes ".png"; `bytes` is the raw request body.
ApiResult putArt(const Config& cfg, const std::string& id, const std::string& name,
                 const std::string& bytes, bool overwrite);
ApiResult deleteArt(const Config& cfg, const std::string& id, const std::string& name);  // DELETE

// [a-z0-9][a-z0-9_-]{0,39}\.png
bool validArtName(const std::string& name);

}  // namespace ote_editor
