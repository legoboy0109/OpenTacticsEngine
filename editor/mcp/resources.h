// Resources (schemas, docs, a game's problems) and prompts of ote-mcp.
#pragma once
#include <string>

#include "context.h"

namespace ote_mcp {

// resources/list entries: ote://schema/<file>, ote://docs/<name>, ote://game/<id>/problems.
Json listAllResources(Context& ctx);
// resources/templates/list entries.
Json resourceTemplates();
// Reads a resource. False with `message` when the uri is unknown or malformed. Big texts are cut at about 100 KB;
// "?offset=N" on the uri continues (the cut text says so).
bool readResourceUri(Context& ctx, const std::string& uri, Json& contents, std::string& message);

Json listPromptDefs();
// False with `message` for an unknown prompt or a missing required argument.
bool getPromptByName(Context& ctx, const std::string& name, const Json& arguments, Json& result, std::string& message);

}  // namespace ote_mcp
