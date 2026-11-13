#include "platform/common/font_registry.h"

#include <cstdio>

#include "core/base/json.h"
#include "core/base/log.h"

namespace ote {
namespace {

bool fileExists(const std::string& path) {
  FILE* f = fopen(path.c_str(), "rb");
  if (!f) return false;
  fclose(f);
  return true;
}

bool isAbsolute(const std::string& p) {
  return !p.empty() && (p[0] == '/' || p[0] == '\\' || (p.size() > 1 && p[1] == ':'));
}

}  // namespace

FontRequest pixelFontRequest() {
  FontRequest r;
  r.file.clear();
  r.size = 8;
  r.smooth = false;
  return r;
}

std::vector<BuiltinFont> listBuiltinFonts(const std::string& assetsDir) {
  std::vector<BuiltinFont> out;
  BuiltinFont pixel;
  pixel.id = "pixel";
  pixel.name = "Built-in pixel font";
  out.push_back(pixel);

  std::string dir = assetsDir + "/fonts";
  Json reg;
  std::string err;
  if (!loadJsonFile(dir + "/fonts.json", reg, err)) {
    OTE_WARN("font registry unavailable (%s); only 'pixel' can be used by name", err.c_str());
    return out;
  }
  for (const Json& f : reg["fonts"].items()) {
    BuiltinFont b;
    b.id = f["id"].asString();
    b.name = f["name"].asString(b.id);
    std::string file = f["file"].asString();
    if (b.id.empty() || file.empty() || b.id == "pixel") continue;
    b.file = dir + "/" + file;
    b.size = f["size"].asInt(10);
    b.smooth = f["smooth"].asBool(true);
    if (!fileExists(b.file)) {
      OTE_WARN("font registry: '%s' lists missing file '%s'", b.id.c_str(), b.file.c_str());
      continue;
    }
    out.push_back(b);
  }
  return out;
}

FontRequest resolveFont(const std::string& assetsDir, const std::string& gameRoot, const FontChoice& choice) {
  FontRequest req;
  req.size = 10;
  req.smooth = true;  // defaults for a custom file; built-ins bring their own

  if (!choice.file.empty()) {
    req.file = (gameRoot.empty() || isAbsolute(choice.file)) ? choice.file : gameRoot + "/" + choice.file;
    if (!fileExists(req.file)) {
      OTE_WARN("font file '%s' not found; using the built-in pixel font", req.file.c_str());
      return pixelFontRequest();
    }
  } else if (choice.name.empty() || choice.name == "pixel") {
    FontRequest px = pixelFontRequest();  // smoothing never applies; size scales in whole multiples of 8
    if (choice.size > 0) px.size = choice.size;
    return px;
  } else {
    bool found = false;
    for (const BuiltinFont& b : listBuiltinFonts(assetsDir))
      if (b.id == choice.name && !b.file.empty()) {
        req.file = b.file;
        req.size = b.size;
        req.smooth = b.smooth;
        found = true;
        break;
      }
    if (!found) {
      OTE_WARN("unknown font '%s' (not in %s/fonts/fonts.json); using the built-in pixel font",
               choice.name.c_str(), assetsDir.c_str());
      return pixelFontRequest();
    }
  }
  if (choice.size > 0) req.size = choice.size;
  if (choice.smooth >= 0) req.smooth = choice.smooth != 0;
  return req;
}

}  // namespace ote
