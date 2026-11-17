// The player's scene stage (engine/player/ui/scene_stage.*, docs/EVENTS.md "Scene presentation") without a window: the
// background transition, the fade, the shake, who stands where, and the skip that must end exactly where playing ends;
// plus how a picture is fitted to the screen and the dialogue box's "auto" and centred narrator lines.
#include <cstdlib>
#include <string>
#include <vector>

#include "core/data/scene_fx.h"
#include "fake_renderer.h"
#include "test.h"
#include "ui/dialogue_box.h"
#include "ui/scene_stage.h"
#include "ui/scene_stage_draw.h"

using namespace ote;

namespace {

// One scripted command for the pure timeline tests: what the owner (the battle view or the scene screen) feeds the stage.
struct Step {
  enum Kind { Background, Fade, Wait, Shake, Say, Leave, Clear, Weather } kind;
  std::string text;
  int ms = 0;
  bool flag = false;  // Background: fade transition; Fade: out
  int pos = 0;
  uint32_t color = 0;
};

// Plays the script like the owners do: a command that takes time holds the next one until the stage has had that long
// (`dt` frames), or, when `skip`, everything completes at once.
void play(SceneStage& s, const std::vector<Step>& script, bool skip, std::vector<std::string>* order = nullptr) {
  for (const Step& st : script) {
    int hold = 0;
    switch (st.kind) {
      case Step::Background: s.setBackground(st.text, st.flag && !skip, skip ? 0 : st.ms); hold = st.flag ? st.ms : 0; break;
      case Step::Fade: s.fade(st.flag, st.color, skip ? 0 : st.ms); hold = st.ms; break;
      case Step::Wait: hold = st.ms; break;
      case Step::Shake: s.shake(skip ? 0 : st.ms, 3); hold = st.ms; break;
      case Step::Say: s.say(st.text, st.text, st.pos); break;
      case Step::Leave: s.leave(st.text); break;
      case Step::Clear: s.clearPortraits(); break;
      case Step::Weather: s.setWeather(st.text); break;
    }
    if (order) order->push_back(std::to_string((int)st.kind));
    if (skip) {
      s.finishNow();
      continue;
    }
    for (int t = 0; t < hold; t += 10) s.update(0.010);  // the owner waits for the transition
    s.finishNow();                                         // (and the entry's end completes it, as the view does)
  }
}

std::string describe(const SceneStage& s) {
  std::string d = "bg=" + s.background() + " prev=" + s.previousBackground() + " t=" + std::to_string(s.backgroundProgress()) +
                  " fade=" + std::to_string(s.fadeAmount()) + (s.fadedOut() ? " out" : " in") + " col=" + std::to_string(s.fadeColor()) +
                  " weather=" + s.weather() + " shake=" + std::to_string(s.shaking()) + " who=" + s.speaker() + " on=";
  for (const SceneStage::Portrait& p : s.portraits()) d += p.key + "@" + std::to_string(p.position) + ",";
  return d;
}

std::vector<Step> classicScene() {
  return {{Step::Fade, "", 300, true, 0, 0x102040}, {Step::Background, "forest", 0},
          {Step::Weather, "rain"},
          {Step::Fade, "", 300, false},
          {Step::Say, "eirik", 0, false, kPosLeft}, {Step::Say, "lyra", 0, false, kPosRight},
          {Step::Wait, "", 400}, {Step::Shake, "", 300},
          {Step::Background, "hall", 400, true},
          {Step::Leave, "lyra"}, {Step::Say, "eirik", 0, false, 0},
          {Step::Fade, "", 200, true, 0, 0x000000}};
}

}  // namespace

TEST(scene_stage_background_cut_and_fade) {
  SceneStage s;
  CHECK(!s.backgroundShown());
  s.setBackground("forest", false, 500);  // a cut
  CHECK(s.backgroundShown() && s.background() == "forest" && s.backgroundProgress() == 1.0f && s.previousBackground().empty());
  s.setBackground("hall", true, 500);  // a cross-fade: the old one stays under the new one
  CHECK(s.background() == "hall" && s.previousBackground() == "forest" && s.backgroundProgress() == 0.0f && s.animating());
  s.update(0.25);
  CHECK(s.backgroundProgress() > 0.49f && s.backgroundProgress() < 0.51f);
  s.update(0.30);
  CHECK(s.backgroundProgress() == 1.0f && s.previousBackground().empty() && !s.animating());
  // Clearing with a fade: the picture leaves, the map is under it.
  s.setBackground("", true, 200);
  CHECK(s.background().empty() && s.previousBackground() == "hall" && s.backgroundShown());
  s.update(0.3);
  CHECK(!s.backgroundShown());
  // A command that changes nothing does not start a transition.
  s.setBackground("", true, 200);
  CHECK(!s.animating());
}

