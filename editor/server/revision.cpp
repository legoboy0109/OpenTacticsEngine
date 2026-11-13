#include "revision.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <fstream>

#include "paths.h"

namespace ote_editor {

std::vector<std::pair<std::string, fs::path>> listProjectFiles(const fs::path& dir) {
  std::vector<std::pair<std::string, fs::path>> out;
  std::error_code ec;
  fs::recursive_directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec), end;
  for (; !ec && it != end; it.increment(ec)) {
    std::error_code e2;
    // symlink_status: never follow links, so a link can't pull outside files in.
    if (!fs::is_regular_file(it->symlink_status(e2))) continue;
    std::string name = it->path().filename().string();
    std::string suffix = kTempSuffix;
    if (name.size() >= suffix.size() &&
        name.compare(name.size() - suffix.size(), suffix.size(), suffix) == 0)
      continue;
    out.emplace_back(fs::relative(it->path(), dir, e2).generic_string(), it->path());
  }
  std::sort(out.begin(), out.end());
  return out;
}

namespace {
struct Fnv {
  uint64_t h = 1469598103934665603ULL;
  void add(const void* data, size_t n) {
    const unsigned char* p = (const unsigned char*)data;
    for (size_t i = 0; i < n; ++i) { h ^= p[i]; h *= 1099511628211ULL; }
  }
  void addByte(unsigned char b) { add(&b, 1); }
};
}  // namespace

std::string computeRevision(const fs::path& dir) {
  Fnv fnv;
  for (const auto& f : listProjectFiles(dir)) {
    fnv.add(f.first.data(), f.first.size());
    fnv.addByte(0);
    std::ifstream in(f.second, std::ios::binary);
    char buf[16384];
    uint64_t total = 0;
    while (in) {
      in.read(buf, sizeof buf);
      std::streamsize n = in.gcount();
      if (n > 0) { fnv.add(buf, (size_t)n); total += (uint64_t)n; }
    }
    // Length marker so file boundaries can't be shifted without a change.
    fnv.add(&total, sizeof total);
    fnv.addByte(0xFF);
  }
  char hex[17];
  std::snprintf(hex, sizeof hex, "%016llx", (unsigned long long)fnv.h);
  return hex;
}

}  // namespace ote_editor
