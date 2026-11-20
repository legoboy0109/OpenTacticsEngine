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

// fread for "read the whole file" loops. On the original Xbox (nxdk) the C library's refill read at end of file fails when
// the file size is not a multiple of the 2 KB disc sector (seen in xemu: ferror set after the last full read), so there
// the request is capped to the bytes that remain and end of file is never probed. Elsewhere it is plain fread.
inline size_t readChunk(std::FILE* f, void* buf, size_t cap) {
#if defined(OTE_NXDK)
  long pos = std::ftell(f);
  if (pos >= 0 && std::fseek(f, 0, SEEK_END) == 0) {
    long end = std::ftell(f);
    std::fseek(f, pos, SEEK_SET);
    if (end >= pos) {
      if ((size_t)(end - pos) < cap) cap = (size_t)(end - pos);
      if (cap == 0) return 0;
    }
  }
#endif
  return std::fread(buf, 1, cap, f);
}

#if defined(_WIN32) && !defined(OTE_NXDK)
// UTF-8 -> UTF-16 for the Win32 "W" calls (shared with the platform layer's folder functions). Invalid input is
// replaced, never rejected. Empty in, empty out.
std::wstring utf8ToWide(const std::string& utf8);
// UTF-16 -> UTF-8 (file names returned by FindNextFileW and the like).
std::string wideToUtf8(const std::wstring& wide);
#endif

}  // namespace ote