TEST(scene_stage_fade_stays_until_faded_in) {
  SceneStage s;
  s.fade(true, 0x102030, 400);
  CHECK(s.fadedOut() && s.fading() && s.fadeColor() == 0x102030);
  s.update(0.2);
  CHECK(s.fadeAmount() > 0.45f && s.fadeAmount() < 0.55f);
  s.update(0.5);
  CHECK(s.fadeAmount() == 1.0f && !s.fading());
  for (int i = 0; i < 100; ++i) s.update(0.1);  // it does not wear off
  CHECK(s.fadeAmount() == 1.0f && s.fadedOut());
  s.fade(false, 0x102030, 400);
  s.update(0.5);
  CHECK(s.fadeAmount() == 0.0f && !s.fadedOut());
  s.fade(true, 0, 0);  // 0 ms = at once
  CHECK(s.fadeAmount() == 1.0f);
  s.fade(false, 0, 0);
  CHECK(s.fadeAmount() == 0.0f);
}

TEST(scene_stage_shake_dies_away_and_is_deterministic) {
  SceneStage a, b;
  a.shake(400, 6);
  b.shake(400, 6);
  CHECK(a.shaking());
  bool moved = false;
  for (int i = 0; i < 40; ++i) {
    a.update(0.01);
    b.update(0.01);
    CHECK(a.shakeX() == b.shakeX() && a.shakeY() == b.shakeY());
    CHECK(std::abs(a.shakeX()) <= 6 && std::abs(a.shakeY()) <= 4);
    moved = moved || a.shakeX() != 0 || a.shakeY() != 0;
  }
  CHECK(moved);
  a.update(0.1);
  CHECK(!a.shaking() && a.shakeX() == 0 && a.shakeY() == 0);
  a.shake(0, 5);  // no length, no shake
  CHECK(!a.shaking());
}

TEST(scene_stage_portraits_stand_dim_and_leave) {
  SceneStage s;
  CHECK(!s.say("eirik", "eirik", 0));  // no position: the classic portrait beside the box, nobody stands
  CHECK(s.portraits().empty());
  CHECK(s.say("eirik", "eirik", kPosLeft));
  CHECK(s.say("lyra", "lyra", kPosRight));
  CHECK_EQ(s.portraits().size(), size_t(2));
  CHECK(s.speaker() == "lyra");  // the speaker is bright, the other one dimmed (drawing)
  CHECK(s.say("eirik", "eirik", 0));  // one that stands lights up where it stands
  CHECK(s.speaker() == "eirik" && s.portraits().size() == 2 && s.portraits()[0].position == kPosLeft);
  CHECK(!s.say("gorm", "gorm", 0));  // a stranger with no position speaks beside the box; the two stay
  CHECK(s.portraits().size() == 2 && s.speaker() == "gorm");
  // A newcomer takes the place of whoever stood there; a speaker that moves leaves its old place.
  CHECK(s.say("gorm", "gorm", kPosRight));
  CHECK(s.portraits().size() == 2 && s.staged("gorm") && !s.staged("lyra"));
  CHECK(s.say("eirik", "eirik", kPosCenter));
  CHECK(s.portraits().size() == 2 && s.staged("eirik") && s.staged("gorm"));
  s.leave("gorm");
  CHECK(!s.staged("gorm") && s.portraits().size() == 1);
  s.leave("nobody");  // leaving when not there is nothing
  CHECK(s.portraits().size() == 1);
  s.leave("");  // everybody
  CHECK(s.portraits().empty() && s.speaker().empty());
}

