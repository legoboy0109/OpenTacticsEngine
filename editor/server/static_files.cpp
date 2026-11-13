#include "static_files.h"

#include <algorithm>
#include <cctype>

#include "paths.h"

namespace ote_editor {

std::string mimeFor(const std::string& path) {
  size_t dot = path.rfind('.');
  std::string ext = dot == std::string::npos ? "" : path.substr(dot);
  std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
  static const struct { const char* ext; const char* mime; } table[] = {
      {".html", "text/html; charset=utf-8"},
      {".js", "text/javascript; charset=utf-8"},
      {".mjs", "text/javascript; charset=utf-8"},
      {".css", "text/css; charset=utf-8"},
      {".json", "application/json"},
      {".wasm", "application/wasm"},  // browsers refuse to stream-compile it otherwise
      {".png", "image/png"},
      {".svg", "image/svg+xml"},
      {".ttf", "font/ttf"},
      {".otf", "font/otf"},
      {".ico", "image/x-icon"},
      {".map", "application/json"},
      {".txt", "text/plain; charset=utf-8"},
  };
  for (const auto& e : table)
    if (ext == e.ext) return e.mime;
  return "application/octet-stream";
}

static bool loadFile(const fs::path& root, const std::string& rel, StaticResponse& out, std::string& why) {
  fs::path full;
  if (!resolveInside(root, rel, full, why)) return false;
  std::error_code ec;
  if (!fs::is_regular_file(full, ec) || !readWholeFile(full, out.body)) { why = "not found"; return false; }
  out.mime = mimeFor(full.string());
  return true;
}

StaticResponse serveFromDir(const fs::path& root, const std::string& rel, bool spaFallback) {
  StaticResponse r;
  std::string why;
  std::string want = rel.empty() ? "index.html" : rel;
  if (loadFile(root, want, r, why)) return r;
  if (spaFallback) {
    r = StaticResponse();
    if (loadFile(root, "index.html", r, why)) return r;
  }
  r = StaticResponse();
  r.status = (why == "not found" || spaFallback) ? 404 : 400;
  r.mime = "text/plain; charset=utf-8";
  r.body = r.status == 404 ? "Not found\n" : "Bad path: " + why + "\n";
  return r;
}

StaticResponse builtinIndexPage(const Config& cfg) {
  StaticResponse r;
  r.mime = "text/html; charset=utf-8";
  r.body =
      "<!doctype html><html lang=\"en\"><head><meta charset=\"utf-8\">"
      "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">"
      "<title>OpenTacticsEngine editor</title>"
      "<style>body{font:16px/1.5 system-ui,sans-serif;max-width:40rem;margin:3rem auto;padding:0 16px}"
      "code{background:#8882;padding:.1em .3em;border-radius:3px}</style></head><body>"
      "<h1>OpenTacticsEngine editor helper</h1>"
      "<p>The helper is running, but the editor web UI is not built yet "
      "(expected in <code>" +
      cfg.webDir.generic_string() +
      "</code>).</p>"
      "<p>The API is available at <a href=\"/api/info\"><code>/api/</code></a> - for example "
      "<a href=\"/api/projects\"><code>/api/projects</code></a>.</p></body></html>\n";
  return r;
}

}  // namespace ote_editor
