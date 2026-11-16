// The data hash of a save (see save.h).
#include <cstdint>

#include "core/battle/save.h"
#include "core/battle/save_hash.h"

namespace ote {
namespace {

using savehash::Fnv;

// Mixes one file (name, then length, then bytes) so that moving text between
// two files, or swapping files, changes the hash.
void mixText(Fnv& f, const std::string& rel, bool have, const std::string& text) {
  f.bytes(rel);
  f.byte(0);
  f.byte(have ? 1 : 0);
  f.number((uint64_t)text.size());
  f.bytes(text);
}

bool mixFile(Fnv& f, const std::string& root, const std::string& rel, std::string& err) {
  std::string text;
  if (!readTextFile(root + "/" + rel, text)) {
    err = root + "/" + rel + ": cannot read the file to check the game's version";
    return false;
  }
  mixText(f, rel, true, text);
  return true;
}

// Keys that only change how things are called or drawn. A save stays valid
// when these change, so a maker can rename things, recolour them or pick
// another font without locking players out of their saves.
bool cosmeticKey(const std::string& k) {
  return k == "name" || k == "description" || k == "author" || k == "color" || k == "glyph" ||
         // What support levels are called (data/supports.json "levelNames"; docs/SRPG.md).
         k == "levelNames" ||
         // Sound (docs/SOUND.md) is presentation, like graphics.json: which sound or track plays never changes a rule.
         k == "sounds" || k == "music" || k == "playerTurnMusic" || k == "enemyTurnMusic";
}

// Wording of story events (docs/EVENTS.md): what is said and how things are labelled. Like names,
// it does not count; but the key stays (with empty text) so that adding or removing a line, or
// turning one into another kind of command, still does.
bool wordingKey(const std::string& k) {
  return k == "say" || k == "speakerName" || k == "label" || k == "text" || k == "playMusic" || k == "playSound";
}

// A copy without cosmetic keys, at any depth. Formula numbers ("props",
// "versus", stat tables...) are kept whole: a number called "name" is a rule.
// `mapFile`: also blank the wording of events and objectives (only maps have any).
Json rulesOnly(const Json& j, bool mapFile, bool numbersTable = false) {
  if (j.isArray()) {
    Json out = Json::makeArray();
    for (const Json& e : j.items()) out.push(rulesOnly(e, mapFile));
    return out;
  }
  if (j.isObject()) {
    // A campaign scene written as { "music": "id", "commands": [...] } (docs/SOUND.md) counts as the plain list of
    // its commands: adding music to a scene must not change the hash of the story.
    if (mapFile && j.size() <= 2 && j.has("commands") && j["commands"].isArray() && (j.size() == 1 || j.has("music")))
      return rulesOnly(j["commands"], mapFile);
    Json out = Json::makeObject();
    for (const Json::Member& m : j.members()) {
      if (!numbersTable && cosmeticKey(m.first)) continue;
      if (!numbersTable && mapFile && wordingKey(m.first) && m.second.isString()) {
        out.set(m.first, Json::makeString(""));
        continue;
      }
      bool table = m.first == "props" || m.first == "versus" || m.first == "stats" || m.first == "growths" || m.first == "caps" ||
                   m.first == "capAdjust" ||
                   m.first == "moveCost" || m.first == "income" || m.first == "cost" || m.first == "resources";
      out.set(m.first, rulesOnly(m.second, mapFile, table && m.second.isObject()));
    }
    return out;
  }
  return j;
}

// True for a strategy-RPG data file with nothing in it: an object whose `list` is empty or missing and whose other
// members are all empty objects or lists. A file that does not parse is not "nothing": it is mixed (and fails) as before.
bool holdsNothing(const std::string& text, const char* list) {
  Json doc;
  std::string perr;
  if (!Json::parse(text, doc, perr) || !doc.isObject()) return false;
  for (const Json::Member& m : doc.members()) {
    if (m.first == list) { if (!m.second.isArray() || m.second.size() > 0) return false; }
    else if (!(m.second.isObject() || m.second.isArray()) || m.second.size() > 0) return false;
  }
  return true;
}

// data/ranks.json with an empty (or missing) ladder is the same game as no file, whatever its "growth" says.
bool holdsNoRanks(const std::string& text) {
  Json doc;
  std::string perr;
  if (!Json::parse(text, doc, perr) || !doc.isObject()) return false;
  return !doc.has("ranks") || (doc["ranks"].isArray() && doc["ranks"].size() == 0);
}

// Mixes the rules content of a JSON file: layout, comments and cosmetic keys
// do not count. `only` (optional) limits the top-level keys that count.
bool mixJson(Fnv& f, const std::string& root, const std::string& rel, bool optional,
             const char* const* only, std::string& err, bool mapFile = false) {
  std::string text;
  if (!readTextFile(root + "/" + rel, text)) {
    if (!optional) {
      err = root + "/" + rel + ": cannot read the file to check the game's version";
      return false;
    }
    mixText(f, rel, false, "");
    return true;
  }
  Json doc;
  std::string perr;
  if (!Json::parse(text, doc, perr)) {
    err = root + "/" + rel + ": " + perr;
    return false;
  }
  Json rules = rulesOnly(doc, mapFile);
  if (only && rules.isObject()) {
    Json kept = Json::makeObject();
    for (const char* const* k = only; *k; ++k) {
      if (!rules.has(*k)) continue;
      // An explicit square grid is the same game as no "grid" at all, so it must not change the hash.
      if (std::string(*k) == "grid" && rules[*k].isObject() && rules[*k].size() == 1 &&
          rules[*k]["type"].asString() == "square")
        continue;
      kept.set(*k, rules[*k]);
    }
    rules = kept;
  }
  mixText(f, rel, true, rules.dump(0));
  return true;
}

}  // namespace

namespace savehash {

std::string Fnv::hex() const {
  static const char* const kHex = "0123456789abcdef";
  std::string out(16, '0');
  for (int i = 0; i < 16; ++i) out[(size_t)i] = kHex[(h >> (60 - 4 * i)) & 15];
  return out;
}

bool mixRules(Fnv& f, const Database& db, std::string& err) {
  const std::string& root = db.rootPath;
  // game.json: only the parts that are rules. Its name, description, author,
  // font, visual styles, screen size, the saving setting and editor notes are
  // not, and the map list is covered by hashing the map being played. Switches and variables
  // count when present (a game without them hashes as before). So does "grid" (hexagon games; docs/HEX.md): changing
  // a game's grid kind or layout refuses its old saves.
  static const char* const kGameKeys[] = {"formatVersion", "stats", "resources", "moveTypes", "roles",
                                          "rules", "formulas", "scripts", "switches", "variables", "grid", nullptr};
  if (!mixJson(f, root, "game.json", false, kGameKeys, err)) return false;
  static const char* const kData[] = {"data/weapons.json", "data/units.json", "data/terrain.json"};
  for (const char* rel : kData)
    if (!mixJson(f, root, rel, false, nullptr, err)) return false;
  if (!mixJson(f, root, "data/structures.json", true, nullptr, err)) return false;  // optional file
  // Items came later than the first saves: a game without the file must keep
  // the hash it always had, so nothing at all is mixed in for a missing file.
  std::string probe;
  if (readTextFile(root + "/data/items.json", probe) && !mixJson(f, root, "data/items.json", false, nullptr, err)) return false;
  // Research likewise: only a game that has the file mixes it in (its names and descriptions are cosmetic keys).
  if (readTextFile(root + "/data/research.json", probe) && !mixJson(f, root, "data/research.json", false, nullptr, err)) return false;
  // The strategy-RPG files (docs/SRPG.md), in this order and each only when the game has it: a game without them
  // keeps its hash. Names and descriptions are cosmetic keys; a support conversation's wording is blanked like a
  // map's (the map-file treatment), so rewriting it keeps saves valid.
  // A file that holds nothing (an empty list; supports with no pair and no setting) is the same game as no file: the
  // editor writes one to remember that the maker switched the feature on, and that must not make a save invalid.
  struct SrpgFile { const char* rel; const char* list; bool mapFile; };
  static const SrpgFile kSrpg[] = {{"data/characters.json", "characters", false}, {"data/skills.json", "skills", false},
                                   {"data/abilities.json", "abilities", false}, {"data/supports.json", "pairs", true}};
  for (const SrpgFile& sf : kSrpg) {
    if (!readTextFile(root + "/" + sf.rel, probe) || holdsNothing(probe, sf.list)) continue;
    if (!mixJson(f, root, sf.rel, false, nullptr, err, sf.mapFile)) return false;
  }
  // Weapon ranks and stat caps (docs/SRPG.md section 7): only a file with something in it counts (an empty ladder /
  // an empty "caps" is the same game as no file).
  if (readTextFile(root + "/data/ranks.json", probe) && !holdsNoRanks(probe) &&
      !mixJson(f, root, "data/ranks.json", false, nullptr, err))
    return false;
  if (readTextFile(root + "/data/caps.json", probe) && !holdsNothing(probe, "") &&
      !mixJson(f, root, "data/caps.json", false, nullptr, err))
    return false;
  for (const std::string& sc : db.scripts)
    if (!mixFile(f, root, sc, err)) return false;  // scripts count byte for byte
  return true;
}

bool mixMapFile(Fnv& f, const Database& db, const std::string& mapFile, std::string& err) {
  return mixJson(f, db.rootPath, mapFile, false, nullptr, err, true);
}

bool mixCampaignFile(Fnv& f, const Database& db, std::string& err) {
  std::string probe;
  if (!readTextFile(db.rootPath + "/data/campaign.json", probe)) return true;
  // Scenes have the same wording keys as events (say, speakerName), so the map-file treatment fits;
  // the campaign's and the chapters' "name" are cosmetic keys anyway.
  return mixJson(f, db.rootPath, "data/campaign.json", false, nullptr, err, true);
}

}  // namespace savehash

bool computeDataHash(const Database& db, const std::string& mapId, std::string& out, std::string& err) {
  int mi = db.findMap(mapId);
  if (mi < 0) {
    err = "unknown map '" + mapId + "'";
    return false;
  }
  Fnv f;
  if (!savehash::mixRules(f, db, err) || !savehash::mixMapFile(f, db, db.maps[(size_t)mi].file, err)) return false;
  // A game with a campaign: the file counts for single-battle saves too (a free battle of such a game
  // is judged by the same data). Games without one keep their exact hash.
  if (!savehash::mixCampaignFile(f, db, err)) return false;
  out = f.hex();
  return true;
}

}  // namespace ote
