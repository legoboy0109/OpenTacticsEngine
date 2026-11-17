// The battle menu's Difficulty row (docs/DIFFICULTY.md): shows the difficulty and, in a campaign whose rule allows it, lets the
// player change it. A change goes into the playthrough and applies from the NEXT battle built (a retry, the next chapter):
// the battle in progress keeps the difficulty it was built with. A game without difficulties has no row.
#include "battle/battle_scene.h"
#include "core/base/log.h"
#include "core/campaign/difficulty.h"
#include "ui/text.h"

namespace ote {

namespace {
// The difficulties the player may move to from `now` under the game's rule, in list order.
std::vector<int> reachable(const DifficultyRules& rules, int now) {
  std::vector<int> out;
  for (size_t i = 0; i < rules.list.size(); ++i)
    if (rules.mayChange(now, (int)i)) out.push_back((int)i);
  return out;
}
}  // namespace

// The setting to show: a campaign's playthrough (it may be ahead of the battle after a change), else the battle's own.
int BattleScene::difficultyNow() const {
  const Database& db = session_.db;
  if (!db.difficulties.any()) return -1;
  return session_.campaign ? playthroughDifficulty(db, session_.campaign->pt) : battle_.difficulty;
}

// Greyed (with the reason in the right-hand column) when it cannot be changed: a free battle ("this battle"), a game whose
// rule is "never" ("fixed"), or "lowerOnly" when already on the easiest ("lowest").
void BattleScene::addDifficultyEntry(std::vector<MenuItem>& items) {
  const Database& db = session_.db;
  const int now = difficultyNow();
  if (now < 0) return;
  MenuItem it;
  it.label = "Difficulty";
  it.id = kDifficultyEntry;
  it.extra = db.difficulties.list[(size_t)now].name;
  if (!session_.campaign) {
    it.enabled = false;
    it.extra += " (this battle)";
  } else if (db.difficulties.change == DifficultyChange::Never) {
    it.enabled = false;
    it.extra += " (fixed)";
  } else if (reachable(db.difficulties, now).empty()) {
    it.enabled = false;
    it.extra += db.difficulties.change == DifficultyChange::LowerOnly ? " (lowest)" : "";
  } else if (now != battle_.difficulty) {
    it.extra += " (next battle)";
  }
  OTE_INFO("battle menu: difficulty row \"%s\"%s", it.extra.c_str(), it.enabled ? "" : " greyed");
  items.push_back(it);
}

void BattleScene::openDifficultyMenu() {
  const Database& db = session_.db;
  const int now = difficultyNow();
  std::vector<MenuItem> items;
  std::string log;
  for (int i : reachable(db.difficulties, now)) {
    MenuItem it;
    it.label = db.difficulties.list[(size_t)i].name;
    it.id = i + 1;  // 0 is "Cancel" in the other menus
    items.push_back(it);
    log += (log.empty() ? "" : ", ") + db.difficulties.list[(size_t)i].id;
  }
  if (items.empty()) {
    openEndMenu();
    return;
  }
  OTE_INFO("difficulty menu: now %s, can change to %s", db.difficulties.list[(size_t)now].id.c_str(), log.c_str());
  mode_ = Mode::DifficultyMenu;
  menu_.open(host_.renderer(), "Difficulty", items, layout_.vw / 2 - 40, layout_.vh / 2 - 20, layout_.vw, layout_.vh);
  menu_.place(layout_.vw / 2 - menu_.w() / 2, layout_.vh / 2 - menu_.h() / 2, layout_.vw, layout_.vh);
}

// A name from the submenu was chosen (`id` = list index + 1).
void BattleScene::difficultyChosen(int id) {
  const Database& db = session_.db;
  if (!session_.campaign || id < 1 || id > (int)db.difficulties.list.size()) return;
  const DifficultyDef& to = db.difficulties.list[(size_t)(id - 1)];
  std::string why;
  if (!setPlaythroughDifficulty(db, session_.campaign->pt, to.id, false, why)) {
    say(why);
    return;
  }
  OTE_INFO("campaign: difficulty changed to \"%s\" (applies from the next battle)", to.id.c_str());
  dirty_ = true;  // the Continue save carries the playthrough's difficulty
  say("Difficulty: " + to.name + " - from the next battle");
}

}  // namespace ote
