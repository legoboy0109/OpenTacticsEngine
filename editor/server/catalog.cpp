// Read-only catalogs: project templates and available fonts.
#include <algorithm>

#include "paths.h"
#include "projects.h"
#include "templates.h"

namespace ote_editor {

using ote::Json;

ApiResult listTemplates(const Config& cfg) {
  // The listing itself is shared with ote-mcp (editor/core/templates.cpp).
  Json arr = Json::makeArray();
  for (const TemplateInfo& t : ote_editor::listTemplates(cfg.templatesDir)) {
    Json o = Json::makeObject();
    o.set("id", Json::makeString(t.id));
    o.set("name", Json::makeString(t.name));
    o.set("description", Json::makeString(t.description));
    Json grid = Json::makeObject();
    grid.set("type", Json::makeString(t.gridType));
    if (!t.gridLayout.empty()) grid.set("layout", Json::makeString(t.gridLayout));
    o.set("grid", grid);
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
