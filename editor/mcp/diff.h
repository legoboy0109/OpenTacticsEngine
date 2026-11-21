// A line diff for the "what would change" reports: unified-style hunks of two texts (the pretty-printed JSON of a file
// before and after), kept short enough to read.
#pragma once
#include <string>

namespace ote_mcp {

struct DiffResult {
  std::string text;   // "@@ -12,3 +12,4 @@" hunks with ' ', '-' and '+' lines; "" when equal
  int added = 0, removed = 0;
  bool truncated = false;
};

DiffResult unifiedDiff(const std::string& before, const std::string& after, int context = 2, int maxLines = 160);

}  // namespace ote_mcp
