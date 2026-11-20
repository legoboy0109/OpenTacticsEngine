// Safe path handling. Every file the API touches goes through here.
#pragma once
#include <string>

#include "common.h"

namespace ote_editor {

// Project / template ids: [a-z0-9][a-z0-9-]{0,39}.
bool validId(const std::string& id);

// Syntax check of a project-relative path: forward slashes, no "..", no
// absolute paths or drive letters, no empty/"." components. On failure `why`
// says what is wrong.
bool validRelPath(const std::string& rel, std::string& why);

// validRelPath + the resolved location (symlinks resolved) must stay inside
// `root`. The file itself need not exist.
bool resolveInside(const fs::path& root, const std::string& rel, fs::path& out, std::string& why);

// rel ends with the given extension (".json" / ".lua").
bool hasExtension(const std::string& rel, const char* ext);

// Atomic write: temp file in the same folder, then rename. Creates parent
// folders. Returns false (and sets err) on failure; leaves no temp file.
bool writeFileAtomic(const fs::path& path, const std::string& bytes, std::string& err);

// Reads a whole file; false if it cannot be read.
bool readWholeFile(const fs::path& path, std::string& out);

// Suffix of in-flight temp files (ignored by revision/file listings).
extern const char* const kTempSuffix;

}  // namespace ote_editor
