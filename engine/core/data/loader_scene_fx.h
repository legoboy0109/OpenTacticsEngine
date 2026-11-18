// The scene presentation commands (docs/EVENTS.md "Scene presentation"), read by both the map-event loader and the campaign
// scene loader so the two accept exactly the same shapes: background, fade, wait, shake, leave, clearStage, weather, and the
// extra keys of say (position, auto), focus (ms, wait) and moveUnit (walk, wait).
#pragma once
#include <functional>
#include <string>

#include "core/data/database.h"
#include "core/data/loader_util.h"
#include "core/data/scene_fx.h"

namespace ote {

struct SceneFxReader {
  const LoadCtx& c;
  const Database& db;
  bool mapEvent;                                  // false = a campaign scene
  std::function<int(const std::string&)> text;    // adds a piece of wording to the owner's table
  // True for the keys read by command() below.
  static bool isKey(const std::string& key);
  // True when `key` (one of the scene command keys) is present in `e` as a command of its own. "fade" and "wait" are also
  // modifiers of other commands (playMusic / stopMusic "fade" seconds; focus / moveUnit "wait"), and then are no command.
  static bool isCommandIn(const Json& e, const char* key);
  // `key` is the command's key present in `e`; `at` names the command in messages.
  bool command(const Json& e, const std::string& key, const std::string& at, SceneFx& out) const;
  // say / narrate: "position" and "auto" (the Say keys themselves are read by the caller).
  bool sayExtras(const Json& e, const std::string& at, SceneFx& out) const;
  // focus: "ms" and "wait" (a map event only). moveUnit: "walk" and "wait".
  bool focusExtras(const Json& e, const std::string& at, SceneFx& out) const;
  bool moveExtras(const Json& e, const std::string& at, SceneFx& out) const;
  // enter / leave / clearStage: "transition" (cut or fade) and "ms".
  bool stageTransition(const Json& e, const std::string& at, SceneFx& out) const;
  bool onlyKeys(const Json& o, std::initializer_list<const char*> keys, const std::string& at) const;
  bool ms(const Json& o, const char* key, const std::string& at, int lo, int hi, int& out) const;
};

}  // namespace ote
