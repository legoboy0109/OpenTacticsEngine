#include "srpg_files.h"

#include "paths.h"
#include "projects.h"

namespace ote_editor {

using ote::Json;

namespace {

struct ListFile { const char* key; const char* file; };
// The list sits under the same key inside the file as in the API.
const ListFile kLists[] = {
    {"characters", "data/characters.json"}, {"skills", "data/skills.json"}, {"abilities", "data/abilities.json"}};
const char* const kSupportsFile = "data/supports.json";

bool isFalse(const Json& v) { return v.type() == Json::Type::Bool && !v.asBool(); }

// Absent file = null with no problem; anything else is read, and a bad file is null plus a problem.
bool fileAbsent(const fs::path& dir, const char* rel) {
  fs::path full;
  std::string why;
  std::error_code ec;
  return resolveInside(dir, rel, full, why) && !fs::exists(full, ec);
}

}  // namespace

void readSrpgSections(const fs::path& dir, Json& doc, std::vector<Problem>& probs) {
  for (const ListFile& l : kLists) {
    Json list;
    if (!fileAbsent(dir, l.file)) {
      Json file;
      Problem p;
      if (!readJsonRel(dir, l.file, file, p)) probs.push_back(p);
      else if (!file.isObject() || !file[l.key].isArray())
        probs.push_back({l.file, std::string("expected an object with a \"") + l.key + "\" list"});
      else list = file[l.key];
    }
    doc.set(l.key, list);
  }
  Json supports;
  if (!fileAbsent(dir, kSupportsFile)) {
    Json s;
    Problem p;
    if (readJsonRel(dir, kSupportsFile, s, p) && s.isObject()) supports = s;
    else {
      if (p.message.empty()) p = {kSupportsFile, "the file must contain a JSON object at the top level"};
      probs.push_back(p);
    }
  }
  doc.set("supports", supports);
}

std::string checkSrpgBody(const Json& body) {
  for (const ListFile& l : kLists) {
    const Json& v = body[l.key];
    if (!v.isNull() && !v.isArray() && !isFalse(v))
      return std::string("\"") + l.key + "\" must be an array, or false to remove the file";
  }
  const Json& s = body["supports"];
  if (!s.isNull() && !s.isObject() && !isFalse(s)) return "\"supports\" must be an object, or false to remove the supports";
  return "";
}

SrpgFileOps planSrpgFiles(const Json& body) {
  SrpgFileOps ops;
  for (const ListFile& l : kLists) {
    const Json& v = body[l.key];
    if (v.isNull()) continue;
    // `false` switches the feature off. An empty list is written: the file is how the editor remembers that the
    // maker switched the feature on before adding anything (the engine treats an empty list as no file at all).
    if (isFalse(v)) { ops.removals.push_back(l.file); continue; }
    Json wrap = Json::makeObject();
    wrap.set(l.key, v);
    ops.writes.push_back({l.file, wrap.dump(2) + "\n"});
  }
  const Json& s = body["supports"];
  if (s.isObject()) ops.writes.push_back({kSupportsFile, s.dump(2) + "\n"});
  else if (isFalse(s)) ops.removals.push_back(kSupportsFile);
  return ops;
}

bool isSrpgFile(const std::string& rel) {
  for (const ListFile& l : kLists)
    if (rel == l.file) return true;
  return rel == kSupportsFile;
}

}  // namespace ote_editor
