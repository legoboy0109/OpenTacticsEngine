// Editing tools: set, add_entry, duplicate_entry, remove_entry, move_entry, enable_block, disable_block. Each one edits the
// working copy of the files in a Txn, checks the changed values against the schema, and lets Txn::commit write (or, for a
// dry run, report) the result.
#include <algorithm>

#include "tools.h"

namespace ote_mcp {

using ote_editor::SchemaNode;
using K = SchemaNode::Kind;

namespace {

CheckEnv envFor(Txn& t, const Json* doc) {
  CheckEnv e;
  e.doc = doc;
  e.docs = t.docLookup();
  return e;
}

// Checks the value now sitting at `cp` (already inserted in `root`) against the schema; appends issues.
void checkPlace(Context& ctx, Txn& txn, const std::string& rel, Json& root, const ConcretePath& cp, bool allowUnknown, std::vector<Issue>& issues) {
  const ote_editor::FileSchema* fs = schemaFor(ctx, rel);
  const NodeAt na = schemaAt(fs, root, cp);
  const Json* v = valueAt(root, cp);
  const std::string label = rel + ":" + pathLabel(root, cp);
  if (!v) return;
  if (na.node) {
    const CheckEnv env = envFor(txn, &root);
    checkValue(na.node, *v, env, label, allowUnknown, issues);
    return;
  }
  if (na.parent && na.parent->resolved().kind == K::Json) return;  // below a field that is raw JSON by design
  if (allowUnknown) return;
  if (na.unknownKey && na.parent) {
    std::vector<std::string> keys;
    for (const auto& f : na.parent->resolved().fields) keys.push_back(f.first);
    std::string all;
    for (size_t i = 0; i < keys.size() && i < 40; ++i) all += (i ? ", " : "") + keys[i];
    issues.push_back({label, "unknown key \"" + na.key + "\" (this section knows: " + all + "; pass allowUnknown=true to write it anyway)", keys});
  } else if (!fs) {
    issues.push_back({label, "this file has no schema, so the value cannot be checked; pass allowUnknown=true to write it anyway", {}});
  } else {
    issues.push_back({label, "the schema does not describe this place (raw part of the file); pass allowUnknown=true to write it anyway", {}});
  }
}

// Sets (or creates) the value at `cp`; false when the container cannot take it.
bool putValue(Json& root, const ConcretePath& cp, const Json& value, std::string& why) {
  if (cp.empty()) { root = value; return true; }
  Json* parent = valueAt(root, parentOf(cp));
  if (!parent) { why = "the container does not exist"; return false; }
  const Step& last = cp.back();
  if (last.isIndex) {
    if (!parent->isArray() || last.index >= parent->items().size()) { why = "list index out of range"; return false; }
    parent->mutableItems()[last.index] = value;
    return true;
  }
  if (!parent->isObject()) { why = "cannot add a key to something that is not an object"; return false; }
  for (auto& m : parent->mutableMembers())
    if (m.first == last.key) { m.second = value; return true; }
  parent->mutableMembers().emplace_back(last.key, value);
  return true;
}

// --- set --------------------------------------------------------------------------------------------------------------

ToolResult setTool(Context& ctx, const Json& args) {
  Game g;
  ToolResult err;
  if (!gameFromArgs(ctx, args, g, err)) return err;
  Txn txn(ctx, g);
  const bool allowUnknown = args["allowUnknown"].asBool(false);
  std::vector<Issue> issues;
  Json changes = jarr();
  for (const Json& ch : args["changes"].items()) {
    std::string rel;
    ToolResult e2;
    Json* root = fileFromArgs(txn, ch, "file", rel, e2);
    if (!root) return e2;
    ConcretePath cp;
    if (!pathFromArgs(*root, ch["path"].asString(), cp, e2, true)) return e2;
    if (cp.empty()) return toolFail("path must name a place inside the file, not the whole file", "to replace a whole file use set with path of its top-level key");
    Json before;
    if (const Json* old = valueAt(*root, cp)) before = *old;
    std::string why;
    if (!putValue(*root, cp, ch["value"], why)) return toolFail("cannot set " + ch["path"].asString() + ": " + why);
    // A key that excludes others (turns / until) removes them, as the editor does.
    {
      const NodeAt na = schemaAt(schemaFor(ctx, rel), *root, cp);
      if (na.node && !na.node->excludes.empty()) {
        Json* parent = valueAt(*root, parentOf(cp));
        if (parent && parent->isObject()) for (const std::string& k : na.node->excludes) parent->erase(k);
      }
    }
    checkPlace(ctx, txn, rel, *root, cp, allowUnknown, issues);
    Json c = jobj();
    c.set("file", jstr(rel));
    c.set("path", jstr(pathLabel(*root, cp)));
    c.set("before", before.isObject() || before.isArray() ? jstr(previewOf(before, 200)) : before);
    c.set("after", ch["value"].isObject() || ch["value"].isArray() ? jstr(previewOf(ch["value"], 200)) : ch["value"]);
    changes.push(c);
  }
  if (!issues.empty()) return refuse(issues, "");
  Json extra = jobj();
  extra.set("changes", changes);
  return finishWrite(txn, args, extra);
}

// --- add / duplicate --------------------------------------------------------------------------------------------------

std::string uniqueId(const Json& list, const std::string& base) {
  auto taken = [&](const std::string& id) {
    for (const Json& it : list.items()) if (it["id"].isString() && it["id"].asString() == id) return true;
    return false;
  };
  if (!taken(base)) return base;
  for (int n = 2; n < 100000; ++n)
    if (!taken(base + "_" + std::to_string(n))) return base + "_" + std::to_string(n);
  return base;
}

// The list the arguments name: the table array of a table file when path is omitted.
bool listFromArgs(Txn& txn, Context& ctx, const Json& args, std::string& rel, Json*& root, ConcretePath& lp, Json*& list, ToolResult& err) {
  root = fileFromArgs(txn, args, "file", rel, err);
  if (!root) return false;
  const std::string pathText = args["path"].asString();
  if (pathText.empty()) {
    const Json* t = ote_editor::tableArray(*root);
    if (!t && root->isObject() && root->members().size() == 1 && root->members()[0].second.isArray()) t = &root->members()[0].second;
    if (!t && !root->isArray()) {
      err = toolFail("this file is not a table: pass path, the list to add to (e.g. events or units)", "describe shows which keys are lists");
      return false;
    }
    if (root->isObject()) { Step k; k.key = root->members()[0].first; lp.push_back(k); }
  } else if (!pathFromArgs(*root, pathText, lp, err)) {
    return false;
  }
  (void)ctx;
  list = valueAt(*root, lp);
  if (!list || !list->isArray()) {
    err = toolFail("the place \"" + pathText + "\" is not a list" + (list ? " (it is " + previewOf(*list) + ")" : ""), "describe shows what is there");
    return false;
  }
  return true;
}

ToolResult addEntry(Context& ctx, const Json& args) {
  Game g;
  ToolResult err;
  if (!gameFromArgs(ctx, args, g, err)) return err;
  Txn txn(ctx, g);
  std::string rel;
  Json* root = nullptr;
  Json* list = nullptr;
  ConcretePath lp;
  if (!listFromArgs(txn, ctx, args, rel, root, lp, list, err)) return err;
  const ote_editor::FileSchema* fs = schemaFor(ctx, rel);
  const SchemaNode* itemNode = itemNodeAt(fs, *root, lp);
  const CheckEnv env = envFor(txn, root);
  Json item;
  if (args.has("value")) {
    item = args["value"];
    if (!args["kind"].asString().empty()) return toolFail("pass either value or kind, not both (kind picks what a new entry starts as)");
  } else {
    if (!itemNode) return toolFail("this list has no schema, so there is no default entry: pass value", "get shows what the existing entries look like");
    std::string why;
    if (!newValueFor(*itemNode, args["kind"].asString(), env, item, why)) return toolFail(why, "describe the list to see the kinds");
  }
  if (args["fields"].isObject()) {
    if (!item.isObject()) return toolFail("fields can only be laid over an entry that is an object");
    for (const auto& m : args["fields"].members()) item.set(m.first, m.second);
  }
  Json generated = jobj();
  if (item.isObject()) {
    if (args["id"].isString()) item.set("id", args["id"]);
    const SchemaNode* idField = itemNode ? itemNode->resolved().field("id") : nullptr;
    const bool wantsId = idField && idField->required && itemNode->resolved().kind == K::Object;  // only where the id is required (map units have an optional one)
    if (wantsId && (!item["id"].isString() || item["id"].asString().empty())) {
      const std::string id = uniqueId(*list, "new_entry");
      item.set("id", jstr(id));
      generated.set("id", jstr(id));
    }
    if (item["id"].isString() && !item["id"].asString().empty()) {
      for (const Json& it : list->items())
        if (it["id"].isString() && it["id"].asString() == item["id"].asString())
          return toolFail("an entry with id \"" + item["id"].asString() + "\" already exists here", "pick another id, or use set to change the existing entry (or duplicate_entry)");
    }
  }
  size_t index = list->size();
  if (args.has("index")) {
    index = (size_t)args["index"].asInt(0);
    if (index > list->size()) return toolFail("index " + std::to_string(index) + " is beyond the end (the list has " + std::to_string(list->size()) + " entries; use " + std::to_string(list->size()) + " to append)");
  }
  list->mutableItems().insert(list->mutableItems().begin() + (long)index, item);
  ConcretePath ip = lp;
  Step s;
  s.isIndex = true;
  s.index = index;
  ip.push_back(s);
  std::vector<Issue> issues;
  checkPlace(ctx, txn, rel, *root, ip, args["allowUnknown"].asBool(false), issues);
  if (!issues.empty()) return refuse(issues, rel);
  Json extra = jobj();
  extra.set("file", jstr(rel));
  extra.set("path", jstr(pathLabel(*root, ip)));
  extra.set("entry", item);
  if (generated.size() > 0) extra.set("note", jstr("no id was given: \"" + generated["id"].asString() + "\" was used; rename it with rename_id or set"));
  return finishWrite(txn, args, extra);
}

ToolResult duplicateEntry(Context& ctx, const Json& args) {
  Game g;
  ToolResult err;
  if (!gameFromArgs(ctx, args, g, err)) return err;
  Txn txn(ctx, g);
  std::string rel;
  Json* root = fileFromArgs(txn, args, "file", rel, err);
  if (!root) return err;
  ConcretePath cp;
  if (!pathFromArgs(*root, args["path"].asString(), cp, err)) return err;
  if (cp.empty() || !cp.back().isIndex) return toolFail("path must name one entry of a list, e.g. [archer] or events[2]");
  Json* list = valueAt(*root, parentOf(cp));
  if (!list || !list->isArray()) return toolFail("the entry is not in a list");
  Json copy = list->items()[cp.back().index];
  Json extra = jobj();
  if (copy.isObject() && copy["id"].isString()) {
    const std::string wanted = args["newId"].asString();
    if (!wanted.empty()) {
      for (const Json& it : list->items())
        if (it["id"].isString() && it["id"].asString() == wanted) return toolFail("an entry with id \"" + wanted + "\" already exists", "choose another newId");
      copy.set("id", jstr(wanted));
    } else {
      copy.set("id", jstr(uniqueId(*list, copy["id"].asString() + "_copy")));
    }
    extra.set("newId", copy["id"]);
  }
  size_t index = cp.back().index + 1;
  if (args.has("index")) {
    index = (size_t)args["index"].asInt(0);
    if (index > list->size()) return toolFail("index " + std::to_string(index) + " is beyond the end of the list");
  }
  list->mutableItems().insert(list->mutableItems().begin() + (long)index, copy);
  ConcretePath ip = parentOf(cp);
  Step s;
  s.isIndex = true;
  s.index = index;
  ip.push_back(s);
  extra.set("file", jstr(rel));
  extra.set("path", jstr(pathLabel(*root, ip)));
  return finishWrite(txn, args, extra);
}

// --- remove / move ----------------------------------------------------------------------------------------------------

ToolResult removeEntry(Context& ctx, const Json& args) {
  Game g;
  ToolResult err;
  if (!gameFromArgs(ctx, args, g, err)) return err;
  Txn txn(ctx, g);
  std::string rel;
  Json* root = fileFromArgs(txn, args, "file", rel, err);
  if (!root) return err;
  ConcretePath cp;
  if (!pathFromArgs(*root, args["path"].asString(), cp, err)) return err;
  if (cp.empty()) return toolFail("path must name an entry or a key, not the whole file");
  Json* parent = valueAt(*root, parentOf(cp));
  if (!parent) return toolFail("nothing at that path");
  const Json removedValue = *valueAt(*root, cp);
  // An entry with an id that other places still point at is not removed while they do.
  if (!args["ignoreReferences"].asBool(false) && cp.back().isIndex && removedValue.isObject() && removedValue["id"].isString()) {
    const std::vector<RefHit> hits = findReferences(txn, rel, parentOf(cp), removedValue["id"].asString());
    if (!hits.empty()) {
      Json uses = jarr();
      for (size_t i = 0; i < hits.size() && i < 15; ++i) {
        Json u = jobj();
        u.set("file", jstr(hits[i].file));
        const Json* doc = txn.peek(hits[i].file);
        u.set("path", jstr(doc ? pathLabel(*doc, hits[i].path) : ""));
        uses.push(u);
      }
      Json extra = jobj();
      extra.set("usedBy", uses);
      extra.set("uses", jnum((double)hits.size()));
      return toolFail("\"" + removedValue["id"].asString() + "\" is still referenced in " + std::to_string(hits.size()) + " place(s), so removing it would leave dangling references",
                      "remove or change those references first (see usedBy), or pass ignoreReferences=true to remove it anyway", extra);
    }
  }
  const Step last = cp.back();
  if (last.isIndex) {
    if (!parent->isArray() || last.index >= parent->items().size()) return toolFail("list index out of range");
    parent->mutableItems().erase(parent->mutableItems().begin() + (long)last.index);
  } else {
    if (!parent->isObject()) return toolFail("not an object");
    std::string why;
    // Required keys cannot be removed (the editor refuses the same).
    const NodeAt na = schemaAt(schemaFor(ctx, rel), *root, cp);
    if (na.node && na.node->required) return toolFail("\"" + last.key + "\" is required here and cannot be removed", "set it to another value instead");
    parent->erase(last.key);
  }
  Json extra = jobj();
  extra.set("file", jstr(rel));
  extra.set("removed", removedValue.isObject() || removedValue.isArray() ? jstr(previewOf(removedValue, 200)) : removedValue);
  return finishWrite(txn, args, extra);
}

ToolResult moveEntry(Context& ctx, const Json& args) {
  Game g;
  ToolResult err;
  if (!gameFromArgs(ctx, args, g, err)) return err;
  Txn txn(ctx, g);
  std::string rel;
  Json* root = fileFromArgs(txn, args, "file", rel, err);
  if (!root) return err;
  ConcretePath cp;
  if (!pathFromArgs(*root, args["path"].asString(), cp, err)) return err;
  if (cp.empty() || !cp.back().isIndex) return toolFail("path must name one entry of a list, e.g. events[2]");
  Json* list = valueAt(*root, parentOf(cp));
  if (!list || !list->isArray()) return toolFail("the entry is not in a list");
  const size_t from = cp.back().index, to = (size_t)args["toIndex"].asInt(0);
  if (to >= list->size()) return toolFail("toIndex " + std::to_string(to) + " is out of range (the list has " + std::to_string(list->size()) + " entries, last index " + std::to_string(list->size() - 1) + ")");
  auto& v = list->mutableItems();
  if (from < to) std::rotate(v.begin() + (long)from, v.begin() + (long)from + 1, v.begin() + (long)to + 1);
  else if (to < from) std::rotate(v.begin() + (long)to, v.begin() + (long)from, v.begin() + (long)from + 1);
  Json extra = jobj();
  extra.set("file", jstr(rel));
  extra.set("from", jnum((double)from));
  extra.set("to", jnum((double)to));
  return finishWrite(txn, args, extra);
}

// --- blocks -----------------------------------------------------------------------------------------------------------

ToolResult enableBlock(Context& ctx, const Json& args) {
  Game g;
  ToolResult err;
  if (!gameFromArgs(ctx, args, g, err)) return err;
  Txn txn(ctx, g);
  std::string rel;
  Json* root = fileFromArgs(txn, args, "file", rel, err);
  if (!root) return err;
  ConcretePath cp;
  if (!pathFromArgs(*root, args["path"].asString(), cp, err, true)) return err;
  const ote_editor::FileSchema* fs = schemaFor(ctx, rel);
  const NodeAt na = schemaAt(fs, *root, cp);
  if (!na.node || !na.node->block) {
    return toolFail("\"" + args["path"].asString() + "\" is not an optional block", "describe the parent place: keys marked optionalBlock can be enabled");
  }
  const Json* now = valueAt(*root, cp);
  if (now && !now->isNull()) return toolFail("the block is already enabled", "use set to change values inside it");
  const CheckEnv env = envFor(txn, root);
  if (na.node->needs) {
    const std::vector<std::string> have = env.ids(na.node->needs->refs);
    if (have.empty()) return toolFail("this block cannot be enabled yet: " + na.node->needs->hint, "add what it needs first (for example a resource in game.json)");
  }
  const Json value = ote_editor::schemaDefault(*na.node, env.lookup());
  std::string why;
  if (!putValue(*root, cp, value, why)) return toolFail(why);
  std::vector<Issue> issues;
  checkPlace(ctx, txn, rel, *root, cp, false, issues);
  if (!issues.empty()) return refuse(issues, rel);
  Json extra = jobj();
  extra.set("file", jstr(rel));
  extra.set("enabled", jstr(args["path"].asString()));
  extra.set("startsAs", value);
  extra.set("next", jstr("describe the path to see the block's keys, then set them"));
  return finishWrite(txn, args, extra);
}

ToolResult disableBlock(Context& ctx, const Json& args) {
  Game g;
  ToolResult err;
  if (!gameFromArgs(ctx, args, g, err)) return err;
  Txn txn(ctx, g);
  std::string rel;
  Json* root = fileFromArgs(txn, args, "file", rel, err);
  if (!root) return err;
  ConcretePath cp;
  if (!pathFromArgs(*root, args["path"].asString(), cp, err)) return err;
  const NodeAt na = schemaAt(schemaFor(ctx, rel), *root, cp);
  if (!na.node || !na.node->block) return toolFail("\"" + args["path"].asString() + "\" is not an optional block", "describe the parent place: keys marked optionalBlock can be disabled");
  Json* parent = valueAt(*root, parentOf(cp));
  if (!parent || !parent->isObject() || cp.back().isIndex) return toolFail("cannot disable this place");
  parent->erase(cp.back().key);
  Json extra = jobj();
  extra.set("file", jstr(rel));
  extra.set("disabled", jstr(args["path"].asString()));
  return finishWrite(txn, args, extra);
}

Json fileProp() { return sType("string", "Project file, e.g. data/units.json, game.json, maps/start.json."); }
Json pathProp(const std::string& what) { return sType("string", what); }

}  // namespace

void registerEditTools(ToolRegistry& r, Context& ctx) {
  Context* c = &ctx;
  const std::string pathSyntax = " Path syntax: keys joined by dots, list entries as [index] or [id], e.g. units[archer].stats.hp; in table files like data/units.json the table's own key may be left out ([archer].stats.hp).";
  {
    Tool t;
    t.name = "set";
    t.title = "Set values";
    t.description = "Changes one or many values in one atomic call: either all changes are written or none. Each change names a file, a path and the new JSON value (a whole object or list is fine). Every value is checked against the editor's schema before anything is written: wrong type, a number out of range, a word that is not in the allowed list, a reference to an id that does not exist, an unknown key in a described section, a missing required key. A refusal says why and lists the allowed values. Parts the schema does not describe (the raw tile grid, for instance) need allowUnknown=true. After the write the engine's project check runs and the result tells how many problems there were before and after and which are new. Use dryRun=true to see the exact diff first." + pathSyntax;
    Props p{{"game", gameProp()},
            {"changes", sArray(sObject({{"file", fileProp()}, {"path", pathProp("Place to set; a missing last key is created.")}, {"value", anyProp("The new value: any JSON.")}}, {"file", "path", "value"}), "The changes, applied in order.")},
            {"allowUnknown", sType("boolean", "true = also allow keys and places the schema does not describe.")}};
    addWriteProps(p);
    t.inputSchema = sObject(p, {"changes"});
    t.readOnly = false;
    t.run = [c](const Json& a) { return setTool(*c, a); };
    r.add(t);
  }
  {
    Tool t;
    t.name = "add_entry";
    t.title = "Add a list entry";
    t.description = "Adds a new entry to a list: a row of a table file (a unit, weapon, terrain, item ...), a map's unit or side, a story event, a command in an event's command list. Without value the entry starts from the schema's defaults (required keys filled; ids from the game); for lists of several shapes pass kind (e.g. a story command kind such as say or addUnit; describe the list to see the kinds). fields lays chosen keys over the starting entry, id sets its id (a table row without one gets new_entry, new_entry_2 ...). The result is checked against the schema like set, and an id that already exists is refused. For a table file leave path out; otherwise path is the list." + pathSyntax;
    Props p{{"game", gameProp()},
            {"file", fileProp()},
            {"path", pathProp("The list to add to (omit for the rows of a table file), e.g. units or events[0].do.")},
            {"value", anyProp("The whole entry as JSON (instead of the schema defaults).")},
            {"kind", sType("string", "For lists of several shapes: which kind to start from (see describe).")},
            {"fields", sType("object", "Keys to lay over the starting entry, e.g. {\"name\": \"Archer\"}.")},
            {"id", sType("string", "The new entry's id (for entries that have one).")},
            {"index", sInt("Position to insert at (0 = first); default: the end.", 0, 1000000)},
            {"allowUnknown", sType("boolean", "true = also allow keys the schema does not describe.")}};
    addWriteProps(p);
    t.inputSchema = sObject(p, {"file"});
    t.readOnly = false;
    t.run = [c](const Json& a) { return addEntry(*c, a); };
    r.add(t);
  }
  {
    Tool t;
    t.name = "duplicate_entry";
    t.title = "Duplicate a list entry";
    t.description = "Copies one list entry (a unit type, a weapon, a map unit, an event ...) and inserts the copy right after it, or at index. An entry with an id gets newId, or the old id plus _copy (made unique). The copy is not renamed anywhere else and nothing refers to it yet." + pathSyntax;
    Props p{{"game", gameProp()},
            {"file", fileProp()},
            {"path", pathProp("The entry to copy, e.g. [archer] or events[2].")},
            {"newId", sType("string", "Id of the copy.")},
            {"index", sInt("Where to insert the copy (default: right after the original).", 0, 1000000)}};
    addWriteProps(p);
    t.inputSchema = sObject(p, {"file", "path"});
    t.readOnly = false;
    t.run = [c](const Json& a) { return duplicateEntry(*c, a); };
    r.add(t);
  }
  {
    Tool t;
    t.name = "remove_entry";
    t.title = "Remove an entry or key";
    t.description = "Removes one list entry or one key. An entry with an id that other places still refer to (found through the schemas' reference fields) is not removed: the refusal lists the references so you can change them first (search finds more); pass ignoreReferences=true to remove it anyway. Required keys cannot be removed. This never deletes files." + pathSyntax;
    Props p{{"game", gameProp()},
            {"file", fileProp()},
            {"path", pathProp("The entry or key to remove, e.g. [archer] or rules.fog.")},
            {"ignoreReferences", sType("boolean", "true = remove even though other places refer to this id.")}};
    addWriteProps(p);
    t.inputSchema = sObject(p, {"file", "path"});
    t.readOnly = false;
    t.destructive = true;
    t.run = [c](const Json& a) { return removeEntry(*c, a); };
    r.add(t);
  }
  {
    Tool t;
    t.name = "move_entry";
    t.title = "Move a list entry";
    t.description = "Moves one list entry to another position in the same list (0 = first); the entries in between shift by one. Order matters for event command lists, map units and anything shown in order." + pathSyntax;
    Props p{{"game", gameProp()},
            {"file", fileProp()},
            {"path", pathProp("The entry to move, e.g. events[2].do[1].")},
            {"toIndex", sInt("The position it should end up at.", 0, 1000000)}};
    addWriteProps(p);
    t.inputSchema = sObject(p, {"file", "path", "toIndex"});
    t.readOnly = false;
    t.run = [c](const Json& a) { return moveEntry(*c, a); };
    r.add(t);
  }
  {
    Tool t;
    t.name = "enable_block";
    t.title = "Switch an optional block on";
    t.description = "Many rule systems of the game are optional blocks in game.json 'rules' (action points, height, fog, economy ...) and some sections elsewhere. A block that is absent is off. This writes the block with exactly what the editor's Enable switch writes: its required keys filled with valid starting values. A block that needs something that does not exist yet (action points need a resource) is refused with a hint. describe the parent place to see which keys are optional blocks and whether they are enabled; then set the block's keys." + pathSyntax;
    Props p{{"game", gameProp()}, {"file", fileProp()}, {"path", pathProp("The block, e.g. rules.actionPoints.")}};
    addWriteProps(p);
    t.inputSchema = sObject(p, {"file", "path"});
    t.readOnly = false;
    t.run = [c](const Json& a) { return enableBlock(*c, a); };
    r.add(t);
  }
  {
    Tool t;
    t.name = "disable_block";
    t.title = "Switch an optional block off";
    t.description = "Removes an optional block (for example rules.fog), so the game uses the engine's defaults for that system again. The block's current contents are lost from the file (a dry run shows them in the diff; enable_block writes a fresh one)." + pathSyntax;
    Props p{{"game", gameProp()}, {"file", fileProp()}, {"path", pathProp("The block, e.g. rules.actionPoints.")}};
    addWriteProps(p);
    t.inputSchema = sObject(p, {"file", "path"});
    t.readOnly = false;
    t.destructive = true;
    t.run = [c](const Json& a) { return disableBlock(*c, a); };
    r.add(t);
  }
}

}  // namespace ote_mcp
