// Game templates (editor/templates/<id>/): listing them and making a new game folder from one. Shared by the browser
// editor's server (new game dialog) and ote-mcp (create_game), so both offer the same templates and make the same folders.
#pragma once
#include <string>
#include <vector>

#include "common.h"

namespace ote_editor {

struct TemplateInfo {
  std::string id, name, description;
  std::string gridType = "square";  // "square" or "hex"
  std::string gridLayout;           // "flat" / "pointy" for hex, else ""
};

// Every folder of `templatesDir` with a valid id and a game.json, sorted by id.
std::vector<TemplateInfo> listTemplates(const fs::path& templatesDir);

enum class CreateStatus { Created, BadTemplate, AlreadyExists, CopyFailed, WriteFailed };

struct CreateResult {
  CreateStatus status = CreateStatus::Created;
  std::string message;  // the reason when not Created (the server's historic wording)
};

// Copies template `tpl` to <gamesDir>/<id> (built in a hidden sibling folder and renamed, so a failure never leaves a
// half-made game) and sets game.json "name". `id` and `tpl` must already be valid ids; `name` non-empty.
CreateResult createFromTemplate(const fs::path& templatesDir, const fs::path& gamesDir, const std::string& id,
                                const std::string& tpl, const std::string& name);

}  // namespace ote_editor
