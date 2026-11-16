// Loader of data/supports.json (docs/SRPG.md "Supports"): the rules, the pairs, the levels with their bonus and
// conversation. The story command / condition / trigger and the campaign scenes' versions are in
// loader_supports_story.cpp.
#include "core/data/events.h"
#include "core/data/loader_events.h"
#include "core/data/loader_srpg.h"
#include "core/data/loader_util.h"

namespace ote {

namespace {

std::string quoted(const std::string& s) { return "\"" + s + "\""; }

// An object may only have these keys; the message names the first stranger.
bool onlyKeys(const LoadCtx& c, const Json& o, std::initializer_list<const char*> keys, const std::string& where) {
  for (const Json::Member& m : o.members()) {
    bool known = false;
    for (const char* k : keys) known = known || m.first == k;
    if (!known) return c.fail(where, "unknown key " + quoted(m.first));
  }
  return true;
}

// A whole number from `lo` to `hi`, key optional (`out` untouched when absent).
bool number(const LoadCtx& c, const Json& o, const char* key, const std::string& where, int lo, int hi, int& out,
            bool required = false) {
  if (!o.has(key)) return !required || c.fail(where, std::string("missing required \"") + key + "\"");
  if (!LoadCtx::isWhole(o[key]) || o[key].asInt() < lo || o[key].asInt() > hi)
    return c.fail(where, quoted(key) + " must be a whole number from " + std::to_string(lo) + " to " + std::to_string(hi));
  out = o[key].asInt();
  return true;
}

bool readRules(const LoadCtx& c, const Json& root, SupportRules& r) {
  if (!root.has("rules")) return true;
  const Json& e = root["rules"];
  if (!e.isObject()) return c.fail("rules", "must be an object");
  if (!onlyKeys(c, e, {"levelNames", "points", "range", "maxLevels", "conversations"}, "rules")) return false;
  if (e.has("levelNames")) {
    if (!c.stringList(e, "levelNames", "rules", r.levelNames)) return false;
    if (r.levelNames.size() > (size_t)kMaxSupportLevels)
      return c.fail("rules", "\"levelNames\" has more than " + std::to_string(kMaxSupportLevels) + " names");
  }
  if (e.has("points")) {
    if (!e["points"].isObject()) return c.fail("rules", "\"points\" must be an object");
    const Json& p = e["points"];
    if (!onlyKeys(c, p, {"endTurnAdjacent", "fightTogether", "heal"}, "rules: \"points\"") ||
        !number(c, p, "endTurnAdjacent", "rules: \"points\"", 0, kMaxSupportPoints, r.endTurnAdjacent) ||
        !number(c, p, "fightTogether", "rules: \"points\"", 0, kMaxSupportPoints, r.fightTogether) ||
        !number(c, p, "heal", "rules: \"points\"", 0, kMaxSupportPoints, r.heal))
      return false;
  }
  if (!number(c, e, "range", "rules", 1, 10, r.range) || !number(c, e, "maxLevels", "rules", 1, 99, r.maxLevels)) return false;
  if (e.has("conversations")) {
    const std::string v = e["conversations"].isString() ? e["conversations"].asString() : "";
    if (v == "battle") r.conversations = SupportTalks::Battle;
    else if (v == "camp") r.conversations = SupportTalks::Camp;
    else if (v == "both") r.conversations = SupportTalks::Both;
    else return c.fail("rules", "\"conversations\" must be \"battle\", \"camp\" or \"both\"");
  }
  return true;
}

bool readBonus(const LoadCtx& c, const Database& db, const Json& e, const std::string& where, SupportBonus& b) {
  if (!e.isObject()) return c.fail(where, "must be an object");
  if (!onlyKeys(c, e, {"hit", "avoid", "crit", "critAvoid", "damage", "damageTaken", "stats"}, where)) return false;
  const int lim = kMaxSupportPoints;
  if (!number(c, e, "hit", where, -lim, lim, b.hit) || !number(c, e, "avoid", where, -lim, lim, b.avoid) ||
      !number(c, e, "crit", where, -lim, lim, b.crit) || !number(c, e, "critAvoid", where, -lim, lim, b.critAvoid) ||
      !number(c, e, "damage", where, -lim, lim, b.damage) || !number(c, e, "damageTaken", where, -lim, lim, b.damageTaken))
    return false;
  if (!e.has("stats")) return true;
  b.stats.assign(db.stats.size(), 0);
  if (!c.statMap(db, e, "stats", where, b.stats)) return false;
  for (const Json::Member& m : e["stats"].members()) {
    const int s = db.findStat(m.first);
    if (s == db.rules.statHp || s == db.rules.statMove)
      return c.fail(where, "\"stats\" cannot hold " + quoted(m.first) + " (the HP and move stats have no combat bonus)");
  }
  return true;
}

// "talk": a list of "say" commands, kept as indices into db.supports.texts.
bool readTalk(const LoadCtx& c, Database& db, const Json& list, const std::string& where, std::vector<SupportLine>& out) {
  if (!list.isArray()) return c.fail(where, "must be a list of say commands");
  if ((int)list.size() > kMaxSupportLines)
    return c.fail(where, "has more than " + std::to_string(kMaxSupportLines) + " lines");
  std::vector<std::string>& texts = db.supports.texts;
  auto text = [&](const std::string& s) {
    texts.push_back(s);
    return (int)texts.size() - 1;
  };
  for (size_t i = 0; i < list.size(); ++i) {
    const Json& e = list[i];
    const std::string n = std::to_string(i + 1);
    if (!e.isObject() || e.members().empty()) return c.fail(where, "command " + n + ": must be a command such as { \"say\": \"Hello\" }");
    if (!e.has("say"))
      return c.fail(where, "command " + n + " (" + e.members()[0].first + "): only \"say\" is allowed in a support conversation");
    const std::string at = where + ": command " + n + " (say)";
    if (!onlyKeys(c, e, {"say", "speaker", "speakerName", "portrait"}, at)) return false;
    SupportLine line;
    if (!e["say"].isString()) return c.fail(at, "\"say\" must be the text to show");
    line.text = text(e["say"].asString());
    if (e.has("speaker") && e.has("speakerName")) return c.fail(at, "give \"speaker\" or \"speakerName\", not both");
    std::string s;
    if (e.has("speaker")) {
      if (!c.str(e, "speaker", at, line.speaker)) return false;
      const int ch = db.findCharacter(line.speaker);
      if (ch < 0) return c.fail(at, "\"speaker\" names unknown character " + quoted(line.speaker));
      line.speakerName = text(db.characters[(size_t)ch].name);  // shown when the character is not on the board
    }
    if (e.has("speakerName")) {
      if (!c.str(e, "speakerName", at, s)) return false;
      line.speakerName = text(s);
    }
    if (e.has("portrait")) {
      if (!c.str(e, "portrait", at, s)) return false;
      line.portrait = text(s);
    }
    out.push_back(line);
  }
  return true;
}

bool readLevels(const LoadCtx& c, Database& db, const Json& list, const std::string& where, SupportPairDef& p) {
  if (!list.isArray() || list.size() < 1 || (int)list.size() > kMaxSupportLevels)
    return c.fail(where, "\"levels\" must have from 1 to " + std::to_string(kMaxSupportLevels) + " entries");
  for (size_t i = 0; i < list.size(); ++i) {
    const Json& e = list[i];
    const std::string at = where + ": level " + std::to_string(i + 1);
    if (!e.isObject()) return c.fail(at, "must be an object");
    if (!onlyKeys(c, e, {"points", "bonus", "talk"}, at)) return false;
    SupportLevelDef lv;
    if (!number(c, e, "points", at, 0, kMaxSupportPoints, lv.points, true)) return false;
    if (i > 0 && lv.points <= p.levels.back().points)
      return c.fail(at, "\"points\" must be more than level " + std::to_string(i) + "'s (" + std::to_string(p.levels.back().points) + ")");
    if (e.has("bonus") && !readBonus(c, db, e["bonus"], at + ": \"bonus\"", lv.bonus)) return false;
    if (e.has("talk") && !readTalk(c, db, e["talk"], at + ": \"talk\"", lv.talk)) return false;
    p.levels.push_back(lv);
  }
  return true;
}

bool readPair(const LoadCtx& c, Database& db, const Json& e, size_t index, SupportPairDef& p) {
  std::string where = "pair " + std::to_string(index + 1);
  if (!e.isObject()) return c.fail(where, "must be an object");
  if (!onlyKeys(c, e, {"a", "b", "start", "levels"}, where)) return false;
  std::string a, b;
  if (!c.str(e, "a", where, a, true) || !c.str(e, "b", where, b, true)) return false;
  for (const std::string& id : {a, b})
    if (db.findCharacter(id) < 0) return c.fail(where, "unknown character " + quoted(id));
  p.a = db.findCharacter(a);
  p.b = db.findCharacter(b);
  if (p.a == p.b) return c.fail(where, quoted(a) + " and " + quoted(b) + " are the same character");
  const int dup = db.findSupportPair(p.a, p.b);
  if (dup >= 0) return c.fail(where, quoted(a) + " and " + quoted(b) + " are already a pair (pair " + std::to_string(dup + 1) + ")");
  where += " (" + quoted(a) + ", " + quoted(b) + ")";
  if (!number(c, e, "start", where, 0, kMaxSupportPoints, p.start)) return false;
  if (!e.has("levels")) return c.fail(where, "\"levels\" must have from 1 to " + std::to_string(kMaxSupportLevels) + " entries");
  return readLevels(c, db, e["levels"], where, p);
}

}  // namespace

bool loadSupports(const std::string& path, Database& db, std::string& err) {
  db.supports = SupportData();
  if (!fileExists(path)) return true;  // a game without the file: nothing to load
  Json root;
  if (!loadObjectFile(path, root, err)) return false;
  LoadCtx c(err, path);
  // A file without pairs needs no characters: the editor writes one to remember that supports are switched on.
  if (db.characters.empty() && root["pairs"].isArray() && root["pairs"].size() > 0)
    return c.fail("", "the game has no characters (data/characters.json)");
  if (!onlyKeys(c, root, {"rules", "pairs"}, "") || !readRules(c, root, db.supports.rules)) return false;
  const Json* list;
  if (!c.array(root, "pairs", "", list, true)) return false;
  if ((int)list->size() > kMaxSupportPairs) return c.fail("", "too many \"pairs\" (at most " + std::to_string(kMaxSupportPairs) + ")");
  for (size_t i = 0; i < list->size(); ++i) {
    SupportPairDef p;
    if (!readPair(c, db, (*list)[i], i, p)) return false;
    db.supports.pairs.push_back(p);
  }
  const SupportRules& r = db.supports.rules;
  for (size_t i = 0; i < db.supports.pairs.size(); ++i)
    if (!r.levelNames.empty() && r.levelNames.size() < db.supports.pairs[i].levels.size())
      return c.fail("rules", "\"levelNames\" has " + std::to_string(r.levelNames.size()) + " names but pair " +
                                 std::to_string(i + 1) + " has " + std::to_string(db.supports.pairs[i].levels.size()) + " levels");
  return true;
}

}  // namespace ote
