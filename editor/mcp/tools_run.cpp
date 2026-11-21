// Tools that run the engine's own programs on a game: render_map (the headless player, silent, one screenshot) and simulate
// (ote-sim: computer-vs-computer battles for balance feedback). Both need the program next to ote-mcp (or --player / --sim).
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdlib>

#include "paths.h"
#include "platform.h"
#include "png.h"
#include "tools.h"

namespace ote_mcp {

namespace {

std::string base64(const std::string& in) {
  static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  std::string out;
  out.reserve((in.size() + 2) / 3 * 4);
  for (size_t i = 0; i < in.size(); i += 3) {
    const unsigned v = (unsigned char)in[i] << 16 | (i + 1 < in.size() ? (unsigned char)in[i + 1] << 8 : 0) | (i + 2 < in.size() ? (unsigned char)in[i + 2] : 0);
    out += tbl[v >> 18 & 63];
    out += tbl[v >> 12 & 63];
    out += i + 1 < in.size() ? tbl[v >> 6 & 63] : '=';
    out += i + 2 < in.size() ? tbl[v & 63] : '=';
  }
  return out;
}

// A scratch folder for one child run, removed when it goes out of scope. Under the system temp folder, never in a game.
struct Scratch {
  fs::path dir;
  Scratch() {
    static std::atomic<unsigned> counter{0};
    std::error_code ec;
    dir = fs::temp_directory_path(ec) / ("ote-mcp-run-" + std::to_string((unsigned long long)std::chrono::steady_clock::now().time_since_epoch().count()) + "-" + std::to_string(counter++));
    fs::create_directories(dir, ec);
  }
  ~Scratch() { std::error_code ec; fs::remove_all(dir, ec); }
};

ToolResult needProgram(const char* what, const char* target, const char* flag) {
  return toolFail(std::string(what) + " was not found, so this tool cannot run",
                  std::string("build the engine (it is the CMake target ") + target + ") so it sits next to ote-mcp or in the build tree, or start ote-mcp with " + flag + " <path>");
}

bool safeTokens(const std::string& s) {
  for (char c : s) if (!(std::isalnum((unsigned char)c) || c == ':' || c == ',' || c == '_' || c == '-')) return false;
  return true;
}

ToolResult renderMap(Context& ctx, const Json& args) {
  Game g;
  ToolResult err;
  if (!gameFromArgs(ctx, args, g, err)) return err;
  if (ctx.cfg.player.empty()) return needProgram("ote-player (the game player)", "ote-player", "--player");
  const std::string map = args["map"].asString();
  const Json* gameJson = nullptr;
  Txn txn(ctx, g);
  gameJson = txn.peek("game.json");
  bool known = false;
  std::string ids;
  if (gameJson)
    for (const Json& m : (*gameJson)["maps"].items()) {
      known = known || (m["id"].isString() && m["id"].asString() == map);
      ids += (ids.empty() ? "" : ", ") + m["id"].asString();
    }
  if (!known) return toolFail("no map \"" + map + "\" in this game (maps: " + ids + ")", "get_overview lists the maps");
  const std::string style = args["style"].asString(), extraInput = args["input"].asString();
  if (!style.empty() && !safeTokens(style)) return toolFail("style must be a style id (letters, digits, _ and -)");
  if (!safeTokens(extraInput)) return toolFail("input must be player input tokens separated by commas (e.g. right,right,confirm)", "see docs for the token list: up down left right confirm cancel menu next prev turnleft turnright tiltup tiltdown zoomin zoomout");
  std::string tokens;
  for (int i = 0; i < args["turn"].asInt(0); ++i) tokens += (tokens.empty() ? "" : ",") + std::string("turnright");
  if (!extraInput.empty()) tokens += (tokens.empty() ? "" : ",") + extraInput;
  const int tokenCount = tokens.empty() ? 0 : (int)splitOn(tokens, ',').size();
  const int frames = std::max(args["frames"].asInt(10), tokenCount * 6 + 10);

  Scratch scratch;
  const fs::path shot = scratch.dir / "shot.bmp";
  std::vector<std::string> argv{ctx.cfg.player.string(), "--headless", "--no-audio", "--no-settings", "--no-saving", "--3d", "software",
                                "--games", g.root.string(), "--game", g.id, "--map", map, "--frames", std::to_string(frames), "--fixed-dt", "0.016",
                                "--seed", "1", "--screenshot", shot.string()};
  if (!style.empty()) { argv.push_back("--style"); argv.push_back(style); }
  if (!ctx.cfg.assets.empty()) { argv.push_back("--assets"); argv.push_back(ctx.cfg.assets.string()); }
  if (args["showAll"].asBool(false)) argv.push_back("--show-all");
  if (!tokens.empty()) { argv.push_back("--input"); argv.push_back(tokens); }
  // Temp settings / save / data folders: the child never reads or writes the user's real ones, whatever it decides to do.
  const std::string home = (scratch.dir / "home").string();
  const std::vector<std::string> env{"HOME=" + home, "XDG_DATA_HOME=" + home + "/data", "XDG_CONFIG_HOME=" + home + "/config", "XDG_CACHE_HOME=" + home + "/cache",
                                     "APPDATA=" + home, "LOCALAPPDATA=" + home, "SDL_AUDIODRIVER=dummy", "SDL_VIDEODRIVER=dummy"};
  std::error_code ec;
  fs::create_directories(scratch.dir / "home", ec);
  const ProcessResult pr = runProcess(argv, env, 90000);
  if (!pr.started) return toolFail("could not start the player: " + pr.error);
  if (pr.timedOut) return toolFail("the player did not finish within 90 seconds and was stopped");
  std::string bmp;
  if (!ote_editor::readWholeFile(shot, bmp) || bmp.empty()) {
    std::string tail = pr.output.size() > 600 ? pr.output.substr(pr.output.size() - 600) : pr.output;
    return toolFail("the player produced no picture (exit code " + std::to_string(pr.exitCode) + "). Its output ended: " + sanitizeUtf8(tail), "validate shows whether the game loads at all");
  }
  Image img;
  std::string why;
  if (!decodeBmp(bmp, img, why)) return toolFail("the player's picture could not be read: " + why);
  const int maxSide = args["maxSize"].asInt(960);
  const Image shown = shrinkToFit(img, maxSide);
  const std::string png = encodePng(shown);
  Json out = jobj();
  out.set("map", jstr(map));
  Json size = jobj();
  size.set("width", jnum(img.width));
  size.set("height", jnum(img.height));
  out.set("pictureSize", size);
  if (shown.width != img.width) {
    Json sh = jobj();
    sh.set("width", jnum(shown.width));
    sh.set("height", jnum(shown.height));
    out.set("shownSize", sh);
  }
  out.set("note", jstr("the picture is the battle screen of the saved files, drawn headless; use style / turn / input to look from another side or open a menu"));
  ToolResult r = toolOk(out);
  Json image = jobj();
  image.set("type", jstr("image"));
  image.set("data", jstr(base64(png)));
  image.set("mimeType", jstr("image/png"));
  r.extraContent.push_back(image);
  return r;
}

ToolResult simulate(Context& ctx, const Json& args) {
  Game g;
  ToolResult err;
  if (!gameFromArgs(ctx, args, g, err)) return err;
  if (ctx.cfg.sim.empty()) return needProgram("ote-sim (the battle simulator)", "ote-sim", "--sim");
  const std::string map = args["map"].asString(), difficulty = args["difficulty"].asString();
  if (!difficulty.empty() && !safeTokens(difficulty)) return toolFail("difficulty must be a difficulty id");
  std::vector<std::string> argv{ctx.cfg.sim.string(), g.dir.string()};
  if (map.empty()) argv.push_back("--campaign");
  else {
    if (!safeTokens(map)) return toolFail("map must be a map id");
    argv.push_back(map);
  }
  argv.push_back("--runs"); argv.push_back(std::to_string(args["runs"].asInt(20)));
  argv.push_back("--seed"); argv.push_back(std::to_string(args["seed"].asInt(1)));
  if (args.has("maxRounds")) { argv.push_back("--max-rounds"); argv.push_back(std::to_string(args["maxRounds"].asInt(60))); }
  if (!difficulty.empty()) { argv.push_back("--difficulty"); argv.push_back(difficulty); }
  const ProcessResult pr = runProcess(argv, {}, 120000);
  if (!pr.started) return toolFail("could not start ote-sim: " + pr.error);
  if (pr.timedOut) return toolFail("the simulation did not finish within 120 seconds and was stopped", "use fewer runs");
  Json runs = jarr();
  std::string summary;
  for (const std::string& line : splitOn(pr.output, '\n')) {
    if (startsWith(line, "summary:")) { summary = line; continue; }
    if (!startsWith(line, "seed ")) continue;
    Json o = jobj();
    const size_t colon = line.find(':');
    o.set("seed", jnum(std::atof(line.substr(5, colon - 5).c_str())));
    const size_t w = line.find("winner ");
    if (w != std::string::npos) o.set("winner", jstr(line.substr(w + 7, line.find(',', w) - w - 7)));
    const size_t rd = line.find("rounds ");
    if (rd != std::string::npos) o.set("rounds", jnum(std::atof(line.c_str() + rd + 7)));
    if (runs.size() < 100) runs.push(o);
  }
  if (summary.empty()) {
    std::string tail = pr.output.size() > 800 ? pr.output.substr(pr.output.size() - 800) : pr.output;
    return toolFail("ote-sim reported an error (exit code " + std::to_string(pr.exitCode) + "): " + sanitizeUtf8(tail), "validate shows whether the game loads");
  }
  Json out = jobj();
  out.set("summary", jstr(sanitizeUtf8(summary.substr(8))));
  out.set("runs", runs);
  out.set("note", jstr("computer plays both sides, each run with a different seed; wins and rounds show whether a map or the unit stats are lopsided"));
  return toolOk(out);
}

}  // namespace

void registerRunTools(ToolRegistry& r, Context& ctx) {
  Context* c = &ctx;
  {
    Tool t;
    t.name = "render_map";
    t.title = "Render a map as a picture";
    t.description = "Shows what a battle map looks like: runs the engine's player in a hidden, silent mode (no window, no sound, temporary settings) on the saved files and returns the battle screen as a PNG image. Use it after painting a map or placing units to see the result. style picks a visual style of the game (square 2D by default; games can offer isometric or 3D styles); turn rotates the camera by quarter turns in the styles that can rotate; input adds player input tokens (comma separated: right, down, confirm, menu ...) to move the cursor or open something before the picture is taken. Needs the ote-player program; when it is missing the error says how to enable it. The picture shows saved files, so write first (not dryRun).";
    Props p{{"game", gameProp()},
            {"map", sType("string", "Map id from game.json.")},
            {"style", sType("string", "Visual style id of the game (see game.json styles); omit for the default.")},
            {"turn", sInt("Quarter turns of the camera (rotating styles).", 0, 3)},
            {"input", sType("string", "Extra player input tokens, comma separated, e.g. right,right,confirm.")},
            {"showAll", sType("boolean", "Draw a fog-of-war map without fog.")},
            {"frames", sInt("Frames to run before the picture (default 10).", 1, 600)},
            {"maxSize", sInt("Longest side of the returned picture in pixels (default 960).", 64, 4096)}};
    t.inputSchema = sObject(p, {"map"});
    t.run = [c](const Json& a) { return renderMap(*c, a); };
    r.add(t);
  }
  {
    Tool t;
    t.name = "simulate";
    t.title = "Simulate battles for balance";
    t.description = "Plays a map computer-against-computer several times with the engine's simulator (ote-sim) and reports who won each run and how many rounds it took, plus a summary: wins per side, stalls (no winner in time) and the average rounds. Use it as balance feedback after changing units, weapons or a map: a map where one side always wins in a few rounds is lopsided. Without map it plays the whole campaign. Needs the ote-sim program; when it is missing the error says how to enable it. It reads the saved files.";
    Props p{{"game", gameProp()},
            {"map", sType("string", "Map id to simulate (omit for the campaign).")},
            {"runs", sInt("Number of battles (default 20).", 1, 200)},
            {"seed", sInt("First random seed (default 1); run i uses seed+i-1.", 0, 2000000000)},
            {"maxRounds", sInt("Give up a battle after this many rounds.", 1, 999)},
            {"difficulty", sType("string", "Difficulty id of the game, if it has some.")}};
    t.inputSchema = sObject(p);
    t.run = [c](const Json& a) { return simulate(*c, a); };
    r.add(t);
  }
}

}  // namespace ote_mcp
