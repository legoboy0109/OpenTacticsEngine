#include "scene_select.h"

namespace ote {

std::string lastMapKey(const std::string& gameId) { return "lastMap." + gameId; }

static std::vector<ListEntry> mapEntries(GameSession* s) {
  std::vector<ListEntry> out;
  for (const MapRef& m : s->db.maps) {
    ListEntry e;
    e.label = m.name.empty() ? m.id : m.name;
    e.detail = m.id;
    out.push_back(e);
  }
  return out;
}

MapSelectScene::MapSelectScene(SceneHost& host)
    : ListScene(host, host.session()->db.name, mapEntries(host.session()),
                "Select a map   Cancel: back"),
      session_(host.session()) {
  // Start the cursor where the player left off.
  std::string last = host.settings().state(lastMapKey(host.currentGameId()), "");
  int idx = last.empty() ? -1 : session_->db.findMap(last);
  if (idx >= 0) select(idx);
}

// Starting a new battle ends the Continue save's life (the first turn writes a new
// one), so ask first when there is one.
void MapSelectScene::onChoose(int index) {
  SaveStore store = host_.saveStore(host_.currentGameId());
  if (store.enabled() && session_->db.saving != SavingMode::Off && store.exists(0)) {
    pendingMap_ = index;
    confirm_.open("This replaces your Continue save.", "Start new battle", "Cancel");
    return;
  }
  startMap(index);
}

void MapSelectScene::onInput(const InputEvent& e) {
  if (!confirm_.isOpen()) { ListScene::onInput(e); return; }
  ConfirmBox::Result a = confirm_.onInput(e, host_.renderer());
  if (a == ConfirmBox::None) return;
  confirm_.close();
  if (a == ConfirmBox::Yes && pendingMap_ >= 0) startMap(pendingMap_);
  pendingMap_ = -1;
}

void MapSelectScene::draw(Renderer& r) {
  ListScene::draw(r);
  confirm_.draw(r);
}

void MapSelectScene::startMap(int index) {
  const std::string& id = session_->db.maps[index].id;
  host_.settings().setState(lastMapKey(host_.currentGameId()), id);
  host_.startBattle(id);
}

}  // namespace ote
