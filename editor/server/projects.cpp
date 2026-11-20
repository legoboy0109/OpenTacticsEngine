// Read-side project operations: list, create, get, problems, files.
#include "projects.h"
#include "srpg_files.h"

#include <algorithm>

#include "log.h"
#include "paths.h"
#include "revision.h"
#include "validate.h"

namespace ote_editor {

using ote::Json;

static Json jstr(const std::string& s) { return Json::makeString(s); }

bool locateProject(const Config& cfg, const std::string& id, fs::path& dir, ApiResult& err) {
  if (!validId(id)) {
    err = apiError(400, "bad project id (use a-z, 0-9 and '-', up to 40 characters)");
    return false;
  }
  fs::path d = cfg.gamesDir / id;
  std::error_code ec;
  if (!fs::is_directory(d, ec) || !fs::is_regular_file(d / "game.json", ec)) {
    err = apiError(404, "no project \"" + id + "\"");
    return false;
  }
  dir = d;
  return true;
}

bool readJsonRel(const fs::path& dir, const std::string& rel, Json& out, Problem& prob) {
  std::string why;
  fs::path full;
  if (!resolveInside(dir, rel, full, why)) { prob = {rel, why}; return false; }
  // Pass the path in the loader's own prefix form so the error converts cleanly.
  std::string abs = dir.generic_string();
  while (abs.size() > 1 && abs.back() == '/') abs.pop_back();
  std::string err;
  if (!ote::loadJsonFile(abs + "/" + rel, out, err)) {
    prob = problemFromMessage(dir, err);
    if (prob.file.empty()) prob.file = rel;
    return false;
  }
  return true;
}

static std::string describeFirst(const std::vector<Problem>& probs) {
  if (probs.empty()) return "";
  const Problem& p = probs[0];
  return p.file.empty() ? p.message : p.file + ": " + p.message;
}

// ---------- list ----------

ApiResult listProjects(const Config& cfg) {
  std::vector<std::string> ids;
  std::error_code ec;
  for (fs::directory_iterator it(cfg.gamesDir, ec), end; !ec && it != end; it.increment(ec)) {
    std::error_code e2;
    std::string name = it->path().filename().string();
    // Folders whose name is not a valid id could never be opened via the API.
    if (validId(name) && fs::is_directory(it->path(), e2) &&
        fs::is_regular_file(it->path() / "game.json", e2))
      ids.push_back(name);
  }
  std::sort(ids.begin(), ids.end());

  Json arr = Json::makeArray();
  for (const std::string& id : ids) {
    fs::path dir = cfg.gamesDir / id;
    Json game;
    Problem prob;
    bool parsed = readJsonRel(dir, "game.json", game, prob) && game.isObject();
    std::vector<Problem> problems = validateProject(dir);
    Json o = Json::makeObject();
    o.set("id", jstr(id));
    o.set("name", jstr(parsed ? game["name"].asString(id) : id));
    o.set("description", jstr(parsed ? game["description"].asString() : ""));
    o.set("author", jstr(parsed ? game["author"].asString() : ""));
    o.set("maps", Json::makeNumber(parsed ? (double)game["maps"].size() : 0));
    o.set("protected", Json::makeBool(protectedProject(id)));
    o.set("ok", Json::makeBool(problems.empty()));
    o.set("error", jstr(describeFirst(problems)));
    arr.push(o);
  }
  Json body;
  body.set("projects", arr);
  return apiOk(body);
}

// ---------- create ----------

ApiResult createProject(const Config& cfg, const Json& body) {
  if (!body.isObject()) return apiError(400, "body must be a JSON object");
  if (!body["id"].isString() || !body["template"].isString())
    return apiError(400, "body needs string fields \"id\" and \"template\"");
  std::string id = body["id"].asString();
  std::string tpl = body["template"].asString();
  if (!validId(id)) return apiError(400, "bad project id (use a-z, 0-9 and '-', up to 40 characters)");
  if (!validId(tpl)) return apiError(400, "unknown template \"" + tpl + "\"");
  if (body.has("name") && !body["name"].isString()) return apiError(400, "\"name\" must be a string");
  std::string name = body["name"].asString(id);
  if (name.empty()) name = id;

  std::error_code ec;
  fs::path src = cfg.templatesDir / tpl;
  if (!fs::is_regular_file(src / "game.json", ec)) return apiError(400, "unknown template \"" + tpl + "\"");
  fs::path dst = cfg.gamesDir / id;
  if (fs::exists(fs::symlink_status(dst, ec))) return apiError(409, "a project \"" + id + "\" already exists");

  // Build in a hidden sibling folder, then rename: a failed copy never leaves
  // a half-made project in the games folder.
  fs::create_directories(cfg.gamesDir, ec);
  fs::path tmp = cfg.gamesDir / ("." + id + kTempSuffix);
  fs::remove_all(tmp, ec);
  fs::copy(src, tmp, fs::copy_options::recursive | fs::copy_options::skip_symlinks, ec);
  if (ec) {
    std::error_code e2;
    fs::remove_all(tmp, e2);
    elog("create %s: copying template %s failed: %s", id.c_str(), tpl.c_str(), ec.message().c_str());
    return apiError(500, "could not copy the template: " + ec.message());
  }
  Json game;
  Problem prob;
  std::string err;
  bool ok = readJsonRel(tmp, "game.json", game, prob) && game.isObject();
  if (ok) {
    game.set("name", jstr(name));
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
    return apiError(500, "could not create the project: " + err);
  }
  Json res;
  res.set("id", jstr(id));
  return apiOk(res, 201);
}

// ---------- get ----------

// Returns the list stored under `key` of a data file, or null + a problem.
static Json readDataList(const fs::path& dir, const std::string& rel, const char* key,
                         std::vector<Problem>& probs) {
  Json doc;
  Problem p;
  if (!readJsonRel(dir, rel, doc, p)) { probs.push_back(p); return Json(); }
  if (!doc.isObject() || !doc[key].isArray()) {
    probs.push_back({rel, std::string("expected an object with a \"") + key + "\" list"});
    return Json();
  }
  return doc[key];
}

// True when the optional data file `rel` does not exist (a path that escapes the project does not count as absent,
// so the read below reports it).
static bool dataFileAbsent(const fs::path& dir, const std::string& rel) {
  fs::path full;
  std::string why;
  std::error_code ec;
  return resolveInside(dir, rel, full, why) && !fs::exists(full, ec);
}

// An optional data file whose whole content is one object (graphics, campaign, audio): absent = null with no problem;
// a file that is not a JSON object is null plus a problem on the file (the screen then refuses to edit, like Items).
static Json readOptionalObject(const fs::path& dir, const std::string& rel, std::vector<Problem>& probs) {
  if (dataFileAbsent(dir, rel)) return Json();
  Json v;
  Problem pb;
  if (readJsonRel(dir, rel, v, pb) && v.isObject()) return v;
  if (pb.message.empty()) pb = {rel, "the file must contain a JSON object at the top level"};
  probs.push_back(pb);
  return Json();
}

// An optional data file that holds a list under `key` (structures, items, research): absent = null, no problem.
static Json readOptionalList(const fs::path& dir, const std::string& rel, const char* key,
                             std::vector<Problem>& probs) {
  if (dataFileAbsent(dir, rel)) return Json();
  return readDataList(dir, rel, key, probs);
}

// game.json: the object, or null plus a problem when it cannot be read.
static Json readGameFile(const fs::path& dir, std::vector<Problem>& probs) {
  Json game;
  Problem gp;
  if (!readJsonRel(dir, "game.json", game, gp)) { probs.push_back(gp); return Json(); }
  if (!game.isObject()) {
    probs.push_back({"game.json", "the file must contain a JSON object at the top level"});
    return Json();
  }
  return game;
}

// The map files listed in game.maps, keyed by map id (null for one that cannot be read).
static Json readMaps(const fs::path& dir, const Json& game, std::vector<Problem>& probs) {
  Json maps = Json::makeObject();
  const Json& mlist = game["maps"];
  for (size_t i = 0; i < mlist.size(); ++i) {
    const Json& m = mlist[i];
    if (!m["id"].isString() || !m["file"].isString()) continue;  // loader reports it
    Json mj;
    Problem p;
    if (readJsonRel(dir, m["file"].asString(), mj, p) && mj.isObject()) maps.set(m["id"].asString(), mj);
    else {
      if (p.file.empty() || p.message.empty())
        p = {m["file"].asString(), "the file must contain a JSON object at the top level"};
      probs.push_back(p);
      maps.set(m["id"].asString(), Json());
    }
  }
  return maps;
}

// The Lua files listed in game.scripts, keyed by their project-relative path.
static Json readScripts(const fs::path& dir, const Json& game, std::vector<Problem>& probs) {
  Json scripts = Json::makeObject();
  const Json& slist = game["scripts"];
  for (size_t i = 0; i < slist.size(); ++i) {
    if (!slist[i].isString()) continue;
    std::string rel = slist[i].asString(), why, text;
    fs::path full;
    if (!resolveInside(dir, rel, full, why)) probs.push_back({rel, why});
    else if (!readWholeFile(full, text)) probs.push_back({rel, "cannot read file"});
    else scripts.set(rel, jstr(text));
  }
  return scripts;
}

// Loader problems first; read problems are added for files the loader did not mention.
static std::vector<Problem> mergeProblems(std::vector<Problem> loaderProbs, const std::vector<Problem>& readProbs) {
  for (const Problem& rp : readProbs) {
    bool known = std::any_of(loaderProbs.begin(), loaderProbs.end(),
                             [&](const Problem& q) { return q.file == rp.file; });
    if (!known) loaderProbs.push_back(rp);
  }
  return loaderProbs;
}

ApiResult getProject(const Config& cfg, const std::string& id) {
  fs::path dir;
  ApiResult err;
  if (!locateProject(cfg, id, dir, err)) return err;

  // Revision first: if files change while we read, a later PUT sees a mismatch
  // instead of silently overwriting the newer state.
  std::string revision = computeRevision(dir);
  std::vector<Problem> readProbs;
  Json doc = Json::makeObject();
  doc.set("id", jstr(id));
  doc.set("revision", jstr(revision));

  const Json game = readGameFile(dir, readProbs);
  doc.set("game", game);
  doc.set("terrain", readDataList(dir, "data/terrain.json", "terrain", readProbs));
  doc.set("weapons", readDataList(dir, "data/weapons.json", "weapons", readProbs));
  doc.set("units", readDataList(dir, "data/units.json", "units", readProbs));
  // The optional files (absent = null, no problem): graphics, structures (docs/STRUCTURES.md), items (docs/ITEMS.md),
  // research (docs/ECONOMY.md; the list sits under "technologies" in the file and comes back as "research"; a broken
  // file only blocks the Research screen), campaign (docs/EVENTS.md), audio (docs/SOUND.md).
  doc.set("graphics", readOptionalObject(dir, "data/graphics.json", readProbs));
  doc.set("structures", readOptionalList(dir, "data/structures.json", "structures", readProbs));
  doc.set("items", readOptionalList(dir, "data/items.json", "items", readProbs));
  doc.set("research", readOptionalList(dir, "data/research.json", "technologies", readProbs));
  // factions (docs/ECONOMY.md "Factions"): data/factions.json, the list under "factions"; null when the file is absent.
  doc.set("factions", readOptionalList(dir, "data/factions.json", "factions", readProbs));
  // commanders (docs/COMMANDERS.md): data/commanders.json, the list under "commanders"; null when the file is absent. No editor form yet: the data
  // only passes through (and the loader judges it through /problems).
  doc.set("commanders", readOptionalList(dir, "data/commanders.json", "commanders", readProbs));
  doc.set("campaign", readOptionalObject(dir, "data/campaign.json", readProbs));
  doc.set("audio", readOptionalObject(dir, "data/audio.json", readProbs));

  // characters / skills / abilities / supports (docs/SRPG.md "Editor"): each optional, null when the file is absent.
  readSrpgSections(dir, doc, readProbs);

  // maps and scripts stay null when game.json is unusable
  doc.set("maps", game.isObject() ? readMaps(dir, game, readProbs) : Json());
  doc.set("scripts", game.isObject() ? readScripts(dir, game, readProbs) : Json());

  doc.set("problems", problemsToJson(mergeProblems(validateProject(dir), readProbs)));
  return apiOk(doc);
}

ApiResult getProblems(const Config& cfg, const std::string& id) {
  fs::path dir;
  ApiResult err;
  if (!locateProject(cfg, id, dir, err)) return err;
  Json body;
  body.set("problems", problemsToJson(validateProject(dir)));
  return apiOk(body);
}

// ---------- files ----------

ApiResult getProjectFiles(const Config& cfg, const std::string& id) {
  fs::path dir;
  ApiResult err;
  if (!locateProject(cfg, id, dir, err)) return err;
  Json arr = Json::makeArray();
  for (const auto& f : listProjectFiles(dir)) {
    std::error_code ec;
    Json o = Json::makeObject();
    o.set("path", jstr(f.first));
    o.set("size", Json::makeNumber((double)fs::file_size(f.second, ec)));
    arr.push(o);
  }
  Json body;
  body.set("files", arr);
  return apiOk(body);
}

RawFile getProjectFile(const Config& cfg, const std::string& id, const std::string& rel) {
  RawFile r;
  fs::path dir, full;
  ApiResult err;
  std::string why;
  if (!locateProject(cfg, id, dir, err)) {
    r.status = err.status;
    r.error = err.body["error"].asString();
    return r;
  }
  if (!resolveInside(dir, rel, full, why)) {
    r.status = 400;
    r.error = why;
    return r;
  }
  std::error_code ec;
  if (!fs::is_regular_file(full, ec) || !readWholeFile(full, r.bytes)) {
    r.status = 404;
    r.error = "no such file in the project";
    r.bytes.clear();
  }
  return r;
}

}  // namespace ote_editor