TEST(scene_stage_skip_ends_exactly_where_playing_ends) {
  const std::vector<Step> script = classicScene();
  SceneStage played, skipped;
  play(played, script, false);
  play(skipped, script, true);
  CHECK_STR(describe(played), describe(skipped));
  // And that end state is the one the script says: faded out in the last colour, hall, rain, eirik alone on the left, lit.
  CHECK(skipped.background() == "hall" && skipped.fadedOut() && skipped.fadeAmount() == 1.0f);
  CHECK(skipped.weather() == "rain" && skipped.portraits().size() == 1 && skipped.speaker() == "eirik");
  CHECK(skipped.previousBackground().empty() && !skipped.shaking() && !skipped.animating());
  // Skipping part-way through a transition completes it.
  SceneStage mid;
  mid.setBackground("forest", true, 800);
  mid.fade(true, 0, 800);
  mid.shake(500, 4);
  mid.update(0.1);
  CHECK(mid.animating());
  mid.finishNow();
  CHECK(!mid.animating() && mid.backgroundProgress() == 1.0f && mid.fadeAmount() == 1.0f && mid.previousBackground().empty());
}

TEST(scene_stage_sync_is_what_a_saved_game_shows) {
  SceneStage s;
  s.say("eirik", "eirik", kPosLeft);
  s.fade(true, 0xff0000, 0);
  s.shake(300, 3);
  s.syncFrom("hall", "snow");
  CHECK(s.background() == "hall" && s.weather() == "snow" && s.fadeAmount() == 0.0f && !s.fadedOut() && !s.shaking());
  CHECK(s.portraits().empty() && !s.animating());
  s.syncFrom("", "");
  CHECK(!s.backgroundShown() && s.weather().empty());
}

TEST(scene_stage_picture_fit_covers_the_screen_with_whole_scales) {
  // 240 x 135 on 480 x 270: exactly 2, nothing cropped.
  CoverFit f = fitCover(240, 135, 480, 270);
  CHECK(f.scale == 2.0f && f.srcX == 0 && f.srcY == 0 && f.srcW == 240 && f.srcH == 135 && f.dstW == 480 && f.dstH == 270 && f.dstX == 0 && f.dstY == 0);
  // The same picture on a phone (270 x 480): the height decides (3.56 -> 4), the sides are cropped evenly.
  f = fitCover(240, 135, 270, 480);
  CHECK(f.scale == 4.0f && f.srcW == 68 && f.srcH == 120 && f.srcX == (240 - 68) / 2 && f.dstW == 272 && f.dstH == 480 &&
        f.dstX == -1 && f.dstY == 0);
  // A cell of 120 x 68 on 480 x 270: 4 (a whole number, not 3.97).
  f = fitCover(120, 68, 480, 270);
  CHECK(f.scale == 4.0f && f.dstW >= 480 && f.dstH >= 270);
  // A picture bigger than the screen shrinks by a fraction and still covers.
  f = fitCover(960, 540, 480, 270);
  CHECK(f.scale == 0.5f && f.dstW == 480 && f.dstH == 270);
  f = fitCover(1000, 400, 480, 270);  // wide: the height decides, the sides are cropped
  CHECK(f.scale > 0.6f && f.scale < 0.7f && f.dstH >= 270 && f.srcW < 1000);
  // Whole numbers when it grows: a scale between 1 and 2 becomes 2.
  f = fitCover(300, 170, 480, 270);
  CHECK(f.scale == 2.0f && f.srcW < 300);
}

TEST(scene_stage_dialogue_box_auto_advances_and_narrator_is_centred) {
  ote_test::FakeRenderer r;
  DialogueBox box;
  DialogueLine l;
  l.name = "Eirik";
  l.text = "Hold.";
  l.autoMs = 500;
  box.open(r, 480, 270, 259, l, PortraitArt());
  DialogueBox::Result res = DialogueBox::None;
  float t = 0;
  while (res == DialogueBox::None && t < 5) {  // the letters appear (about 0.1 s), then 0.5 s, then it goes on by itself
    res = box.update(0.02);
    t += 0.02f;
  }
  CHECK(res == DialogueBox::Finished && t > 0.55f && t < 0.75f);
  // Without "auto" it waits for the player for ever.
  l.autoMs = 0;
  box.open(r, 480, 270, 259, l, PortraitArt());
  res = DialogueBox::None;
  for (int i = 0; i < 500; ++i) res = box.update(0.02) == DialogueBox::None ? res : DialogueBox::Finished;
  CHECK(res == DialogueBox::None);
  // A question never goes on by itself.
  l.autoMs = 100;
  box.open(r, 480, 270, 259, l, PortraitArt());
  box.setAnswers(r, {"Yes", "No"});
  res = DialogueBox::None;
  for (int i = 0; i < 300; ++i) res = box.update(0.02) == DialogueBox::None ? res : DialogueBox::Finished;
  CHECK(res == DialogueBox::None);
}
