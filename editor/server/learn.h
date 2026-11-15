// The tutorial's remembered state ("Learn", docs/TUTORIAL.md): one JSON file in the editor's user data folder,
// outside every game folder and the repository. The server stores it as is (the web editor owns the shape).
#pragma once
#include "config.h"

namespace ote_editor {

// Largest accepted state (bytes of JSON text).
constexpr size_t kMaxLearnBytes = 64 * 1024;

// Where the state lives by default: <user data>/OpenTacticsEngine/editor/learn.json
// (~/.local/share, ~/Library/Application Support or %APPDATA%). Empty when no user data folder can be found.
// Only computes a path; creates nothing.
fs::path defaultLearnFile();

// GET: the stored object; a missing, unreadable or damaged file (or memory-only mode with nothing stored) is `{}`.
ApiResult getLearn(const Config& cfg);
// PUT: the body must be a JSON object of at most kMaxLearnBytes; it replaces the stored state.
// Memory-only mode (cfg.learnFile empty) keeps it until the helper stops.
ApiResult putLearn(const Config& cfg, const std::string& bodyText);

}  // namespace ote_editor
