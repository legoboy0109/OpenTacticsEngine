// Event loader, part 2: the commands of an event's "do" list (docs/EVENTS.md "Commands").
#include <algorithm>

#include "core/data/loader_command_kind.h"
#include "core/data/loader_content.h"
#include "core/data/loader_difficulty.h"
#include "core/data/loader_events.h"
#include "core/data/loader_scene_fx.h"

namespace ote {

namespace {

std::string quoted(const std::string& s) { return "\"" + s + "\""; }

constexpr const char* kCommandKeys[] = {"say",       "addUnit",     "removeUnit", "moveUnit",   "changeSide", "giveItem",
                                        "giveResource", "setSwitch", "setVariable", "addVariable", "objective", "win",
                                        "lose",      "if",          "endTurn",    "focus",      "grantResearch", "playMusic", "stopMusic", "playSound", "takeItem"};
// The commands of the strategy-RPG features (docs/SRPG.md), read by their own areas (loader_promotion.cpp,
// loader_skills.cpp, loader_supports.cpp). A list of their own so that the "is not a command" message keeps naming
// only what every game can use until those areas exist.
constexpr const char* kSrpgCommandKeys[] = {"promote", "teachSkill", "forgetSkill", "addSupport", "choice"};
// Doors and chests (docs/SRPG.md section 10), read by loader_places_story.cpp.
constexpr const char* kPlaceCommandKeys[] = {"openDoor", "closeDoor", "lockDoor", "unlockDoor", "openChest"};
// Status conditions (docs/SRPG.md section 9), read in loader_story_conditions.cpp.
constexpr const char* kConditionCommandKeys[] = {"addCondition", "removeCondition"};
// The computer opponent's behaviour (docs/AI.md), read in loader_behaviour.cpp.
constexpr const char* kBehaviourCommandKeys[] = {"setBehaviour", "wakeGroup"};
// Changing the map and fog (docs/EVENTS.md "Changing the map"), read in loader_story_mapchange.cpp.
constexpr const char* kMapCommandKeys[] = {"setTerrain", "applyChange", "revertChange", "setFog", "setVision"};
// Scene presentation (docs/EVENTS.md "Scene presentation"), read by loader_scene_fx.cpp (and narrate, a say line).
constexpr const char* kSceneCommandKeys[] = {"background", "fade", "wait", "shake", "leave", "clearStage", "weather", "narrate", "enter", "move"};

SceneFxReader readerFor(StoryLoader& L) {
  return SceneFxReader{L.ctx(), L.db(), true, [&L](const std::string& s) { return L.text(s); }};
}

// background, fade, wait, shake, leave, clearStage, weather; narrate is a say line with no speaker.
bool sceneFxCommand(StoryLoader& L, const Json& e, const std::string& key, const std::string& at, Command& out) {
  SceneFxReader r = readerFor(L);
  if (key == "narrate") {
    out.kind = Command::Kind::Say;
    if (!r.onlyKeys(e, {"narrate", "auto"}, at)) return false;
    if (!e["narrate"].isString()) return L.fail(at, "\"narrate\" must be the text to show");
    out.text = L.text(e["narrate"].asString());
    out.fx.narrate = true;
    return r.sayExtras(e, at, out.fx);
  }
  out.kind = Command::Kind::Scene;
  return r.command(e, key, at, out.fx);
}
// Music by situation (docs/SOUND.md), read in loader_story_music.cpp.
constexpr const char* kMusicCommandKeys[] = {"setPhaseMusic"};
// Small extras (docs/EVENTS.md "Sending a unit's items to storage"), read in loader_story_extras.cpp.
constexpr const char* kExtrasCommandKeys[] = {"storeItems", "showUnit", "fight"};
// Another map shown by a scene (docs/EVENTS.md "Showing another map"), read in loader_story_show_map.cpp.
constexpr const char* kShowMapCommandKeys[] = {"showMap", "returnMap"};
// The commands that act on the battle itself (its sides, items, turns, objectives, the computer's units): refused while another map is shown.
constexpr const char* kNotOnShownMap[] = {"win", "lose", "endTurn", "choice", "objective", "giveItem", "takeItem", "storeItems", "giveResource",
                                          "grantResearch", "promote", "teachSkill", "forgetSkill", "addSupport", "setBehaviour", "wakeGroup",
                                          "setFog", "setVision", "showUnit"};

}  // namespace

bool StoryLoader::addUnitCommand(const Json& e, const std::string& where, Command& out) {
  const Json& u = e["addUnit"];
  if (!u.isObject()) return fail(where, "\"addUnit\" must be a unit: { \"type\": ..., \"side\": ..., \"x\": ..., \"y\": ... }");
  if (!onlyKeys(e, {"addUnit"}, where) ||
      !onlyKeys(u, {"id", "character", "type", "side", "x", "y", "name", "level", "xp", "merit", "leader", "stats", "items", "ranks", "goesFor", "skills", "behaviour", "difficulty", "difficultyShift", "job", "jobs", "equipped", "learned"}, where + ": \"addUnit\""))
    return false;
  std::string at = where;
  if (!readUnitHead(c_, db_, *map_, u, at, out.newUnit)) return false;
  if (!readUnitBody(c_, db_, u, at, out.newUnit)) return false;
  // A character already placed (map or an earlier addUnit) is not an error: the second add does nothing at run time.
  if (!checkBehaviourUnits(c_, db_, unitIds, at, out.newUnit.behaviour)) return false;  // docs/AI.md
  if (!out.newUnit.id.empty() && !u.has("character")) {
    for (const auto& used : usedIds) {
      if (used.first != out.newUnit.id) continue;
      const int d = sharedDifficulty(used.second, out.newUnit.onlyOn);
      if (d == kNoSharedDifficulty) continue;  // can never be in the same battle
      return fail(at, "\"id\" " + quoted(out.newUnit.id) + " is already used by another unit of the map" +
                          (d >= 0 ? " on difficulty " + quoted(db_.difficulties.list[(size_t)d].id) : std::string()));
    }
    usedIds.push_back({out.newUnit.id, out.newUnit.onlyOn});
  }
  return true;
}

bool StoryLoader::command(const Json& e, const std::string& where, bool talk, Command& out, bool& usesEndTurn) {
  if (!e.isObject()) return fail(where, "a command must be an object such as { \"say\": \"Hello\" }");
  // All the "two commands in one entry" checks run here, before ANY command is dispatched (pinned: the first error
  // for a bad entry must not depend on what the commands themselves would say).
  const char* kind = nullptr;
  // showMap's own "focus" (its camera start) is not the focus command.
  const auto named = [&e](const char* k) { return e.has(k) && !(e.has("showMap") && std::string(k) == "focus"); };
  if (!pickCommandKind(c_, where, kCommandKeys, named, kind)) return false;
  if (!pickCommandKind(c_, e, where, kSrpgCommandKeys, kind)) return false;
  if (!pickCommandKind(c_, e, where, kPlaceCommandKeys, kind)) return false;
  if (!pickCommandKind(c_, e, where, kConditionCommandKeys, kind)) return false;
  if (!pickCommandKind(c_, e, where, kBehaviourCommandKeys, kind)) return false;
  if (!pickCommandKind(c_, e, where, kMapCommandKeys, kind)) return false;
  if (!pickCommandKind(c_, e, where, kMusicCommandKeys, kind)) return false;
  if (!pickCommandKind(c_, e, where, kExtrasCommandKeys, kind)) return false;
  if (!pickCommandKind(c_, e, where, kShowMapCommandKeys, kind)) return false;
  const auto isSceneFx = [&e](const char* k) { return SceneFxReader::isCommandIn(e, k); };
  if (!pickCommandKind(c_, where, kSceneCommandKeys, isSceneFx, kind)) return false;
  if (shown_ && kind)  // docs/EVENTS.md "Showing another map": the shown map's units are temporary; the battle's own state waits for the return
    for (const char* k : kNotOnShownMap)
      if (std::string(k) == kind)
        return fail(where, std::string("\"") + kind + "\" acts on the battle itself and cannot run while another map is shown (showMap " +
                               quoted(shown_->id) + "): put it after the { \"returnMap\": true }");
  // A staged fight on a shown map is only a scene (its units are thrown away at the return), so beforePrep allows it there.
  if (beforePrep_ && kind && !(shown_ && std::string(kind) == "fight")) {  // docs/EVENTS.md "Before the preparation screen": no deployed party and no turns yet
    static const char* const kNotBefore[] = {"win", "lose", "endTurn", "choice", "fight"};
    for (const char* k : kNotBefore)
      if (std::string(k) == kind)
        return fail(where, std::string("\"") + kind + "\" makes no sense in a \"beforePrep\" event (it runs before the preparation screen: no "
                           "party is deployed, no turn has started, the battle cannot be decided, and the screen's choices change the map it "
                           "runs on again and again, so it must not ask or fight)");
  }
  if (kind && std::string(kind) == "showMap") return showMapCommand(e, where + " (showMap)", out);
  if (kind && std::string(kind) == "returnMap") return returnMapCommand(e, where + " (returnMap)", out);
  if (kind && std::string(kind) == "fight") return fightCommand(e, where + " (fight)", out);
  if (kind && std::string(kind) == "showUnit") return showUnitCommand(e, where + " (showUnit)", out);
  if (kind && std::string(kind) == "setPhaseMusic") {
    out.kind = Command::Kind::SetPhaseMusic;
    return onRealMap([&] { return phaseMusicCommand(e, where + " (setPhaseMusic)", out); });  // the battle's own sides
  }
  if (kind && std::string(kind) == "storeItems") {
    out.kind = Command::Kind::StoreItems;
    return storeItemsCommand(e, where + " (storeItems)", out);
  }
  for (const char* k : kMapCommandKeys)
    if (kind && std::string(kind) == k) return mapChangeCommand(e, kind, where + " (" + kind + ")", out);
  for (const char* k : kBehaviourCommandKeys)
    if (kind && std::string(kind) == k) return behaviourCommand(e, where + " (" + kind + ")", out);
  for (const char* k : kSceneCommandKeys)
    if (kind && std::string(kind) == k) return sceneFxCommand(*this, e, kind, where + " (" + kind + ")", out);
  for (const char* k : kPlaceCommandKeys)
    if (kind && std::string(kind) == k) return placeCommand(e, kind, where + " (" + kind + ")", out);
  if (!kind) {
    std::string list;
    for (const char* k : kCommandKeys) list += (list.empty() ? "" : ", ") + std::string(k);
    return fail(where, "is not a command; it needs one of " + list);
  }
  std::string k = kind;
  std::string at = where + " (" + k + ")";
  const Json& v = e[kind];
  using K = Command::Kind;

  if (k == "promote") {
    out.kind = K::Promote;
    return promoteCommand(e, at, out);
  }
  if (k == "teachSkill" || k == "forgetSkill") {
    out.kind = k == "teachSkill" ? K::TeachSkill : K::ForgetSkill;
    return skillCommand(e, at, k == "teachSkill", out);
  }
  if (k == "addCondition" || k == "removeCondition") {
    out.kind = k == "addCondition" ? K::AddCondition : K::RemoveCondition;
    return conditionCommand(e, at, k == "addCondition", out);
  }
  if (k == "addSupport") {
    out.kind = K::AddSupport;
    return supportCommand(e, at, out);
  }
  if (k == "choice") {
    out.kind = K::Choice;
    return choiceCommand(e, at, talk, out, usesEndTurn);
  }
  if (k == "say") {
    out.kind = K::Say;
    if (!onlyKeys(e, {"say", "speaker", "speakerName", "portrait", "position", "auto"}, at)) return false;
    if (!readerFor(*this).sayExtras(e, at, out.fx)) return false;
    if (!v.isString()) return fail(at, "\"say\" must be the text to show");
    out.text = text(v.asString());
    if (e.has("speaker") && e.has("speakerName")) return fail(at, "give \"speaker\" or \"speakerName\", not both");
    if (e.has("speaker") && !unitId(e["speaker"], at, "\"speaker\"", out.unit)) return false;
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
  if (k == "addUnit") {
    out.kind = K::AddUnit;
    return addUnitCommand(e, at, out);
  }
  if (k == "removeUnit") {
    out.kind = K::RemoveUnit;
    if (!onlyKeys(e, {"removeUnit", "hide"}, at) || !filter(v, at, "\"removeUnit\"", out.filter)) return false;
    if (!c_.boolean(e, "hide", at, out.hide)) return false;  // "hide": true keeps the unit in the story (docs/EVENTS.md)
    if (out.hide && shown_) return fail(at, "\"hide\" keeps a unit of the battle for later; the shown map's units are thrown away at the return");
    return true;
  }
  if (k == "moveUnit") {
    out.kind = K::MoveUnit;
    if (!onlyKeys(e, {"moveUnit", "x", "y", "walk", "wait"}, at) || !unitId(v, at, "\"moveUnit\"", out.unit)) return false;
    if (!readerFor(*this).moveExtras(e, at, out.fx)) return false;
    if (!c_.integer(e, "x", at, out.x, true) || !c_.integer(e, "y", at, out.y, true)) return false;
    if (out.x < 0 || out.y < 0 || out.x >= map_->width || out.y >= map_->height)
      return fail(at, "the square " + std::to_string(out.x) + "," + std::to_string(out.y) + " is outside the map");
    return true;
  }
  if (k == "changeSide") {
    out.kind = K::ChangeSide;
    std::string side;
    if (!onlyKeys(e, {"changeSide", "side", "ready"}, at) || !filter(v, at, "\"changeSide\"", out.filter) ||
        !c_.str(e, "side", at, side, true) || !c_.boolean(e, "ready", at, out.on))  // "ready" travels in `on`
      return false;
    return sideIndex(side, at, "\"side\"", out.side);
  }
  if (k == "giveItem") {
    out.kind = K::GiveItem;
    if (!onlyKeys(e, {"giveItem", "to"}, at)) return false;
    if (!v.isString() || (out.item = db_.findItem(v.asString())) < 0)
      return fail(at, "\"giveItem\" names unknown item " + quoted(v.asString()) +
                          (db_.items.empty() ? " (this game has no items, data/items.json)" : ""));
    if (!e.has("to")) return fail(at, "missing required \"to\" (who gets the item)");
    return filter(e["to"], at, "\"to\"", out.filter);
  }
  if (k == "giveResource") {
    out.kind = K::GiveResource;
    std::string side;
    if (!onlyKeys(e, {"giveResource", "side"}, at) || !c_.amounts(db_, e, "giveResource", at, out.amounts) ||
        !c_.str(e, "side", at, side, true))
      return false;
    for (const Amount& a : out.amounts)
      if (a.amount < -kMaxStoryNumber || a.amount > kMaxStoryNumber) return fail(at, "an amount is unreasonably large");
    return sideIndex(side, at, "\"side\"", out.side);
  }
  if (k == "takeItem") {
    out.kind = K::TakeItem;
    if (!onlyKeys(e, {"takeItem", "from", "all"}, at)) return false;
    if (!v.isString() || (out.item = db_.findItem(v.asString())) < 0)
      return fail(at, "\"takeItem\" names unknown item " + quoted(v.asString()) +
                          (db_.items.empty() ? " (this game has no items, data/items.json)" : ""));
    if (!e.has("from")) return fail(at, "missing required \"from\" (who loses the item)");
    return filter(e["from"], at, "\"from\"", out.filter) && c_.boolean(e, "all", at, out.on);
  }
  if (k == "grantResearch") {
    out.kind = K::GrantResearch;
    std::string side;
    if (!onlyKeys(e, {"grantResearch", "side"}, at)) return false;
    if (!v.isString() || (out.index = db_.findTechnology(v.asString())) < 0)
      return fail(at, "\"grantResearch\" names unknown technology " + quoted(v.asString()) +
                          (db_.research.empty() ? " (this game has no data/research.json)" : ""));
    return c_.str(e, "side", at, side, true) && sideIndex(side, at, "\"side\"", out.side);
  }
  if (k == "playMusic" || k == "stopMusic" || k == "playSound") {
    // Presentation only (docs/SOUND.md): the id is opaque here (it goes into the text table, so the save hash
    // ignores which track it is); the player checks it against data/audio.json.
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
  if (k == "setSwitch" || k == "setVariable" || k == "addVariable") {
    bool isSwitch = k == "setSwitch";
    const char* extra = k == "addVariable" ? "by" : "to";
    if (!onlyKeys(e, isSwitch ? std::initializer_list<const char*>{kind, extra}
                              : std::initializer_list<const char*>{kind, extra, "level", "hp", "maxHp", "stat", "of"}, at))
      return false;
    out.kind = isSwitch ? K::SetSwitch : k == "setVariable" ? K::SetVariable : K::AddVariable;
    out.index = v.isString() ? (isSwitch ? db_.findSwitch(v.asString()) : db_.findVariable(v.asString())) : -1;
    if (out.index < 0)
      return fail(at, std::string("\"") + k + "\" names unknown " + (isSwitch ? "switch " : "variable ") +
                          quoted(v.asString()) + (isSwitch ? " (game.json \"switches\")" : " (game.json \"variables\")"));
    if (isSwitch) return c_.boolean(e, "to", at, out.on, true);
    bool reads = false;
    if (!readUnitReading(c_, db_, e, at, extra, true, out.reading, reads)) return false;
    if (reads) {
      // The unit must be one the map names (like moveUnit); the number is read when the command runs. Always the battle's own map: a
      // variable is game state, and it is read on the real battle even while another map is shown (docs/EVENTS.md "Showing another map").
      const Json& idj = e[out.reading.kind == UnitReading::Kind::Stat ? "of" : out.reading.kind == UnitReading::Kind::Level ? "level" : out.reading.kind == UnitReading::Kind::Hp ? "hp" : "maxHp"];
      std::string unit;
      return onRealMap([&] { return unitId(idj, at, "the unit to read from", unit); });
    }
    if (!c_.integer(e, extra, at, out.value, true)) return false;
    if (out.value < -kMaxStoryNumber || out.value > kMaxStoryNumber) return fail(at, "the number is unreasonably large");
    return true;
  }
  if (k == "objective") {
    out.kind = K::Objective;
    if (!onlyKeys(e, {"objective"}, at)) return false;
    ObjectiveSet set;
    if (!objectiveSet(v, at, set)) return false;
    data_.objectives.push_back(set);
    out.objectives = (int)data_.objectives.size() - 1;
    return true;
  }
  if (k == "win" || k == "lose") {
    out.kind = k == "win" ? K::Win : K::Lose;
    if (!onlyKeys(e, {kind}, at)) return false;
    if (!v.isString()) return fail(at, std::string("\"") + k + "\" must name a team");
    return teamIndex(v.asString(), at, (std::string("\"") + k + "\"").c_str(), out.team);
  }
  if (k == "if") {
    out.kind = K::If;
    // The conditions are judged on the battle itself (its switches, variables and units), also while another map is shown.
    if (!onlyKeys(e, {"if", "then", "else"}, at) || !onRealMap([&] { return conditions(v, at, out.cond); })) return false;
    if (!e.has("then")) return fail(at, "missing required \"then\" (the commands to run)");
    return commands(e["then"], at + ": \"then\"", talk, out.thenDo, usesEndTurn) &&
           (!e.has("else") || commands(e["else"], at + ": \"else\"", talk, out.elseDo, usesEndTurn));
  }
  if (k == "endTurn") {
    out.kind = K::EndTurn;
    if (!onlyKeys(e, {"endTurn"}, at)) return false;
    if (!v.isBool() || !v.asBool()) return fail(at, "write { \"endTurn\": true }");
    if (!talk) return fail(at, "\"endTurn\" only works in an event whose trigger is \"talk\"");
    usesEndTurn = true;
    return true;
  }
  // focus
  out.kind = K::Focus;
  if (!onlyKeys(e, {"focus", "ms", "speed", "wait"}, at) || !readerFor(*this).focusExtras(e, at, out.fx)) return false;
  if (v.isString()) return unitId(v, at, "\"focus\"", out.unit);
  if (!v.isObject()) return fail(at, "\"focus\" must be a unit id or a square { \"x\": 3, \"y\": 4 }");
  if (!onlyKeys(v, {"x", "y"}, at + ": \"focus\"") || !c_.integer(v, "x", at, out.x, true) || !c_.integer(v, "y", at, out.y, true))
    return false;
  if (out.x < 0 || out.y < 0 || out.x >= map_->width || out.y >= map_->height)
    return fail(at, "the square " + std::to_string(out.x) + "," + std::to_string(out.y) + " is outside the map");
  return true;
}

bool StoryLoader::commands(const Json& list, const std::string& where, bool talk, std::vector<Command>& out,
                           bool& usesEndTurn) {
  if (!list.isArray()) return fail(where, "must be a list of commands");
  // depth_ 1 = the event's own list: the only one where a showMap section may start and end (loader_story_show_map.cpp).
  struct Depth {
    StoryLoader& L;
    const Json* list;
    explicit Depth(StoryLoader& l, const Json* own) : L(l), list(l.list_) { if (++L.depth_ == 1) L.list_ = own; }
    ~Depth() { if (--L.depth_ == 0) L.list_ = list; }
  } depth(*this, &list);
  for (size_t i = 0; i < list.size(); ++i) {
    Command cmd;
    if (depth_ == 1) at_ = i;
    if (!command(list[i], where + ": command " + std::to_string(i + 1), talk, cmd, usesEndTurn)) return false;
    out.push_back(cmd);
  }
  if (depth_ == 1) endShownMap();  // the event ended with the map still shown: the return is implied (the battle does the same)
  return true;
}

}  // namespace ote
