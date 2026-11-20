// PUT /api/projects/:id — validates the whole request first (types, paths,
// revision), and only then writes, so a rejected request changes nothing.
#include <algorithm>
#include <set>

#include "log.h"
#include "paths.h"
#include "projects.h"
#include "revision.h"
#include "srpg_files.h"
#include "validate.h"

namespace ote_editor {

using ote::Json;

namespace {

struct MapRef { std::string id, file; };

// Entries of game.maps that have a string id and file. Malformed entries are
// skipped here; the loader reports them in `problems`.
std::vector<MapRef> collectMapRefs(const Json& game) {
  std::vector<MapRef> out;
  const Json& list = game["maps"];
  for (size_t i = 0; i < list.size(); ++i)
    if (list[i]["id"].isString() && list[i]["file"].isString())
      out.push_back({list[i]["id"].asString(), list[i]["file"].asString()});
  return out;
}

bool isReservedFile(const std::string& rel) {
  return rel == "game.json" || rel == "data/terrain.json" || rel == "data/weapons.json" ||
         rel == "data/units.json" || rel == "data/graphics.json" ||
         rel == "data/structures.json" || rel == "data/items.json" || rel == "data/research.json" || rel == "data/factions.json" || rel == "data/campaign.json" ||
         rel == "data/audio.json" || isSrpgFile(rel);
}

struct PendingWrite {
  fs::path path;
  std::string bytes;
};

// A section may be absent/null (untouched) or must have the right type.
bool badType(const Json& section, Json::Type want) { return !section.isNull() && section.type() != want; }

// A failed resolve of a fixed name means a symlink points outside the project.
const char* const kEscape = "a project file resolves outside the project folder";

// Part 1 of a PUT: the request's own shape. Returns the first error message, "" when the body is fine. The order of the
// checks is the order the errors are reported in.
std::string checkBodyShape(const Json& body) {
  const Json& maps = body["maps"];
  const Json& scripts = body["scripts"];
  if (badType(body["game"], Json::Type::Object)) return "\"game\" must be an object";
  if (badType(body["terrain"], Json::Type::Array)) return "\"terrain\" must be an array";
  if (badType(body["weapons"], Json::Type::Array)) return "\"weapons\" must be an array";
  if (badType(body["units"], Json::Type::Array)) return "\"units\" must be an array";
  if (badType(body["structures"], Json::Type::Array)) return "\"structures\" must be an array";
  if (badType(body["items"], Json::Type::Array)) return "\"items\" must be an array";
  if (badType(body["research"], Json::Type::Array)) return "\"research\" must be an array";
  if (badType(body["factions"], Json::Type::Array)) return "\"factions\" must be an array";
  // campaign: an object is written; `false` removes data/campaign.json (null / absent leave it alone).
  if (!body["campaign"].isNull() && !body["campaign"].isObject() &&
      !(body["campaign"].type() == Json::Type::Bool && !body["campaign"].asBool()))
    return "\"campaign\" must be an object, or false to remove the campaign";
  // audio: an object is written; `false` removes data/audio.json (null / absent leave it alone).
  if (!body["audio"].isNull() && !body["audio"].isObject() &&
      !(body["audio"].type() == Json::Type::Bool && !body["audio"].asBool()))
    return "\"audio\" must be an object, or false to remove all sound";
  if (std::string bad = checkSrpgBody(body); !bad.empty()) return bad;
  if (badType(body["graphics"], Json::Type::Object)) return "\"graphics\" must be an object";
  if (badType(maps, Json::Type::Object)) return "\"maps\" must be an object of objects";
  if (badType(scripts, Json::Type::Object)) return "\"scripts\" must be an object of strings";
  for (const auto& m : maps.members())
    if (!m.second.isNull() && !m.second.isObject())  // null = leave that map alone
      return "maps[\"" + m.first + "\"] must be an object";
  for (const auto& s : scripts.members())
    if (!s.second.isString()) return "scripts[\"" + s.first + "\"] must be a string";
  return "";
}

// Map references that will be in force after the PUT: the new game.json's, else the one on disk.
std::vector<MapRef> mapRefsAfterPut(const fs::path& dir, const Json& game) {
  if (!game.isNull()) return collectMapRefs(game);
  std::vector<MapRef> refs;
  Json disk;
  Problem p;
  if (readJsonRel(dir, "game.json", disk, p) && disk.isObject()) refs = collectMapRefs(disk);
  return refs;
}

// Part 2: the paths in a new game.json (map files, Lua scripts) must be plain project-relative names.
std::string checkGamePaths(const Json& game, const std::vector<MapRef>& newRefs) {
  std::string why;
  for (const MapRef& r : newRefs) {
    if (!validRelPath(r.file, why) || !hasExtension(r.file, ".json"))
      return "game.maps: file \"" + r.file + "\" must be a relative .json path inside the project" +
             (why.empty() ? "" : " (" + why + ")");
  }
  const Json& sl = game["scripts"];
  for (size_t i = 0; i < sl.size(); ++i) {
    if (!sl[i].isString()) continue;
    std::string rel = sl[i].asString();
    if (!validRelPath(rel, why) || !hasExtension(rel, ".lua"))
      return "game.scripts: \"" + rel + "\" must be a relative .lua path inside the project";
  }
  return "";
}

// Everything a PUT is going to do, collected before the first byte is written.
struct PutPlan {
  fs::path dir;
  std::vector<PendingWrite> writes;
  std::vector<fs::path> deletions;
  fs::path full;    // scratch: the last path resolveInside produced
  std::string why;  // scratch: why the last resolveInside refused

