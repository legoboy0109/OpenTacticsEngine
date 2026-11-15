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
      bool table = m.first == "props" || m.first == "versus" || m.first == "stats" || m.first == "growths" ||
                   m.first == "moveCost" || m.first == "income" || m.first == "cost" || m.first == "resources";
      out.set(m.first, rulesOnly(m.second, mapFile, table && m.second.isObject()));
    }
    return out;
  }
  return j;
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
    for (const char* const* k = only; *k; ++k)
      if (rules.has(*k)) kept.set(*k, rules[*k]);
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
  // count when present (a game without them hashes as before).
  static const char* const kGameKeys[] = {"formatVersion", "stats", "resources", "moveTypes", "roles",
                                          "rules", "formulas", "scripts", "switches", "variables", nullptr};
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
