#include "paths.h"

#include <algorithm>
#include <fstream>

namespace ote_editor {

const char* const kTempSuffix = ".ote-tmp";

bool validId(const std::string& id) {
  if (id.empty() || id.size() > 40) return false;
  for (size_t i = 0; i < id.size(); ++i) {
    char c = id[i];
    bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    if (!(alnum || (c == '-' && i > 0))) return false;
  }
  return true;
}

bool validRelPath(const std::string& rel, std::string& why) {
  if (rel.empty()) { why = "path is empty"; return false; }
  if (rel.size() > 240) { why = "path is too long"; return false; }
  if (rel[0] == '/') { why = "path must be relative"; return false; }
  size_t start = 0;
  for (size_t i = 0; i <= rel.size(); ++i) {
    if (i < rel.size()) {
      char c = rel[i];
      // ':' also blocks Windows drive letters and alternate data streams.
      if (c == '\0' || c == '\\' || c == ':' || (unsigned char)c < 0x20) {
        why = "path contains an illegal character";
        return false;
      }
    }
    if (i == rel.size() || rel[i] == '/') {
      std::string part = rel.substr(start, i - start);
      if (part.empty() || part == "." || part == "..") {
        why = "path must not contain empty, \".\" or \"..\" parts";
        return false;
      }
      start = i + 1;
    }
  }
  return true;
}

bool resolveInside(const fs::path& root, const std::string& rel, fs::path& out, std::string& why) {
  if (!validRelPath(rel, why)) return false;
  std::error_code ec;
  fs::path canonRoot = fs::weakly_canonical(root, ec);
  if (ec) { why = "cannot resolve the project folder"; return false; }
  fs::path full = fs::weakly_canonical(canonRoot / fs::path(rel), ec);
  if (ec) { why = "cannot resolve the path"; return false; }
  // Compare component-wise so "/a/bc" is not "inside" "/a/b".
  auto mm = std::mismatch(canonRoot.begin(), canonRoot.end(), full.begin(), full.end());
  if (mm.first != canonRoot.end()) { why = "path leaves the project folder"; return false; }
  out = full;
  return true;
}

bool hasExtension(const std::string& rel, const char* ext) {
  std::string e = ext;
  return rel.size() > e.size() && rel.compare(rel.size() - e.size(), e.size(), e) == 0;
}

bool readWholeFile(const fs::path& path, std::string& out) {
  std::ifstream f(path, std::ios::binary);
  if (!f) return false;
  out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
  return !f.bad();
}

bool writeFileAtomic(const fs::path& path, const std::string& bytes, std::string& err) {
  std::error_code ec;
  fs::create_directories(path.parent_path(), ec);
  if (ec) { err = "cannot create folder " + path.parent_path().string() + ": " + ec.message(); return false; }
  fs::path tmp = path;
  tmp += kTempSuffix;
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (!f) { err = "cannot write " + tmp.string(); return false; }
    f.write(bytes.data(), (std::streamsize)bytes.size());
    f.flush();
    if (!f) {
      f.close();
      fs::remove(tmp, ec);
      err = "write failed for " + path.string() + " (disk full?)";
      return false;
    }
  }
  fs::rename(tmp, path, ec);
  if (ec) {
    std::error_code ec2;
    fs::remove(tmp, ec2);
    err = "cannot replace " + path.string() + ": " + ec.message();
    return false;
  }
  return true;
}

}  // namespace ote_editor
