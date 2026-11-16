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
// One JSON object per file, in the API under the key shown; `false` in the PUT body removes the file. Weapon ranks and
// stat caps (docs/SRPG.md section 7) are two of these: `ranks` is the file's whole object ({growth, ranks}), `caps`
// likewise ({caps}). An empty ladder / empty caps object is written like an empty list: the loader treats it as no
// file at all, and the file is how the editor remembers that the maker switched the feature on.
const ListFile kObjects[] = {{"supports", "data/supports.json"}, {"ranks", "data/ranks.json"}, {"caps", "data/caps.json"}};

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
  for (const ListFile& o : kObjects) {
    Json obj;
    if (!fileAbsent(dir, o.file)) {
      Json s;
      Problem p;
      if (readJsonRel(dir, o.file, s, p) && s.isObject()) obj = s;
      else {
        if (p.message.empty()) p = {o.file, "the file must contain a JSON object at the top level"};
        probs.push_back(p);
      }
    }
    doc.set(o.key, obj);
  }
}

std::string checkSrpgBody(const Json& body) {
  for (const ListFile& l : kLists) {
    const Json& v = body[l.key];
    if (!v.isNull() && !v.isArray() && !isFalse(v))
      return std::string("\"") + l.key + "\" must be an array, or false to remove the file";
  }
  for (const ListFile& o : kObjects) {
    const Json& v = body[o.key];
    if (!v.isNull() && !v.isObject() && !isFalse(v))
      return std::string("\"") + o.key + "\" must be an object, or false to remove the file";
  }
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
  for (const ListFile& o : kObjects) {
    const Json& v = body[o.key];
    if (v.isObject()) ops.writes.push_back({o.file, v.dump(2) + "\n"});
    else if (isFalse(v)) ops.removals.push_back(o.file);
  }
  return ops;
}

bool isSrpgFile(const std::string& rel) {
  for (const ListFile& l : kLists)
    if (rel == l.file) return true;
  for (const ListFile& o : kObjects)
    if (rel == o.file) return true;
  return false;
}

}  // namespace ote_editor
