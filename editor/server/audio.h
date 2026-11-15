// Audio file endpoints (docs/SOUND.md, docs/EDITOR_API.md "Audio files"): list, upload and delete the .wav / .ogg files
// in a project's audio/ folder. Mirrors art.h. Callers hold the API mutex.
#pragma once
#include <string>

#include "config.h"

namespace ote_editor {

// Largest accepted upload, in bytes (music included).
constexpr size_t kMaxAudioBytes = 8u * 1024 * 1024;

ApiResult listAudioFiles(const Config& cfg, const std::string& id);  // GET .../audio
// PUT .../audio/<name>: `name` includes ".wav" or ".ogg"; `bytes` is the raw request body.
ApiResult putAudioFile(const Config& cfg, const std::string& id, const std::string& name,
                       const std::string& bytes, bool overwrite);
ApiResult deleteAudioFile(const Config& cfg, const std::string& id, const std::string& name);  // DELETE

// [a-z0-9][a-z0-9_-]{0,39}\.(wav|ogg)
bool validAudioName(const std::string& name);

// Content type for serving a project file: "audio/wav", "audio/ogg" or "" for other names.
std::string audioContentType(const std::string& relPath);

}  // namespace ote_editor
