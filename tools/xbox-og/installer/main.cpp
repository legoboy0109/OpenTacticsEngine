// Test vehicle for the original-Xbox player in an emulator (docs/XBOX_OG.md "Testing in xemu"). NOT how users install: they
// copy the app folder to the hard disk themselves. xemu can only start discs, and there is no FATX writer on the Linux side,
// so this tiny program does the copy on the emulated console:
//   mode "install": copies D:\payload (the app folder) to E:\Apps\OpenTacticsEngine, leaving an existing userdata\ alone,
//   mode "launch":  copies nothing;
// then it starts E:\Apps\OpenTacticsEngine\default.xbe. The mode is the word in D:\mode.txt. Everything it does is printed
// on the screen (so a screenshot shows what happened) and appended to E:\Apps\installer-log.txt.
#include <hal/debug.h>
#include <hal/video.h>
#include <hal/xbox.h>
#include <nxdk/mount.h>
#include <windows.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace {

const char* kDest = "E:\\Apps\\OpenTacticsEngine";
int g_files = 0, g_errors = 0;
unsigned long long g_bytes = 0;
FILE* g_log = nullptr;

void say(const char* fmt, const char* a = "", const char* b = "") {
  char line[300];
  std::snprintf(line, sizeof line, fmt, a, b);
  debugPrint("%.78s\n", line);
  if (g_log) {
    std::fprintf(g_log, "%s\n", line);
    std::fflush(g_log);
  }
}

bool isDir(const std::string& p) {
  DWORD a = GetFileAttributesA(p.c_str());
  return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY);
}

void copyTree(const std::string& from, const std::string& to, int depth) {
  if (!CreateDirectoryA(to.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) {
    say("cannot make folder %s", to.c_str());
    ++g_errors;
    return;
  }
  WIN32_FIND_DATAA fd;
  HANDLE h = FindFirstFileA((from + "\\*").c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) return;
  do {
    if (g_errors >= 3) break;  // enough to see what is wrong, and short enough to read on one screen
    std::string name = fd.cFileName;
    if (name == "." || name == "..") continue;
    if (depth == 0 && name == "userdata") continue;  // saves and settings of an earlier run stay
    std::string src = from + "\\" + name, dst = to + "\\" + name;
    if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
      copyTree(src, dst, depth + 1);
    } else {
      // nxdk's CopyFileA cannot overwrite (FILE_SUPERSEDE fails on FATX), so an old copy is deleted first.
      // A file from a disc is read-only, and CopyFileA copies that attribute: clear it so the old copy can go.
      SetFileAttributesA(dst.c_str(), FILE_ATTRIBUTE_NORMAL);
      BOOL del = DeleteFileA(dst.c_str());
      DWORD delErr = del ? 0 : GetLastError();
      if (CopyFileA(src.c_str(), dst.c_str(), TRUE)) {
        SetFileAttributesA(dst.c_str(), FILE_ATTRIBUTE_NORMAL);  // the copy is read-only too
        ++g_files;
        g_bytes += fd.nFileSizeLow;
      } else {
        char e[64];
        std::snprintf(e, sizeof e, "delete error %lu, copy error %lu", (unsigned long)delErr, (unsigned long)GetLastError());
        say("COPY FAILED: %s", e);
        say("  file: %s", name.c_str());
        ++g_errors;
      }
    }
  } while (FindNextFileA(h, &fd));
  FindClose(h);
}

void listDir(const char* path) {
  WIN32_FIND_DATAA fd;
  std::string pat = std::string(path) + "\\*";
  HANDLE h = FindFirstFileA(pat.c_str(), &fd);
  if (h == INVALID_HANDLE_VALUE) {
    say("%s: not readable", path);
    return;
  }
  std::string all;
  do {
    all += fd.cFileName;
    all += (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? "/ " : " ";
  } while (FindNextFileA(h, &fd));
  FindClose(h);
  say("%s : %s", path, all.c_str());
}

}  // namespace

int main() {
  XVideoSetMode(640, 480, 32, REFRESH_DEFAULT);
  debugPrint("OpenTacticsEngine test installer\n");
  if (!nxIsDriveMounted('E')) nxMountDrive('E', "\\Device\\Harddisk0\\Partition1\\");
  CreateDirectoryA("E:\\Apps", nullptr);
  g_log = std::fopen("E:\\Apps\\installer-log.txt", "ab");
  say("start", "", "");
  listDir("E:");
  listDir("E:\\Apps");
  char mode[16] = "";
  if (FILE* m = std::fopen("D:\\mode.txt", "rb")) {
    size_t n = std::fread(mode, 1, sizeof mode - 1, m);
    mode[n] = 0;
    std::fclose(m);
  }
  if (std::strncmp(mode, "install", 7) == 0) {
    // A test script (args.txt) from an earlier run must not outlive a build that has none.
    SetFileAttributesA("E:\\Apps\\OpenTacticsEngine\\args.txt", FILE_ATTRIBUTE_NORMAL);
    DeleteFileA("E:\\Apps\\OpenTacticsEngine\\args.txt");
    say("copying D:\\payload to %s ...", kDest);
    copyTree("D:\\payload", kDest, 0);
    char b[80];
    std::snprintf(b, sizeof b, "%d files, %llu KB, %d errors", g_files, g_bytes / 1024, g_errors);
    say("install done: %s", b);
  } else {
    say("mode '%s': nothing to copy", mode);
  }
  if (std::strncmp(mode, "dash", 4) == 0) {  // test of the player's exit path: ask for the dashboard exactly as the player does
    say("asking for the dashboard (XLaunchXBEEx with no path)", "", "");
    if (g_log) std::fclose(g_log);
    XLaunchXBEEx(nullptr, nullptr);
    for (;;) Sleep(1000);
  }
  std::string xbe = std::string(kDest) + "\\default.xbe";
  if (GetFileAttributesA(xbe.c_str()) == INVALID_FILE_ATTRIBUTES || g_errors) {
    say("STOP: %s missing or copy had errors", xbe.c_str());
    for (;;) Sleep(1000);
  }
  // XLaunchXBE wants the kernel's own path: nxdk's drive-letter conversion does not know a drive mounted with nxMountDrive.
  const char* nt = "\\Device\\Harddisk0\\Partition1\\Apps\\OpenTacticsEngine\\default.xbe";
  say("launching %s", nt);
  if (g_log) std::fclose(g_log);
  XLaunchXBE(nt);
  for (;;) Sleep(1000);  // XLaunchXBE only returns when it failed
}
