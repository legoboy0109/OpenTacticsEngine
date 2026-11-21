// Files compiled into ote-mcp by embed_gen (editor/mcp/gen): the schema files and the docs. See CMakeLists.txt.
#pragma once
#include <cstddef>

namespace ote_mcp {

struct Embedded {
  const char* name;  // "units.json" (schemas) or "GAME_FORMAT" (docs)
  const unsigned char* data;
  size_t size;
};

extern const Embedded kSchemas[];
extern const size_t kSchemasCount;
extern const Embedded kDocs[];
extern const size_t kDocsCount;

}  // namespace ote_mcp
