// Shared basic types for the rules core.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace ote {

// A grid cell is an opaque index. Only Grid knows how indices map to
// coordinates/neighbors, so rules code works unchanged on square, hex,
// isometric or 3D layouts.
using Cell = int32_t;
constexpr Cell kNoCell = -1;

// Small generic "name -> number" bag. Used for data-defined properties
// (terrain defense, weapon might...) that the engine does not interpret
// itself but exposes to Lua formulas.
struct NamedNumber {
  std::string key;
  double value = 0;
};
using Props = std::vector<NamedNumber>;

inline bool propFind(const Props& p, const std::string& key, double& out) {
  for (const NamedNumber& n : p)
    if (n.key == key) { out = n.value; return true; }
  return false;
}

}  // namespace ote
