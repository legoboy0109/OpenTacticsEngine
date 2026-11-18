#include "core/data/game_info.h"

#include "core/base/json.h"

namespace ote {

bool loadGameInfo(const std::string& gameDir, GameInfo& out, std::string& err) {
  out = GameInfo();
  Json root;
  if (!loadJsonFile(gameDir + "/game.json", root, err)) return false;
  if (!root.isObject()) {
    err = gameDir + "/game.json: must be a JSON object";
    return false;
  }
  out.name = root["name"].asString();
  out.description = root["description"].asString();
  out.author = root["author"].asString();
  parseSavingMode(root["saving"].asString("any"), out.saving);
  out.battleScene = root["battleScene"].asString() == "scene";
  out.menuHide = root["menuUnavailable"].asString() == "hide";
  const Json& grid = root["grid"];
  if (grid["type"].asString() == "hex") {
    out.gridType = GridType::Hex;
    if (grid["layout"].asString() == "flat") out.hexLayout = HexLayout::Flat;
  }
  out.mapCount = (int)root["maps"].size();
  const Json& f = root["font"];
  out.font.name = f["name"].asString("pixel");
  out.font.file = f["file"].asString();
  out.font.size = f["size"].asInt(0);
  if (f["smooth"].isBool()) out.font.smooth = f["smooth"].asBool() ? 1 : 0;
  for (const Json& s : root["styles"].items()) {
    StyleInfo si;
    si.id = s["id"].asString();
    if (si.id.empty()) continue;
    si.name = s["name"].asString(si.id);
    std::string view = s["view"].asString();
    si.view = view == "isometric" ? StyleView::Isometric
              : view == "isometric-3d" ? StyleView::Isometric3D
              : view == "3d" ? StyleView::Full3D : StyleView::TopDown;
    out.styles.push_back(si);
  }
  if (out.styles.empty()) out.styles.push_back(StyleInfo{"default", "Default"});
  return true;
}

}  // namespace ote
