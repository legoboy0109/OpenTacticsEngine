// Campaign loader, part 2: scene commands and their conditions (docs/EVENTS.md "Campaign").
// Same JSON shapes as the event commands of the same name, minus everything that needs a map.
#include "core/data/loader_campaign.h"

namespace ote {

namespace {

constexpr const char* kSceneKeys[] = {"say", "setSwitch", "setVariable", "addVariable", "giveResource", "if", "goto",
                                       "playMusic", "stopMusic", "playSound", "takeItem"};

// The strategy-RPG scene commands and conditions (read in loader_supports_story.cpp). Listed apart so that the
// "is not a scene command" message keeps naming only the old ones.
constexpr const char* kSrpgSceneKeys[] = {"addSupport", "choice"};
constexpr const char* kSrpgConditionKeys[] = {"support"};

bool parseOp(const std::string& t, CmpOp& op) {
  if (t == "=" || t == "==") op = CmpOp::Eq;
  else if (t == "!=") op = CmpOp::Ne;
  else if (t == "<") op = CmpOp::Lt;
  else if (t == "<=") op = CmpOp::Le;
  else if (t == ">") op = CmpOp::Gt;
  else if (t == ">=") op = CmpOp::Ge;
  else return false;
  return true;
}

}  // namespace

bool CampaignLoader::condition(const Json& e, const std::string& where, SceneCondition& out) const {
  if (!e.isObject()) return fail(where, "a condition must be an object");
  static const char* const kKinds[] = {"switch", "variable", "alive", "dead"};
  const char* kind = nullptr;
  for (const char* k : kKinds)
    if (e.has(k)) {
      if (kind) return fail(where, std::string("has two conditions (") + kind + " and " + k + "); use one entry for each");
      kind = k;
    }
  for (const char* k : kSrpgConditionKeys)
    if (e.has(k)) {
      if (kind) return fail(where, std::string("has two conditions (") + kind + " and " + k + "); use one entry for each");
      kind = k;
    }
  if (!kind) return fail(where, "is not a condition: a scene condition is one of switch, variable, alive, dead");
  std::string k = kind;
  const Json& v = e[kind];
  if (k == "support") return supportCondition(e, where, out);
  if (k == "switch") {
    if (!onlyKeys(e, {"switch", "is"}, where)) return false;
    out.kind = SceneCondition::Kind::Switch;
    if (!v.isString() || (out.index = db_.findSwitch(v.asString())) < 0)
      return fail(where, "\"switch\" names unknown switch " + quoted(v.asString()) + " (game.json \"switches\")");
    return c_.boolean(e, "is", where, out.is);
  }
  if (k == "variable") {
    if (!onlyKeys(e, {"variable", "op", "value"}, where)) return false;
    out.kind = SceneCondition::Kind::Variable;
    if (!v.isString() || (out.index = db_.findVariable(v.asString())) < 0)
      return fail(where, "\"variable\" names unknown variable " + quoted(v.asString()) + " (game.json \"variables\")");
    std::string op = "=";
    if (!c_.str(e, "op", where, op) || !c_.integer(e, "value", where, out.value, true)) return false;
    if (!parseOp(op, out.op)) return fail(where, "\"op\" must be one of = != < <= > >=, not " + quoted(op));
    return true;
  }
  if (!onlyKeys(e, {kind}, where)) return false;
  out.kind = k == "alive" ? SceneCondition::Kind::Alive : SceneCondition::Kind::Dead;
  // The unit may join the roster in a later chapter, so only the shape of the id is checked.
  if (!v.isString() || !isStoryId(v.asString()))
    return fail(where, "\"" + k + "\" must be a roster unit id (letters, digits, \"_\" and \"-\")");
  out.unit = v.asString();
  return true;
}

bool CampaignLoader::command(const Json& e, const std::string& where, bool allowGoto, SceneCommand& out) {
  if (!e.isObject()) return fail(where, "a command must be an object such as { \"say\": \"Hello\" }");
  const char* kind = nullptr;
  for (const char* k : kSceneKeys)
    if (e.has(k)) {
      if (kind) return fail(where, std::string("has two commands (") + kind + " and " + k + "); use one entry for each");
      kind = k;
    }
  for (const char* k : kSrpgSceneKeys)
    if (e.has(k)) {
      if (kind) return fail(where, std::string("has two commands (") + kind + " and " + k + "); use one entry for each");
      kind = k;
    }
  if (!kind) {
    std::string list;
    for (const char* k : kSceneKeys) list += (list.empty() ? "" : ", ") + std::string(k);
    return fail(where, "is not a scene command; it needs one of " + list);
  }
  std::string k = kind;
  std::string at = where + " (" + k + ")";
  const Json& v = e[kind];
  using K = SceneCommand::Kind;

  if (k == "addSupport") return supportCommand(e, at, out);
  if (k == "choice") {
    out.kind = K::Choice;
    return choiceCommand(e, at, allowGoto, out);
  }
  if (k == "say") {
    out.kind = K::Say;
    if (!onlyKeys(e, {"say", "speaker", "speakerName", "portrait"}, at)) return false;
    if (!v.isString()) return fail(at, "\"say\" must be the text to show");
    out.text = text(v.asString());
    if (e.has("speaker") && e.has("speakerName")) return fail(at, "give \"speaker\" or \"speakerName\", not both");
    if (e.has("speaker")) {
      if (!e["speaker"].isString() || !isStoryId(e["speaker"].asString()))
        return fail(at, "\"speaker\" must be a roster unit id (letters, digits, \"_\" and \"-\")");
      out.unit = e["speaker"].asString();
    }
    std::string s;
    if (e.has("speakerName")) {
      if (!c_.str(e, "speakerName", at, s)) return false;
      out.speakerName = text(s);
    }
    if (e.has("portrait")) {
      if (!c_.str(e, "portrait", at, s)) return false;
      out.portrait = text(s);
    }
    return true;
  }
  if (k == "setSwitch" || k == "setVariable" || k == "addVariable") {
    bool isSwitch = k == "setSwitch";
    const char* extra = k == "addVariable" ? "by" : "to";
    if (!onlyKeys(e, isSwitch ? std::initializer_list<const char*>{kind, extra}
                              : std::initializer_list<const char*>{kind, extra, "level", "maxHp", "hp", "stat", "of"}, at))
      return false;
    out.kind = isSwitch ? K::SetSwitch : k == "setVariable" ? K::SetVariable : K::AddVariable;
    out.index = v.isString() ? (isSwitch ? db_.findSwitch(v.asString()) : db_.findVariable(v.asString())) : -1;
    if (out.index < 0)
      return fail(at, std::string("\"") + k + "\" names unknown " + (isSwitch ? "switch " : "variable ") + quoted(v.asString()) +
                          (isSwitch ? " (game.json \"switches\")" : " (game.json \"variables\")"));
    if (isSwitch) return c_.boolean(e, "to", at, out.on, true);
    bool reads = false;
    if (!readUnitReading(c_, db_, e, at, extra, false, out.reading, reads)) return false;
    if (reads) {
      // The character may join the roster in a later chapter, so only the shape of the id is checked (as alive / dead).
      if (!isStoryId(out.reading.unit))
        return fail(at, "the unit to read from must be a roster unit id (letters, digits, \"_\" and \"-\")");
      return true;
    }
    if (!c_.integer(e, extra, at, out.value, true)) return false;
    if (out.value < -kMaxStoryNumber || out.value > kMaxStoryNumber) return fail(at, "the number is unreasonably large");
    return true;
  }
  if (k == "takeItem") {
    out.kind = K::TakeItem;
    if (!onlyKeys(e, {"takeItem", "from", "all"}, at)) return false;
    if (!v.isString() || (out.item = db_.findItem(v.asString())) < 0)
      return fail(at, "\"takeItem\" names unknown item " + quoted(v.asString()) +
                          (db_.items.empty() ? " (this game has no items, data/items.json)" : ""));
    if (!e.has("from")) return fail(at, "missing required \"from\" (the roster unit who loses the item)");
    // The character may join the roster in a later chapter, so only the shape of the id is checked (as alive / dead).
    if (!e["from"].isString() || !isStoryId(e["from"].asString()))
      return fail(at, "\"from\" must be a roster unit id (letters, digits, \"_\" and \"-\")");
    out.unit = e["from"].asString();
    return c_.boolean(e, "all", at, out.on);
  }
  if (k == "giveResource") {
    out.kind = K::GiveResource;
    if (!onlyKeys(e, {"giveResource", "side"}, at) || !c_.amounts(db_, e, "giveResource", at, out.amounts) ||
        !c_.str(e, "side", at, out.side, true))
      return false;
    for (const Amount& a : out.amounts)
      if (a.amount < -kMaxStoryNumber || a.amount > kMaxStoryNumber) return fail(at, "an amount is unreasonably large");
    // No map is known here, so the side id is only checked for its shape; it names a side of the
    // chapter maps (the carried resources are kept per side id).
    if (!isStoryId(out.side)) return fail(at, "\"side\" must be a side id (letters, digits, \"_\" and \"-\")");
    return true;
  }
  if (k == "if") {
    out.kind = K::If;
    if (!onlyKeys(e, {"if", "then", "else"}, at)) return false;
    if (!v.isArray()) return fail(at, "\"if\" must be a list of conditions");
    for (size_t i = 0; i < v.size(); ++i) {
      SceneCondition cnd;
      if (!condition(v[i], at + ": condition " + std::to_string(i + 1), cnd)) return false;
      out.cond.push_back(cnd);
    }
    if (!e.has("then")) return fail(at, "missing required \"then\" (the commands to run)");
    return scene(e["then"], at + ": \"then\"", allowGoto, out.thenDo) &&
           (!e.has("else") || scene(e["else"], at + ": \"else\"", allowGoto, out.elseDo));
  }
  if (k == "playMusic" || k == "stopMusic" || k == "playSound") {
    // Same shapes and meaning as the event commands (loader_story_commands.cpp); the id is checked by the player.
    const bool sound = k == "playSound", stop = k == "stopMusic";
    out.kind = sound ? K::PlaySound : stop ? K::StopMusic : K::PlayMusic;
    if (!onlyKeys(e, sound ? std::initializer_list<const char*>{kind} : std::initializer_list<const char*>{kind, "fade"}, at))
      return false;
    if (stop) {
      if (!v.isBool() || !v.asBool()) return fail(at, "write { \"stopMusic\": true }");
    } else if (!v.isString() || !isStoryId(v.asString())) {
      return fail(at, "\"" + k + "\" must name " + (sound ? "a sound" : "a music track") +
                          " of data/audio.json (letters, digits, \"_\" and \"-\")");
    } else {
      out.text = text(v.asString());
    }
    double fade = -1;  // not given: the player cross-fades (0.5 s); 0 = a cut
    if (!sound && e.has("fade") && !c_.num(e, "fade", at, fade)) return false;
    if (e.has("fade") && (fade < 0 || fade > 60)) return fail(at, "\"fade\" is seconds, between 0 and 60");
    out.value = e.has("fade") ? (int)(fade * 1000 + 0.5) : -1;
    return true;
  }
  // goto
  out.kind = K::Goto;
  if (!onlyKeys(e, {"goto"}, at)) return false;
  if (!allowGoto) return fail(at, "\"goto\" only works in a chapter's \"after\" scene (it picks the next chapter)");
  if (!v.isString()) return fail(at, "\"goto\" must be a chapter id");
  out.chapter = v.asString();  // checked once all chapters are known
  return true;
}

bool CampaignLoader::chapterScene(const Json& v, const std::string& where, bool allowGoto, std::vector<SceneCommand>& out) {
  beginScene();
  if (!v.isObject()) return scene(v, where, allowGoto, out);
  if (!onlyKeys(v, {"music", "commands"}, where)) return false;
  if (v.has("music")) {
    if (!v["music"].isString() || !isStoryId(v["music"].asString()))
      return fail(where, "\"music\" must name a music track of data/audio.json (letters, digits, \"_\" and \"-\")");
    SceneCommand play;
    play.kind = SceneCommand::Kind::PlayMusic;
    play.text = text(v["music"].asString());
    play.value = -1;
    out.push_back(play);
  }
  return !v.has("commands") || scene(v["commands"], where + ": \"commands\"", allowGoto, out);
}

bool CampaignLoader::scene(const Json& list, const std::string& where, bool allowGoto, std::vector<SceneCommand>& out) {
  if (!list.isArray()) return fail(where, "must be a list of commands");
  for (size_t i = 0; i < list.size(); ++i) {
    SceneCommand cmd;
    if (!command(list[i], where + ": command " + std::to_string(i + 1), allowGoto, cmd)) return false;
    out.push_back(cmd);
  }
  return true;
}

}  // namespace ote
