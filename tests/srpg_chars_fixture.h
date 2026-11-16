// A writable copy of tests/fixtures/srpg-chars (docs/SRPG.md step C1): the characters tests change one file,
// load the copy with the real loader and read back the message. The source fixture is never written.
#pragma once
#include <string>

#include "core/data/loader.h"
#include "platform/platform.h"
#include "save_helpers.h"

namespace charstest {

using namespace ote;

struct Copy {
  std::string dir;

  explicit Copy(const std::string& name) {
    dir = savetest::tempFolder(name);
    makeDir(dir + "/data");
    makeDir(dir + "/maps");
    static const char* const files[] = {"game.json",          "data/terrain.json", "data/weapons.json",
                                        "data/items.json",    "data/units.json",   "data/characters.json",
                                        "maps/field.json",    "maps/plain.json"};
    for (const char* f : files) {
      std::string text;
      if (readTextFile(ote_test::sourceDir() + "/tests/fixtures/srpg-chars/" + f, text)) writeTextFile(dir + "/" + f, text);
    }
  }
  void put(const std::string& rel, const std::string& text) { writeTextFile(dir + "/" + rel, text); }
  void drop(const std::string& rel) { removeFile(dir + "/" + rel); }
  std::string read(const std::string& rel) const {
    std::string text;
    readTextFile(dir + "/" + rel, text);
    return text;
  }
  // `rel` with the first `from` replaced by `to`.
  void swap(const std::string& rel, const std::string& from, const std::string& to) {
    std::string text = read(rel);
    size_t at = text.find(from);
    if (at == std::string::npos) std::printf("  the test's text to replace is missing: %s\n", from.c_str());
    else put(rel, text.replace(at, from.size(), to));
  }
  // The game's load message ("" = it loaded).
  std::string gameError() const {
    Database db;
    std::string err;
    return loadGame(dir, db, err) ? "" : err;
  }
  // The map's load message ("" = it loaded).
  std::string mapError(const char* map) const {
    Database db;
    MapDef m;
    std::string err;
    if (!loadGame(dir, db, err)) return err;
    return loadMap(db, map, m, err) ? "" : err;
  }
};

inline bool has(const std::string& hay, const std::string& needle) { return hay.find(needle) != std::string::npos; }

// The message must contain `needle` (prints both when not).
inline bool saysIn(const std::string& err, const std::string& needle) {
  if (!has(err, needle)) std::printf("  got: %s\n  wanted: %s\n", err.c_str(), needle.c_str());
  return has(err, needle);
}

}  // namespace charstest
