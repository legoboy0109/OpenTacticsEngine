#include "validate.h"

#include <memory>

#include "core/battle/battle.h"
#include "core/data/loader.h"
#include "core/script/formulas.h"
#include "log.h"

namespace ote_editor {

// The loader builds paths as "<dir as given>/<relative>", so we pass a
// generic_string without trailing slash and can strip exactly that prefix.
static std::string dirString(const fs::path& dir) {
  std::string s = dir.generic_string();
  while (s.size() > 1 && s.back() == '/') s.pop_back();
  return s;
}

Problem problemFromMessage(const fs::path& dir, const std::string& message) {
  std::string prefix = dirString(dir) + "/";
  if (message.compare(0, prefix.size(), prefix) != 0) return {"", message};
  std::string rest = message.substr(prefix.size());
  size_t colon = rest.find(": ");
  if (colon == std::string::npos) return {rest, ""};
  return {rest.substr(0, colon), rest.substr(colon + 2)};
}

std::vector<Problem> validateProject(const fs::path& dir) {
  std::vector<Problem> out;
  std::string root = dirString(dir);
  EngineLogCapture capture;
  std::string err;

  ote::Database db;
  if (!ote::loadGame(root, db, err)) {
    out.push_back(problemFromMessage(dir, err));
    return out;
  }

  std::vector<ote::MapDef> maps(db.maps.size());
  std::vector<bool> mapOk(db.maps.size(), false);
  for (size_t i = 0; i < db.maps.size(); ++i) {
    if (ote::loadMap(db, db.maps[i].id, maps[i], err)) mapOk[i] = true;
    else out.push_back(problemFromMessage(dir, err));
  }

  // Heap: Formulas owns a Lua state and is not copyable/movable.
  std::unique_ptr<ote::Formulas> formulas(new ote::Formulas());
  if (!formulas->init(db, err)) {
    out.push_back(problemFromMessage(dir, err));
    return out;
  }

  for (size_t i = 0; i < db.maps.size(); ++i) {
    if (!mapOk[i]) continue;
    std::unique_ptr<ote::Battle> battle(new ote::Battle());
    if (!battle->init(db, maps[i], *formulas, 1, err)) {
      Problem p = problemFromMessage(dir, err);
      if (p.file.empty()) p.file = db.maps[i].file;  // battle setup errors belong to the map
      out.push_back(p);
      continue;
    }
    std::vector<ote::Event> events;
    battle->start(events);
  }

  for (const std::string& line : capture.lines) out.push_back({"", line});
  return out;
}

ote::Json problemsToJson(const std::vector<Problem>& problems) {
  ote::Json arr = ote::Json::makeArray();
  for (const Problem& p : problems) {
    ote::Json o = ote::Json::makeObject();
    o.set("file", ote::Json::makeString(p.file));
    o.set("message", ote::Json::makeString(p.message));
    arr.push(o);
  }
  return arr;
}

}  // namespace ote_editor
