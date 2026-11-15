#include "learn.h"

#include <cstdlib>

#include "log.h"
#include "paths.h"

namespace ote_editor {

using ote::Json;

namespace {
// Memory-only mode: what the last PUT stored. (API calls are serialized by the server.)
std::string g_memory;
}  // namespace

fs::path defaultLearnFile() {
  fs::path base;
#ifdef _WIN32
  if (const char* a = std::getenv("APPDATA")) base = a;
#elif defined(__APPLE__)
  if (const char* h = std::getenv("HOME")) base = fs::path(h) / "Library" / "Application Support";
#else
  const char* x = std::getenv("XDG_DATA_HOME");
  const char* h = std::getenv("HOME");
  if (x && *x && fs::path(x).is_absolute()) base = x;
  else if (h && *h) base = fs::path(h) / ".local" / "share";
#endif
  if (base.empty()) return {};
  return base / "OpenTacticsEngine" / "editor" / "learn.json";
}

ApiResult getLearn(const Config& cfg) {
  std::string text;
  if (cfg.learnFile.empty()) text = g_memory;
  else if (!readWholeFile(cfg.learnFile, text)) return apiOk(Json::makeObject());  // not there yet: no state
  Json state;
  std::string err;
  if (text.empty()) return apiOk(Json::makeObject());
  if (text.size() > kMaxLearnBytes || !Json::parse(text, state, err) || !state.isObject()) {
    elog("learn state %s is damaged or too large; treating it as empty", cfg.learnFile.string().c_str());
    return apiOk(Json::makeObject());
  }
  return apiOk(state);
}

ApiResult putLearn(const Config& cfg, const std::string& bodyText) {
  if (bodyText.size() > kMaxLearnBytes) return apiError(413, "the Learn state is larger than 64 KB");
  Json state;
  std::string err;
  if (bodyText.empty()) return apiError(400, "request body is empty");
  if (!Json::parse(bodyText, state, err)) return apiError(400, "invalid JSON body: " + err);
  if (!state.isObject()) return apiError(400, "the Learn state must be a JSON object");
  std::string text = state.dump(0);  // stored compactly
  if (cfg.learnFile.empty()) { g_memory = text; return apiOk(state); }
  if (!writeFileAtomic(cfg.learnFile, text, err)) {
    elog("learn state: %s", err.c_str());
    return apiError(500, "could not save the Learn state: " + err);
  }
  return apiOk(state);
}

}  // namespace ote_editor
