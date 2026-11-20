#include "core/base/fatx_names.h"

#include <cstdint>
#include <cstdio>

namespace ote {

bool fatxCharOk(unsigned char c) {
  if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) return true;
  switch (c) {
    case ' ': case '!': case '#': case '$': case '%': case '&': case '\'': case '(': case ')': case '-': case '.':
    case '@': case '[': case ']': case '^': case '_': case '`': case '{': case '}': case '~':
      return true;
    default:
      return false;
  }
}

std::string fatxNameProblem(const std::string& name) {
  if (name.empty() || name == "." || name == "..") return "is not a file name";
  if (name.size() > kFatxMaxName)
    return "42 characters at most (this has " + std::to_string(name.size()) + ")";
  for (unsigned char c : name) {
    if (fatxCharOk(c)) continue;
    char buf[64];
    if (c < 0x20 || c >= 0x7F) std::snprintf(buf, sizeof buf, "contains the byte 0x%02X, which FATX does not allow", c);
    else std::snprintf(buf, sizeof buf, "contains '%c', which FATX does not allow", (char)c);
    return buf;
  }
  return "";
}

std::vector<std::string> fatxPathProblems(const std::string& path) {
  std::vector<std::string> out;
  size_t start = 0;
  for (size_t i = 0; i <= path.size(); ++i) {
    if (i < path.size() && path[i] != '/' && path[i] != '\\') continue;
    if (i > start || (i == path.size() && start < path.size())) {
      std::string part = path.substr(start, i - start);
      std::string why = fatxNameProblem(part);
      if (!why.empty()) out.push_back(path + ": '" + part + "' " + why);
    }
    start = i + 1;
  }
  if (path.size() > kFatxMaxPath) out.push_back(path + ": path is " + std::to_string(path.size()) + " characters, over " + std::to_string(kFatxMaxPath));
  return out;
}

namespace {
std::string lower(const std::string& s) {
  std::string r = s;
  for (char& c : r)
    if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
  return r;
}
}  // namespace

std::vector<std::string> fatxCaseClashes(const std::vector<std::string>& names) {
  std::vector<std::string> out;
  for (size_t i = 0; i < names.size(); ++i)
    for (size_t j = i + 1; j < names.size(); ++j)
      if (names[i] != names[j] && lower(names[i]) == lower(names[j])) out.push_back(names[i] + " / " + names[j]);
  return out;
}

std::string fatxSafeName(const std::string& name) {
  if (fatxNameProblem(name).empty()) return name;
  std::string out;
  for (unsigned char c : name) {
    if (fatxCharOk(c) && c != '~') {
      out.push_back((char)c);
    } else {
      char buf[8];
      std::snprintf(buf, sizeof buf, "~%02X", c);
      out += buf;
    }
  }
  if (out.empty()) return "~00";
  if (out.size() > kFatxMaxName) {
    uint32_t h = 2166136261u;  // FNV-1a of the original name
    for (unsigned char c : name) h = (h ^ c) * 16777619u;
    char tail[16];
    std::snprintf(tail, sizeof tail, "~%08X", h);
    out.resize(kFatxMaxName - 9);
    // Do not leave half of a "~XX" escape at the cut.
    size_t t = out.rfind('~');
    if (t != std::string::npos && t + 3 > out.size()) out.resize(t);
    out += tail;
  }
  return out;
}

}  // namespace ote
