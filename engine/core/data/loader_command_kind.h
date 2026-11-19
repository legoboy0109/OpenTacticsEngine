// Shared by the event and campaign command readers: which command does one entry name?
// An entry names exactly one command; each table of command keys is scanned in turn and a second hit is refused.
#pragma once
#include <cstddef>
#include <string>

#include "core/data/loader_util.h"

namespace ote {

// Scans one table of command keys. `named(key)` says whether the entry uses that key. `kind` carries the command
// found so far (nullptr for none) from one table to the next; a second one is an error. Returns false after
// writing "has two commands (A and B); use one entry for each".
template <size_t N, class Named>
bool pickCommandKind(const LoadCtx& c, const std::string& where, const char* const (&keys)[N], Named named,
                     const char*& kind) {
  for (const char* k : keys)
    if (named(k)) {
      if (kind) return c.fail(where, std::string("has two commands (") + kind + " and " + k + "); use one entry for each");
      kind = k;
    }
  return true;
}

// The usual case: a command is named by a key of the entry itself.
template <size_t N>
bool pickCommandKind(const LoadCtx& c, const Json& e, const std::string& where, const char* const (&keys)[N],
                     const char*& kind) {
  return pickCommandKind(c, where, keys, [&e](const char* k) { return e.has(k); }, kind);
}

}  // namespace ote
