// The game page's Difficulty row (docs/DIFFICULTY.md section 8): a campaign with a Continue save shows the difficulty its playthrough
// is on and, where the game's "change" rule allows, lets the player change it between battles. The change edits the Continue save
// (the playthrough's "difficulty"), so the next battle built uses it; a battle that is under way in that save keeps the difficulty
// it was built with. The rule and the wording are the battle menu's (difficulty_rows.h).
#include "campaign/campaign_logic.h"
#include "core/base/log.h"
#include "core/campaign/campaign_save.h"
#include "core/campaign/difficulty.h"
#include "core/data/loader.h"
#include "difficulty_rows.h"
#include "scene_game.h"

namespace ote {

// Called while the rows are built, with the game loaded and the Continue save judged usable.
void GameScene::addDifficultyRow(const Database& db, const SaveInfo& info) {
  const DifficultyRules& rules = db.difficulties;
  if (!rules.any()) return;
  const int now = rules.find(info.difficulty) >= 0 ? rules.find(info.difficulty) : rules.defaultIndex;
  const std::vector<int> reach = difficultiesReachable(rules, now);
  diffChoices_ = reach;
  Row r{kDifficulty, "Difficulty", rules.list[(size_t)now].name + difficultyBlockedNote(rules, now), !reach.empty(), ""};
  if (info.inBattle && !info.battleDifficulty.empty() && info.battleDifficulty != rules.list[(size_t)now].id)
    r.detail += " (next battle)";  // a battle in the save was built with another: the change shows from the next one
  if (!r.enabled) r.mapId = rules.change == DifficultyChange::Never ? "This game does not allow changing the difficulty."
                                                                     : "This is already the easiest difficulty.";
  OTE_INFO("game page: difficulty row \"%s\"%s", r.detail.c_str(), r.enabled ? "" : " greyed");
  diffRow_ = r;
}

void GameScene::openDifficultyMenu() {
  std::string dir = game_.dir, err;
  Database db;
  if (!loadGame(dir, db, err)) {
    host_.showError("Difficulty", err, SceneHost::ToGame);
    return;
  }
  std::vector<MenuItem> items;
  std::string log;
  for (size_t i = 0; i < diffChoices_.size(); ++i) {
    MenuItem it;
    it.label = db.difficulties.list[(size_t)diffChoices_[i]].name;
    it.id = (int)i + 1;
    items.push_back(it);
    log += (log.empty() ? "" : ", ") + db.difficulties.list[(size_t)diffChoices_[i]].id;
  }
  OTE_INFO("game page: difficulty menu offers %s", log.c_str());
  Renderer& r = host_.renderer();
  menu_.open(r, "Difficulty", items, r.width() / 2 - 40, r.height() / 2 - 20, r.width(), r.height());
  menu_.place(r.width() / 2 - menu_.w() / 2, r.height() / 2 - menu_.h() / 2, r.width(), r.height());
}

void GameScene::changeDifficulty(int listIndex) {
  const std::string dir = game_.dir;
  std::string err;
  Database db;
  if (!loadGame(dir, db, err) || listIndex < 0 || listIndex >= (int)db.difficulties.list.size()) {
    host_.showError("Difficulty", err.empty() ? "That difficulty is not in this game." : err, SceneHost::ToGame);
    return;
  }
  SaveStore store = host_.saveStore(game_.id);
  SaveFile file;
  Playthrough pt;
  if (!store.read(0, db, game_.id, file, err) || !restorePlaythrough(db, file.root["playthrough"], pt, err)) {
    host_.showError("Difficulty", err, SceneHost::ToGame);
    return;
  }
  const std::string id = db.difficulties.list[(size_t)listIndex].id;
  std::string why;
  if (!setPlaythroughDifficulty(db, pt, id, false, why)) {  // the same rule as the battle menu's
    host_.showError("Difficulty", why, SceneHost::ToGame);
    return;
  }
  // Only the one field changes: the rest of the save stays byte for byte as it was (a battle in it keeps its own difficulty).
  Json root = file.root;
  Json playthrough = root["playthrough"];
  playthrough.set("difficulty", Json::makeString(id));
  root.set("playthrough", playthrough);
  if (!store.write(0, root, err)) {
    host_.showError("Difficulty", err, SceneHost::ToGame);
    return;
  }
  OTE_INFO("campaign: difficulty changed to \"%s\" (applies from the next battle)", id.c_str());
  rebuild();
}

// The rows again (the Continue save changed), the cursor on the same kind of row.
void GameScene::rebuild() {
  const Kind was = rows_[(size_t)list_.sel].kind;
  rows_.clear();
  diffRow_ = Row{kHeading, "", "", false, ""};
  buildRows();
  list_.count = (int)rows_.size();
  list_.sel = 0;
  for (int i = 0; i < (int)rows_.size(); ++i)
    if (rows_[(size_t)i].kind == was) list_.sel = i;
}

}  // namespace ote
