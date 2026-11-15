// Copies of a JSON document with one value replaced or removed: how the damaged-save tests make their
// damaged saves. Paths are lists of keys; "#3" means element 3 of a list.
#pragma once
#include <cstdlib>
#include <initializer_list>
#include <string>
#include <vector>

#include "core/base/json.h"

namespace jsonedit {

using ote::Json;

inline Json setAt(const Json& j, const std::vector<std::string>& path, size_t at, const Json& v) {
  if (at == path.size()) return v;
  const std::string& step = path[at];
  if (j.isArray() && step[0] == '#') {
    Json out = Json::makeArray();
    size_t index = (size_t)std::atoi(step.c_str() + 1);
    for (size_t i = 0; i < j.size(); ++i) out.push(i == index ? setAt(j[i], path, at + 1, v) : j[i]);
    return out;
  }
  Json out = Json::makeObject();
  for (const Json::Member& m : j.members()) out.set(m.first, m.first == step ? setAt(m.second, path, at + 1, v) : m.second);
  if (!j.has(step.c_str())) out.set(step, setAt(Json(), path, at + 1, v));
  return out;
}
inline Json withValue(const Json& file, const std::vector<std::string>& path, const Json& v) { return setAt(file, path, 0, v); }

// `j` (an object) without one member.
inline Json without(const Json& j, const std::string& key) {
  Json out = Json::makeObject();
  for (const Json::Member& m : j.members())
    if (m.first != key) out.set(m.first, m.second);
  return out;
}
// A copy of the file with the member at the end of the path removed.
inline Json withoutAt(const Json& file, const std::vector<std::string>& path) {
  std::vector<std::string> parent(path.begin(), path.end() - 1);
  const Json* node = &file;
  for (const std::string& step : parent)
    node = step[0] == '#' ? &(*node)[(size_t)std::atoi(step.c_str() + 1)] : &(*node)[step.c_str()];
  return setAt(file, parent, 0, without(*node, path.back()));
}

inline Json num(double d) { return Json::makeNumber(d); }
inline Json str(const char* s) { return Json::makeString(s); }
inline Json list(std::initializer_list<double> xs) {
  Json a = Json::makeArray();
  for (double x : xs) a.push(Json::makeNumber(x));
  return a;
}

}  // namespace jsonedit
