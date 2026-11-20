// A position inside an ote::Json tree: object keys and array indexes from the root. The editor edits the tree itself
// (never typed structs), so every edit, notification and undo command names its target with one of these.
#pragma once
#include <QString>
#include <vector>

#include "core/base/json.h"

namespace ote_native {

struct PathStep {
  bool isIndex = false;
  size_t index = 0;
  std::string key;

  static PathStep at(size_t i) { PathStep s; s.isIndex = true; s.index = i; return s; }
  static PathStep member(std::string k) { PathStep s; s.key = std::move(k); return s; }
  bool operator==(const PathStep& o) const { return isIndex == o.isIndex && index == o.index && key == o.key; }
};

using JsonPath = std::vector<PathStep>;

// The node at `path`, or nullptr when a step does not exist or has the wrong container type.
ote::Json* resolvePath(ote::Json& root, const JsonPath& path);
const ote::Json* resolvePath(const ote::Json& root, const JsonPath& path);

// Readable form for undo texts and messages: units[2].stats.hp (the root is "$").
QString pathToString(const JsonPath& path);

// True when `prefix` is the start of `path` (or equal).
bool pathStartsWith(const JsonPath& path, const JsonPath& prefix);

}  // namespace ote_native
