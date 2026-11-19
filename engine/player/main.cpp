// ote-player: runs a game folder (see docs/GAME_FORMAT.md) with the platform backend chosen at build time (SDL3 or SDL2).
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

#include "app.h"
#include "args.h"
#include "core/base/log.h"
#include "sound/audio_render.h"
#include "platform/common/android_roots.h"
#include "platform/platform.h"
#include "platform/platform_main.h"  // SDL_main on the platforms that need it; must stay in this file

using namespace ote;

int main(int argc, char** argv) {
  attachParentConsole();  // Windows GUI program started from a terminal: show its output there (no-op elsewhere)
#ifdef __ANDROID__
  installAndroidLogSink();  // stderr is not shown on Android; logcat tag "OTE"
  // Debug aid: player arguments and environment from the launching intent's extras (docs/ANDROID.md). Before the log level
  // is read below, so ote_env can set OTE_LOG_LEVEL.
  std::vector<std::string> intentArgs = androidIntentArgs();
  std::vector<char*> intentArgv(1, argc > 0 ? argv[0] : const_cast<char*>("ote-player"));
  for (std::string& a : intentArgs) intentArgv.push_back(&a[0]);
  if (!intentArgs.empty()) {
    argc = (int)intentArgv.size();
    argv = intentArgv.data();
  }
#endif
  // A packaged app that cannot be given a command line (the UWP app) may take one from a file; empty everywhere else.
  // NAME=value words in it go into the environment, so this runs before OTE_LOG_LEVEL is read.
  std::vector<std::string> fileArgs = platformLaunchArgs();
  std::vector<char*> fileArgv(1, argc > 0 ? argv[0] : const_cast<char*>("ote-player"));
  for (std::string& a : fileArgs) fileArgv.push_back(&a[0]);
  if (!fileArgs.empty()) {
    argc = (int)fileArgv.size();
    argv = fileArgv.data();
  }
  // OTE_LOG_LEVEL=debug shows the debug lines too (the walk directions; tests read them). Normal runs log info and above only.
  if (const char* lv = std::getenv("OTE_LOG_LEVEL"))
    if (!std::strcmp(lv, "debug")) setLogLevel(LogLevel::Debug);
  Args args = parseArgs(argc, argv);
  if (args.help || !args.error.empty()) {
    if (!args.error.empty()) fprintf(stderr, "error: %s\n", args.error.c_str());
    fputs(usageText(), args.help ? stdout : stderr);
    return args.help ? 0 : 1;
  }

  applyContentRoots(args);

  // Offline mixing needs neither a window nor a sound device.
  if (!args.audioRender.empty()) return runAudioRender(args);

  PlatformConfig cfg;
  cfg.title = "OpenTacticsEngine";
  cfg.windowScale = args.scale;
  cfg.headless = args.headless;
  cfg.headlessScale = args.scale3d;
  cfg.fixedDt = args.fixedDt;
  cfg.maxFrames = args.frames;
  cfg.screenshotPath = args.screenshot;

  PlayerApp app(args);
  std::string err;
  if (!app.start(err)) {
    fprintf(stderr, "error: %s\n", err.c_str());
    return 1;
  }
  return runApp(app, cfg);
}
