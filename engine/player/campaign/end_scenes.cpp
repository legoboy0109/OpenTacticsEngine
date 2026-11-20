#include "campaign/end_scenes.h"
#include "sound/cue_sink.h"

#include <algorithm>

#include "ui/chrome.h"
#include "ui/text.h"

namespace ote {

namespace {
constexpr float kAutoplayWait = 1.0f;
}

Color storyBackground(SceneHost& host) {
  const Database& db = host.session()->db;
  if (db.styles.empty()) return Color(10, 10, 16);
  const StyleDef& st = db.styles[(size_t)std::max(0, std::min(host.styleIndex(), (int)db.styles.size() - 1))];
  return st.hasBackground ? Color::rgb(st.background) : Color(10, 10, 16);
}

// ---- EndScene

EndScene::EndScene(SceneHost& host, const std::string& big, const std::string& small)
    : host_(host), big_(big), small_(small), bg_(storyBackground(host)) {}

void EndScene::leave() {
  if (left_) return;
  left_ = true;
  host_.showGame(host_.currentGameId());  // replaces this scene: nothing after this line
}

void EndScene::onInput(const InputEvent& e) {
  bool done = e.type == InputEvent::PointerDown ||
              (e.type == InputEvent::ButtonDown &&
               (e.button == Button::Confirm || e.button == Button::Cancel || e.button == Button::Skip));
  if (done) leave();
}

void EndScene::update(double dt) {
  time_ += (float)dt;
  if (host_.args().autoplay && time_ >= kAutoplayWait) leave();
}

void EndScene::draw(Renderer& r) {
  r.clear(bg_);
  int w = r.width(), h = r.height();
  int sc = fitScale(r, big_, w - 24, r.lineHeight() <= 11 ? 3 : 2);
  int smallH = small_.empty() ? 0 : r.lineHeight() + 8;
  int y = (h - r.lineHeight(sc) - smallH) / 2;
  drawTextCentered(r, w / 2, y, fitText(r, big_, w - 24, sc), chrome::kAccent, sc);
  if (!small_.empty()) drawTextCentered(r, w / 2, y + r.lineHeight(sc) + 8, fitText(r, small_, w - 24), chrome::kText);
  drawFooter(r, "{okTap}: back to the game page");
}

// ---- RetryScene

RetryScene::RetryScene(SceneHost& host, const std::string& heading, const std::string& title)
    : host_(host), heading_(heading), title_(title), bg_(storyBackground(host)) {
  list_.count = 2;
}

void RetryScene::layout(const Renderer& r) {
  int lh = r.lineHeight();
  list_.rowH = lh + 10;
  list_.w = std::min(r.width() - 24, 200);
  list_.x = (r.width() - list_.w) / 2;
  list_.h = 2 * list_.rowH;
  // Heading, name and a line of explanation above the rows; the block as a whole sits in the middle.
  int top = lh + 6 + lh + 6 + lh + 12;
  int total = top + list_.h;
  list_.y = std::max(top, (r.height() - total) / 2 + top);
}

void RetryScene::choose(int row) {
  if (left_) return;
  left_ = true;
  uiCue(row == 0 ? Cue::Confirm : Cue::Cancel);
  if (row == 0) host_.campaignNext();  // the playthrough is still in its battle phase: a new battle starts
  else host_.showGame(host_.currentGameId());
}

void RetryScene::onInput(const InputEvent& e) {
  layout(host_.renderer());
  if (e.type == InputEvent::PointerMove) {
    int row = list_.rowAt(e.x, e.y);
    if (row >= 0) list_.sel = row;
  } else if (e.type == InputEvent::PointerDown) {
    int row = list_.rowAt(e.x, e.y);
    if (row >= 0) { list_.sel = row; choose(row); }
  } else if (e.type == InputEvent::ButtonDown) {
    if (list_.step(e.button)) return;
    if (e.button == Button::Confirm) choose(list_.sel);
    else if (e.button == Button::Cancel) choose(1);
  }
}

void RetryScene::update(double dt) {
  time_ += (float)dt;
  if (host_.args().autoplay && time_ >= kAutoplayWait) choose(0);
}

void RetryScene::draw(Renderer& r) {
  layout(r);
  r.clear(bg_);
  const int lh = r.lineHeight(), w = r.width();
  int y = list_.y - (lh + 6 + lh + 6 + lh + 12);
  drawTextCentered(r, w / 2, y, fitText(r, heading_, w - 24), chrome::kDim);
  drawTextCentered(r, w / 2, y + lh + 6, fitText(r, title_, w - 24), chrome::kTitle);
  drawTextCentered(r, w / 2, y + 2 * (lh + 6), fitText(r, "The battle was lost.", w - 24), chrome::kText);
  static const char* const kLabels[2] = {"Try again", "Back to title"};
  for (int i = 0; i < 2; ++i) {
    int ry = list_.rowY(i);
    bool sel = i == list_.sel;
    drawRowBox(r, list_.x, ry, list_.w, list_.rowH - 3, sel);
    r.drawText(list_.x + 8, ry + 3, fitText(r, kLabels[i], list_.w - 16).c_str(),
               sel ? chrome::kTextSel : chrome::kText);
  }
  drawFooter(r, "{updown}: choose   {confirm}: select");
}

}  // namespace ote
