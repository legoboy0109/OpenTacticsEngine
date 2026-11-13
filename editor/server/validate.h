// "Is this project valid?" answered by the real engine: loadGame, loadMap for
// every map, Lua formula compilation, and a battle start on each map.
#pragma once
#include <string>
#include <vector>

#include "config.h"

namespace ote_editor {

// Runs the engine checks on the saved files of `dir`. Empty result = valid.
// Like the loader, it reports at most one problem per stage/map.
std::vector<Problem> validateProject(const fs::path& dir);

// Turns a loader message into a Problem. Messages that start with a path
// inside `dir` become {relative file, rest}; anything else keeps file "".
Problem problemFromMessage(const fs::path& dir, const std::string& message);

ote::Json problemsToJson(const std::vector<Problem>& problems);

}  // namespace ote_editor
