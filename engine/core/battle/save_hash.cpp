// The data hash of a save (see save.h).
#include <cstdint>

#include "core/battle/save.h"

namespace ote {
namespace {

struct Fnv {
  uint64_t h = 1469598103934665603ULL;
  void byte(uint8_t b) { h ^= b; h *= 1099511628211ULL; }
  void bytes(const std::string& s) { for (char c : s) byte((uint8_t)c); }
  void number(uint64_t v) { for (int i = 0; i < 8; ++i) byte((uint8_t)(v >> (8 * i))); }
};

// Mixes one file (name, then length, then bytes) so that moving text between
// two files, or swapping files, changes the hash.
bool mixFile(Fnv& f, const std::string& root, const std::string& rel, bool optional, std::string& err) {
  std::string text;
  bool have = readTextFile(root + "/" + rel, text);
  if (!have && !optional) {
    err = root + "/" + rel + ": cannot read the file to check the game's version";
    return false;
  }
  f.bytes(rel);
  f.byte(0);
  f.byte(have ? 1 : 0);
  f.number((uint64_t)text.size());
  f.bytes(text);
  return true;
}

}  // namespace

bool computeDataHash(const Database& db, const std::string& mapId, std::string& out, std::string& err) {
  int mi = db.findMap(mapId);
  if (mi < 0) {
    err = "unknown map '" + mapId + "'";
    return false;
  }
  Fnv f;
  const std::string& root = db.rootPath;
  if (!mixFile(f, root, "game.json", false, err)) return false;
  static const char* const kData[] = {"data/weapons.json", "data/units.json", "data/terrain.json"};
  for (const char* rel : kData)
    if (!mixFile(f, root, rel, false, err)) return false;
  if (!mixFile(f, root, "data/structures.json", true, err)) return false;  // optional file
  for (const std::string& s : db.scripts)
    if (!mixFile(f, root, s, false, err)) return false;
  if (!mixFile(f, root, db.maps[(size_t)mi].file, false, err)) return false;

  static const char* const kHex = "0123456789abcdef";
  out.assign(16, '0');
  for (int i = 0; i < 16; ++i) out[(size_t)i] = kHex[(f.h >> (60 - 4 * i)) & 15];
  return true;
}

}  // namespace ote
