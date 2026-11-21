// Tool registry: a tool is a name, a description, a JSON Schema for its arguments and a function. The arguments are checked
// against the tool's own schema before the function runs (so the schema the model sees is exactly what is enforced).
#pragma once
#include <functional>
#include <map>
#include <string>
#include <vector>

#include "util.h"

namespace ote_mcp {

struct Tool {
  std::string name;
  std::string title;
  std::string description;  // one paragraph for a model that knows nothing about the engine
  Json inputSchema;
  bool readOnly = true;     // false = may write files (disabled by --read-only)
  bool destructive = false; // can remove or overwrite content
  std::function<ToolResult(const Json& args)> run;
};

class ToolRegistry {
 public:
  void add(Tool t) { tools_.push_back(std::move(t)); }
  const std::vector<Tool>& all() const { return tools_; }
  const Tool* find(const std::string& name) const;

 private:
  std::vector<Tool> tools_;
};

// Checks `value` against a (small subset of) JSON Schema: type, enum, required, properties, additionalProperties,
// items, minimum, maximum. Returns "" when fine, else a one-line reason naming the offending argument.
std::string checkAgainstSchema(const Json& schema, const Json& value, const std::string& where = "arguments");

// Builders for input schemas, to keep the tool definitions short.
Json sType(const char* type, const std::string& description);
Json sEnum(const std::vector<std::string>& words, const std::string& description);
Json sInt(const std::string& description, double minimum, double maximum);
Json sObject(const std::vector<std::pair<std::string, Json>>& props, const std::vector<std::string>& required = {}, bool additional = false);
Json sArray(const Json& items, const std::string& description);

}  // namespace ote_mcp