  // Queues a write of project-relative `rel`; false when it resolves outside the project (`why` says why).
  bool add(const std::string& rel, const std::string& bytes) {
    if (!resolveInside(dir, rel, full, why)) return false;
    writes.push_back({full, bytes});
    return true;
  }
  // Queues the removal of `rel` if the file exists; false when it resolves outside the project.
  bool remove(const std::string& rel) {
    std::error_code ec;
    if (!resolveInside(dir, rel, full, why)) return false;
    if (fs::is_regular_file(full, ec)) deletions.push_back(full);
    return true;
  }
};

// Part 3a: game.json and the fixed data files (lists, graphics, campaign, audio, strategy-RPG files).
// Returns the error message, "" when planned.
std::string planDataFiles(PutPlan& plan, const Json& body) {
  const Json& game = body["game"];
  if (!game.isNull() && !plan.add("game.json", game.dump(2) + "\n")) return kEscape;
  // `wrap` = the key the list sits under inside the file (research.json calls its list "technologies").
  struct { const char* key; const char* file; const char* wrap; } lists[] = {
      {"terrain", "data/terrain.json", "terrain"}, {"weapons", "data/weapons.json", "weapons"},
      {"units", "data/units.json", "units"}, {"structures", "data/structures.json", "structures"},
      {"items", "data/items.json", "items"}, {"research", "data/research.json", "technologies"},
      {"factions", "data/factions.json", "factions"}};
  for (const auto& l : lists) {
    if (body[l.key].isNull()) continue;
    // The loader refuses an empty technology (or faction) list ("delete the file if the game has no research"), so an empty
    // `research` / `factions` removes its file instead of writing one that would break the game.
    if ((std::string(l.key) == "research" || std::string(l.key) == "factions") && body[l.key].size() == 0) {
      if (!plan.remove(l.file)) return kEscape;
      continue;
    }
    Json wrap = Json::makeObject();
    wrap.set(l.wrap, body[l.key]);
    if (!plan.add(l.file, wrap.dump(2) + "\n")) return kEscape;
  }

  if (!body["graphics"].isNull() && !plan.add("data/graphics.json", body["graphics"].dump(2) + "\n")) return kEscape;

  // campaign and audio: an object is written, `false` removes the file.
  const char* optionalObjects[][2] = {{"campaign", "data/campaign.json"}, {"audio", "data/audio.json"}};
  for (const auto& o : optionalObjects) {
    if (body[o[0]].isObject()) {
      if (!plan.add(o[1], body[o[0]].dump(2) + "\n")) return kEscape;
    } else if (body[o[0]].type() == Json::Type::Bool) {
      if (!plan.remove(o[1])) return kEscape;
    }
  }

  // The strategy-RPG files (srpg_files.cpp): an empty list or `supports: false` removes its file.
  const SrpgFileOps srpg = planSrpgFiles(body);
  for (const auto& w : srpg.writes)
    if (!plan.add(w.rel, w.bytes)) return kEscape;
  for (const std::string& rel : srpg.removals)
    if (!plan.remove(rel)) return kEscape;
  return "";
}

// Part 3b: the maps sent along, each needing a game.maps entry with its own non-reserved file.
std::string planMaps(PutPlan& plan, const Json& maps, const std::vector<MapRef>& newRefs) {
  std::set<std::string> writtenMapFiles;
  for (const auto& m : maps.members()) {
    if (m.second.isNull()) continue;
    auto ref = std::find_if(newRefs.begin(), newRefs.end(), [&](const MapRef& r) { return r.id == m.first; });
    if (ref == newRefs.end())
      return "maps[\"" + m.first + "\"] has no entry in game.maps (add one with a \"file\")";
    if (isReservedFile(ref->file))
      return "map \"" + m.first + "\": file \"" + ref->file + "\" is reserved for game data";
    if (!writtenMapFiles.insert(ref->file).second)
      return "two maps use the same file \"" + ref->file + "\"";
    if (!plan.add(ref->file, m.second.dump(2) + "\n"))
      return "map \"" + m.first + "\": bad file \"" + ref->file + "\" (" + plan.why + ")";
  }
  return "";
}

// Part 3c: the Lua scripts sent along.
std::string planScripts(PutPlan& plan, const Json& scripts) {
  for (const auto& s : scripts.members()) {
    if (!hasExtension(s.first, ".lua")) return "script \"" + s.first + "\" must end in .lua";
    if (!plan.add(s.first, s.second.asString())) return "script \"" + s.first + "\": " + plan.why;
  }
  return "";
}

// Part 3d: map files dropped from game.maps are deleted: only plain files under maps/.
void planDroppedMaps(PutPlan& plan, const std::vector<MapRef>& newRefs) {
  Json disk;
  Problem p;
  std::set<std::string> keep;
  for (const MapRef& r : newRefs) keep.insert(r.file);
  if (!(readJsonRel(plan.dir, "game.json", disk, p) && disk.isObject())) return;
  for (const MapRef& r : collectMapRefs(disk)) {
    if (keep.count(r.file) || r.file.compare(0, 5, "maps/") != 0) continue;
    std::error_code ec;
    if (resolveInside(plan.dir, r.file, plan.full, plan.why) && fs::is_regular_file(plan.full, ec))
      plan.deletions.push_back(plan.full);
  }
}

// Part 4: the writes (byte-identical files skipped), then the deletions. False with a 500 in `err` when a write fails;
// the writes before it stay (each one is atomic), as before.
bool applyPlan(const PutPlan& plan, const std::string& id, ApiResult& err) {
  for (const PendingWrite& w : plan.writes) {
    std::string existing, werr;
    // Skip byte-identical files: keeps hand-written comments and avoids
    // pointless sync traffic when only one section changed.
    if (readWholeFile(w.path, existing) && existing == w.bytes) continue;
    if (!writeFileAtomic(w.path, w.bytes, werr)) {
      elog("PUT %s: %s", id.c_str(), werr.c_str());
      err = apiError(500, "saving failed: " + werr);
      return false;
    }
  }
  for (const fs::path& d : plan.deletions) {
    std::error_code ec;
    fs::remove(d, ec);
    if (ec) elog("PUT %s: could not delete %s: %s", id.c_str(), d.string().c_str(), ec.message().c_str());
  }
  return true;
}

}  // namespace

ApiResult putProject(const Config& cfg, const std::string& id, const Json& body) {
  PutPlan plan;
  ApiResult err;
  if (!locateProject(cfg, id, plan.dir, err)) return err;
  const fs::path& dir = plan.dir;
  if (!body.isObject()) return apiError(400, "body must be a JSON object");
  if (!body["baseRevision"].isString()) return apiError(400, "\"baseRevision\" (string) is required");

  if (std::string bad = checkBodyShape(body); !bad.empty()) return apiError(400, bad);
  const Json& game = body["game"];
  const std::vector<MapRef> newRefs = mapRefsAfterPut(dir, game);
  if (!game.isNull())
    if (std::string bad = checkGamePaths(game, newRefs); !bad.empty()) return apiError(400, bad);

  if (std::string bad = planDataFiles(plan, body); !bad.empty()) return apiError(400, bad);
  if (std::string bad = planMaps(plan, body["maps"], newRefs); !bad.empty()) return apiError(400, bad);
  if (std::string bad = planScripts(plan, body["scripts"]); !bad.empty()) return apiError(400, bad);
  if (!game.isNull()) planDroppedMaps(plan, newRefs);

  // The revision check comes right before the first write.
  std::string current = computeRevision(dir);
  if (body["baseRevision"].asString() != current) {
    ApiResult conflict = apiError(409, "the project changed on disk since it was loaded; reload it");
    conflict.body.set("revision", Json::makeString(current));
    elog("PUT %s: revision conflict (client %s, disk %s)", id.c_str(),
         body["baseRevision"].asString().c_str(), current.c_str());
    return conflict;
  }

  if (!applyPlan(plan, id, err)) return err;

  Json res = Json::makeObject();
  res.set("revision", Json::makeString(computeRevision(dir)));
  res.set("problems", problemsToJson(validateProject(dir)));
  return apiOk(res);
}

}  // namespace ote_editor
