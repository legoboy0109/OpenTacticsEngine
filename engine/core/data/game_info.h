// A cheap preview of a game folder for launchers: just the fields of
// game.json a game list or options screen needs, without loading terrain,
// units, maps or Lua. Full validation stays in loadGame().
#pragma once
#include <string>
#include <vector>

#include "core/data/database.h"

namespace ote {

struct StyleInfo {
  std::string id, name;
  StyleView view = StyleView::TopDown;  // unknown names read as TopDown (loadGame rejects them)
};

struct GameInfo {
  std::string name, description, author;
  int mapCount = 0;
  SavingMode saving = SavingMode::Any;  // unknown values read as Any (loadGame rejects them)
  GridType gridType = GridType::Square;     // game.json "grid" (unknown values read as square; loadGame rejects them)
  HexLayout hexLayout = HexLayout::Pointy;  // meaningful when gridType is Hex
  bool battleScene = false;       // game.json "battleScene": "scene" (docs/BATTLE_SCENE.md)
  bool menuHide = false;          // game.json "menuUnavailable": "hide" (docs/SRPG.md "Menu options")
  bool enemyRanges = true;        // game.json rules.display.enemyRanges (docs/SRPG.md "Enemy ranges"); false = the game switched it off
  FontConfig font;                // the game's own font choice
  std::vector<StyleInfo> styles;  // never empty; [0] is the default style
};

// Reads <gameDir>/game.json. False (with err naming the file and problem)
// if it is missing or not a JSON object; individual odd fields fall back to
// defaults rather than failing.
bool loadGameInfo(const std::string& gameDir, GameInfo& out, std::string& err);

}  // namespace ote
