// Types shared by the editor's HTTP shell (editor/server) and the HTTP-free core (editor/core), which the native
// editor links too: the filesystem alias and the "problem" record that validation produces.
#pragma once
#include <filesystem>
#include <string>

#include "core/base/json.h"

namespace ote_editor {

namespace fs = std::filesystem;

// A problem found in a project; `file` is project-relative ("" if unknown).
struct Problem {
  std::string file, message;
};

}  // namespace ote_editor
