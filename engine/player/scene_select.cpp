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

void MapSelectScene::onChoose(int index) {
  const std::string& id = session_->db.maps[index].id;
  host_.settings().setState(lastMapKey(host_.currentGameId()), id);
  host_.startBattle(id);
}

}  // namespace ote
