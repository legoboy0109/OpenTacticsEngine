// The seven stage slots and the silent portrait commands (docs/EVENTS.md "Standing portraits"): the loader reads and refuses
// enter / move / leave and the four added positions, the events carry them, the stage state follows (enter, slide, fade out),
// and the layout maths never lets neighbours overlap badly at desktop and phone sizes.
#include <algorithm>

#include "story_fixture.h"
#include "test.h"
#include "ui/scene_stage.h"
#include "ui/scene_stage_layout.h"

using namespace story;

namespace {

const std::vector<std::string> kRows = {".........", ".........", "........."};
const std::vector<UnitSpec> kUnits = {{"soldier", 0, 0, 1, false}, {"soldier", 1, 8, 1, false}};

struct Scene : StoryWorld {
  Scene() : StoryWorld(kRows, kUnits, {"eirik", "gorm"}) {}
  std::string err(const std::string& commands) {
    return load(R"([{"id":"a","when":{"battle":"start"},"do":)" + commands + "}]");
  }
};

bool has(const std::string& s, const char* part) { return s.find(part) != std::string::npos; }

}  // namespace

TEST(stage_slot_commands_load) {
  Scene w;
  CHECK_STR(w.err(R"([{"enter":"eirik","position":"midLeft"},{"enter":"gorm","position":"farRight","transition":"fade","ms":300},
                       {"enter":"Old Man","position":"center","portrait":"elder"}])"), std::string());
  CHECK_STR(w.err(R"([{"move":"eirik","position":"right"},{"move":"eirik","position":"farLeft","transition":"cut"},
                       {"move":"eirik","position":"center","ms":900}])"), std::string());
  CHECK_STR(w.err(R"([{"leave":"eirik","transition":"fade","ms":250},{"clearStage":true,"transition":"fade"}])"), std::string());
  CHECK_STR(w.err(R"([{"say":"Hi","speaker":"eirik","position":"farLeft"},{"say":"Ho","speaker":"gorm","position":"midRight"}])"), std::string());
}

TEST(stage_slot_commands_are_checked) {
  Scene w;
  CHECK(has(w.err(R"([{"enter":"eirik"}])"), "needs a \"position\""));
  CHECK(has(w.err(R"([{"enter":"eirik","position":"nowhere"}])"), "\"position\" must be"));
  CHECK(has(w.err(R"([{"enter":"","position":"left"}])"), "\"enter\" must name the speaker"));
  CHECK(has(w.err(R"([{"enter":"eirik","position":"left","transition":"wipe"}])"), "\"transition\" must be \"cut\" or \"fade\""));
  CHECK(has(w.err(R"([{"enter":"eirik","position":"left","ms":99999}])"), "\"ms\" is milliseconds"));
  CHECK(has(w.err(R"([{"enter":"eirik","position":"left","speed":2}])"), "unknown key \"speed\""));
  CHECK(has(w.err(R"([{"move":"eirik","position":"left","portrait":"x"}])"), "unknown key \"portrait\""));
  CHECK(has(w.err(R"([{"move":"eirik"}])"), "needs a \"position\""));
  CHECK(has(w.err(R"([{"move":"eirik","position":"left","transition":"wipe"}])"), "\"transition\" must be"));
  CHECK(has(w.err(R"([{"leave":"eirik","transition":"slide"}])"), "\"transition\" must be \"cut\" or \"fade\""));
  CHECK(has(w.err(R"([{"say":"x","position":"farther"}])"), "\"position\" must be"));
  CHECK(has(w.err(R"([{"enter":"eirik","position":"left","fade":"out"}])"), "has two commands"));
}

TEST(stage_slot_events_carry_slot_transition_and_portrait) {
  Scene w;
  CHECK(w.play(R"([{"id":"a","when":{"battle":"start"},"do":[
      {"enter":"eirik","position":"midLeft","transition":"fade","ms":300,"portrait":"hero"},
      {"move":"eirik","position":"farRight"},{"move":"eirik","position":"left","transition":"cut"},
      {"leave":"eirik","transition":"fade"},{"leave":"gorm"},
      {"say":"Hi","speaker":"eirik","position":"midRight"}]}])"));
  std::vector<const Event*> fx;
  for (const Event& e : w.events)
    if (e.type == EventType::SceneEffect) fx.push_back(&e);
  CHECK_EQ(fx.size(), size_t(5));
  if (fx.size() == 5) {
    CHECK((SceneFx::Kind)fx[0]->a == SceneFx::Kind::Enter && fx[0]->d == kPosMidLeft && fx[0]->c == 300 && (fx[0]->flags & kSceneFadeTrans));
    CHECK(w.battle.storyText(fx[0]->b) == "eirik" && fx[0]->data.size() == 1 && w.battle.storyText(fx[0]->data[0]) == "hero");
    CHECK((SceneFx::Kind)fx[1]->a == SceneFx::Kind::Move && fx[1]->d == kPosFarRight && fx[1]->c == kDefaultSlideMs && (fx[1]->flags & kSceneFadeTrans));
    CHECK((SceneFx::Kind)fx[2]->a == SceneFx::Kind::Move && fx[2]->d == kPosLeft && !(fx[2]->flags & kSceneFadeTrans));
    CHECK((SceneFx::Kind)fx[3]->a == SceneFx::Kind::Leave && (fx[3]->flags & kSceneFadeTrans) && fx[3]->c == kDefaultFadeMs);
    // A plain leave is exactly what it was before this feature: no ms, no flags, no data.
    CHECK((SceneFx::Kind)fx[4]->a == SceneFx::Kind::Leave && fx[4]->c == -1 && fx[4]->flags == 0 && fx[4]->d == 0 && fx[4]->data.empty());
  }
  const Event* say = nullptr;
  for (const Event& e : w.events)
    if (e.type == EventType::Dialogue) say = &e;
  CHECK(say && (say->flags & kDialoguePosMask) == kPosMidRight);
}

