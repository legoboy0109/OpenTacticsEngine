#include <algorithm>

#include "paths.h"
#include "revision.h"
#include "tools.h"

namespace ote_mcp {

Json gameProp() {
  return sType("string", "Id of the game (a folder name from list_games). Optional when only one game exists under the roots.");
}

void addWriteProps(Props& props) {
  props.emplace_back("dryRun", sType("boolean", "true = change nothing on disk; return exactly what would change (a per-file diff) and the project problems the result would have."));
  props.emplace_back("force", sType("boolean", "true = write even if a file was changed on disk (for example by an editor) since you last read it. Default false: such a write is refused."));
}

void addPagingProps(Props& props, int defaultLimit) {
  props.emplace_back("limit", sInt("How many entries to return (default " + std::to_string(defaultLimit) + ").", 1, 1000));
  props.emplace_back("offset", sInt("Skip this many entries (use the nextOffset of the previous result).", 0, 100000000));
}

bool gameFromArgs(Context& ctx, const Json& args, Game& out, ToolResult& err) {
  std::string why;
  if (ctx.findGame(args["game"].asString(), out, why)) return true;
  err = toolFail(why, "list_games");
  return false;
}

ToolResult finishWrite(Txn& txn, const Json& args, Json extra) {
  CommitOptions opt;
  opt.dryRun = args["dryRun"].asBool(false);
  opt.force = args["force"].asBool(false);
  bool failed = false;
  Json report = txn.commit(opt, failed);
  Json out = extra.isObject() ? extra : jobj();
  for (const auto& m : report.members()) out.set(m.first, m.second);
  ToolResult r;
  r.data = out;
  r.isError = failed;
  return r;
}

Json* fileFromArgs(Txn& txn, const Json& args, const char* key, std::string& rel, ToolResult& err) {
  rel = args[key].asString();
  std::string why;
  Json* j = txn.file(rel, why);
  if (!j) err = toolFail(why, "list_files shows the game's files");
  return j;
}

bool pathFromArgs(Json& root, const std::string& pathText, ConcretePath& out, ToolResult& err, bool create) {
  std::vector<Seg> segs;
  std::string why;
  if (!parsePath(pathText, segs, why) || !resolvePathIn(root, segs, out, why, create)) {
    err = toolFail("path \"" + pathText + "\": " + why, "get (without a path) shows the top of the file; describe shows the keys");
    return false;
  }
  return true;
}

ToolResult refuse(const std::vector<Issue>& issues, const std::string& file) {
  Json arr = jarr();
  std::string first;
  for (const Issue& i : issues) {
    Json o = jobj();
    o.set("path", jstr(i.path));
    o.set("problem", jstr(i.message));
    if (!i.allowed.empty()) o.set("allowed", jstrs(std::vector<std::string>(i.allowed.begin(), i.allowed.begin() + (long)std::min<size_t>(i.allowed.size(), 40))));
    arr.push(o);
    if (first.empty()) first = (i.path.empty() ? "" : i.path + ": ") + i.message;
  }
  Json extra = jobj();
  extra.set("refused", arr);
  extra.set("written", jbool(false));
  return toolFail("refused, nothing was written" + (file.empty() ? "" : " (" + file + ")") + ": " + first +
                      (issues.size() > 1 ? " (and " + std::to_string(issues.size() - 1) + " more, see refused)" : ""),
                  "fix the value (describe the path shows what is allowed), or pass allowUnknown=true for parts the schema does not describe", extra);
}

std::string previewOf(const Json& v, size_t maxChars) {
  std::string s;
  if (v.isString()) s = "\"" + v.asString() + "\"";
  else if (v.isObject()) s = "{" + std::to_string(v.size()) + " keys}";
  else if (v.isArray()) s = "[" + std::to_string(v.size()) + " items]";
  else s = v.dump(0);
  if (s.size() > maxChars) {
    size_t cut = maxChars;
    while (cut > 0 && ((unsigned char)s[cut] & 0xC0) == 0x80) --cut;  // do not cut a UTF-8 character in half
    s = s.substr(0, cut) + "...";
  }
  return s;
}

const ote_editor::SchemaNode* itemNodeAt(const ote_editor::FileSchema* fs, const Json& root, const ConcretePath& listPath) {
  if (!fs) return nullptr;
  const NodeAt na = schemaAt(fs, root, listPath);
  if (na.isTable) return fs->rows.get();
  if (na.node && na.node->kind == ote_editor::SchemaNode::Kind::List && na.node->of) return &na.node->of->resolved();
  return nullptr;
}

std::vector<std::string> jsonFilesOf(const Game& g) {
  std::vector<std::string> out;
  for (const auto& f : ote_editor::listProjectFiles(g.dir))
    if (ote_editor::hasExtension(f.first, ".json")) out.push_back(f.first);
  return out;
}

}  // namespace ote_mcp
