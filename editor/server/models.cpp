#include "models.h"

#include <algorithm>
#include <cstring>
#include <fstream>

#include "log.h"
#include "paths.h"
#include "projects.h"
#include "revision.h"

namespace ote_editor {

using ote::Json;

namespace {

Json jstr(const std::string& s) { return Json::makeString(s); }

const char* const kBadName =
    "bad file name: use lower-case letters, digits, '-' and '_' (up to 40 characters, "
    "starting with a letter or digit) and end in .glb, for example \"knight.glb\"";

bool hasSuffix(const std::string& s, const std::string& ext) {
  return s.size() > ext.size() && s.compare(s.size() - ext.size(), ext.size(), ext) == 0;
}

// Name stem rule shared by the upload check and the listing.
bool validStem(const std::string& stem) {
  if (stem.empty() || stem.size() > 40) return false;
  for (size_t i = 0; i < stem.size(); ++i) {
    char c = stem[i];
    bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    if (!(alnum || (i > 0 && (c == '-' || c == '_')))) return false;
  }
  return true;
}

bool modelFileName(const std::string& n) {
  if (hasSuffix(n, ".glb")) return validStem(n.substr(0, n.size() - 4));
  if (hasSuffix(n, ".gltf")) return validStem(n.substr(0, n.size() - 5));
  return false;
}

// Resolves models/<name> inside the project and refuses links (see art.cpp).
bool modelPath(const fs::path& dir, const std::string& name, fs::path& out, std::string& why) {
  fs::path resolved;
  if (!resolveInside(dir, "models/" + name, resolved, why)) return false;
  fs::path direct = dir / "models" / name;
  std::error_code ec;
  fs::file_status st = fs::symlink_status(direct, ec);
  if (fs::exists(st) && !fs::is_regular_file(st)) {
    why = "models/" + name + " is not a plain file (links are not followed)";
    return false;
  }
  out = direct;
  return true;
}

void collectUses(const Json& styles, const std::string& kind, const std::string& styleId,
                 const std::string& file, Json& out) {
  const Json& group = styles[styleId][kind];
  if (!group.isObject()) return;
  for (const auto& e : group.members()) {
    if (e.second.isObject() && e.second["model"].isString() && e.second["model"].asString() == file) {
      Json u = Json::makeObject();
      u.set("style", jstr(styleId));
      u.set("kind", jstr(kind));
      u.set("id", jstr(e.first));
      out.push(u);
    }
  }
}

// The same for a terrain's standing part ("stand": a "model" or a list of model "variants"), reported as kind "terrain".
void collectStandUses(const Json& styles, const std::string& styleId, const std::string& file, Json& out) {
  const Json& group = styles[styleId]["terrain"];
  if (!group.isObject()) return;
  for (const auto& e : group.members()) {
    if (!e.second.isObject()) continue;
    const Json& stand = e.second["stand"];
    if (!stand.isObject()) continue;
    bool uses = stand["model"].isString() && stand["model"].asString() == file;
    const Json& list = stand["variants"];
    if (list.isArray())
      for (size_t i = 0; i < list.size(); ++i) uses = uses || (list[i].isString() && list[i].asString() == file);
    if (!uses) continue;
    Json u = Json::makeObject();
    u.set("style", jstr(styleId));
    u.set("kind", jstr("terrain"));
    u.set("id", jstr(e.first));
    out.push(u);
  }
}

// Entries of the SAVED data/graphics.json that name `file` ("models/x.glb").
Json entriesUsing(const fs::path& dir, const std::string& file) {
  Json out = Json::makeArray();
  Json doc;
  Problem p;
  if (!readJsonRel(dir, "data/graphics.json", doc, p) || !doc.isObject()) return out;
  const Json& styles = doc["styles"];
  if (!styles.isObject()) return out;
  for (const auto& s : styles.members()) {
    collectUses(styles, "units", s.first, file, out);
    collectUses(styles, "structures", s.first, file, out);
    collectStandUses(styles, s.first, file, out);
  }
  return out;
}

}  // namespace

bool validModelUploadName(const std::string& name) {
  return hasSuffix(name, ".glb") && validStem(name.substr(0, name.size() - 4));
}

ApiResult listModels(const Config& cfg, const std::string& id) {
  fs::path dir;
  ApiResult err;
  if (!locateProject(cfg, id, dir, err)) return err;
  std::vector<std::string> names;
  fs::path mdir;
  std::string why;
  std::error_code ec;
  if (resolveInside(dir, "models", mdir, why) && fs::is_directory(mdir, ec)) {
    for (fs::directory_iterator it(mdir, ec), end; !ec && it != end; it.increment(ec)) {
      std::error_code e2;
      std::string n = it->path().filename().string();
      if (modelFileName(n) && fs::is_regular_file(it->symlink_status(e2))) names.push_back(n);
    }
  }
  std::sort(names.begin(), names.end());
  Json arr = Json::makeArray();
  for (const std::string& n : names) {
    std::error_code e2;
    Json o = Json::makeObject();
    o.set("file", jstr("models/" + n));
    o.set("size", Json::makeNumber((double)fs::file_size(mdir / n, e2)));
    o.set("usedBy", entriesUsing(dir, "models/" + n));
    arr.push(o);
  }
  Json body;
  body.set("models", arr);
  return apiOk(body);
}

ApiResult putModel(const Config& cfg, const std::string& id, const std::string& name,
                   const std::string& bytes, bool overwrite) {
  fs::path dir;
  ApiResult err;
  if (!locateProject(cfg, id, dir, err)) return err;
  if (!validModelUploadName(name)) {
    if (hasSuffix(name, ".gltf"))
      return apiError(400, "only .glb model files can be uploaded; save the model as a single .glb file");
    return apiError(400, kBadName);
  }
  fs::path target;
  std::string why;
  if (!modelPath(dir, name, target, why)) return apiError(400, why);

  if (bytes.empty()) return apiError(400, "the request body is empty; send the .glb file's bytes");
  if (bytes.size() > kMaxModelBytes)
    return apiError(413, "the model file is larger than 8 MB; reduce its textures or polygon count");
  // glTF binary header: magic "glTF", then version (little-endian uint32) 2.
  if (bytes.size() < 12 || std::memcmp(bytes.data(), "glTF", 4) != 0)
    return apiError(400, "this is not a .glb model file (it does not start with \"glTF\"); export it as glTF Binary (.glb)");
  const unsigned char* b = (const unsigned char*)bytes.data();
  uint32_t version = (uint32_t)b[4] | ((uint32_t)b[5] << 8) | ((uint32_t)b[6] << 16) | ((uint32_t)b[7] << 24);
  if (version != 2)
    return apiError(400, "this .glb file is glTF version " + std::to_string(version) + "; only version 2 is supported");

  std::error_code ec;
  if (fs::exists(target, ec) && !overwrite)
    return apiError(409, "models/" + name + " already exists; send ?overwrite=1 to replace it");

  fs::path tmp = target;
  tmp += kTempSuffix;
  fs::create_directories(target.parent_path(), ec);
  if (ec) return apiError(500, "cannot create the models folder: " + ec.message());
  auto cleanup = [&]() { std::error_code e; fs::remove(tmp, e); };
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (f) { f.write(bytes.data(), (std::streamsize)bytes.size()); f.flush(); }
    if (!f) {
      f.close();
      cleanup();
      elog("model upload %s/%s: cannot write temp file", id.c_str(), name.c_str());
      return apiError(500, "saving failed (disk full or folder not writable)");
    }
  }
  fs::rename(tmp, target, ec);
  if (ec) {
    cleanup();
    elog("model upload %s/%s: rename failed: %s", id.c_str(), name.c_str(), ec.message().c_str());
    return apiError(500, "saving failed: " + ec.message());
  }
  Json o = Json::makeObject();
  o.set("file", jstr("models/" + name));
  o.set("size", Json::makeNumber((double)bytes.size()));
  o.set("revision", jstr(computeRevision(dir)));
  return apiOk(o, 201);
}

ApiResult deleteModel(const Config& cfg, const std::string& id, const std::string& name) {
  fs::path dir;
  ApiResult err;
  if (!locateProject(cfg, id, dir, err)) return err;
  if (!validModelUploadName(name)) return apiError(400, kBadName);
  fs::path target;
  std::string why;
  if (!modelPath(dir, name, target, why)) return apiError(400, why);
  std::error_code ec;
  if (!fs::is_regular_file(fs::symlink_status(target, ec))) return apiError(404, "no such file: models/" + name);

  Json used = entriesUsing(dir, "models/" + name);
  if (used.size() > 0) {
    ApiResult r = apiError(409, "models/" + name + " is still used by a unit, structure or terrain in the saved art "
                                    "settings; change that style (and save) first");
    r.body.set("usedBy", used);
    return r;
  }
  fs::remove(target, ec);
  if (ec) {
    elog("model delete %s/%s: %s", id.c_str(), name.c_str(), ec.message().c_str());
    return apiError(500, "could not delete the file: " + ec.message());
  }
  Json o = Json::makeObject();
  o.set("revision", jstr(computeRevision(dir)));
  return apiOk(o);
}

}  // namespace ote_editor
