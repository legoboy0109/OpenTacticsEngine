// Static file serving logic (no HTTP types here; server.cpp wires it up).
#pragma once
#include <string>

#include "config.h"

namespace ote_editor {

struct StaticResponse {
  int status = 200;
  std::string mime;
  std::string body;
};

// Content-Type by file extension; unknown extensions are octet-stream.
std::string mimeFor(const std::string& path);

// Serves `rel` (already URL-decoded) from `root`. With spaFallback, anything
// that is not an existing file returns root/index.html (single-page app).
StaticResponse serveFromDir(const fs::path& root, const std::string& rel, bool spaFallback);

// Shown at "/" when the web UI has not been built yet.
StaticResponse builtinIndexPage(const Config& cfg);

}  // namespace ote_editor
