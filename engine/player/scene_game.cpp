#include "scene_game.h"

#include <algorithm>

#include "sound/cue_sink.h"
#include "campaign/campaign_logic.h"
#include "core/base/log.h"
#include "core/campaign/playthrough.h"
#include "core/data/loader.h"
#include "platform/platform.h"
#include "ui/chrome.h"
#include "ui/text.h"

namespace ote {

namespace {
const Color kGreyed(112, 114, 130), kReason(225, 150, 130);
}

GameScene::GameScene(SceneHost& host, const GameEntry& game) : host_(host), game_(game) {
  buildRows();
  list_.count = (int)rows_.size();
  // Rest the cursor on Continue when there is one, else on Play / New game.
  for (int i = 0; i < (int)rows_.size(); ++i)
    if (rows_[i].kind == kContinue || (rows_[0].kind != kContinue && (rows_[i].kind == kPlay || rows_[i].kind == kNewGame))) {
      list_.sel = i;
      break;
    }
}

void GameScene::buildRows() {
  SavingMode mode = game_.info.saving;
  SaveStore store = host_.saveStore(game_.id);
  bool saving = store.enabled() && mode != SavingMode::Off;
  twoLines_ = saving;  // rows get a second line only where saves are involved
  // The game's data is needed to judge the Continue save against the game as it is now (a different
  // version greys it out) and, for a game with a campaign, for its maps. A game without either is not loaded.
  const std::string dir = host_.args().gamesDir + "/" + game_.id;
  std::string probe;
  const bool campaignFile = readTextFile(dir + "/data/campaign.json", probe);
  const bool continueFile = saving && store.exists(0);
  Database db;
  bool haveDb = false;
  if (continueFile || campaignFile) {
    std::string err;
    haveDb = loadGame(dir, db, err);
    if (!haveDb) OTE_WARN("cannot check the Continue save / campaign of '%s': %s", game_.id.c_str(), err.c_str());
  }
  const bool campaign = haveDb && db.campaign != nullptr;
  if (continueFile) {
    SaveInfo info = store.inspect(0, haveDb ? &db : nullptr, game_.id);
    Row r{kContinue, "Continue", "", true, ""};
    if (info.state == SaveInfo::Ok) {
      r.detail = saveTitle(info) + " - " + saveDetail(info);
    } else {
      r.enabled = false;
      r.detail = info.reason;
      continueProblem_ = info.reason;
    }
    rows_.push_back(r);
  }
  if (campaign) rows_.push_back({kNewGame, "New game", "Begin the story", true, ""});
  else rows_.push_back({kPlay, "Play", "Start a new battle", true, ""});
  if (saving && mode == SavingMode::Any) {
    int used = 0;
    for (int s = 1; s <= SaveStore::kSlots; ++s) used += store.exists(s) ? 1 : 0;
    rows_.push_back({kLoad, "Load", used == 0 ? "No saved games yet" : std::to_string(used) + " of " + std::to_string(SaveStore::kSlots) + " slots in use", true, ""});
  }
  rows_.push_back({kOptions, "Game options", "Font, text size and style", true, ""});
  rows_.push_back({kBack, "Back", "Return to the game list", true, ""});
  if (!campaign) return;
  // Free battles: only the maps the game maker allows. They are not saved automatically (the Continue file
  // belongs to the story), which the rows say where saving is on.
  std::vector<Row> maps;
  for (const MapRef& m : db.maps)
    if (freeBattleAllowed(db, m.id))
      maps.push_back({kFreeMap, m.name.empty() ? m.id : m.name, "Not saved automatically", true, m.id});
  if (maps.empty()) return;
  rows_.push_back({kHeading, "Free battles", "", false, ""});
  rows_.insert(rows_.end(), maps.begin(), maps.end());
}

void GameScene::layout(const Renderer& r) {
  int lh = r.lineHeight();
  int top = 8 + r.lineHeight(lh <= 11 ? 2 : 1) + 8 + lh;  // title + room for "^"
  list_.rowH = twoLines_ ? 2 * lh + 10 : lh + 8;
  list_.w = std::min(r.width() - 24, 260);
  list_.x = (r.width() - list_.w) / 2;
  list_.y = top;
  int count = (int)rows_.size();
  int room = r.height() - top - footerHeight(r) - lh;
  list_.h = std::max(list_.rowH, std::min(count * list_.rowH, room));
}

void GameScene::activate(int row) {
  const Row& r = rows_[(size_t)row];
  uiCue(r.kind == kBack ? Cue::Cancel : (r.kind == kContinue && !r.enabled) ? Cue::Error : Cue::Confirm);
  switch (r.kind) {
    case kContinue: {
      std::string err;
      if (!r.enabled) host_.showError("Cannot continue", continueProblem_, SceneHost::ToGame);
      else if (!host_.resumeSave(0, err)) host_.showError("Cannot continue", err, SceneHost::ToGame);
      break;
    }
    case kPlay: host_.loadGame(game_.id); break;
    case kNewGame: startNewGame(); break;
    case kFreeMap: host_.startFreeBattle(game_.id, r.mapId); break;
    case kHeading: break;
    case kLoad: host_.showSlots(game_.id); break;
    case kOptions: host_.showOptions(game_.id); break;
    case kBack: host_.showHome(); break;
  }
}

// A new story replaces the Continue save (the first scene's end writes a new one): ask first, as starting
// a battle does.
void GameScene::startNewGame() {
  SaveStore store = host_.saveStore(game_.id);
  if (store.enabled() && game_.info.saving != SavingMode::Off && store.exists(0)) {
    confirm_.open("This replaces your Continue save.", "Start new game", "Cancel");
    return;
  }
  host_.startCampaign(game_.id);
}

void GameScene::skipHeading(Button dir) {
  for (int guard = 0; guard < 2 && rows_[(size_t)list_.sel].kind == kHeading; ++guard) list_.step(dir);
}

void GameScene::onInput(const InputEvent& e) {
  layout(host_.renderer());
  if (confirm_.isOpen()) {
    ConfirmBox::Result a = confirm_.onInput(e, host_.renderer());
    if (a == ConfirmBox::None) return;
    confirm_.close();
    if (a == ConfirmBox::Yes) host_.startCampaign(game_.id);
    return;
  }
  auto selectable = [&](int row) { return row >= 0 && rows_[(size_t)row].kind != kHeading; };
  if (e.type == InputEvent::PointerMove) {
    int row = list_.rowAt(e.x, e.y);
    if (selectable(row)) list_.sel = row;
  } else if (e.type == InputEvent::PointerDown) {
    int row = list_.rowAt(e.x, e.y);
    if (selectable(row)) { list_.sel = row; activate(row); }
  } else if (e.type == InputEvent::ButtonDown) {
    if (list_.step(e.button)) { skipHeading(e.button); return; }
    if (e.button == Button::Confirm) activate(list_.sel);
    else if (e.button == Button::Cancel) { uiCue(Cue::Cancel); host_.showHome(); }
  }
}

void GameScene::draw(Renderer& r) {
  layout(r);
  const int lh = r.lineHeight(), count = (int)rows_.size();
  drawTitle(r, game_.name);
  int first = list_.first();
  for (int i = 0; i < list_.visible() && first + i < count; ++i) {
    int row = first + i, ry = list_.rowY(row);
    const Row& it = rows_[(size_t)row];
    bool sel = row == list_.sel;
    if (it.kind == kHeading) {  // a caption, no box: the free battles below it
      r.drawText(list_.x + 8, ry + list_.rowH - lh - 6, fitText(r, it.label, list_.w - 16).c_str(), chrome::kAccent);
      continue;
    }
    drawRowBox(r, list_.x, ry, list_.w, list_.rowH - 3, sel);
    int tw = list_.w - 16;
    int ty = ry + (twoLines_ ? 4 : 3);
    r.drawText(list_.x + 8, ty, fitText(r, it.label, tw).c_str(),
               !it.enabled ? kGreyed : (sel ? chrome::kTextSel : chrome::kText));
    if (twoLines_ && !it.detail.empty())
      r.drawText(list_.x + 8, ty + lh + 2, fitText(r, it.detail, tw).c_str(), it.enabled ? chrome::kDim : kReason);
  }
  list_.drawMore(r, chrome::kAccent);

  // Description, author and map count in the space left under the rows.
  int rh = lh + 2;
  int dy = list_.y + list_.h + (list_.first() + list_.visible() < count ? lh : 0) + 6;
  int dw = std::min(r.width() - 16, 400), dx = (r.width() - dw) / 2;
  int maxLines = (r.height() - footerHeight(r) - dy) / rh;
  std::vector<std::string> lines =
      wrapText(r, game_.info.description.empty() ? "(no description)" : game_.info.description, dw);
  std::vector<std::string> meta;
  if (!game_.info.author.empty()) meta.push_back("by " + game_.info.author);
  meta.push_back(intStr(game_.info.mapCount) + (game_.info.mapCount == 1 ? " map" : " maps"));
  int n = std::min((int)lines.size(), std::max(0, maxLines - (int)meta.size()));
  for (int i = 0; i < n; ++i) {
    std::string t = lines[i];
    if (i == n - 1 && n < (int)lines.size()) t = fitText(r, t + "..", dw);
    r.drawText(dx, dy + i * rh, t.c_str(), chrome::kText);
  }
  for (size_t i = 0; i < meta.size(); ++i) {
    int line = n + (int)i;
    if (line >= maxLines) break;
    r.drawText(dx, dy + line * rh, fitText(r, meta[i], dw).c_str(), chrome::kDim);
  }
  drawFooter(r, "Up/Down: choose   Confirm: open   Cancel: back");
  confirm_.draw(r);
}

}  // namespace ote
