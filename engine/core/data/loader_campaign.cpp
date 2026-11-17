// data/campaign.json (docs/EVENTS.md "Campaign"): the campaign's rules, chapters and scenes.
// The scene commands are in loader_campaign_scene.cpp.
#include <memory>

#include "core/data/campaign.h"
#include "core/data/loader_campaign.h"

namespace ote {

bool CampaignLoader::onlyKeys(const Json& o, std::initializer_list<const char*> keys, const std::string& where) const {
  for (const Json::Member& m : o.members()) {
    bool known = false;
    for (const char* k : keys) known = known || m.first == k;
    if (known) continue;
    std::string list;
    for (const char* k : keys) list += (list.empty() ? "" : ", ") + quoted(k);
    return fail(where, "unknown key " + quoted(m.first) + " (expected " + list + ")");
  }
  return true;
}

namespace {

bool readRules(const CampaignLoader& L, const Json& root, CampaignDef& out) {
  if (!root.has("rules")) return true;
  const Json& r = root["rules"];
  if (!r.isObject()) return L.fail("", "\"rules\" must be an object such as { \"fallen\": \"gone\", \"onDefeat\": \"retry\" }");
  if (!L.onlyKeys(r, {"fallen", "onDefeat"}, "\"rules\"")) return false;
  std::string v;
  if (r.has("fallen")) {
    if (!L.ctx().str(r, "fallen", "\"rules\"", v)) return false;
    if (v != "gone" && v != "return") return L.fail("\"rules\"", "\"fallen\" must be \"gone\" or \"return\", not " + quoted(v));
    out.fallenReturn = v == "return";
  }
  if (r.has("onDefeat")) {
    if (!L.ctx().str(r, "onDefeat", "\"rules\"", v)) return false;
    if (v != "retry" && v != "end") return L.fail("\"rules\"", "\"onDefeat\" must be \"retry\" or \"end\", not " + quoted(v));
    out.endOnDefeat = v == "end";
  }
  return true;
}

bool readFreeBattles(const CampaignLoader& L, const Database& db, const Json& root, CampaignDef& out) {
  if (!root.has("freeBattles")) return true;
  const Json& f = root["freeBattles"];
  if (f.isString()) {
    if (f.asString() == "none") out.freeBattles = CampaignDef::FreeBattles::None;
    else if (f.asString() == "all") out.freeBattles = CampaignDef::FreeBattles::All;
    else return L.fail("\"freeBattles\"", "must be \"none\", \"all\" or a list of map ids, not " + quoted(f.asString()));
    return true;
  }
  if (!f.isArray()) return L.fail("", "\"freeBattles\" must be \"none\", \"all\" or a list of map ids");
  out.freeBattles = CampaignDef::FreeBattles::List;
  for (size_t i = 0; i < f.size(); ++i) {
    if (!f[i].isString()) return L.fail("\"freeBattles\"", "entry " + std::to_string(i + 1) + " must be a map id");
    if (db.findMap(f[i].asString()) < 0)
      return L.fail("\"freeBattles\"", "entry " + std::to_string(i + 1) + " names unknown map " + quoted(f[i].asString()));
    out.freeMaps.push_back(f[i].asString());
  }
  return true;
}

bool readChapter(CampaignLoader& L, const Database& db, const Json& e, size_t index, CampaignDef& out) {
  std::string where = "chapter " + std::to_string(index + 1);
  if (!e.isObject()) return L.fail(where, "must be an object");
  ChapterDef ch;
  if (!L.ctx().str(e, "id", where, ch.id, true)) return false;
  if (!isStoryId(ch.id)) return L.fail(where, "\"id\" must be letters, digits, \"_\" and \"-\" (got " + quoted(ch.id) + ")");
  for (const ChapterDef& o : out.chapters)
    if (o.id == ch.id) return L.fail("chapter " + quoted(ch.id), "duplicate chapter id");
  where = "chapter " + quoted(ch.id);
  if (!L.onlyKeys(e, {"id", "name", "map", "before", "after", "next"}, where)) return false;
  ch.name = ch.id;
  std::string map;
  if (!L.ctx().str(e, "name", where, ch.name) || !L.ctx().str(e, "map", where, map, true)) return false;
  ch.map = db.findMap(map);
  if (ch.map < 0) return L.fail(where, "\"map\" names unknown map " + quoted(map));
  if (!L.ctx().str(e, "next", where, ch.next)) return false;  // checked once every chapter is known
  if (e.has("before") && !L.chapterScene(e["before"], where + ": \"before\"", true, ch.before)) return false;
  if (e.has("after") && !L.chapterScene(e["after"], where + ": \"after\"", true, ch.after)) return false;
  out.chapters.push_back(ch);
  return true;
}

// next / goto name chapters that may come later in the file, so they are checked at the end.
// `self` = the chapter's id for a "before" scene (a goto there may not name its own chapter: its scene would run forever), "" for "after".
bool checkScene(const CampaignLoader& L, const CampaignDef& c, const std::vector<SceneCommand>& list, const std::string& where,
                const std::string& self = "") {
  for (const SceneCommand& cmd : list) {
    if (cmd.kind == SceneCommand::Kind::Goto && c.findChapter(cmd.chapter) < 0)
      return L.fail(where, "\"goto\" names unknown chapter " + quoted(cmd.chapter));
    if (cmd.kind == SceneCommand::Kind::Goto && !self.empty() && cmd.chapter == self)
      return L.fail(where, "\"goto\" names its own chapter " + quoted(cmd.chapter) + " (a \"before\" scene can only jump to another chapter)");
    if (cmd.kind == SceneCommand::Kind::If &&
        (!checkScene(L, c, cmd.thenDo, where, self) || !checkScene(L, c, cmd.elseDo, where, self)))
      return false;
    if (cmd.kind == SceneCommand::Kind::Choice)
      for (const SceneAnswer& a : cmd.answers)
        if (!checkScene(L, c, a.commands, where, self)) return false;
  }
  return true;
}

bool checkTargets(const CampaignLoader& L, const CampaignDef& c) {
  for (const ChapterDef& ch : c.chapters) {
    std::string where = "chapter " + quoted(ch.id);
    if (!ch.next.empty() && c.findChapter(ch.next) < 0) return L.fail(where, "\"next\" names unknown chapter " + quoted(ch.next));
    if (!checkScene(L, c, ch.before, where + ": \"before\"", ch.id) || !checkScene(L, c, ch.after, where + ": \"after\"")) return false;
  }
  return true;
}

}  // namespace

bool loadCampaign(const std::string& path, Database& db, std::string& err) {
  db.campaign.reset();
  if (!fileExists(path)) return true;
  Json root;
  if (!loadObjectFile(path, root, err)) return false;
  auto def = std::make_shared<CampaignDef>();
  CampaignLoader L(db, err, path, *def);
  if (!L.onlyKeys(root, {"name", "rules", "freeBattles", "chapters"}, "")) return false;
  def->name = "Campaign";
  if (!L.ctx().str(root, "name", "", def->name)) return false;
  if (!readRules(L, root, *def) || !readFreeBattles(L, db, root, *def)) return false;
  const Json* chapters;
  if (!L.ctx().array(root, "chapters", "", chapters, true)) return false;
  if (chapters->size() == 0) return L.fail("", "\"chapters\" must not be empty (a campaign needs at least one chapter)");
  for (size_t i = 0; i < chapters->size(); ++i)
    if (!readChapter(L, db, (*chapters)[i], i, *def)) return false;
  if (!checkTargets(L, *def)) return false;
  db.campaign = def;
  return true;
}

}  // namespace ote
