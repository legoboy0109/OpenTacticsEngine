// The table of user options. The options screens are generated from it, so
// a new option is one row in optionDefs() (option_defs.cpp) plus, if it
// changes the look, one field in LookLayer and its use in resolveLook().
// See docs/ARCHITECTURE.md "Launcher and options".
#pragma once
#include <string>
#include <vector>

#include "core/base/json.h"
#include "core/data/settings.h"
#include "game_library.h"
#include "options/effective.h"

namespace ote {

// One selectable value. A Null `value` means "unset": the option is left to
// the next level (global screen: the game's own default; per-game screen:
// the global option).
struct OptionChoice {
  std::string label;
  Json value;
};

// What a choices() function may need to know.
struct OptionContext {
  std::string assetsDir;
  const std::vector<GameEntry>* games = nullptr;  // every installed game
  const GameEntry* game = nullptr;                // set on a per-game screen
};

struct OptionDef {
  const char* key;    // key in settings.json ("global" and per-game sections)
  const char* label;  // shown on the options screen
  // Selectable values; element 0 is the "unset" choice (Null value).
  std::vector<OptionChoice> (*choices)(const OptionContext&);
  // Copies a stored value into the layer used by resolveLook().
  void (*apply)(const Json& value, LookLayer& layer);
};

const std::vector<OptionDef>& optionDefs();

// The settings of one level as a layer: global options (gameId empty) or
// only that game's overrides.
LookLayer globalLayer(const Settings& s);
LookLayer overrideLayer(const Settings& s, const std::string& gameId);

// Same value (type and content)?
bool sameValue(const Json& a, const Json& b);

}  // namespace ote