TEST(stage_slots_are_ordered_left_to_right) {
  CHECK_EQ(stageSlot(kPosFarLeft), 0);
  CHECK_EQ(stageSlot(kPosLeft), 1);
  CHECK_EQ(stageSlot(kPosMidLeft), 2);
  CHECK_EQ(stageSlot(kPosCenter), 3);
  CHECK_EQ(stageSlot(kPosMidRight), 4);
  CHECK_EQ(stageSlot(kPosRight), 5);
  CHECK_EQ(stageSlot(kPosFarRight), 6);
  CHECK_EQ(stageSlot(kPosNone), -1);
  // Saved dialogue flags and old logs keep their values.
  CHECK(kPosLeft == 1 && kPosRight == 2 && kPosCenter == 3);
}

TEST(stage_enter_move_leave_state) {
  SceneStage s;
  s.enter("a", "a", kPosFarLeft, 0);
  s.enter("b", "b", kPosCenter, 400);
  CHECK(s.wide());
  CHECK_EQ(s.portraits().size(), size_t(2));
  CHECK(s.portraits()[1].alpha == 0 && s.portraits()[0].alpha == 1);
  s.update(0.2);
  CHECK(s.portraits()[1].alpha > 0.4f && s.portraits()[1].alpha < 0.6f);
  CHECK(s.animating());
  s.finishNow();
  CHECK(s.portraits()[1].alpha == 1 && !s.animating());
  // A move slides: halfway it is between the two slots, and the portrait standing there leaves.
  s.move("a", kPosCenter, 1000);
  CHECK_EQ(s.portraits().size(), size_t(1));  // b stood in the target slot
  s.update(0.5);
  CHECK(s.portraits()[0].slotNow() > 0.5f && s.portraits()[0].slotNow() < 2.5f);
  s.finishNow();
  CHECK(s.portraits()[0].slotNow() == 3.0f && s.portraits()[0].position == kPosCenter);
  CHECK(!s.wide());  // nothing out in the far slots any more: the classic layout
  // A moving a portrait that is not on stage does nothing; a fading leave removes it when done.
  s.move("nobody", kPosLeft, 100);
  CHECK_EQ(s.portraits().size(), size_t(1));
  s.leave("a", 200);
  CHECK(s.portraits()[0].leaving);
  s.update(0.3);
  CHECK(s.portraits().empty());
  // A cut is at once, and skipping completes a fade-out.
  s.enter("c", "c", kPosLeft, 0);
  s.leave("c", 0);
  CHECK(s.portraits().empty());
  s.enter("d", "d", kPosRight, 0);
  s.leave("d", 500);
  s.finishNow();
  CHECK(s.portraits().empty());
}

namespace {

bool overlapOk(const std::vector<StageItem>& items, const std::vector<StagePlace>& pl, float maxOverlap) {
  std::vector<size_t> order;
  for (size_t i = 0; i < items.size(); ++i)
    if (pl[i].shown) order.push_back(i);
  std::sort(order.begin(), order.end(), [&](size_t a, size_t b) { return items[a].slot < items[b].slot; });
  for (size_t k = 0; k + 1 < order.size(); ++k) {
    const StagePlace &a = pl[order[k]], &b = pl[order[k + 1]];
    const int overlap = a.x + a.w - b.x;
    if (overlap > (int)(maxOverlap * (float)std::max(a.w, b.w)) + 1) return false;
  }
  return true;
}

std::vector<StageItem> fullStage() {
  std::vector<StageItem> items;
  for (int s = 0; s < 7; ++s) {
    StageItem it;
    it.slot = (float)s;
    it.artW = it.artH = 48;
    it.lit = s == 3;
    items.push_back(it);
  }
  return items;
}

}  // namespace

TEST(stage_layout_classic_is_exactly_the_old_one) {
  // left against the edge, right against the other, center in the middle, 3x on 480 x 270 with 48 px faces.
  std::vector<StageItem> items(3);
  const float slots[3] = {1, 3, 5};
  for (int i = 0; i < 3; ++i) {
    items[(size_t)i].slot = slots[i];
    items[(size_t)i].artW = items[(size_t)i].artH = 48;
  }
  const std::vector<StagePlace> pl = layoutStage(480, 270, 200, false, items);
  CHECK(pl[0].shown && pl[0].w == 144 && pl[0].h == 144 && pl[0].x == 8);
  CHECK_EQ(pl[1].x, (480 - 144) / 2);
  CHECK_EQ(pl[2].x, 480 - 144 - 8);
  // The phone: 270 x 480; classic never hides anyone.
  const std::vector<StagePlace> ph = layoutStage(270, 480, 360, false, items);
  CHECK(ph[0].shown && ph[1].shown && ph[2].shown);
}

