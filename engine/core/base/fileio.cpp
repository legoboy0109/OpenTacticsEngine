#include "core/base/fileio.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace ote {

#if defined(_WIN32)
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
#else
std::FILE* openFile(const std::string& utf8Path, const char* mode) { return std::fopen(utf8Path.c_str(), mode); }
#endif

}  // namespace ote
