// User settings shared by the player (and later the editors).
//
// Model (owner decision): options are GLOBAL and apply to every game. Any
// single option can be overridden for one game; everything not overridden
// keeps following the global value. An option that is not set anywhere is
// "unset" (callers then use the game's own default, then the engine's).
//
// Options are free-form key -> JSON value so new ones need no changes
// here. A separate "state" section remembers UI state (last game, cursor
// positions) that is not an option.
//
// File (JSON):
//   { "version": 1,
//     "global": { "font": "sans", "uiScale": 150 },
//     "games":  { "demo-srpg": { "font": "serif" } },
//     "state":  { "lastGame": "demo-srpg" } }
#pragma once
#include <string>

#include "core/base/json.h"

namespace ote {

class Settings {
 public:
  // Reads the file. A missing file is fine (empty settings, returns true).
  // A corrupt file returns false with err set; settings stay empty and the
  // bad file is preserved as "<path>.bad" by the next save().
  bool load(const std::string& path, std::string& err);
  // Writes to the path given to load() (via a temp file + rename, so a
  // crash cannot leave a half-written file). No-op success if no path.
  bool save(std::string& err);
  bool dirty() const { return dirty_; }

  // ---- options ----
  // Effective value for a game: its override, else the global value, else
  // Null. Pass an empty gameId for "no game" (global only).
  const Json& get(const std::string& gameId, const std::string& key) const;
  std::string getString(const std::string& gameId, const std::string& key,
                        const std::string& def) const;
  int getInt(const std::string& gameId, const std::string& key, int def) const;

  const Json& global(const std::string& key) const;
  bool hasOverride(const std::string& gameId, const std::string& key) const;
  // Passing a Null value removes the entry (back to "unset" / "follow global").
  void setGlobal(const std::string& key, const Json& value);
  void setOverride(const std::string& gameId, const std::string& key, const Json& value);
  void clearOverride(const std::string& gameId, const std::string& key);

  // ---- remembered UI state ----
  std::string state(const std::string& key, const std::string& def) const;
  void setState(const std::string& key, const std::string& value);

 private:
  std::string path_;
  Json global_ = Json::makeObject();
  Json games_ = Json::makeObject();
  Json state_ = Json::makeObject();
  bool dirty_ = false;
  bool corrupt_ = false;  // load() found an unparsable file
};

}  // namespace ote
