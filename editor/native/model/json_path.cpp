#include "json_path.h"

namespace ote_native {

ote::Json* resolvePath(ote::Json& root, const JsonPath& path) {
  ote::Json* cur = &root;
  for (const PathStep& s : path) {
    if (s.isIndex) {
      if (!cur->isArray() || s.index >= cur->items().size()) return nullptr;
      cur = &cur->mutableItems()[s.index];
    } else {
      if (!cur->isObject()) return nullptr;
      ote::Json* next = nullptr;
      for (auto& m : cur->mutableMembers())
        if (m.first == s.key) { next = &m.second; break; }
      if (!next) return nullptr;
      cur = next;
    }
  }
  return cur;
}

const ote::Json* resolvePath(const ote::Json& root, const JsonPath& path) {
  // Same walk on a const tree; the non-const overload only hands out pointers, it never changes anything.
  return resolvePath(const_cast<ote::Json&>(root), path);
}

QString pathToString(const JsonPath& path) {
  if (path.empty()) return QStringLiteral("$");
  QString out;
  for (const PathStep& s : path) {
    if (s.isIndex) out += QStringLiteral("[%1]").arg(s.index);
    else out += (out.isEmpty() ? QString() : QStringLiteral(".")) + QString::fromStdString(s.key);
  }
  return out;
}

bool pathStartsWith(const JsonPath& path, const JsonPath& prefix) {
  if (prefix.size() > path.size()) return false;
  for (size_t i = 0; i < prefix.size(); ++i)
    if (!(path[i] == prefix[i])) return false;
  return true;
}

}  // namespace ote_native
