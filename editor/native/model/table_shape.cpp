#include "table_shape.h"

namespace ote_native {

namespace {
bool isArrayOfObjects(const ote::Json& j) {
  if (!j.isArray() || j.size() == 0) return false;  // an empty array says nothing about its columns
  for (const ote::Json& item : j.items())
    if (!item.isObject()) return false;
  return true;
}
}  // namespace

std::optional<JsonPath> findTableArray(const ote::Json& root) {
  if (isArrayOfObjects(root)) return JsonPath{};
  if (root.isObject() && root.members().size() == 1 && isArrayOfObjects(root.members()[0].second))
    return JsonPath{PathStep::member(root.members()[0].first)};
  return std::nullopt;
}

}  // namespace ote_native
