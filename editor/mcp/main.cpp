// ote-mcp: an MCP (Model Context Protocol) server for OpenTacticsEngine games, over stdio. See docs/MCP.md.
// stdout carries protocol messages and nothing else; every log line goes to stderr. No network port is ever opened.
#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "context.h"
#include "platform.h"
#include "rpc.h"
#include "tools.h"

#ifndef OTE_MCP_DEFAULT_TEMPLATES
#define OTE_MCP_DEFAULT_TEMPLATES ""
#endif

namespace {

using namespace ote_mcp;

constexpr size_t kMaxLine = 4u << 20;  // a request line longer than this is refused (4 MiB)

void usage() {
  std::fprintf(stderr,
               "usage: ote-mcp [--root DIR]... [--read-only] [--player PATH] [--sim PATH] [--assets DIR]\n"
               "               [--templates DIR] [--schema-dir DIR]\n"
               "\n"
               "An MCP server (stdio) that lets an AI assistant build and edit OpenTacticsEngine games.\n"
               "  --root DIR        a folder that holds game folders (repeatable; default: the current folder).\n"
               "                    Only files below the roots are ever touched.\n"
               "  --read-only       offer no tool that writes anything.\n"
               "  --player PATH     ote-player, for render_map (default: found next to ote-mcp or in the build tree).\n"
               "  --sim PATH        ote-sim, for simulate (same search).\n"
               "  --assets DIR      the assets folder the player uses for render_map.\n"
               "  --templates DIR   the game templates for create_game (default: found near the program).\n"
               "  --schema-dir DIR  read the editor schemas from here instead of the copy built in (also OTE_SCHEMA_DIR).\n"
               "Environment: OTE_PLAYER, OTE_SIM, OTE_ASSETS, OTE_TEMPLATES_DIR, OTE_SCHEMA_DIR.\n"
               "Documentation: docs/MCP.md\n");
}

bool exists(const fs::path& p, bool dir) {
  std::error_code ec;
  return dir ? fs::is_directory(p, ec) : fs::is_regular_file(p, ec);
}

fs::path firstExisting(const std::vector<fs::path>& candidates, bool dir) {
  for (const fs::path& c : candidates)
    if (!c.empty() && exists(c, dir)) {
      std::error_code ec;
      return fs::weakly_canonical(c, ec);
    }
  return fs::path();
}

const char* envOr(const char* name) {
  const char* v = std::getenv(name);
  return v && *v ? v : nullptr;
}

// Reads one line (without the line end). `tooLong` is set when it exceeded kMaxLine (the rest is skipped). False at end of input.
bool readLine(std::string& line, bool& tooLong) {
  line.clear();
  tooLong = false;
  bool any = false;
  for (;;) {
    const int c = std::getc(stdin);
    if (c == EOF) return any;
    any = true;
    if (c == '\n') return true;
    if (tooLong) continue;
    if (line.size() >= kMaxLine) { tooLong = true; line.clear(); continue; }
    line.push_back((char)c);
  }
}

bool writeLine(const std::string& s) {
  if (s.empty()) return true;
  if (std::fwrite(s.data(), 1, s.size(), stdout) != s.size() || std::fputc('\n', stdout) == EOF) return false;
  return std::fflush(stdout) == 0;
}

}  // namespace

int main(int argc, char** argv) {
  setBinaryStdio();
  Config cfg;
  std::vector<std::string> roots;
  std::string player, sim, assets, templates, schemaDir;
  if (const char* v = envOr("OTE_PLAYER")) player = v;
  if (const char* v = envOr("OTE_SIM")) sim = v;
  if (const char* v = envOr("OTE_ASSETS")) assets = v;
  if (const char* v = envOr("OTE_TEMPLATES_DIR")) templates = v;
  if (const char* v = envOr("OTE_SCHEMA_DIR")) schemaDir = v;
  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto value = [&](std::string& out) {
      if (i + 1 >= argc) { std::fprintf(stderr, "ote-mcp: %s needs a value\n", a.c_str()); return false; }
      out = argv[++i];
      return true;
    };
    std::string v;
    if (a == "--help" || a == "-h") { usage(); return 0; }
    else if (a == "--version") { std::printf("ote-mcp %s\n", Server::kServerVersion); return 0; }
    else if (a == "--read-only") cfg.readOnly = true;
    else if (a == "--root") { if (!value(v)) return 2; roots.push_back(v); }
    else if (a == "--player") { if (!value(player)) return 2; }
    else if (a == "--sim") { if (!value(sim)) return 2; }
    else if (a == "--assets") { if (!value(assets)) return 2; }
    else if (a == "--templates") { if (!value(templates)) return 2; }
    else if (a == "--schema-dir") { if (!value(schemaDir)) return 2; }
    else { std::fprintf(stderr, "ote-mcp: unknown argument %s\n", a.c_str()); usage(); return 2; }
  }
  if (roots.empty()) roots.push_back(".");
  for (const std::string& r : roots) {
    std::error_code ec;
    const fs::path canon = fs::weakly_canonical(fs::path(r), ec);
    if (ec || !fs::is_directory(canon, ec)) {
      std::fprintf(stderr, "ote-mcp: the root folder \"%s\" does not exist (create it, or pass another --root)\n", r.c_str());
      return 2;
    }
    cfg.roots.push_back(canon);
  }

  cfg.exeDir = executableDir(argv[0]);
  const fs::path exe = cfg.exeDir;
  const char* exeSuffix =
