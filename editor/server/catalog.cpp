// Read-only catalogs: project templates and available fonts.
#include <algorithm>

#include "paths.h"
#include "projects.h"

namespace ote_editor {

using ote::Json;

ApiResult listTemplates(const Config& cfg) {
  std::vector<std::string> ids;
  std::error_code ec;
  for (fs::directory_iterator it(cfg.templatesDir, ec), end; !ec && it != end; it.increment(ec)) {
    std::error_code e2;
    std::string name = it->path().filename().string();
    if (validId(name) && fs::is_regular_file(it->path() / "game.json", e2)) ids.push_back(name);
  }
  std::sort(ids.begin(), ids.end());
  Json arr = Json::makeArray();
  for (const std::string& id : ids) {
    Json game;
    Problem p;
    bool ok = readJsonRel(cfg.templatesDir / id, "game.json", game, p) && game.isObject();
    Json o = Json::makeObject();
    o.set("id", Json::makeString(id));
    o.set("name", Json::makeString(ok ? game["name"].asString(id) : id));
    o.set("description", Json::makeString(ok ? game["description"].asString() : ""));
    arr.push(o);
  }
  Json body;
  body.set("templates", arr);
  return apiOk(body);
}

ApiResult listFonts(const Config& cfg) {
  Json arr = Json::makeArray();
  // The built-in bitmap font needs no file and is always available.
  Json builtin = Json::makeObject();
  builtin.set("id", Json::makeString("pixel"));
  builtin.set("name", Json::makeString("Pixel (built-in)"));
  builtin.set("size", Json::makeNumber(8));
  builtin.set("smooth", Json::makeBool(false));
  arr.push(builtin);

  Json doc;
  Problem p;
  if (readJsonRel(cfg.assetsDir, "fonts/fonts.json", doc, p)) {
    const Json& list = doc["fonts"];
    for (size_t i = 0; i < list.size(); ++i) {
      const Json& f = list[i];
      if (!f["id"].isString()) continue;
      Json o = Json::makeObject();
      o.set("id", f["id"]);
      o.set("name", Json::makeString(f["name"].asString(f["id"].asString())));
      o.set("size", Json::makeNumber(f["size"].asInt(0)));
      o.set("smooth", Json::makeBool(f["smooth"].asBool(false)));
      arr.push(o);
    }
  }
  Json body;
  body.set("fonts", arr);
  return apiOk(body);
}

}  // namespace ote_editor
