// Android glue: content extraction from the APK, the extra games folder, and logcat output.
#ifdef __ANDROID__
#include <SDL3/SDL.h>
#include <android/log.h>

#include <string>
#include <vector>

#include "core/base/log.h"
#include "platform/common/android_roots.h"

namespace ote {
namespace {

const char* kManifest = "ote-manifest.txt";  // written by android/app/build.gradle (stageContent): line 1 = "version <id>"
const char* kVersionFile = ".ote-content-version";

void logSink(LogLevel level, const char* message) {
  static const int prio[] = {ANDROID_LOG_DEBUG, ANDROID_LOG_INFO, ANDROID_LOG_WARN, ANDROID_LOG_ERROR};
  __android_log_write(prio[(int)level], "OTE", message);
}

// Removes a folder and everything in it (SDL_RemovePath only removes empty folders).
SDL_EnumerationResult removeEntry(void* user, const char* dirname, const char* fname) {
  std::string path = std::string(dirname) + fname;  // SDL gives dirname with its trailing separator
  SDL_PathInfo info;
  if (SDL_GetPathInfo(path.c_str(), &info) && info.type == SDL_PATHTYPE_DIRECTORY) {
    SDL_EnumerateDirectory(path.c_str(), removeEntry, user);
  }
  SDL_RemovePath(path.c_str());
  return SDL_ENUM_CONTINUE;
}
void removeTree(const std::string& dir) {
  SDL_EnumerateDirectory(dir.c_str(), removeEntry, nullptr);
  SDL_RemovePath(dir.c_str());
}

std::string parentOf(const std::string& path) {
  size_t slash = path.rfind('/');
  return slash == std::string::npos ? "" : path.substr(0, slash);
}

std::string readText(const std::string& path) {
  size_t n = 0;
  void* data = SDL_LoadFile(path.c_str(), &n);
  if (!data) return "";
  std::string s((const char*)data, n);
  SDL_free(data);
  return s;
}

// Copies every asset named in the manifest to `dest`. Relative names given to SDL_IOFromFile read the APK's assets.
bool extractAll(const std::vector<std::string>& files, const std::string& dest) {
  for (const std::string& rel : files) {
    size_t n = 0;
    void* data = SDL_LoadFile(rel.c_str(), &n);
    if (!data) {
      OTE_ERROR("android: cannot read APK asset '%s': %s", rel.c_str(), SDL_GetError());
      return false;
    }
    std::string out = dest + "/" + rel;
    bool ok = SDL_CreateDirectory(parentOf(out).c_str()) && SDL_SaveFile(out.c_str(), data, n);
    SDL_free(data);
    if (!ok) {
      OTE_ERROR("android: cannot write '%s': %s", out.c_str(), SDL_GetError());
      return false;
    }
  }
  return true;
}

std::string prepareContent() {
  const char* internal = SDL_GetAndroidInternalStoragePath();
  if (!internal) {
    OTE_ERROR("android: no internal storage path: %s", SDL_GetError());
    return "";
  }
  const std::string root = std::string(internal) + "/content";
  const std::string manifest = readText(kManifest);
  if (manifest.empty()) {
    OTE_ERROR("android: the APK has no %s (it was not built by android/app/build.gradle)", kManifest);
    return "";
  }
  size_t eol = manifest.find('\n');
  const std::string version = manifest.substr(0, eol);  // "version <versionCode>-<content hash>"
  if (readText(root + "/" + kVersionFile) == version) {
    OTE_INFO("android: content up to date (%s) in %s", version.c_str(), root.c_str());
    return root;
  }

  OTE_INFO("android: extracting content (%s) to %s", version.c_str(), root.c_str());
  Uint64 t0 = SDL_GetTicks();
  std::vector<std::string> files;
  for (size_t pos = eol == std::string::npos ? manifest.size() : eol + 1; pos < manifest.size();) {
    size_t e = manifest.find('\n', pos);
    if (e == std::string::npos) e = manifest.size();
    std::string line = manifest.substr(pos, e - pos);
    if (!line.empty() && line.back() == '\r') line.pop_back();
    if (!line.empty()) files.push_back(line);
    pos = e + 1;
  }
  // Into a fresh folder first, swapped in at the end, so an interrupted run never leaves a half-extracted tree
  // that looks complete (the version file is written last as well).
  const std::string staging = root + ".new";
  removeTree(staging);
  if (!extractAll(files, staging) ||
      !SDL_SaveFile((staging + "/" + kVersionFile).c_str(), version.data(), version.size())) {
    removeTree(staging);
    return "";
  }
  removeTree(root);
  if (!SDL_RenamePath(staging.c_str(), root.c_str())) {
    OTE_ERROR("android: cannot move %s to %s: %s", staging.c_str(), root.c_str(), SDL_GetError());
    return "";
  }
  OTE_INFO("android: extracted %zu files in %llu ms", files.size(), (unsigned long long)(SDL_GetTicks() - t0));
  return root;
}

}  // namespace

std::string androidContentRoot() {
  static const std::string root = prepareContent();
  return root;
}

std::string androidExtraGamesRoot() {
  const char* ext = SDL_GetAndroidExternalStoragePath();
  if (!ext) return "";
  std::string dir = std::string(ext) + "/games";
  if (!SDL_CreateDirectory(dir.c_str())) return "";
  return dir;
}

void installAndroidLogSink() { setLogSink(logSink); }

}  // namespace ote
#endif  // __ANDROID__