#if defined(_WIN32)
      ".exe";
#else
      "";
#endif
  // Helper programs: explicit path, else next to ote-mcp, else the build tree layout (ote-player lives in engine/player/).
  cfg.player = !player.empty() ? fs::path(player)
                               : firstExisting({exe / (std::string("ote-player") + exeSuffix), exe / "engine" / "player" / (std::string("ote-player") + exeSuffix),
                                                exe.parent_path() / "engine" / "player" / (std::string("ote-player") + exeSuffix)}, false);
  cfg.sim = !sim.empty() ? fs::path(sim) : firstExisting({exe / (std::string("ote-sim") + exeSuffix), exe.parent_path() / (std::string("ote-sim") + exeSuffix)}, false);
  if (!cfg.player.empty() && !exists(cfg.player, false)) { std::fprintf(stderr, "ote-mcp: warning: --player %s is not a file; render_map will not work\n", cfg.player.string().c_str()); cfg.player.clear(); }
  if (!cfg.sim.empty() && !exists(cfg.sim, false)) { std::fprintf(stderr, "ote-mcp: warning: --sim %s is not a file; simulate will not work\n", cfg.sim.string().c_str()); cfg.sim.clear(); }
  if (!cfg.player.empty()) {
    const fs::path pd = fs::path(cfg.player).parent_path();
    cfg.assets = !assets.empty() ? fs::path(assets) : firstExisting({pd / "assets", pd.parent_path() / "assets", exe / "assets", exe.parent_path() / "assets", pd.parent_path().parent_path() / "assets"}, true);
  } else if (!assets.empty()) {
    cfg.assets = assets;
  }
  cfg.templatesDir = !templates.empty() ? fs::path(templates)
                                        : firstExisting({exe / "templates", exe / "editor" / "templates", exe.parent_path() / "editor" / "templates",
                                                         exe.parent_path().parent_path() / "editor" / "templates", fs::path(OTE_MCP_DEFAULT_TEMPLATES)}, true);
  cfg.schemaDir = schemaDir;

  Context ctx(cfg);
  ToolRegistry tools;
  registerDiscoveryTools(tools, ctx);
  registerEditTools(tools, ctx);
  registerRenameTool(tools, ctx);
  registerMapTools(tools, ctx);
  registerGameTools(tools, ctx);
  registerRunTools(tools, ctx);
  Server server(ctx, tools);

  logInfo(std::string("started; protocol revisions 2026-07-28, 2025-11-25, 2025-06-18, 2025-03-26, 2024-11-05; ") + (cfg.readOnly ? "READ-ONLY; " : "") + "schemas: " + ctx.schemaSource);
  for (const fs::path& r : cfg.roots) logInfo("root: " + r.string());
  logInfo(std::string("player: ") + (cfg.player.empty() ? "not found (render_map off)" : cfg.player.string()) + "; sim: " + (cfg.sim.empty() ? "not found (simulate off)" : cfg.sim.string()) +
          "; templates: " + (cfg.templatesDir.empty() ? "not found (create_game off)" : cfg.templatesDir.string()));

  std::string line;
  bool tooLong = false;
  while (readLine(line, tooLong)) {
    std::string answer;
    if (tooLong) {
      logWarn("a request line over " + std::to_string(kMaxLine) + " bytes was refused");
      answer = Server::errorLine(-32600, "Invalid Request: the message is longer than " + std::to_string(kMaxLine) + " bytes");
    } else {
      if (!line.empty() && line.back() == '\r') line.pop_back();
      if (line.find_first_not_of(" \t") == std::string::npos) continue;
      answer = server.handleLine(line);
    }
    if (!writeLine(answer)) { logError("cannot write to stdout: the client has gone"); return 1; }
  }
  logInfo("input closed, exiting");
  return 0;
}
