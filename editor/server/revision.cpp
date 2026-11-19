#include "revision.h"

#include <algorithm>
#include <chrono>
#include <climits>
#include <map>
#include <mutex>
#include <cstdint>
#include <cstdio>
#include <fstream>

#include "paths.h"

namespace ote_editor {

namespace {

// One covered file as the directory scan sees it. size/mtime are only filled when asked for (the revision cache's key).
struct ScannedFile {
  std::string rel;
  fs::path path;
  uintmax_t size = 0;
  int64_t mtimeNs = 0;  // full file-system precision, nanoseconds since the clock's epoch
};

std::vector<ScannedFile> scanProject(const fs::path& dir, bool withStat) {
  std::vector<ScannedFile> out;
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
    ScannedFile f;
    f.rel = fs::relative(it->path(), dir, e2).generic_string();
    f.path = it->path();
    if (withStat) {
      std::error_code e3;
      f.size = fs::file_size(f.path, e3);
      if (e3) f.size = (uintmax_t)-1;
      fs::file_time_type t = fs::last_write_time(f.path, e3);
      f.mtimeNs = e3 ? INT64_MIN
                     : (int64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(t.time_since_epoch()).count();
    }
    out.push_back(std::move(f));
  }
  std::sort(out.begin(), out.end(), [](const ScannedFile& a, const ScannedFile& b) {
    return a.rel != b.rel ? a.rel < b.rel : a.path < b.path;  // same order as sorting {rel, path} pairs
  });
  return out;
}

}  // namespace

std::vector<std::pair<std::string, fs::path>> listProjectFiles(const fs::path& dir) {
  std::vector<std::pair<std::string, fs::path>> out;
  for (ScannedFile& f : scanProject(dir, false)) out.emplace_back(std::move(f.rel), std::move(f.path));
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

namespace {

// Revision cache. Hashing every byte of the game on every request was the cost; the revision is a pure function of the
// files, so when the whole scan (path, size, modification time to the nanosecond) is identical to the one the cached
// revision was computed from, the content is taken to be identical and the cached value is returned. Any difference (a
// file added, removed, rewritten by this helper, by hand, by another helper or by a sync) recomputes from the bytes, so
// the VALUE is exactly what a full hash gives (FNV over one running state cannot be combined per file, hence one cache
// entry per project, not per file).
// Same-tick hole: a file rewritten with the same size inside one timestamp tick of the scan would look unchanged. So a
// snapshot is only trusted (stored) when every file's mtime is older than kSettle at the time of the scan; a project with
// a recently touched file is simply hashed in full. A later write then gets an mtime >= kSettle newer than any trusted
// entry's, so its key differs. (Not covered: a tool that sets an old mtime on different content of the same size.)
struct Snapshot {
  std::vector<ScannedFile> files;
  std::string revision;
};
constexpr std::chrono::seconds kSettle{2};
constexpr size_t kMaxCachedProjects = 32;
std::mutex gCacheMutex;
std::map<std::string, Snapshot> gCache;  // key: the project folder's generic string

bool sameScan(const std::vector<ScannedFile>& a, const std::vector<ScannedFile>& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i)
    if (a[i].rel != b[i].rel || a[i].path != b[i].path || a[i].size != b[i].size || a[i].mtimeNs != b[i].mtimeNs)
      return false;
  return true;
}

std::string hashFiles(const std::vector<ScannedFile>& files) {
  Fnv fnv;
  for (const ScannedFile& f : files) {
    fnv.add(f.rel.data(), f.rel.size());
    fnv.addByte(0);
    std::ifstream in(f.path, std::ios::binary);
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

}  // namespace

std::string computeRevision(const fs::path& dir) {
  // `now` is read BEFORE the scan: a file touched after it has an mtime >= now - tick, so it is never trusted.
  const int64_t nowNs = (int64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(
                            fs::file_time_type::clock::now().time_since_epoch()).count();
  const int64_t settleNs = (int64_t)std::chrono::duration_cast<std::chrono::nanoseconds>(kSettle).count();
  std::vector<ScannedFile> scan = scanProject(dir, true);
  const std::string key = dir.generic_string();
  {
    std::lock_guard<std::mutex> lock(gCacheMutex);
    auto it = gCache.find(key);
    if (it != gCache.end() && sameScan(it->second.files, scan)) return it->second.revision;
  }
  // Hash outside the lock (other requests are not held up). A file changed while we read is caught by the next scan.
  std::string revision = hashFiles(scan);
  bool settled = true;
  for (const ScannedFile& f : scan)
    if (f.mtimeNs == INT64_MIN || f.size == (uintmax_t)-1 || f.mtimeNs > nowNs - settleNs) { settled = false; break; }
  std::lock_guard<std::mutex> lock(gCacheMutex);
  if (!settled) {
    gCache.erase(key);  // never leave an older entry that a same-tick rewrite could match
    return revision;
  }
  if (gCache.size() >= kMaxCachedProjects && !gCache.count(key)) gCache.clear();
  Snapshot& snap = gCache[key];
  snap.files = std::move(scan);
  snap.revision = revision;
  return revision;
}

}  // namespace ote_editor
