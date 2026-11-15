#include "scene_slots.h"

#include "core/base/log.h"
#include "core/data/loader.h"
#include "platform/platform.h"
#include "ui/chrome.h"

namespace ote {

std::vector<SaveInfo> inspectSlots(const SaveStore& store, const std::string& gamesDir, const std::string& gameId) {
  std::vector<SaveInfo> out;
  bool any = false;
  for (int s = 1; s <= SaveStore::kSlots; ++s) any |= store.exists(s);
  Database db;
  std::string err;
  bool haveDb = any && loadGame(gamesDir + "/" + gameId, db, err);
  if (any && !haveDb) OTE_WARN("cannot check saves against '%s': %s", gameId.c_str(), err.c_str());
  for (int s = 1; s <= SaveStore::kSlots; ++s) out.push_back(store.inspect(s, haveDb ? &db : nullptr, gameId));
  return out;
}

SlotScene::SlotScene(SceneHost& host, const GameEntry& game) : host_(host), game_(game) {
  SaveStore store = host.saveStore(game.id);
  // Loading starts a battle, and the next autosave replaces the Continue save.
  std::string warning = store.exists(0) ? "This replaces your Continue save." : "";
  // In a campaign game only a story save replaces the Continue file; a free battle's plain save does not.
  std::string probe;
  const bool campaignGame = readTextFile(host.args().gamesDir + "/" + game.id + "/data/campaign.json", probe);
  panel_.open(SlotPanel::Load, inspectSlots(store, host.args().gamesDir, game.id), warning, campaignGame);
}

void SlotScene::onInput(const InputEvent& e) {
  Renderer& r = host_.renderer();
  panel_.fitScreen(r);
  SlotPanel::Result res = panel_.onInput(e, r);
  if (res == SlotPanel::Back) {
    host_.showGame(game_.id);
  } else if (res == SlotPanel::Picked) {
    std::string err;
    if (!host_.resumeSave(panel_.slot(), err)) panel_.showMessage("Cannot load this save: " + err);
  }
}

void SlotScene::draw(Renderer& r) {
  r.clear(chrome::kBg);
  panel_.fitScreen(r);
  panel_.draw(r);
}

}  // namespace ote
