#include "templates.h"

#include <algorithm>

#include "log.h"
#include "paths.h"
#include "validate.h"

namespace ote_editor {

using ote::Json;

namespace {
// Reads <dir>/game.json the way the server always did (the loader's reader, so comments are accepted).
bool readGameJson(const fs::path& dir, Json& out, Problem& prob) {
  std::string why;
  fs::path full;
  if (!resolveInside(dir, "game.json", full, why)) { prob = {"game.json", why}; return false; }
  std::string abs = dir.generic_string();
  while (abs.size() > 1 && abs.back() == '/') abs.pop_back();
  std::string err;
  if (!ote::loadJsonFile(abs + "/game.json", out, err)) {
    prob = problemFromMessage(dir, err);
    if (prob.file.empty()) prob.file = "game.json";
    return false;
  }
  return true;
}
}  // namespace

std::vector<TemplateInfo> listTemplates(const fs::path& templatesDir) {
  std::vector<std::string> ids;
  std::error_code ec;
  for (fs::directory_iterator it(templatesDir, ec), end; !ec && it != end; it.increment(ec)) {
    std::error_code e2;
    std::string name = it->path().filename().string();
    if (validId(name) && fs::is_regular_file(it->path() / "game.json", e2)) ids.push_back(name);
  }
  std::sort(ids.begin(), ids.end());
  std::vector<TemplateInfo> out;
  for (const std::string& id : ids) {
    Json game;
    Problem p;
    const bool ok = readGameJson(templatesDir / id, game, p) && game.isObject();
    TemplateInfo t;
    t.id = id;
    t.name = ok ? game["name"].asString(id) : id;
    t.description = ok ? game["description"].asString() : "";
    // The grid the template's game.json asks for (docs/HEX.md). Missing = squares.
    const Json& g = game["grid"];
    t.gridType = ok && g["type"].isString() ? g["type"].asString() : "square";
    if (ok && g["layout"].isString()) t.gridLayout = g["layout"].asString();
    out.push_back(t);
  }
  return out;
}

CreateResult createFromTemplate(const fs::path& templatesDir, const fs::path& gamesDir, const std::string& id,
                                const std::string& tpl, const std::string& name) {
  CreateResult res;
  std::error_code ec;
  fs::path src = templatesDir / tpl;
  if (!fs::is_regular_file(src / "game.json", ec)) { res.status = CreateStatus::BadTemplate; res.message = "unknown template \"" + tpl + "\""; return res; }
  fs::path dst = gamesDir / id;
  if (fs::exists(fs::symlink_status(dst, ec))) { res.status = CreateStatus::AlreadyExists; res.message = "a project \"" + id + "\" already exists"; return res; }

  // Build in a hidden sibling folder, then rename: a failed copy never leaves a half-made project in the games folder.
  fs::create_directories(gamesDir, ec);
  fs::path tmp = gamesDir / ("." + id + kTempSuffix);
  fs::remove_all(tmp, ec);
  fs::copy(src, tmp, fs::copy_options::recursive | fs::copy_options::skip_symlinks, ec);
  if (ec) {
    std::error_code e2;
    fs::remove_all(tmp, e2);
    elog("create %s: copying template %s failed: %s", id.c_str(), tpl.c_str(), ec.message().c_str());
    res.status = CreateStatus::CopyFailed;
    res.message = "could not copy the template: " + ec.message();
    return res;
  }
  Json game;
  Problem prob;
  std::string err;
  bool ok = readGameJson(tmp, game, prob) && game.isObject();
  if (ok) {
    game.set("name", Json::makeString(name));
    ok = writeFileAtomic(tmp / "game.json", game.dump(2) + "\n", err);
  } else {
    err = "template game.json is broken: " + prob.message;
  }
  if (ok) {
    fs::rename(tmp, dst, ec);
    if (ec) { ok = false; err = ec.message(); }
  }
  if (!ok) {
    std::error_code e2;
    fs::remove_all(tmp, e2);
    elog("create %s from %s failed: %s", id.c_str(), tpl.c_str(), err.c_str());
    res.status = CreateStatus::WriteFailed;
    res.message = "could not create the project: " + err;
  }
  return res;
}

}  // namespace ote_editor
