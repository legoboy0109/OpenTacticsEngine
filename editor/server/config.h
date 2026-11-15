// Command-line configuration and the small result type shared by all API
// operations (see docs/EDITOR_API.md).
#pragma once
#include <filesystem>
#include <string>
#include <vector>

#include "core/base/json.h"

namespace ote_editor {

namespace fs = std::filesystem;

struct Config {
  fs::path gamesDir = "games";
  fs::path assetsDir = "assets";
  fs::path templatesDir = "editor/templates";
  fs::path webDir = "editor/web/dist";
  fs::path playerDir = "build-web/engine/player";
  std::string host = "127.0.0.1";
  int port = 8765;
  std::vector<std::string> allowHosts;  // extra accepted Host header names
  // Tutorial state file (learn.h). main sets it from --learn-file or the user data default; empty = memory only.
  fs::path learnFile;
};

// What an API operation produces; server.cpp turns it into an HTTP response.
struct ApiResult {
  int status = 200;
  ote::Json body;
};

inline ApiResult apiOk(const ote::Json& body, int status = 200) { return {status, body}; }

inline ApiResult apiError(int status, const std::string& message) {
  ApiResult r;
  r.status = status;
  r.body.set("error", ote::Json::makeString(message));
  return r;
}

// A problem found in a project; `file` is project-relative ("" if unknown).
struct Problem {
  std::string file, message;
};

}  // namespace ote_editor
