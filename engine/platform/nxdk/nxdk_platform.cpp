// Original Xbox (nxdk) platform pieces: user data folder, log file, args file, video mode, memory, dashboard exit.
// See nxdk.h and docs/XBOX_OG.md.
#ifdef OTE_NXDK
#include "platform/nxdk/nxdk.h"

#include <hal/video.h>
#include <hal/xbox.h>
#include <nxdk/mount.h>
#include <windows.h>
#include <xboxkrnl/xboxkrnl.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/base/fileio.h"
#include "core/base/log.h"
#include "platform/common/launch_args.h"
#include "platform/nxdk/video_pick.h"
#include "platform/platform.h"

extern "C" int ote_setenv(const char* name, const char* value);  // tools/xbox-og/nx_compat.c (nxdk has no putenv)

namespace ote {
namespace {

std::string g_userDir;
bool g_userDecided = false, g_userFallback = false;
HANDLE g_log = INVALID_HANDLE_VALUE;
unsigned long g_logLines = 0;
DWORD g_lastFlush = 0;
NxdkScreen g_screen;

std::string backslashes(std::string p) {
  for (char& c : p)
    if (c == '/') c = '\\';
  return p;
}

// Creates every folder of `path` ("E:/a/b/c"), one level at a time. True if the last one exists afterwards.
bool makeDirChain(const std::string& path) {
  std::string p = backslashes(path);
  for (size_t i = 3; i <= p.size(); ++i) {  // skip "E:\"
    if (i < p.size() && p[i] != '\\') continue;
    std::string part = p.substr(0, i);
    if (!CreateDirectoryA(part.c_str(), nullptr) && GetLastError() != ERROR_ALREADY_EXISTS) return false;
  }
  return true;
}

// A folder counts as writable when a file can be made in it and removed again. A disc (or any read-only volume) fails here.
bool writableDir(const std::string& dir) {
  if (!makeDirChain(dir)) return false;
  std::string probe = dir + "/.write-test";
  std::FILE* f = openFile(probe, "wb");
  if (!f) return false;
  std::fputs("x", f);
  std::fclose(f);
  DeleteFileA(backslashes(probe).c_str());
  return true;
}

const char* kFallbackDir = "E:/Apps/OpenTacticsEngine/userdata";

std::string readWhole(const std::string& path) {
  std::string text;
  std::FILE* f = openFile(path, "rb");
  if (!f) return text;
  char buf[512];
  size_t n;
  while ((n = readChunk(f, buf, sizeof buf)) > 0) text.append(buf, n);
  std::fclose(f);
  return text;
}

// Recent log lines for the overlay.
constexpr int kKeep = 12;
char g_recent[kKeep][96];
int g_recentCount = 0;

void fileSink(LogLevel level, const char* message) {
  static const char* names[] = {"debug", "info", "warn", "ERROR"};
  char line[1200];
  // (no %f anywhere in the Xbox build: nxdk's printf has no floating-point conversion, docs/XBOX_OG.md "nxdk traps")
  const unsigned long ms = (unsigned long)GetTickCount();
  std::snprintf(line, sizeof line, "[%5lu.%03lu] [%s] %s\n", ms / 1000, ms % 1000, names[(int)level], message);
  if (g_log != INVALID_HANDLE_VALUE) {
    // Straight to the kernel (not stdio): FATX keeps written data in the kernel's cache until the file is closed or
    // flushed, so without NtFlushBuffersFile a crash (or an emulator kill) leaves an empty log. Flushing every line is slow
    // on a real disk, so: every line for the first 200 (start-up is where it breaks), warnings and errors at once, then
    // at most twice a second.
    DWORD wrote = 0;
    WriteFile(g_log, line, (DWORD)std::strlen(line), &wrote, nullptr);
    const DWORD now = GetTickCount();
    if (g_logLines++ < 200 || level >= LogLevel::Warn || now - g_lastFlush >= 500) {
      IO_STATUS_BLOCK io;
      NtFlushBuffersFile(g_log, &io);
      g_lastFlush = now;
    }
  }
  std::snprintf(g_recent[g_recentCount % kKeep], sizeof g_recent[0], "%s%s", level >= LogLevel::Warn ? "! " : "", message);
  ++g_recentCount;
}

}  // namespace

std::vector<std::string> nxdkRecentLog(int max) {
  std::vector<std::string> out;
  int total = g_recentCount < kKeep ? g_recentCount : kKeep;
  int first = g_recentCount - total;
  if (max < total) {
    first += total - max;
    total = max;
  }
  for (int i = 0; i < total; ++i) out.push_back(g_recent[(first + i) % kKeep]);
  return out;
}

std::string nxdkAppDir() { return "D:"; }

std::string nxdkUserDataDir() {
  if (g_userDecided) return g_userDir;
  g_userDecided = true;
  const std::string primary = nxdkAppDir() + "/userdata";
  if (writableDir(primary)) {
    g_userDir = primary;
    return g_userDir;
  }
  g_userFallback = true;
  if (!nxIsDriveMounted('E')) nxMountDrive('E', "\\Device\\Harddisk0\\Partition1\\");
  if (writableDir(kFallbackDir)) g_userDir = kFallbackDir;
  return g_userDir;
}

bool nxdkUserDataIsFallback() { return g_userFallback; }

void nxdkInstallLog() {
  std::string dir = nxdkUserDataDir();
  if (dir.empty()) return;  // nowhere to write: no file log, no saves
  std::string path = dir + "/log.txt";
  replaceFile(path, dir + "/log-previous.txt");  // false when there was no previous log: fine
  g_log = CreateFileA(backslashes(path).c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
  if (g_log == INVALID_HANDLE_VALUE) return;
  setLogSink(fileSink);
  OTE_INFO("OpenTacticsEngine for the original Xbox, log file %s", path.c_str());
  if (g_userFallback)
    OTE_WARN("the launch folder is not writable (a disc?): settings and saves go to %s instead", dir.c_str());
  else
    OTE_INFO("user data (settings, saves, log) in the launch folder: %s", dir.c_str());
}

std::vector<std::string> nxdkLaunchArgs() {
  std::vector<std::pair<std::string, std::string>> env;
  std::vector<std::string> out = parseLaunchArgs(readWhole(nxdkAppDir() + "/args.txt"), env);
  std::string dir = nxdkUserDataDir();
  if (!dir.empty()) {
    std::vector<std::string> more = parseLaunchArgs(readWhole(dir + "/args.txt"), env);
    if (!more.empty()) out = more;  // the hard-disk file wins over the one shipped with the app
  }
  for (const auto& e : env) ote_setenv(e.first.c_str(), e.second.c_str());
  if (!out.empty() || !env.empty()) OTE_INFO("args file: %d arguments, %d environment words", (int)out.size(), (int)env.size());
  return out;
}

std::string nxdkMemoryStatus() {
  MM_STATISTICS st;
  std::memset(&st, 0, sizeof st);
  st.Length = sizeof st;
  if (MmQueryStatistics(&st) != 0) return "";
  char buf[64];
  std::snprintf(buf, sizeof buf, "used %lu MB of %lu MB", (unsigned long)((st.TotalPhysicalPages - st.AvailablePages) * 4ull / 1024ull),
                (unsigned long)(st.TotalPhysicalPages * 4ull / 1024ull));
  return buf;
}

NxdkScreen nxdkPrepareVideo() {
  const DWORD enc = XVideoGetEncoderSettings();
  VideoWish wish;
  wish.widescreen = (enc & VIDEO_WIDESCREEN) != 0;
  if (const char* p = std::getenv("OTE_VIDEO")) wish.preference = p;
  std::vector<VideoModeInfo> modes;
  void* ctx = nullptr;
  VIDEO_MODE vm;
  OTE_INFO("video: encoder settings 0x%08lX (widescreen %s, 480p %s, 720p %s, 1080i %s)", (unsigned long)enc,
           wish.widescreen ? "yes" : "no", (enc & VIDEO_MODE_480P) ? "yes" : "no", (enc & VIDEO_MODE_720P) ? "yes" : "no",
           (enc & VIDEO_MODE_1080I) ? "yes" : "no");
  while (XVideoListModes(&vm, 32, 0, &ctx)) {
    VideoModeInfo m;
    m.w = vm.width;
    m.h = vm.height;
    modes.push_back(m);
    OTE_INFO("video: mode offered %dx%d %d bpp %d Hz", vm.width, vm.height, vm.bpp, vm.refresh);
  }
  int pick = pickVideoMode(modes, wish);
  VIDEO_MODE cur = XVideoGetMode();
  if (pick >= 0) {
    if (!XVideoSetMode(modes[(size_t)pick].w, modes[(size_t)pick].h, 32, REFRESH_DEFAULT))
      OTE_WARN("video: cannot set %dx%d, keeping the current mode", modes[(size_t)pick].w, modes[(size_t)pick].h);
  } else {
    OTE_WARN("video: no usable mode listed, keeping the current %dx%d", cur.width, cur.height);
  }
  cur = XVideoGetMode();
  g_screen.w = cur.width;
  g_screen.h = cur.height;
  g_screen.widescreen = wish.widescreen;
  g_screen.highDefinition = cur.height >= 720;
  g_screen.anamorphic = wish.widescreen && cur.width == 640 && cur.height == 480;
  OTE_INFO("video: using %dx%d %d bpp (%s%s, preference '%s')", cur.width, cur.height, cur.bpp,
           g_screen.highDefinition ? "high definition" : "standard definition", g_screen.anamorphic ? ", 16:9 anamorphic" : "",
           wish.preference.c_str());
  return g_screen;
}

const NxdkScreen& nxdkScreen() { return g_screen; }

bool nxdkReplaceFile(const std::string& from, const std::string& to) {
  // MoveFileA of nxdk asks for "do not replace"; ask the kernel for "replace if it exists" (what POSIX rename does), so
  // a save is swapped in one step and never missing in between.
  std::string f = backslashes(from), t = backslashes(to);
  ANSI_STRING path;
  RtlInitAnsiString(&path, f.c_str());
  OBJECT_ATTRIBUTES attr;
  InitializeObjectAttributes(&attr, &path, OBJ_CASE_INSENSITIVE, ObDosDevicesDirectory(), NULL);
  HANDLE h;
  IO_STATUS_BLOCK io;
  NTSTATUS st = NtOpenFile(&h, DELETE | SYNCHRONIZE, &attr, &io, FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                           FILE_SYNCHRONOUS_IO_NONALERT | FILE_OPEN_FOR_BACKUP_INTENT);
  if (!NT_SUCCESS(st)) return false;
  FILE_RENAME_INFORMATION info;
  info.ReplaceIfExists = TRUE;
  info.RootDirectory = ObDosDevicesDirectory();
  RtlInitAnsiString(&info.FileName, t.c_str());
  st = NtSetInformationFile(h, &io, &info, sizeof info, FileRenameInformation);
  NtClose(h);
  return NT_SUCCESS(st);
}

void nxdkReturnToDashboard() {
  OTE_INFO("leaving for the dashboard");
  if (g_log != INVALID_HANDLE_VALUE) {
    IO_STATUS_BLOCK io;
    NtFlushBuffersFile(g_log, &io);
  }
  // A null path = "launch the dashboard" (LDT_LAUNCH_DASHBOARD in the launch data page, then a quick reboot into the BIOS,
  // which starts whatever dashboard the console has); nxdk's XReboot would be a plain restart instead.
  XLaunchXBEEx(nullptr, nullptr);
  HalReturnToFirmware(HalQuickRebootRoutine);  // only reached if the launch call returned (it could not allocate its page)
  for (;;) Sleep(1000);  // not reached
}

}  // namespace ote
#endif  // OTE_NXDK
