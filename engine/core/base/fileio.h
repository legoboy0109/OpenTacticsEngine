// openFile(): fopen() for the engine's UTF-8 paths. Every file the engine reads or writes through the C library goes
// through here (and nothing else in the engine calls fopen).
//
// Why it exists: engine paths are UTF-8 `char` strings everywhere (game folders, the user's folder, file names inside
// games). On Linux, macOS, Android and the web fopen() takes exactly that. On Windows fopen() would read the string in
// the ANSI code page, so a user name or game folder with a non-ASCII letter would not open; there the path is converted
// to UTF-16 and opened with _wfopen. This is the only place with that difference (fileio.cpp). It lives in core/base
// (not in platform/) because the rules core loads game files too and may not depend on the platform layer.
#pragma once
#include <cstdio>
#include <string>

namespace ote {

// Opens `utf8Path` with the C library's mode string ("rb", "wb", ...; ASCII only). nullptr on failure, like fopen.
std::FILE* openFile(const std::string& utf8Path, const char* mode);

#if defined(_WIN32)
// UTF-8 -> UTF-16 for the Win32 "W" calls (shared with the platform layer's folder functions). Invalid input is
// replaced, never rejected. Empty in, empty out.
std::wstring utf8ToWide(const std::string& utf8);
// UTF-16 -> UTF-8 (file names returned by FindNextFileW and the like).
std::string wideToUtf8(const std::wstring& wide);
#endif

}  // namespace ote
