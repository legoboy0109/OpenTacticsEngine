#include "core/base/fileio.h"

#if defined(OTE_NXDK)
#include "core/base/fatx_names.h"
#include "core/base/log.h"
#endif

#if defined(_WIN32) && !defined(OTE_NXDK)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ote {

#if defined(_WIN32) && !defined(OTE_NXDK)
std::wstring utf8ToWide(const std::string& utf8) {
  if (utf8.empty()) return std::wstring();
  int n = MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), nullptr, 0);
  if (n <= 0) return std::wstring();
  std::wstring w((size_t)n, L'\0');
  MultiByteToWideChar(CP_UTF8, 0, utf8.data(), (int)utf8.size(), &w[0], n);
  return w;
}

std::string wideToUtf8(const std::wstring& wide) {
  if (wide.empty()) return std::string();
  int n = WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), nullptr, 0, nullptr, nullptr);
  if (n <= 0) return std::string();
  std::string s((size_t)n, '\0');
  WideCharToMultiByte(CP_UTF8, 0, wide.data(), (int)wide.size(), &s[0], n, nullptr, nullptr);
  return s;
}

std::FILE* openFile(const std::string& utf8Path, const char* mode) {
  std::wstring wmode;
  for (const char* m = mode; *m; ++m) wmode.push_back((wchar_t)(unsigned char)*m);
  std::FILE* f = nullptr;
  if (_wfopen_s(&f, utf8ToWide(utf8Path).c_str(), wmode.c_str()) != 0) return nullptr;
  return f;
}
#elif defined(OTE_NXDK)
// Original Xbox (nxdk): _WIN32 is defined, but there is no UTF-16 API and the kernel only knows backslashes
// ("D:\\games\\demo-srpg\\game.json"). Paths are used as bytes.
std::FILE* openFile(const std::string& utf8Path, const char* mode) {
  std::string p = utf8Path;
  for (char& c : p)
    if (c == '/') c = '\\';
  std::FILE* f = std::fopen(p.c_str(), mode);
  if (!f) {
    // A name the hard disk's file system cannot have is a clear error here, not a silent "file not found" (docs/XBOX_OG.md).
    // Only reported when the open failed, so reading a game that exists costs nothing.
    std::vector<std::string> bad = fatxPathProblems(p.size() > 2 && p[1] == ':' ? p.substr(2) : p);
    for (const std::string& b : bad) OTE_ERROR("cannot open a file whose name the Xbox disk does not allow: %s", b.c_str());
  }
  return f;
}
#else
std::FILE* openFile(const std::string& utf8Path, const char* mode) { return std::fopen(utf8Path.c_str(), mode); }
#endif

}  // namespace ote
