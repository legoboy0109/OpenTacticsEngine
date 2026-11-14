// Model endpoints (docs/GRAPHICS.md "Models", docs/EDITOR_API.md): list, upload
// and delete the .glb/.gltf files in a project's models/ folder. Mirrors art.h.
// Callers hold the API mutex.
#pragma once
#include <string>

#include "config.h"

namespace ote_editor {

// Largest accepted upload, in bytes.
constexpr size_t kMaxModelBytes = 8u * 1024 * 1024;

ApiResult listModels(const Config& cfg, const std::string& id);  // GET .../models
// PUT .../models/<name>.glb: `bytes` is the raw request body.
ApiResult putModel(const Config& cfg, const std::string& id, const std::string& name,
                   const std::string& bytes, bool overwrite);
ApiResult deleteModel(const Config& cfg, const std::string& id, const std::string& name);

// [a-z0-9][a-z0-9_-]{0,39}\.glb  (uploads); listing also shows .gltf files.
bool validModelUploadName(const std::string& name);

}  // namespace ote_editor
