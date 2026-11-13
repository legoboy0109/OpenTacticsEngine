#include "art.h"

#include <algorithm>
#include <fstream>

#include "core/data/graphics.h"
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
    "starting with a letter or digit) and end in .png, for example \"tiles.png\"";

// Resolves art/<name> inside the project and refuses links, so a symlink can
// never carry a read, write or delete outside the project.
bool artPath(const fs::path& dir, const std::string& name, fs::path& out, std::string& why) {
  fs::path resolved;
  if (!resolveInside(dir, "art/" + name, resolved, why)) return false;
  fs::path direct = dir / "art" / name;
  std::error_code ec;
  fs::file_status st = fs::symlink_status(direct, ec);
  if (fs::exists(st) && !fs::is_regular_file(st)) {
    why = "art/" + name + " is not a plain file (links are not followed)";
    return false;
  }
  out = direct;
  return true;
}

// Sheet ids in the SAVED data/graphics.json that use art/<name>.
Json sheetsUsing(const fs::path& dir, const std::string& name) {
  Json ids = Json::makeArray();
  Json doc;
  Problem p;
  if (!readJsonRel(dir, "data/graphics.json", doc, p) || !doc.isObject()) return ids;
  const Json& sheets = doc["sheets"];
  for (size_t i = 0; i < sheets.size(); ++i)
    if (sheets[i]["file"].isString() && sheets[i]["file"].asString() == "art/" + name)
      ids.push(jstr(sheets[i]["id"].asString()));
  return ids;
}

}  // namespace

bool validArtName(const std::string& name) {
  const std::string ext = ".png";
  if (name.size() <= ext.size() || name.size() > 40 + ext.size()) return false;
  if (name.compare(name.size() - ext.size(), ext.size(), ext) != 0) return false;
  for (size_t i = 0; i + ext.size() < name.size(); ++i) {
    char c = name[i];
    bool alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
    if (!(alnum || (i > 0 && (c == '-' || c == '_')))) return false;
  }
  return true;
}

ApiResult listArt(const Config& cfg, const std::string& id) {
  fs::path dir;
  ApiResult err;
  if (!locateProject(cfg, id, dir, err)) return err;
  std::vector<std::string> names;
  fs::path artDir;
  std::string why;
  std::error_code ec;
  // An art/ that is a link leaving the project is treated as empty.
  if (resolveInside(dir, "art", artDir, why) && fs::is_directory(artDir, ec)) {
    for (fs::directory_iterator it(artDir, ec), end; !ec && it != end; it.increment(ec)) {
      std::error_code e2;
      std::string n = it->path().filename().string();
      if (validArtName(n) && fs::is_regular_file(it->symlink_status(e2))) names.push_back(n);
    }
  }
  std::sort(names.begin(), names.end());
  Json arr = Json::makeArray();
  for (const std::string& n : names) {
    fs::path full = artDir / n;
    Json o = Json::makeObject();
    int w = 0, h = 0;
    std::string perr;
    bool ok = ote::readPngSize(full.generic_string(), w, h, perr);
    std::error_code e2;
    o.set("file", jstr("art/" + n));
    o.set("width", Json::makeNumber(ok ? w : 0));
    o.set("height", Json::makeNumber(ok ? h : 0));
    o.set("size", Json::makeNumber((double)fs::file_size(full, e2)));
    if (!ok) o.set("error", jstr(perr));
    arr.push(o);
  }
  Json body;
  body.set("art", arr);
  return apiOk(body);
}

ApiResult putArt(const Config& cfg, const std::string& id, const std::string& name,
                 const std::string& bytes, bool overwrite) {
  fs::path dir;
  ApiResult err;
  if (!locateProject(cfg, id, dir, err)) return err;
  // Name first: nothing below touches the filesystem for a bad name.
  if (!validArtName(name)) return apiError(400, kBadName);
  fs::path target;
  std::string why;
  if (!artPath(dir, name, target, why)) return apiError(400, why);

  if (bytes.empty()) return apiError(400, "the request body is empty; send the PNG file's bytes");
  if (bytes.size() > kMaxArtBytes)
    return apiError(413, "the image file is larger than 8 MB; shrink it or save it with fewer colours");
  std::error_code ec;
  if (fs::exists(target, ec) && !overwrite)
    return apiError(409, "art/" + name + " already exists; send ?overwrite=1 to replace it");

  // Write to a temp file next to the target, check it with the same reader the
  // game uses, and only then rename it into place.
  fs::path tmp = target;
  tmp += kTempSuffix;
  fs::create_directories(target.parent_path(), ec);
  if (ec) return apiError(500, "cannot create the art folder: " + ec.message());
  auto cleanup = [&]() { std::error_code e; fs::remove(tmp, e); };
  {
    std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
    if (f) { f.write(bytes.data(), (std::streamsize)bytes.size()); f.flush(); }
    if (!f) {
      f.close();
      cleanup();
      elog("art upload %s/%s: cannot write temp file", id.c_str(), name.c_str());
      return apiError(500, "saving failed (disk full or folder not writable)");
    }
  }
  int w = 0, h = 0;
  std::string perr;
  if (!ote::readPngSize(tmp.generic_string(), w, h, perr)) {
    cleanup();
    return apiError(400, "this is not a PNG image (" + perr + "); export it from your art program as PNG");
  }
  if (w > ote::kMaxSheetSize || h > ote::kMaxSheetSize) {
    cleanup();
    return apiError(400, "the image is " + std::to_string(w) + " x " + std::to_string(h) +
                             " pixels; the largest allowed is " + std::to_string(ote::kMaxSheetSize) + " x " +
                             std::to_string(ote::kMaxSheetSize));
  }
  fs::rename(tmp, target, ec);
  if (ec) {
    cleanup();
    elog("art upload %s/%s: rename failed: %s", id.c_str(), name.c_str(), ec.message().c_str());
    return apiError(500, "saving failed: " + ec.message());
  }
  Json o = Json::makeObject();
  o.set("file", jstr("art/" + name));
  o.set("width", Json::makeNumber(w));
  o.set("height", Json::makeNumber(h));
  o.set("size", Json::makeNumber((double)bytes.size()));
  o.set("revision", jstr(computeRevision(dir)));
  return apiOk(o, 201);
}

ApiResult deleteArt(const Config& cfg, const std::string& id, const std::string& name) {
  fs::path dir;
  ApiResult err;
  if (!locateProject(cfg, id, dir, err)) return err;
  if (!validArtName(name)) return apiError(400, kBadName);
  fs::path target;
  std::string why;
  if (!artPath(dir, name, target, why)) return apiError(400, why);
  std::error_code ec;
  if (!fs::is_regular_file(fs::symlink_status(target, ec))) return apiError(404, "no such file: art/" + name);

  Json used = sheetsUsing(dir, name);
  if (used.size() > 0) {
    ApiResult r = apiError(409, "art/" + name + " is still used by a sheet in the saved art settings; remove "
                                    "the sheet (and save) first");
    r.body.set("usedBy", used);
    return r;
  }
  fs::remove(target, ec);
  if (ec) {
    elog("art delete %s/%s: %s", id.c_str(), name.c_str(), ec.message().c_str());
    return apiError(500, "could not delete the file: " + ec.message());
  }
  Json o = Json::makeObject();
  o.set("revision", jstr(computeRevision(dir)));
  return apiOk(o);
}

}  // namespace ote_editor
