// The project operations behind the API endpoints. Each returns an ApiResult
// and never throws on bad input; callers hold the global API mutex.
#pragma once
#include <string>

#include "config.h"

namespace ote_editor {

ApiResult listProjects(const Config& cfg);                                      // GET /api/projects
ApiResult createProject(const Config& cfg, const ote::Json& body);              // POST /api/projects
ApiResult getProject(const Config& cfg, const std::string& id);                 // GET /api/projects/:id
ApiResult putProject(const Config& cfg, const std::string& id, const ote::Json& body);  // PUT
ApiResult getProblems(const Config& cfg, const std::string& id);                // .../problems
ApiResult getProjectFiles(const Config& cfg, const std::string& id);            // .../files

struct RawFile {
  int status = 200;
  std::string error;  // set when status != 200
  std::string bytes;
};
RawFile getProjectFile(const Config& cfg, const std::string& id, const std::string& rel);

ApiResult listTemplates(const Config& cfg);  // GET /api/templates
ApiResult listFonts(const Config& cfg);      // GET /api/fonts

// Shared by the project files: locates games/<id> (400 bad id, 404 missing).
bool locateProject(const Config& cfg, const std::string& id, fs::path& dir, ApiResult& err);

// Reads + parses a project-relative JSON file (lenient parser: comments ok).
// On failure `prob` says why, with the project-relative file name.
bool readJsonRel(const fs::path& dir, const std::string& rel, ote::Json& out, Problem& prob);

}  // namespace ote_editor
