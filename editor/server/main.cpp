// ote-editor: local helper for the browser game editor (docs/EDITOR_API.md).
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <thread>

#ifndef _WIN32
#include <pthread.h>
#include <unistd.h>
#endif

#include "learn.h"
#include "log.h"
#include "server.h"

using namespace ote_editor;

static void usage() {
  std::printf(
      "usage: ote-editor [--games DIR=games] [--assets DIR=assets]\n"
      "                  [--templates DIR=editor/templates] [--web DIR=editor/web/dist]\n"
      "                  [--player DIR=build-web/engine/player]\n"
      "                  [--host 127.0.0.1] [--port 8765] [--allow-host NAME]...\n"
      "                  [--learn-file FILE=<user data>/OpenTacticsEngine/editor/learn.json]\n"
      "                  (--learn-file \"\" keeps the tutorial state in memory only)\n");
}

static bool parseArgs(int argc, char** argv, Config& cfg, bool& help) {
  cfg.learnFile = defaultLearnFile();
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "-h" || a == "--help") { help = true; return true; }
    if (i + 1 >= argc) { std::fprintf(stderr, "ote-editor: %s needs a value\n", a.c_str()); return false; }
    std::string v = argv[++i];
    if (a == "--games") cfg.gamesDir = v;
    else if (a == "--assets") cfg.assetsDir = v;
    else if (a == "--templates") cfg.templatesDir = v;
    else if (a == "--web") cfg.webDir = v;
    else if (a == "--player") cfg.playerDir = v;
    else if (a == "--learn-file") cfg.learnFile = v;
    else if (a == "--host") cfg.host = v;
    else if (a == "--allow-host") cfg.allowHosts.push_back(v);
    else if (a == "--port") {
      char* end = nullptr;
      long p = std::strtol(v.c_str(), &end, 10);
      if (!end || *end || p < 1 || p > 65535) { std::fprintf(stderr, "ote-editor: bad --port \"%s\"\n", v.c_str()); return false; }
      cfg.port = (int)p;
    } else {
      std::fprintf(stderr, "ote-editor: unknown option %s\n", a.c_str());
      return false;
    }
  }
  return true;
}

static void absolutize(fs::path& p) {
  std::error_code ec;
  fs::path a = fs::absolute(p, ec);
  if (!ec) p = a.lexically_normal();
  if (!p.has_filename() && p != p.root_path()) p = p.parent_path();  // drop trailing slash
}

#ifdef _WIN32
static std::atomic<bool> g_stop{false};
static void onSignal(int) { g_stop = true; }
#endif

int main(int argc, char** argv) {
  Config cfg;
  bool help = false;
  if (!parseArgs(argc, argv, cfg, help)) { usage(); return 2; }
  if (help) { usage(); return 0; }
  for (fs::path* p : {&cfg.gamesDir, &cfg.assetsDir, &cfg.templatesDir, &cfg.webDir, &cfg.playerDir}) absolutize(*p);
  if (!cfg.learnFile.empty()) absolutize(cfg.learnFile);

  installEngineLogSink();

  std::error_code ec;
  if (!fs::is_directory(cfg.gamesDir, ec))
    elog("warning: games folder %s does not exist yet (it is created with the first new project)",
         cfg.gamesDir.string().c_str());
  if (!fs::is_directory(cfg.templatesDir, ec))
    elog("warning: templates folder %s not found - creating projects will fail", cfg.templatesDir.string().c_str());

  // Block the stop signals before any thread exists so every thread inherits
  // the mask and main can wait for them synchronously.
#ifndef _WIN32
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGINT);
  sigaddset(&set, SIGTERM);
  pthread_sigmask(SIG_BLOCK, &set, nullptr);
#else
  std::signal(SIGINT, onSignal);
  std::signal(SIGTERM, onSignal);
#endif

  EditorServer server(cfg);
  std::string err;
  if (!server.bind(err)) {
    std::fprintf(stderr, "ote-editor: %s\n", err.c_str());
    return 1;
  }

  std::printf("OpenTacticsEngine editor helper\n");
  std::printf("  Open:       http://%s:%d/\n", cfg.host.c_str(), cfg.port);
  std::printf("  Games:      %s\n", cfg.gamesDir.string().c_str());
  std::printf("  Templates:  %s\n", cfg.templatesDir.string().c_str());
  std::printf("  Tutorial:   %s\n", cfg.learnFile.empty() ? "(kept in memory only)" : cfg.learnFile.string().c_str());
  std::printf("  Press Ctrl+C to stop.\n");
  std::fflush(stdout);

  std::thread serving([&] {
    server.serve();
#ifndef _WIN32
    kill(getpid(), SIGTERM);  // wake main if the server ended by itself
#endif
  });

#ifndef _WIN32
  int sig = 0;
  sigwait(&set, &sig);
#else
  while (!g_stop) std::this_thread::sleep_for(std::chrono::milliseconds(100));
#endif
  elog("shutting down");
  server.stop();
  serving.join();
  return 0;
}
