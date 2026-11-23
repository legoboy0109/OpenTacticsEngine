#include "scene_select.h"

namespace ote {

std::string lastMapKey(const std::string& gameId) { return "lastMap." + gameId; }

// The playable maps (a cut-scene map is only shown by story scenes: docs/EVENTS.md "Showing another map"), as indices into db.maps.
static std::vector<int> playableMaps(const GameSession* s) {
  std::vector<int> out;
  for (size_t i = 0; i < s->db.maps.size(); ++i)
    if (!s->db.maps[i].cutScene) out.push_back((int)i);
  return out;
}

static std::vector<ListEntry> mapEntries(GameSession* s) {
  std::vector<ListEntry> out;
  for (int i : playableMaps(s)) {
    const MapRef& m = s->db.maps[(size_t)i];
    ListEntry e;
    e.label = m.name.empty() ? m.id : m.name;
    e.detail = m.id;
    out.push_back(e);
  }
  return out;
}

MapSelectScene::MapSelectScene(SceneHost& host)
    : ListScene(host, host.session()->db.name, mapEntries(host.session()),
                "Select a map   {cancelWord}: back"),
      session_(host.session()), maps_(playableMaps(host.session())) {
  // Start the cursor where the player left off.
  std::string last = host.settings().state(lastMapKey(host.currentGameId()), "");
  const int at = last.empty() ? -1 : session_->db.findMap(last);
  for (size_t row = 0; row < maps_.size(); ++row)
    if (maps_[row] == at) select((int)row);
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
  if (index < 0 || index >= (int)maps_.size()) return;
  const std::string& id = session_->db.maps[(size_t)maps_[(size_t)index]].id;
  host_.settings().setState(lastMapKey(host_.currentGameId()), id);
  host_.startBattle(id);
}

}  // namespace ote
