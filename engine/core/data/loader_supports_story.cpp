// Supports in stories and scenes (docs/SRPG.md "Supports": Story and campaign): the command addSupport, the
// condition support and the trigger support of map events, the same command and condition in campaign scenes, and
// the check of those scenes once data/supports.json is loaded.
#include "core/data/loader_campaign.h"
#include "core/data/loader_events.h"
#include "core/data/loader_srpg.h"

namespace ote {

bool parseCmpOp(const std::string& t, CmpOp& op) {
  if (t == "=" || t == "==") op = CmpOp::Eq;
  else if (t == "!=") op = CmpOp::Ne;
  else if (t == "<") op = CmpOp::Lt;
  else if (t == "<=") op = CmpOp::Le;
  else if (t == ">") op = CmpOp::Gt;
  else if (t == ">=") op = CmpOp::Ge;
  else return false;
  return true;
}

std::string notASupportPair(const std::string& a, const std::string& b) {
  return quoted(a) + " and " + quoted(b) + " are not a support pair (data/supports.json)";
}

bool readSupportCharacters(const LoadCtx& c, const Database& db, const Json& v, const std::string& where, const char* what,
                           std::string& a, std::string& b) {
  if (!v.isArray() || v.size() != 2 || !v[0].isString() || !v[1].isString())
    return c.fail(where, std::string("\"") + what + "\" must be two character ids, such as [\"eirik\", \"lyra\"]");
  a = v[0].asString();
  b = v[1].asString();
  for (const std::string& id : {a, b})
    if (db.findCharacter(id) < 0) return c.fail(where, std::string("\"") + what + "\" names unknown character " + quoted(id));
  return true;
}

namespace {

// The pair of two characters of a map's story: they must be a declared pair (maps are read after supports.json).
bool storyPair(const StoryLoader& L, const Json& v, const std::string& at, const char* what, int& pair) {
  std::string a, b;
  if (!readSupportCharacters(L.ctx(), L.db(), v, at, what, a, b)) return false;
  pair = L.db().findSupportPair(L.db().findCharacter(a), L.db().findCharacter(b));
  return pair >= 0 || L.fail(at, notASupportPair(a, b));
}

// A scene's pair is only judged once supports.json is loaded (resolveSupportRefs): here the characters must exist.
bool scenePair(const CampaignLoader& L, const Json& v, const std::string& at, const char* what, std::string& a, std::string& b) {
  return readSupportCharacters(L.ctx(), L.db(), v, at, what, a, b);
}

bool readOpValue(const LoadCtx& c, const Json& e, const std::string& at, CmpOp& op, int& value) {
  std::string text = ">=";
  if (!c.str(e, "op", at, text) || !c.integer(e, "value", at, value, true)) return false;
  if (value < 0 || value > kMaxSupportLevels)
    return c.fail(at, "\"value\" must be a support level from 0 to " + std::to_string(kMaxSupportLevels));
  return parseCmpOp(text, op) || c.fail(at, "\"op\" must be one of = != < <= > >=, not " + quoted(text));
}

bool readBy(const LoadCtx& c, const Json& e, const std::string& at, int& by) {
  if (!c.integer(e, "by", at, by, true)) return false;
  if (by < -kMaxSupportPoints || by > kMaxSupportPoints)
    return c.fail(at, "\"by\" must be from -" + std::to_string(kMaxSupportPoints) + " to " + std::to_string(kMaxSupportPoints));
  return true;
}

}  // namespace

// ---- map events ----

bool StoryLoader::supportCommand(const Json& e, const std::string& at, Command& out) {
  return onlyKeys(e, {"addSupport", "by"}, at) && storyPair(*this, e["addSupport"], at, "addSupport", out.index) &&
         readBy(c_, e, at, out.value);
}

bool StoryLoader::supportCondition(const Json& e, const std::string& at, Condition& out) const {
  if (!onlyKeys(e, {"support", "op", "value"}, at) || !storyPair(*this, e["support"], at, "support", out.index)) return false;
  out.op = CmpOp::Ge;
  return readOpValue(c_, e, at, out.op, out.value);
}

bool StoryLoader::supportTrigger(const Json& w, const std::string& at, Trigger& out) {
  if (!onlyKeys(w, {"support", "level"}, at) || !storyPair(*this, w["support"], at, "support", out.pair)) return false;
  const int levels = (int)db_.supports.pairs[(size_t)out.pair].levels.size();
  if (!w.has("level")) return true;
  if (!LoadCtx::isWhole(w["level"]) || w["level"].asInt() < 1 || w["level"].asInt() > levels)
    return fail(at, "\"level\" must be from 1 to " + std::to_string(levels) + " (the pair's levels)");
  out.level = w["level"].asInt();
  return true;
}

// ---- campaign scenes ----

bool CampaignLoader::supportCommand(const Json& e, const std::string& at, SceneCommand& out) {
  out.kind = SceneCommand::Kind::AddSupport;
  return onlyKeys(e, {"addSupport", "by"}, at) && scenePair(*this, e["addSupport"], at, "addSupport", out.unit, out.with) &&
         readBy(c_, e, at, out.value);
}

bool CampaignLoader::supportCondition(const Json& e, const std::string& at, SceneCondition& out) const {
  out.kind = SceneCondition::Kind::Support;
  if (!onlyKeys(e, {"support", "op", "value"}, at) || !scenePair(*this, e["support"], at, "support", out.unit, out.with))
    return false;
  out.op = CmpOp::Ge;
  return readOpValue(c_, e, at, out.op, out.value);
}

namespace {

// Walks a scene (and its if branches) looking for support commands and conditions whose characters are no pair.
bool checkScene(const LoadCtx& c, const Database& db, const std::vector<SceneCommand>& list, const std::string& where) {
  auto isPair = [&](const std::string& a, const std::string& b) {
    return db.findSupportPair(db.findCharacter(a), db.findCharacter(b)) >= 0;
  };
  for (size_t i = 0; i < list.size(); ++i) {
    const SceneCommand& cmd = list[i];
    const std::string at = where + ": command " + std::to_string(i + 1);
    if (cmd.kind == SceneCommand::Kind::AddSupport && !isPair(cmd.unit, cmd.with))
      return c.fail(at + " (addSupport)", notASupportPair(cmd.unit, cmd.with));
    if (cmd.kind != SceneCommand::Kind::If) continue;
    for (size_t k = 0; k < cmd.cond.size(); ++k)
      if (cmd.cond[k].kind == SceneCondition::Kind::Support && !isPair(cmd.cond[k].unit, cmd.cond[k].with))
        return c.fail(at + " (if): condition " + std::to_string(k + 1), notASupportPair(cmd.cond[k].unit, cmd.cond[k].with));
    if (!checkScene(c, db, cmd.thenDo, at + " (if): \"then\"") || !checkScene(c, db, cmd.elseDo, at + " (if): \"else\""))
      return false;
  }
  return true;
}

}  // namespace

// The support part of resolveSrpgRefs: campaign.json is read before supports.json, so its pairs are checked here.
bool resolveSupportRefs(const std::string& root, Database& db, std::string& err) {
  if (!db.campaign) return true;
  LoadCtx c(err, root + "/data/campaign.json");
  for (const ChapterDef& ch : db.campaign->chapters) {
    const std::string where = "chapter " + quoted(ch.id);
    if (!checkScene(c, db, ch.before, where + ": \"before\"") || !checkScene(c, db, ch.after, where + ": \"after\"")) return false;
  }
  return true;
}

}  // namespace ote
