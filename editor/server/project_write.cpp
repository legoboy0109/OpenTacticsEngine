// PUT /api/projects/:id — validates the whole request first (types, paths,
// revision), and only then writes, so a rejected request changes nothing.
#include <algorithm>
#include <set>

#include "log.h"
#include "paths.h"
#include "projects.h"
#include "revision.h"
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
         rel == "data/structures.json" || rel == "data/items.json" || rel == "data/research.json" || rel == "data/campaign.json";
}

struct PendingWrite {
  fs::path path;
  std::string bytes;
};

// A section may be absent/null (untouched) or must have the right type.
bool badType(const Json& section, Json::Type want) { return !section.isNull() && section.type() != want; }

}  // namespace

ApiResult putProject(const Config& cfg, const std::string& id, const Json& body) {
  fs::path dir;
  ApiResult err;
  if (!locateProject(cfg, id, dir, err)) return err;
  if (!body.isObject()) return apiError(400, "body must be a JSON object");
  if (!body["baseRevision"].isString()) return apiError(400, "\"baseRevision\" (string) is required");

  const Json& game = body["game"];
  const Json& maps = body["maps"];
  const Json& scripts = body["scripts"];
  if (badType(game, Json::Type::Object)) return apiError(400, "\"game\" must be an object");
  if (badType(body["terrain"], Json::Type::Array)) return apiError(400, "\"terrain\" must be an array");
  if (badType(body["weapons"], Json::Type::Array)) return apiError(400, "\"weapons\" must be an array");
  if (badType(body["units"], Json::Type::Array)) return apiError(400, "\"units\" must be an array");
  if (badType(body["structures"], Json::Type::Array)) return apiError(400, "\"structures\" must be an array");
  if (badType(body["items"], Json::Type::Array)) return apiError(400, "\"items\" must be an array");
  if (badType(body["research"], Json::Type::Array)) return apiError(400, "\"research\" must be an array");
  // campaign: an object is written; `false` removes data/campaign.json (null / absent leave it alone).
  if (!body["campaign"].isNull() && !body["campaign"].isObject() &&
      !(body["campaign"].type() == Json::Type::Bool && !body["campaign"].asBool()))
    return apiError(400, "\"campaign\" must be an object, or false to remove the campaign");
  if (badType(body["graphics"], Json::Type::Object)) return apiError(400, "\"graphics\" must be an object");
  if (badType(maps, Json::Type::Object)) return apiError(400, "\"maps\" must be an object of objects");
  if (badType(scripts, Json::Type::Object)) return apiError(400, "\"scripts\" must be an object of strings");
  for (const auto& m : maps.members())
    if (!m.second.isNull() && !m.second.isObject())  // null = leave that map alone
      return apiError(400, "maps[\"" + m.first + "\"] must be an object");
  for (const auto& s : scripts.members())
    if (!s.second.isString()) return apiError(400, "scripts[\"" + s.first + "\"] must be a string");

  // Map references that will be in force after this PUT.
  std::vector<MapRef> newRefs;
  if (!game.isNull()) {
    newRefs = collectMapRefs(game);
  } else {
    Json disk;
    Problem p;
    if (readJsonRel(dir, "game.json", disk, p) && disk.isObject()) newRefs = collectMapRefs(disk);
  }

  std::string why;
  fs::path full;
  if (!game.isNull()) {
    for (const MapRef& r : newRefs) {
      if (!validRelPath(r.file, why) || !hasExtension(r.file, ".json"))
        return apiError(400, "game.maps: file \"" + r.file + "\" must be a relative .json path inside the project" +
                                 (why.empty() ? "" : " (" + why + ")"));
    }
    const Json& sl = game["scripts"];
    for (size_t i = 0; i < sl.size(); ++i) {
      if (!sl[i].isString()) continue;
      std::string rel = sl[i].asString();
      if (!validRelPath(rel, why) || !hasExtension(rel, ".lua"))
        return apiError(400, "game.scripts: \"" + rel + "\" must be a relative .lua path inside the project");
    }
  }

  std::vector<PendingWrite> writes;
  auto add = [&](const std::string& rel, const std::string& bytes) -> bool {
    if (!resolveInside(dir, rel, full, why)) return false;
    writes.push_back({full, bytes});
    return true;
  };

  // A failed resolve of a fixed name means a symlink points outside the project.
  const char* escape = "a project file resolves outside the project folder";
  if (!game.isNull() && !add("game.json", game.dump(2) + "\n")) return apiError(400, escape);
  // `wrap` = the key the list sits under inside the file (research.json calls its list "technologies").
  struct { const char* key; const char* file; const char* wrap; } lists[] = {
      {"terrain", "data/terrain.json", "terrain"}, {"weapons", "data/weapons.json", "weapons"},
      {"units", "data/units.json", "units"}, {"structures", "data/structures.json", "structures"},
      {"items", "data/items.json", "items"}, {"research", "data/research.json", "technologies"}};
  std::vector<fs::path> deletions;
  for (const auto& l : lists) {
    if (body[l.key].isNull()) continue;
    // The loader refuses an empty technology list ("delete the file if the game has no research"), so an empty
    // `research` removes data/research.json instead of writing a file that would break the game.
    if (std::string(l.key) == "research" && body[l.key].size() == 0) {
      std::error_code rec;
      if (!resolveInside(dir, l.file, full, why)) return apiError(400, escape);
      if (fs::is_regular_file(full, rec)) deletions.push_back(full);
      continue;
    }
    Json wrap = Json::makeObject();
    wrap.set(l.wrap, body[l.key]);
    if (!add(l.file, wrap.dump(2) + "\n")) return apiError(400, escape);
  }

  if (!body["graphics"].isNull() && !add("data/graphics.json", body["graphics"].dump(2) + "\n"))
    return apiError(400, escape);

  if (body["campaign"].isObject()) {
    if (!add("data/campaign.json", body["campaign"].dump(2) + "\n")) return apiError(400, escape);
  } else if (body["campaign"].type() == Json::Type::Bool) {
    std::error_code cec;
    if (!resolveInside(dir, "data/campaign.json", full, why)) return apiError(400, escape);
    if (fs::is_regular_file(full, cec)) deletions.push_back(full);
  }

  std::set<std::string> writtenMapFiles;
  for (const auto& m : maps.members()) {
    if (m.second.isNull()) continue;
    auto ref = std::find_if(newRefs.begin(), newRefs.end(), [&](const MapRef& r) { return r.id == m.first; });
    if (ref == newRefs.end())
      return apiError(400, "maps[\"" + m.first + "\"] has no entry in game.maps (add one with a \"file\")");
    if (isReservedFile(ref->file))
      return apiError(400, "map \"" + m.first + "\": file \"" + ref->file + "\" is reserved for game data");
    if (!writtenMapFiles.insert(ref->file).second)
      return apiError(400, "two maps use the same file \"" + ref->file + "\"");
    if (!add(ref->file, m.second.dump(2) + "\n"))
      return apiError(400, "map \"" + m.first + "\": bad file \"" + ref->file + "\" (" + why + ")");
  }
  for (const auto& s : scripts.members()) {
    if (!hasExtension(s.first, ".lua")) return apiError(400, "script \"" + s.first + "\" must end in .lua");
    if (!add(s.first, s.second.asString()))
      return apiError(400, "script \"" + s.first + "\": " + why);
  }
  // Map files dropped from game.maps: only plain files under maps/.
  if (!game.isNull()) {
    Json disk;
    Problem p;
    std::set<std::string> keep;
    for (const MapRef& r : newRefs) keep.insert(r.file);
    if (readJsonRel(dir, "game.json", disk, p) && disk.isObject()) {
      for (const MapRef& r : collectMapRefs(disk)) {
        if (keep.count(r.file) || r.file.compare(0, 5, "maps/") != 0) continue;
        std::error_code ec;
        if (resolveInside(dir, r.file, full, why) && fs::is_regular_file(full, ec)) deletions.push_back(full);
      }
    }
  }

  // The revision check comes right before the first write.
  std::string current = computeRevision(dir);
  if (body["baseRevision"].asString() != current) {
    ApiResult conflict = apiError(409, "the project changed on disk since it was loaded; reload it");
    conflict.body.set("revision", Json::makeString(current));
    elog("PUT %s: revision conflict (client %s, disk %s)", id.c_str(),
         body["baseRevision"].asString().c_str(), current.c_str());
    return conflict;
  }

  for (const PendingWrite& w : writes) {
    std::string existing, werr;
    // Skip byte-identical files: keeps hand-written comments and avoids
    // pointless sync traffic when only one section changed.
    if (readWholeFile(w.path, existing) && existing == w.bytes) continue;
    if (!writeFileAtomic(w.path, w.bytes, werr)) {
      elog("PUT %s: %s", id.c_str(), werr.c_str());
      return apiError(500, "saving failed: " + werr);
    }
  }
  for (const fs::path& d : deletions) {
    std::error_code ec;
    fs::remove(d, ec);
    if (ec) elog("PUT %s: could not delete %s: %s", id.c_str(), d.string().c_str(), ec.message().c_str());
  }

  Json res = Json::makeObject();
  res.set("revision", Json::makeString(computeRevision(dir)));
  res.set("problems", problemsToJson(validateProject(dir)));
  return apiOk(res);
}

}  // namespace ote_editor