TEST(stage_layout_wide_never_overlaps_badly_on_a_desktop_screen) {
  const std::vector<StageItem> items = fullStage();
  const std::vector<StagePlace> pl = layoutStage(480, 270, 200, true, items);
  for (const StagePlace& p : pl) CHECK(p.shown && p.w >= 24 && p.x >= 0 && p.x + p.w <= 480);
  CHECK(overlapOk(items, pl, 0.40f));
  // fewer on stage = bigger
  std::vector<StageItem> two = {items[0], items[6]};
  const std::vector<StagePlace> p2 = layoutStage(480, 270, 200, true, two);
  CHECK(p2[0].w > pl[0].w);
  CHECK(overlapOk(two, p2, 0.40f));
  // a bigger screen
  const std::vector<StagePlace> big = layoutStage(1280, 720, 560, true, items);
  CHECK(overlapOk(items, big, 0.40f));
  for (const StagePlace& p : big) CHECK(p.x >= 0 && p.x + p.w <= 1280 && p.h <= 720 * 55 / 100);
}

TEST(stage_layout_phone_shows_the_speaker_and_its_neighbours) {
  std::vector<StageItem> items = fullStage();
  const std::vector<StagePlace> pl = layoutStage(270, 480, 360, true, items);
  int shown = 0;
  for (size_t i = 0; i < pl.size(); ++i) shown += pl[i].shown ? 1 : 0;
  CHECK_EQ(shown, 3);
  CHECK(pl[2].shown && pl[3].shown && pl[4].shown);  // the speaker (slot 3) and its neighbours
  CHECK(overlapOk(items, pl, 0.40f));
  for (const StagePlace& p : pl)
    if (p.shown) CHECK(p.x >= 0 && p.x + p.w <= 270);
  // the speaker at the far end: it and the two next to it
  for (StageItem& it : items) it.lit = it.slot == 0;
  const std::vector<size_t> vis = phoneVisible(items);
  CHECK(vis.size() == 3 && vis[0] == 0 && vis[2] == 2);
  for (StageItem& it : items) it.lit = it.slot == 6;
  const std::vector<size_t> vis2 = phoneVisible(items);
  CHECK(vis2.size() == 3 && vis2[0] == 4 && vis2[2] == 6);
  // nobody speaking: the middle three
  for (StageItem& it : items) it.lit = false;
  const std::vector<size_t> vis3 = phoneVisible(items);
  CHECK(vis3.size() == 3 && vis3[0] == 2 && vis3[2] == 4);
  // two portraits fit as they are (edges), one stays where its slot is
  std::vector<StageItem> two = {items[1], items[5]};
  const std::vector<StagePlace> p2 = layoutStage(270, 480, 360, true, two);
  CHECK(p2[0].shown && p2[1].shown && p2[0].x < p2[1].x);
  CHECK(overlapOk(two, p2, 0.40f));
}

// ---- the camera's "speed" (docs/EVENTS.md "The camera") ----

TEST(focus_speed_loads_is_refused_and_rides_on_the_entry) {
  Scene w;
  CHECK_STR(w.err(R"([{"focus":{"x":3,"y":1},"speed":4},{"focus":"eirik","speed":0.5,"wait":false},{"focus":"gorm","speed":100}])"), std::string());
  CHECK(has(w.err(R"([{"focus":"eirik","speed":2,"ms":500}])"), "give \"ms\" (how long the scroll takes) or \"speed\""));
  CHECK(has(w.err(R"([{"focus":"eirik","speed":0}])"), "\"speed\" is squares per second, from 0.1 to 100"));
  CHECK(has(w.err(R"([{"focus":"eirik","speed":101}])"), "\"speed\" is squares per second"));
  CHECK(has(w.err(R"([{"focus":"eirik","speed":"fast"}])"), "\"speed\" is squares per second"));
  CHECK(w.play(R"([{"id":"a","when":{"battle":"start"},"do":[{"focus":{"x":4,"y":1},"speed":2.5},{"focus":{"x":5,"y":1},"ms":700},{"focus":{"x":6,"y":1}}]}])"));
  std::vector<const Event*> f;
  for (const Event& e : w.events)
    if (e.type == EventType::Focus) f.push_back(&e);
  CHECK_EQ(f.size(), size_t(3));
  if (f.size() == 3) {
    CHECK(f[0]->c == -1 && f[0]->data.size() == 1 && f[0]->data[0] == 2500);  // squares per second x 1000; no ms
    CHECK(f[1]->c == 700 && f[1]->data.empty());                              // unchanged
    CHECK(f[2]->c == -1 && f[2]->data.empty());                               // a plain focus is exactly what it was
  }
}
