#include "sound/audio_data.h"
#include "core/data/prep.h"

#include <algorithm>
#include <initializer_list>

#include "core/base/json.h"
#include "core/data/campaign.h"
#include "core/data/events.h"
#include "core/data/loader.h"
#include "core/data/loader_util.h"

namespace ote {

int AudioData::findMusic(const std::string& id) const {
  for (size_t i = 0; i < music.size(); ++i)
    if (music[i].id == id) return (int)i;
  return -1;
}

int AudioData::findSound(const std::string& id) const {
  for (size_t i = 0; i < sounds.size(); ++i)
    if (sounds[i].id == id) return (int)i;
  return -1;
}

namespace {

std::string q(const std::string& s) { return "\"" + s + "\""; }

// An object may only have these keys: a typo must not pass silently (same as graphics.json).
bool onlyKeys(const LoadCtx& c, const Json& o, const std::string& where, std::initializer_list<const char*> keys) {
  for (const Json::Member& m : o.members()) {
    bool known = false;
    for (const char* k : keys) known = known || m.first == k;
    if (known) continue;
    std::string list;
    for (const char* k : keys) list += (list.empty() ? "" : ", ") + q(k);
    return c.fail(where, "unknown key " + q(m.first) + " (expected " + list + ")");
  }
  return true;
}

// Inside the game folder, forward slashes, .ogg or .wav.
bool goodAudioPath(const std::string& p) {
  if (p.size() < 5 || p[0] == '/' || p.find("..") != std::string::npos || p.find('\\') != std::string::npos ||
      p.find(':') != std::string::npos)
    return false;
  std::string low = p;
  for (char& ch : low)
    if (ch >= 'A' && ch <= 'Z') ch = (char)(ch - 'A' + 'a');
  auto ends = [&](const char* e) {
    std::string s(e);
    return low.size() > s.size() && low.compare(low.size() - s.size(), s.size(), s) == 0;
  };
  return ends(".ogg") || ends(".wav");
}

bool readFilePath(const LoadCtx& c, const std::string& where, const char* key, const Json& v, std::string& out) {
  if (!v.isString() || !goodAudioPath(v.asString()))
    return c.fail(where, q(key) + " must be a path inside the game folder to a .ogg or .wav file, such as \"audio/hit.wav\"");
  out = v.asString();
  return true;
}

bool readMusic(const LoadCtx& c, const Json& root, AudioData& out) {
  if (!root.has("music")) return true;
  if (!root["music"].isObject()) return c.fail("", "\"music\" must be an object of tracks by id");
  for (const Json::Member& m : root["music"].members()) {
    std::string where = "music " + q(m.first);
    if (!isStoryId(m.first)) return c.fail(where, "an id is letters, digits, \"_\" and \"-\" (at most 64)");
    const Json& e = m.second;
    if (!e.isObject()) return c.fail(where, "must be an object such as { \"file\": \"audio/title.ogg\" }");
    if (!onlyKeys(c, e, where, {"file", "loopStart", "volume", "loop"})) return false;
    MusicDef d;
    d.id = m.first;
    if (!e.has("file")) return c.fail(where, "missing required \"file\"");
    if (!readFilePath(c, where, "file", e["file"], d.file)) return false;
    double ls = 0;
    if (!c.num(e, "loopStart", where, ls)) return false;
    if (ls < 0 || ls > kMaxLoopStart) return c.fail(where, "\"loopStart\" is seconds, 0 or more");
    d.loopStart = (float)ls;
    if (!c.integer(e, "volume", where, d.volume)) return false;
    if (d.volume < 0 || d.volume > 100) return c.fail(where, "\"volume\" is a percent between 0 and 100");
    if (!c.boolean(e, "loop", where, d.loop)) return false;
    out.music.push_back(d);
  }
  return true;
}

bool readSounds(const LoadCtx& c, const Json& root, AudioData& out) {
  if (!root.has("sounds")) return true;
  if (!root["sounds"].isObject()) return c.fail("", "\"sounds\" must be an object of sound effects by id");
  for (const Json::Member& m : root["sounds"].members()) {
    std::string where = "sound " + q(m.first);
    if (!isStoryId(m.first)) return c.fail(where, "an id is letters, digits, \"_\" and \"-\" (at most 64)");
    const Json& e = m.second;
    if (!e.isObject()) return c.fail(where, "must be an object such as { \"file\": \"audio/hit.wav\" }");
    if (!onlyKeys(c, e, where, {"file", "files", "volume"})) return false;
    if (e.has("file") == e.has("files")) return c.fail(where, "needs exactly one of \"file\" or \"files\"");
    SoundDef d;
    d.id = m.first;
    if (e.has("file")) {
      std::string f;
      if (!readFilePath(c, where, "file", e["file"], f)) return false;
      d.files.push_back(f);
    } else {
      const Json& list = e["files"];
      if (!list.isArray() || list.size() < 1 || list.size() > (size_t)kMaxSoundVariants)
        return c.fail(where, "\"files\" must list between 1 and " + std::to_string(kMaxSoundVariants) + " paths");
      for (size_t i = 0; i < list.size(); ++i) {
        std::string f;
        if (!readFilePath(c, where, "files", list[i], f)) return false;
        d.files.push_back(f);
      }
    }
    if (!c.integer(e, "volume", where, d.volume)) return false;
    if (d.volume < 0 || d.volume > 100) return c.fail(where, "\"volume\" is a percent between 0 and 100");
    out.sounds.push_back(d);
  }
  return true;
}

// A music id of the `use` block: "" = none.
bool useMusic(const LoadCtx& c, const AudioData& d, const Json& use, const char* key, int& out) {
  out = -1;
  if (!use.has(key)) return true;
  const std::string where = std::string("use.") + key;
  if (!use[key].isString()) return c.fail(where, "must be a music id (or \"\" for none)");
  const std::string& id = use[key].asString();
  if (id.empty()) return true;
  out = d.findMusic(id);
  if (out < 0) return c.fail(where, "names unknown music " + q(id) + " (data/audio.json \"music\")");
  return true;
}

// "phaseMusic": { "<side id>": "<music id>" } and "fewEnemies": { "music": id, "count": n } of the `use` block.
bool readSituations(const LoadCtx& c, const Json& use, const std::string& where, AudioData& out) {
  if (use.has("phaseMusic")) {
    const Json& p = use["phaseMusic"];
    if (!p.isObject()) return c.fail(where, "\"phaseMusic\" must be an object of side id -> music id");
    for (const Json::Member& m : p.members()) {
      if (!isStoryId(m.first)) return c.fail(where + ".phaseMusic", q(m.first) + " is not a side id (letters, digits, \"_\" and \"-\")");
      if (!m.second.isString()) return c.fail(where + ".phaseMusic", q(m.first) + " must be a music id (or \"\" for none)");
      int t = -1;
      if (!m.second.asString().empty() && (t = out.findMusic(m.second.asString())) < 0)
        return c.fail(where + ".phaseMusic", q(m.first) + " names unknown music " + q(m.second.asString()) + " (data/audio.json \"music\")");
      out.phaseMusic.push_back(std::make_pair(m.first, t));
    }
  }
  if (use.has("fewEnemies")) {
    const Json& f = use["fewEnemies"];
    if (!f.isObject()) return c.fail(where, "\"fewEnemies\" must be { \"music\": \"id\", \"count\": 2 }");
    if (!onlyKeys(c, f, where + ".fewEnemies", {"music", "count"})) return false;
    if (!f.has("music") || !f["music"].isString() || f["music"].asString().empty())
      return c.fail(where + ".fewEnemies", "\"music\" must name a music track of data/audio.json");
    out.fewMusic = out.findMusic(f["music"].asString());
    if (out.fewMusic < 0) return c.fail(where + ".fewEnemies", "\"music\" names unknown music " + q(f["music"].asString()) + " (data/audio.json \"music\")");
    if (!f.has("count") || !LoadCtx::isWhole(f["count"]) || f["count"].asInt() < 1 || f["count"].asInt() > 999)
      return c.fail(where + ".fewEnemies", "\"count\" is a whole number of hostile units, from 1 to 999");
    out.fewCount = f["count"].asInt();
  }
  return true;
}

// "promotionSounds": [ { "atMs": 0, "sound": "id" }, ... ] (docs/SOUND.md "Promotion sounds"): sounds played at those times from the start of a promotion,
// next to the promotion jingle. At most 16; atMs 0..60000; kept in time order (the listed order for equal times).
bool readPromotionSounds(const LoadCtx& c, const Json& use, AudioData& out) {
  if (!use.has("promotionSounds")) return true;
  const Json& list = use["promotionSounds"];
  const std::string where = "use.promotionSounds";
  if (!list.isArray() || list.size() == 0 || list.size() > 16)
    return c.fail("use", "\"promotionSounds\" must be a list of 1 to 16 entries such as { \"atMs\": 0, \"sound\": \"id\" }");
  for (size_t i = 0; i < list.size(); ++i) {
    const Json& e = list[i];
    const std::string at = where + " entry " + std::to_string(i + 1);
    if (!e.isObject()) return c.fail("use", at + " must be { \"atMs\": 0, \"sound\": \"id\" }");
    if (!onlyKeys(c, e, at, {"atMs", "sound"})) return false;
    if (!e.has("atMs") || !LoadCtx::isWhole(e["atMs"]) || e["atMs"].asInt() < 0 || e["atMs"].asInt() > 60000)
      return c.fail(at, "\"atMs\" must be a whole number of milliseconds from 0 to 60000");
    if (!e.has("sound") || !e["sound"].isString() || e["sound"].asString().empty()) return c.fail(at, "\"sound\" must be the id of a sound of data/audio.json");
    const int s = out.findSound(e["sound"].asString());
    if (s < 0) return c.fail(at, "\"sound\" names unknown sound " + q(e["sound"].asString()) + " (data/audio.json \"sounds\")");
    out.promotionSounds.push_back(std::make_pair(e["atMs"].asInt(), s));
  }
  std::stable_sort(out.promotionSounds.begin(), out.promotionSounds.end(), [](const std::pair<int, int>& x, const std::pair<int, int>& y) { return x.first < y.first; });
  return true;
}

bool readUse(const LoadCtx& c, const Json& root, AudioData& out) {
  if (!root.has("use")) return true;
  const Json& use = root["use"];
  if (!use.isObject()) return c.fail("", "\"use\" must be an object (the game's standard set)");
  if (!onlyKeys(c, use, "use", {"titleMusic", "mapMusic", "playerTurnMusic", "enemyTurnMusic", "sceneMusic", "victory",
                                 "defeat", "levelUp", "sounds", "phaseMusic", "fewEnemies", "prepMusic", "promotion", "chapterCard", "shopMusic", "bootJingle", "promotionSounds"}))
    return false;
  if (!useMusic(c, out, use, "titleMusic", out.titleMusic) || !useMusic(c, out, use, "mapMusic", out.mapMusic) ||
      !useMusic(c, out, use, "playerTurnMusic", out.playerTurnMusic) ||
      !useMusic(c, out, use, "enemyTurnMusic", out.enemyTurnMusic) ||
      !useMusic(c, out, use, "sceneMusic", out.sceneMusic) || !useMusic(c, out, use, "victory", out.victory) ||
      !useMusic(c, out, use, "defeat", out.defeat) || !useMusic(c, out, use, "levelUp", out.levelUp) ||
      !useMusic(c, out, use, "prepMusic", out.prepMusic) || !useMusic(c, out, use, "promotion", out.promotion) ||
      !useMusic(c, out, use, "chapterCard", out.chapterCard) || !useMusic(c, out, use, "shopMusic", out.shopMusic) || !useMusic(c, out, use, "bootJingle", out.bootJingle) || !readPromotionSounds(c, use, out) || !readSituations(c, use, "use", out))
    return false;
  for (int jingle : {out.victory, out.defeat, out.levelUp, out.promotion, out.chapterCard, out.bootJingle})
    if (jingle >= 0 && out.music[(size_t)jingle].loop)
      return c.fail("use", "the jingle " + q(out.music[(size_t)jingle].id) + " must have \"loop\": false (victory, defeat, levelUp, promotion, chapterCard and bootJingle play once)");
  if (!use.has("sounds")) return true;
  if (!use["sounds"].isObject()) return c.fail("use", "\"sounds\" must be an object (event -> sound id)");
  for (const Json::Member& m : use["sounds"].members()) {
    Cue cue;
    if (!cueFromKey(m.first, cue)) {
      std::string list;
      for (int i = 0; i < kCueCount; ++i) list += (list.empty() ? "" : ", ") + q(cueKey((Cue)i));
      return c.fail("use.sounds", "unknown key " + q(m.first) + " (expected " + list + ")");
    }
    if (!m.second.isString()) return c.fail("use.sounds", q(m.first) + " must be a sound id (or \"\" for none)");
    if (m.second.asString().empty()) continue;
    int s = out.findSound(m.second.asString());
    if (s < 0) return c.fail("use.sounds", q(m.first) + " names unknown sound " + q(m.second.asString()) + " (data/audio.json \"sounds\")");
    out.standard.sound[(int)cue] = s;
  }
  return true;
}

// The optional "sounds" object of a weapon / unit / item entry.
bool readBlock(const LoadCtx& c, const AudioData& d, const Json& entry, const std::string& where, OverrideScope scope,
               CueSet& out, float* stepGap = nullptr) {
  if (!entry.has("sounds")) return true;
  const Json& block = entry["sounds"];
  if (!block.isObject()) return c.fail(where, "\"sounds\" must be an object (event -> sound id)");
  for (const Json::Member& m : block.members()) {
    if (m.first == "stepInterval" && stepGap) {  // seconds between two steps of a walk (docs/SOUND.md "Step sounds by movement type")
      if (!m.second.isNumber() || m.second.asNumber() < 0.02 || m.second.asNumber() > 2.0)
        return c.fail(where + ": \"sounds\"", "\"stepInterval\" is seconds between two steps, from 0.02 to 2");
      *stepGap = (float)m.second.asNumber();
      continue;
    }
    Cue cue;
    const bool sceneHeal = scope == OverrideScope::Item && m.first == "healScene";  // an item's heal sound in the battle scene only
    if (!sceneHeal && !overrideCue(scope, m.first, cue))
      return c.fail(where + ": \"sounds\"", "unknown key " + q(m.first) + " (expected " + overrideKeys(scope) + (stepGap ? ", \"stepInterval\"" : "") + ")");
    if (!m.second.isString()) return c.fail(where + ": \"sounds\"", q(m.first) + " must be a sound id (or \"\" for silence)");
    const std::string& id = m.second.asString();
    int& slot = sceneHeal ? out.healScene : out.sound[(int)cue];
    if (id.empty()) { slot = kSilent; continue; }
    int s = d.findSound(id);
    if (s < 0)
      return c.fail(where + ": \"sounds\"", q(m.first) + " names unknown sound " + q(id) +
                        (d.present ? " (data/audio.json \"sounds\")" : " (this game has no data/audio.json)"));
    slot = s;
  }
  return true;
}

// Reads <root>/<rel> when it mentions one of `words` (cheap test: most games never do), parses it.
bool readIfMentions(const std::string& path, std::initializer_list<const char*> words, Json& root, bool& have, std::string& err) {
  have = false;
  std::string text;
  if (!readTextFile(path, text)) return true;  // optional files (items.json) may be missing
  bool mentions = false;
  for (const char* w : words) mentions = mentions || text.find(w) != std::string::npos;
  if (!mentions) return true;
  if (!Json::parse(text, root, err)) {
    err = path + ": " + err;
    return false;
  }
  have = true;
  return true;
}

// One of data/weapons.json, units.json, items.json: "<key>": [ { "id": ..., "sounds": {...} } ].
template <typename Find>
bool readList(const Database& db, AudioData& d, const char* file, const char* key, const char* noun, OverrideScope scope,
              Find find, std::vector<CueSet>& out, std::string& err, std::vector<float>* gaps = nullptr) {
  const std::string path = db.rootPath + (std::string(file) == "game.json" ? "/" : "/data/") + file;
  Json root;
  bool have;
  if (!readIfMentions(path, {"\"sounds\""}, root, have, err)) return false;
  if (!have) return true;
  LoadCtx c(err, path);
  const Json& list = root[key];
  for (size_t i = 0; i < list.size(); ++i) {
    const Json& e = list[i];
    if (!e.isObject() || !e["id"].isString()) continue;  // the core loader already judged these
    int idx = find(e["id"].asString());
    if (idx < 0 || (size_t)idx >= out.size()) continue;
    if (!readBlock(c, d, e, std::string(noun) + " " + q(e["id"].asString()), scope, out[(size_t)idx], gaps ? &(*gaps)[(size_t)idx] : nullptr)) return false;
  }
  return true;
}

bool mapMusicKey(const LoadCtx& c, const AudioData& d, const Json& root, const char* key, const std::string& where, int& out) {
  if (!root.has(key)) return true;
  if (!root[key].isString()) return c.fail(where, q(key) + " must be a music id (or \"\")");
  const std::string& id = root[key].asString();
  if (id.empty()) { out = kSilent; return true; }
  int m = d.findMusic(id);
  if (m < 0)
    return c.fail(where, q(key) + " names unknown music " + q(id) +
                      (d.present ? " (data/audio.json \"music\")" : " (this game has no data/audio.json)"));
  out = m;
  return true;
}

// A map file's "phaseMusic" (side ids of this map) and "fewEnemies".
bool mapSituations(const LoadCtx& c, const AudioData& d, const Json& root, const std::string& where, MapMusic& m) {
  if (root.has("phaseMusic")) {
    const Json& p = root["phaseMusic"];
    if (!p.isObject()) return c.fail(where, "\"phaseMusic\" must be an object of side id -> music id");
    for (const Json::Member& e : p.members()) {
      bool side = false;
      if (root["sides"].isArray())
        for (size_t i = 0; i < root["sides"].size(); ++i) side = side || (root["sides"][i]["id"].isString() && root["sides"][i]["id"].asString() == e.first);
      if (!side) return c.fail(where, "\"phaseMusic\" names unknown side " + q(e.first) + " (this map's \"sides\")");
      if (!e.second.isString()) return c.fail(where, "\"phaseMusic\" " + q(e.first) + " must be a music id (or \"\")");
      int t = kSilent;
      if (!e.second.asString().empty() && (t = d.findMusic(e.second.asString())) < 0)
        return c.fail(where, "\"phaseMusic\" " + q(e.first) + " names unknown music " + q(e.second.asString()) +
                                 (d.present ? " (data/audio.json \"music\")" : " (this game has no data/audio.json)"));
      m.phase.push_back(std::make_pair(e.first, t));
    }
  }
  if (root.has("fewEnemies")) {
    const Json& f = root["fewEnemies"];
    if (!f.isObject()) return c.fail(where, "\"fewEnemies\" must be { \"music\": \"id\", \"count\": 2 }");
    if (!onlyKeys(c, f, where + ": \"fewEnemies\"", {"music", "count"})) return false;
    if (!f.has("music") || !f["music"].isString()) return c.fail(where, "\"fewEnemies\" needs a \"music\" id (or \"\" for none on this map)");
    if (f["music"].asString().empty()) m.few = kSilent;
    else if ((m.few = d.findMusic(f["music"].asString())) < 0)
      return c.fail(where, "\"fewEnemies\" names unknown music " + q(f["music"].asString()) + (d.present ? " (data/audio.json \"music\")" : " (this game has no data/audio.json)"));
    if (!f.has("count") || !LoadCtx::isWhole(f["count"]) || f["count"].asInt() < 1 || f["count"].asInt() > 999)
      return c.fail(where, "\"fewEnemies\" \"count\" is a whole number of hostile units, from 1 to 999");
    m.fewCount = f["count"].asInt();
  }
  return true;
}

}  // namespace

bool loadAudioFile(const std::string& dir, AudioData& out, std::string& err) {
  out = AudioData();
  const std::string path = dir + "/data/audio.json";
  std::string text;
  if (!readTextFile(path, text)) return true;  // no file: a silent game
  Json root;
  std::string perr;
  if (!Json::parse(text, root, perr)) {
    err = path + ": " + perr;
    return false;
  }
  LoadCtx c(err, path);
  if (!root.isObject()) return c.fail("", "must be an object with \"music\", \"sounds\" and \"use\"");
  if (!onlyKeys(c, root, "", {"music", "sounds", "use"})) return false;
  out.present = true;
  if (!readMusic(c, root, out) || !readSounds(c, root, out) || !readUse(c, root, out)) return false;
  return true;
}

bool loadAudioOverrides(const Database& db, AudioData& d, std::string& err) {
  d.weapons.assign(db.weapons.size(), CueSet());
  d.units.assign(db.unitTypes.size(), CueSet());
  d.items.assign(db.items.size(), CueSet());
  d.maps.assign(db.maps.size(), MapMusic());
  d.moveTypes.assign(db.moveTypes.size(), CueSet());
  d.unitStepGap.assign(db.unitTypes.size(), 0.0f);
  d.moveStepGap.assign(db.moveTypes.size(), 0.0f);
  d.hasOverrides = true;
  if (!readList(db, d, "weapons.json", "weapons", "weapon", OverrideScope::Weapon,
                [&](const std::string& id) { return db.findWeapon(id); }, d.weapons, err) ||
      !readList(db, d, "units.json", "units", "unit", OverrideScope::Unit,
                [&](const std::string& id) { return db.findUnitType(id); }, d.units, err, &d.unitStepGap) ||
      !readList(db, d, "game.json", "moveTypes", "move type", OverrideScope::MoveType,
                [&](const std::string& id) { for (size_t i = 0; i < db.moveTypes.size(); ++i) if (db.moveTypes[i].id == id) return (int)i; return -1; },
                d.moveTypes, err, &d.moveStepGap) ||
      !readList(db, d, "items.json", "items", "item", OverrideScope::Item,
                [&](const std::string& id) { return db.findItem(id); }, d.items, err))
    return false;
  for (size_t i = 0; i < db.maps.size(); ++i) {
    const std::string path = db.rootPath + "/" + db.maps[i].file;
    Json root;
    bool have;
    if (!readIfMentions(path, {"\"music\"", "\"playerTurnMusic\"", "\"enemyTurnMusic\"", "\"phaseMusic\"", "\"fewEnemies\"", "\"prepMusic\""}, root, have, err)) return false;
    if (!have || !root.isObject()) continue;
    LoadCtx c(err, path);
    const std::string where = "map " + q(db.maps[i].id);
    MapMusic& m = d.maps[i];
    if (!mapMusicKey(c, d, root, "music", where, m.music) || !mapMusicKey(c, d, root, "playerTurnMusic", where, m.playerTurn) ||
        !mapMusicKey(c, d, root, "enemyTurnMusic", where, m.enemyTurn) || !mapMusicKey(c, d, root, "prepMusic", where, m.prep) ||
        !mapSituations(c, d, root, where, m))
      return false;
  }
  return true;
}

namespace {

// Story commands may sit inside `if` branches, so the lists are walked recursively.
bool checkCommands(const AudioData& d, const std::vector<Command>& list, const std::vector<std::string>& texts,
                   const std::string& where, std::string& err) {
  for (const Command& c : list) {
    if (c.kind == Command::Kind::If) {
      if (!checkCommands(d, c.thenDo, texts, where, err) || !checkCommands(d, c.elseDo, texts, where, err)) return false;
      continue;
    }
    if (c.kind == Command::Kind::Choice) {  // an answer's commands may play sounds too
      for (const ChoiceAnswer& a : c.answers)
        if (!checkCommands(d, a.commands, texts, where, err)) return false;
      continue;
    }
    if (c.kind == Command::Kind::SetPhaseMusic) {  // "" and null (no text) name no track
      if (c.text < 0 || texts[(size_t)c.text].empty() || d.findMusic(texts[(size_t)c.text]) >= 0) continue;
      err = where + ": \"setPhaseMusic\" names unknown music " + q(texts[(size_t)c.text]) +
            (d.present ? " (data/audio.json \"music\")" : " (this game has no data/audio.json)");
      return false;
    }
    if (c.kind != Command::Kind::PlayMusic && c.kind != Command::Kind::PlaySound) continue;
    const std::string& id = texts[(size_t)c.text];
    const bool music = c.kind == Command::Kind::PlayMusic;
    if ((music ? d.findMusic(id) : d.findSound(id)) >= 0) continue;
    err = where + ": " + (music ? "\"playMusic\" names unknown music " : "\"playSound\" names unknown sound ") + q(id) +
          (d.present ? (music ? " (data/audio.json \"music\")" : " (data/audio.json \"sounds\")")
                     : " (this game has no data/audio.json)");
    return false;
  }
  return true;
}

bool checkScene(const AudioData& d, const std::vector<SceneCommand>& list, const std::vector<std::string>& texts,
                const std::string& where, std::string& err) {
  for (const SceneCommand& c : list) {
    if (c.kind == SceneCommand::Kind::If) {
      if (!checkScene(d, c.thenDo, texts, where, err) || !checkScene(d, c.elseDo, texts, where, err)) return false;
      continue;
    }
    if (c.kind == SceneCommand::Kind::Choice) {
      for (const SceneAnswer& a : c.answers)
        if (!checkScene(d, a.commands, texts, where, err)) return false;
      continue;
    }
    if (c.kind != SceneCommand::Kind::PlayMusic && c.kind != SceneCommand::Kind::PlaySound) continue;
    const std::string& id = texts[(size_t)c.text];
    const bool music = c.kind == SceneCommand::Kind::PlayMusic;
    if ((music ? d.findMusic(id) : d.findSound(id)) >= 0) continue;
    err = where + ": " + (music ? "music " : "sound ") + q(id) + " is not in " +
          (d.present ? "data/audio.json" : "this game (it has no data/audio.json)");
    return false;
  }
  return true;
}

}  // namespace

bool checkStoryAudio(const Database& db, const AudioData& d, std::string& err) {
  for (const MapRef& ref : db.maps) {
    // Only a map whose file mentions one of the commands can fail, so the others are not even parsed.
    std::string text;
    if (!readTextFile(db.rootPath + "/" + ref.file, text)) continue;
    const bool shops = text.find("\"shop") != std::string::npos;  // a preparation shop may name music
    if (text.find("\"playMusic\"") == std::string::npos && text.find("\"playSound\"") == std::string::npos &&
        text.find("\"setPhaseMusic\"") == std::string::npos && !shops)
      continue;
    MapDef map;
    std::string lerr;
    if (!loadMap(db, ref.id, map, lerr)) continue;  // the core reports a broken map when it is played
    if (map.prep)
      for (const PrepShop& s : map.prep->shops)
        if (!s.music.empty() && d.findMusic(s.music) < 0) {
          err = db.rootPath + "/" + ref.file + ": prep shop " + q(s.id.empty() ? std::string("shop") : s.id) + ": \"music\" names unknown music " + q(s.music) +
                (d.present ? " (data/audio.json \"music\")" : " (this game has no data/audio.json)");
          return false;
        }
    if (!map.story) continue;
    for (const StoryEvent& e : map.story->events)
      if (!checkCommands(d, e.commands, map.story->texts, db.rootPath + "/" + ref.file + ": event " + q(e.id), err)) return false;
  }
  // The sounds a battle scene animation plays at its frames (docs/BATTLE_SCENE.md "Sounds").
  for (const BattleSet& set : db.graphics.battle.sets)
    for (int a = 0; a < kBattleAnims; ++a)
      for (const auto& s : set.anims[a].sounds)
        if (d.findSound(s.second) < 0) {
          err = db.rootPath + "/data/graphics.json: battle set " + q(set.id) + " animation " + q(battleAnimKey((BattleAnimId)a)) + ": sound " +
                q(s.second) + " is not in " + (d.present ? "data/audio.json \"sounds\"" : "this game (it has no data/audio.json)");
          return false;
        }
  // The sounds of a battle scene effect (docs/BATTLE_SCENE.md "Effect sounds").
  for (size_t i = 0; i < db.graphics.battle.effects.size(); ++i) {
    const BattleEffect& fx = db.graphics.battle.effects[i];
    for (const auto& s : fx.sounds)
      if (d.findSound(s.second) < 0) {
        err = db.rootPath + "/data/graphics.json: battle effect " + std::to_string(i + 1) + ": sound " + q(s.second) + " is not in " +
              (d.present ? "data/audio.json \"sounds\"" : "this game (it has no data/audio.json)");
        return false;
      }
  }
  if (db.campaign) {
    for (const ChapterDef& ch : db.campaign->chapters) {
      const std::string where = db.rootPath + "/data/campaign.json: chapter " + q(ch.id);
      if (!checkScene(d, ch.before, db.campaign->texts, where + " \"before\"", err) ||
          !checkScene(d, ch.after, db.campaign->texts, where + " \"after\"", err))
        return false;
    }
  }
  return true;
}

}  // namespace ote
