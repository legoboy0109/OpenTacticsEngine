// rename_id: renames an entry's id and every reference to it that the schemas know about, in one atomic change.
#include <algorithm>
#include <cctype>

#include "paths.h"
#include "revision.h"
#include "tools.h"

namespace ote_mcp {

namespace {

// Renames an object member in place, keeping its position (key order is part of how a file reads).
bool renameKey(Json& obj, const std::string& from, const std::string& to) {
  if (!obj.isObject()) return false;
  for (auto& m : obj.mutableMembers())
    if (m.first == from) { m.first = to; return true; }
  return false;
}

ToolResult renameId(Context& ctx, const Json& args) {
  Game g;
  ToolResult err;
  if (!gameFromArgs(ctx, args, g, err)) return err;
  Txn txn(ctx, g);
  std::string rel;
  Json* root = fileFromArgs(txn, args, "file", rel, err);
  if (!root) return err;
  ConcretePath cp;
  if (!pathFromArgs(*root, args["path"].asString(), cp, err)) return err;
  if (cp.empty() || !cp.back().isIndex) return toolFail("path must name one list entry that has an id, e.g. [archer] in data/units.json or sides[blue] in a map");
  Json* list = valueAt(*root, parentOf(cp));
  if (!list || !list->isArray()) return toolFail("the entry is not in a list");
  Json& entry = list->mutableItems()[cp.back().index];
  if (!entry.isObject() || !entry["id"].isString()) return toolFail("that entry has no text \"id\" to rename", "get shows the entry");
  const std::string oldId = entry["id"].asString(), newId = args["newId"].asString();
  if (newId.empty()) return toolFail("newId must not be empty");
  for (char ch : newId) if ((unsigned char)ch < 0x20) return toolFail("newId must not contain control characters");
  if (newId == oldId) return toolFail("the id is already \"" + oldId + "\"");
  for (const Json& it : list->items())
    if (it["id"].isString() && it["id"].asString() == newId) return toolFail("another entry already has the id \"" + newId + "\"", "choose a different newId");

  std::vector<RefHit> hits = findReferences(txn, rel, parentOf(cp), oldId);
  // Deepest places first: renaming a map key moves everything under it, so the paths of the places below it must be used before.
  std::stable_sort(hits.begin(), hits.end(), [](const RefHit& a, const RefHit& b) { return a.path.size() > b.path.size(); });
  entry.set("id", jstr(newId));
  Json byFile = jobj();
  for (const RefHit& h : hits) {
    std::string e;
    Json* doc = txn.file(h.file, e);
    if (!doc) return toolFail(e);
    if (h.isKey) {
      Json* parent = valueAt(*doc, parentOf(h.path));
      if (!parent || !renameKey(*parent, oldId, newId)) return toolFail("could not rename a key in " + h.file);
    } else {
      Json* v = valueAt(*doc, h.path);
      if (!v) return toolFail("a reference in " + h.file + " moved while renaming");
      *v = jstr(newId);
    }
    byFile.set(h.file, jnum(byFile[h.file].asNumber(0) + 1));
  }

  // Scripts name ids as plain text; they cannot be rewritten safely, so say where they mention the old id.
  Json lua = jarr();
  for (const auto& f : ote_editor::listProjectFiles(g.dir)) {
    if (!ote_editor::hasExtension(f.first, ".lua")) continue;
    std::string text;
    if (!ote_editor::readWholeFile(f.second, text)) continue;
    size_t lineNo = 0;
    for (const std::string& line : splitOn(text, '\n')) {
      ++lineNo;
      if (line.find(oldId) != std::string::npos && lua.size() < 10) lua.push(jstr(f.first + ":" + std::to_string(lineNo)));
    }
  }

  Json extra = jobj();
  extra.set("renamed", jstr(oldId + " -> " + newId));
  extra.set("referencesUpdated", jnum((double)hits.size()));
  extra.set("referencesByFile", byFile);
  if (lua.size() > 0) {
    extra.set("scriptsMentionOldId", lua);
    extra.set("note", jstr("Lua scripts mention the old id as text; they were not changed. Check those lines."));
  }
  return finishWrite(txn, args, extra);
}

}  // namespace

void registerRenameTool(ToolRegistry& r, Context& ctx) {
  Context* c = &ctx;
  Tool t;
  t.name = "rename_id";
  t.title = "Rename an id everywhere";
  t.description = "Renames the id of a list entry (a unit type, weapon, terrain, item, skill, a map's side, a structure ...) and rewrites every reference to it that the editor's schemas know about: unit type ids in map units, weapon lists, enemy lock lists, resource and stat keys in maps keyed by ids, and so on, across all files of the game, in one atomic change. The result counts the references updated per file. References inside Lua scripts are plain text and are NOT changed; the result lists the script lines that mention the old id. The id must not already exist. The project check runs before and after: problemsBefore and problemsAfter in the result show that the game stayed valid. Use dryRun=true to see everything that would change.";
  Props p{{"game", gameProp()},
          {"file", sType("string", "File that holds the entry, e.g. data/units.json.")},
          {"path", sType("string", "The entry, e.g. [archer] or units[archer] (list entries as [index] or [id]).")},
          {"newId", sType("string", "The new id.")}};
  addWriteProps(p);
  t.inputSchema = sObject(p, {"file", "path", "newId"});
  t.readOnly = false;
  t.run = [c](const Json& a) { return renameId(*c, a); };
  r.add(t);
}

}  // namespace ote_mcp
