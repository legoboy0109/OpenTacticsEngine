#include "util.h"

#include <cstdint>

namespace ote_mcp {

void logLine(const char* level, const std::string& message) {
  std::fprintf(stderr, "ote-mcp [%s] %s\n", level, message.c_str());
  std::fflush(stderr);
}

std::string sanitizeUtf8(const std::string& s) {
  std::string out;
  out.reserve(s.size());
  for (size_t i = 0; i < s.size();) {
    const unsigned char c = (unsigned char)s[i];
    size_t len = c < 0x80 ? 1 : (c >> 5) == 0x6 ? 2 : (c >> 4) == 0xE ? 3 : (c >> 3) == 0x1E ? 4 : 0;
    bool ok = len > 0 && i + len <= s.size();
    for (size_t k = 1; ok && k < len; ++k) ok = ((unsigned char)s[i + k] & 0xC0) == 0x80;
    if (ok && len == 2 && c < 0xC2) ok = false;  // overlong
    if (ok && len == 3) {
      const unsigned cp = (c & 0x0Fu) << 12 | ((unsigned char)s[i + 1] & 0x3Fu) << 6 | ((unsigned char)s[i + 2] & 0x3Fu);
      if (cp < 0x800 || (cp >= 0xD800 && cp <= 0xDFFF)) ok = false;
    }
    if (ok && len == 4) {
      const unsigned cp = (c & 0x07u) << 18 | ((unsigned char)s[i + 1] & 0x3Fu) << 12 | ((unsigned char)s[i + 2] & 0x3Fu) << 6 | ((unsigned char)s[i + 3] & 0x3Fu);
      if (cp < 0x10000 || cp > 0x10FFFF) ok = false;
    }
    if (ok) { out.append(s, i, len); i += len; }
    else { out += "\xEF\xBF\xBD"; ++i; }
  }
  return out;
}

std::string baseNameOf(const std::string& rel) {
  const size_t slash = rel.find_last_of('/');
  return slash == std::string::npos ? rel : rel.substr(slash + 1);
}

bool endsWith(const std::string& s, const std::string& suffix) {
  return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool startsWith(const std::string& s, const std::string& prefix) { return s.compare(0, prefix.size(), prefix) == 0; }

std::vector<std::string> splitOn(const std::string& s, char sep) {
  std::vector<std::string> out;
  size_t start = 0;
  for (;;) {
    const size_t p = s.find(sep, start);
    if (p == std::string::npos) { out.push_back(s.substr(start)); return out; }
    out.push_back(s.substr(start, p - start));
    start = p + 1;
  }
}

std::string hashHex(const std::string& bytes) {
  uint64_t h = 1469598103934665603ull;
  for (unsigned char c : bytes) { h ^= c; h *= 1099511628211ull; }
  char buf[17];
  std::snprintf(buf, sizeof buf, "%016llx", (unsigned long long)h);
  return buf;
}

bool validName(const std::string& s) {
  if (s.empty() || s.size() > 40) return false;
  for (size_t i = 0; i < s.size(); ++i) {
    const char c = s[i];
    const bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    if (!(alnum || (c == '-' && i > 0))) return false;
  }
  return true;
}

}  // namespace ote_mcp
