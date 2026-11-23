#include "scene_home.h"

#include <algorithm>

#include "sound/cue_sink.h"
#include "ui/chrome.h"
#include "ui/text.h"

namespace ote {
namespace {
constexpr int kMargin = 8, kGap = 8;
const Color kWarn(255, 170, 90), kErr(255, 150, 140);
}  // namespace

bool HomeScene::hasQuit() const {
#ifdef __EMSCRIPTEN__
  return false;  // a web page cannot close itself
#else
  return true;
#endif
}

HomeScene::HomeScene(SceneHost& host) : host_(host) {
  list_.count = gameCount() + 1 + (hasQuit() ? 1 : 0);
  std::string last = host.settings().state("lastGame", "");
  for (int i = 0; i < gameCount(); ++i)
    if (host.games()[i].id == last) list_.sel = i;
}

std::string HomeScene::label(int row) const {
  if (row < gameCount()) return host_.games()[row].name;
  return row == optionsRow() ? "Options" : "Quit";
}

void HomeScene::layout(const Renderer& r) {
  int lh = r.lineHeight(), w = r.width(), h = r.height();
  int top = 8 + r.lineHeight(lh <= 11 ? 2 : 1) + 8;
  if (!host_.notice().empty()) top += lh + 4;
  top += lh;  // room for the "^" marker
  list_.rowH = lh + 8;
  int widest = 0;
  for (int i = 0; i < list_.count; ++i) widest = std::max(widest, textW(r, label(i)));
  list_.x = kMargin;
  list_.w = std::max(w * 34 / 100, std::min(widest + 20, w * 55 / 100));
  list_.y = top;
  list_.h = std::max(list_.rowH, h - top - footerHeight(r) - lh);
  infoX_ = list_.x + list_.w + kGap;
  infoW_ = std::max(0, w - kMargin - infoX_);
}

void HomeScene::activate(int row) {
  if (row < 0 || row >= list_.count) return;
  uiCue(Cue::Confirm);
  if (row < gameCount()) {
    const GameEntry& g = host_.games()[row];
    host_.settings().setState("lastGame", g.id);
    if (!g.readable) host_.showError("Cannot load game '" + g.id + "'", g.error, SceneHost::ToHome);
    else host_.showGame(g.id);
  } else if (row == optionsRow()) {
    host_.showOptions("");
  } else {
    host_.quit();
  }
}

void HomeScene::onInput(const InputEvent& e) {
  layout(host_.renderer());
  if (exitAsk_.isOpen()) {  // modal: Cancel / a tap outside answers No
    ConfirmBox::Result a = exitAsk_.onInput(e, host_.renderer());
    if (a == ConfirmBox::None) return;
    exitAsk_.close();
    if (a == ConfirmBox::Yes) host_.quit();
    return;
  }
  if (list_.input(e)) return;  // the wheel, a "^" / "v" marker, a finger's drag (ui/row_list.h)
  if (e.type == InputEvent::PointerMove) {
    int row = list_.rowAt(e.x, e.y);
    if (row >= 0) list_.sel = row;
  } else if (e.type == InputEvent::PointerDown) {
    int row = list_.rowAt(e.x, e.y);
    if (row >= 0) { list_.sel = row; activate(row); }
  } else if (e.type == InputEvent::ButtonDown) {
    if (list_.step(e.button)) return;
    if (e.button == Button::Confirm) activate(list_.sel);
    if (e.button == Button::Cancel) {
      // What back does here is the platform's convention (platform.h LauncherBack): Android leaves, UWP asks first,
      // elsewhere nothing, so a stray Esc cannot close the player.
      LauncherBack b = launcherBack();
      if (b == LauncherBack::Exit) host_.quit();
      else if (b == LauncherBack::ConfirmExit) exitAsk_.open("Exit the player?", "Yes", "No");
    }
  }
}

void HomeScene::drawInfo(Renderer& r, int x, int y, int w, int h) const {
  if (w < 16 || h < 8) return;
  r.fillRect(x, y, w, h, chrome::kPanel);
  r.drawRect(x, y, w, h, Color(52, 58, 88));
  int lh = r.lineHeight(), rh = lh + 2, pad = 6, tw = w - 2 * pad;
  int maxLines = (h - pad) / rh;
  std::vector<std::pair<std::string, Color>> lines;
  auto add = [&](const std::string& text, Color c) {
    for (const std::string& l : wrapText(r, text, tw)) lines.push_back({l, c});
  };
  std::vector<std::pair<std::string, Color>> meta;
  int sel = list_.sel;
  if (gameCount() == 0) {
    add("No games found. Put game folders (each with a game.json) into the games folder.", chrome::kDim);
  } else if (sel < gameCount()) {
    const GameEntry& g = host_.games()[sel];
    if (!g.readable) {
      add("This game cannot be loaded:", kWarn);
      add(g.error, kErr);
    } else {
      add(g.info.description.empty() ? "(no description)" : g.info.description, chrome::kText);
      if (!g.info.author.empty()) meta.push_back({"by " + g.info.author, chrome::kDim});
      meta.push_back({intStr(g.info.mapCount) + (g.info.mapCount == 1 ? " map" : " maps"), chrome::kDim});
    }
  } else if (sel == optionsRow()) {
    add("Font, text size, UI scale and visual style. They apply to every game; each game can override them.",
        chrome::kDim);
  } else {
    add("Close OpenTacticsEngine.", chrome::kDim);
  }
  // Author / map count stay visible (bottom) if there is room for them plus a line of text.
  int metaLines = (int)meta.size();
  if (metaLines && maxLines < metaLines + 2) { meta.clear(); metaLines = 0; }
  int descMax = maxLines - metaLines - (metaLines ? 1 : 0);
  int n = std::min((int)lines.size(), std::max(0, descMax));
  for (int i = 0; i < n; ++i) {
    std::string t = lines[i].first;
    if (i == n - 1 && n < (int)lines.size()) t = fitText(r, t + "..", tw);  // cut off: say so
    r.drawText(x + pad, y + pad + i * rh, t.c_str(), lines[i].second);
  }
  for (int i = 0; i < metaLines; ++i)
    r.drawText(x + pad, y + h - pad - (metaLines - i) * rh + 2, fitText(r, meta[i].first, tw).c_str(), meta[i].second);
}

void HomeScene::draw(Renderer& r) {
  layout(r);
  int y = drawTitle(r, "OpenTacticsEngine");
  if (!host_.notice().empty())
    drawTextCentered(r, r.width() / 2, y - 4, fitText(r, host_.notice(), r.width() - 2 * kMargin), kWarn);
  int first = list_.first();
  for (int i = 0; i < list_.visible() && first + i < list_.count; ++i) {
    int row = first + i, ry = list_.rowY(row);
    bool sel = row == list_.sel;
    drawRowBox(r, list_.x, ry, list_.w, list_.rowH - 3, sel);
    Color c = sel ? chrome::kTextSel : chrome::kText;
    if (row >= gameCount()) c = sel ? chrome::kAccent : chrome::kDim;  // system entries
    r.drawText(list_.x + 8, ry + 3, fitText(r, label(row), list_.w - 16).c_str(), c);
  }
  list_.drawMore(r, chrome::kAccent);
  drawInfo(r, infoX_, list_.y, infoW_, list_.h);
  drawFooter(r, "{updown}: choose   {confirm}: open   {orTap}");
  exitAsk_.draw(r);
}

}  // namespace ote
