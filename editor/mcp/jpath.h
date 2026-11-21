// Addressing a value inside a game file. The path syntax a model writes:
//   units[archer].stats.hp     key "units", the row whose "id" is archer, key "stats", key "hp"
//   units[2].name              the third row (0-based)       units[-1]   the last row
//   [archer].stats.hp          shorthand for table files (units.json, weapons.json ...): the table's own key may be left out
//   rules.turnOrder            plain keys
//   events[id=intro].do[0]     [key=value] picks the list item whose key has that value
//   ["odd.key"]                a key written in quotes (keys that contain dots or brackets)
// A path is parsed into segments, then resolved against the real document into a concrete path (keys and numeric indexes).
#pragma once
#include <string>
#include <vector>

#include "util.h"

namespace ote_mcp {

struct Seg {
  enum Kind { Key, Index, Match } kind = Key;
  std::string key;       // Key: the key. Match: the member name ("id" by default)
  long index = 0;        // Index (negative counts from the end)
  std::string matchVal;  // Match: the wanted text
};

struct Step {
  bool isIndex = false;
  size_t index = 0;
  std::string key;
};
using ConcretePath = std::vector<Step>;

bool parsePath(const std::string& text, std::vector<Seg>& out, std::string& err);

// Resolves against `root`. With `create`, missing OBJECT keys on the way (not the last one) are made as empty objects
// (only in the working copy of a change); without it a missing step is an error that lists what is there.
// `tableKey` receives the table array's key when the table shorthand was used ("" otherwise).
bool resolvePathIn(Json& root, const std::vector<Seg>& segs, ConcretePath& out, std::string& err, bool create = false);

// The value at a concrete path (nullptr when a step is missing).
Json* valueAt(Json& root, const ConcretePath& p);
const Json* valueAt(const Json& root, const ConcretePath& p);

// Readable path in the same syntax a model writes, with [id] instead of numbers where the row has a text id.
std::string pathLabel(const Json& root, const ConcretePath& p);

// Splits off the last step: the container's path and the final step.
inline ConcretePath parentOf(const ConcretePath& p) { return ConcretePath(p.begin(), p.end() - (p.empty() ? 0 : 1)); }

}  // namespace ote_mcp
