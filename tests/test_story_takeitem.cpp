// The takeItem story command (docs/EVENTS.md "Taking an item away"): one copy / every copy, the equipped weapon,
// a unit that carries none, giving it back, the loader's refusals, save / load, and a campaign scene whose effect
// is still there in the next chapter.
#include "campaign_run.h"
#include "save_helpers.h"
#include "story_fixture.h"

using namespace ote;
using namespace story;
using namespace fixture::items;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }
std::vector<std::string> field() { return {".........", ".........", "........."}; }
std::string startEvent(const std::string& commands) {
  return "[{\"id\":\"e\",\"when\":{\"battle\":\"start\"},\"do\":" + commands + "}]";
}
struct ItemWorld : StoryWorld {
  ItemWorld()
      : StoryWorld(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 0, 1, 1, false}, {"soldier", 1, 8, 1, false}},
                   {"eirik", "lyra", "gorm"}) {
    fixture::addItems(*this);
  }
};
int count(const Battle& b, int unit, int item) {
  int n = 0;
  for (const ItemStack& s : b.units[(size_t)unit].items) n += s.item == item;
  return n;
}

}  // namespace

TEST(takeitem_one_copy_leaves_the_others) {
  ItemWorld w;
  fixture::carry(w, 0, {Potion, Ring, Potion});
  CHECK(w.play(startEvent(R"([{"takeItem":"potion","from":"eirik"}])")));
  CHECK_EQ(count(w.battle, 0, Potion), 1);
  CHECK_EQ(count(w.battle, 0, Ring), 1);
  CHECK(w.battle.units[0].items[0].item == Ring);  // the FIRST potion went; order of the rest is kept
}
TEST(takeitem_all_copies_from_a_side_and_none_carried_is_silent) {
  ItemWorld w;
  fixture::carry(w, 0, {Potion, Ring, Potion});
  fixture::carry(w, 2, {Potion});
  CHECK(w.play(startEvent(R"([{"takeItem":"potion","from":{"side":"blue"},"all":true}])")));
  CHECK_EQ(count(w.battle, 0, Potion), 0);
  CHECK_EQ(count(w.battle, 0, Ring), 1);
  CHECK_EQ(w.battle.units[1].items.size(), (size_t)0);  // lyra carried none: nothing happens
  CHECK_EQ(count(w.battle, 2, Potion), 1);              // the red unit is not matched
}
TEST(takeitem_the_equipped_weapon_falls_back_to_the_next_one) {
  ItemWorld w;
  w.db.rules.items.carriedWeapons = true;
  w.db.weapons[1].group = "bow";
  w.db.unitTypes[0].wields = {"bow"};  // a soldier fights with its sword item, or else with a bow
  fixture::carry(w, 0, {SwordItem, BowItem});
  std::string err;
  CHECK(w.play(startEvent(R"([{"takeItem":"sword_item","from":"eirik"}])")));
  const Battle& b = w.battle;
  CHECK_EQ(b.equippedSlot(b.units[0]), 0);
  CHECK(b.units[0].items[0].item == BowItem);  // the bow is now what it fights with
  ItemWorld v;
  v.db.rules.items.carriedWeapons = true;
  fixture::carry(v, 0, {SwordItem});
  CHECK(v.play(startEvent(R"([{"takeItem":"sword_item","from":"eirik"}])")));
  CHECK_EQ(v.battle.equippedSlot(v.battle.units[0]), -1);  // unarmed
}
TEST(takeitem_then_give_it_back_later) {
  ItemWorld w;
  fixture::carry(w, 0, {Potion});
  CHECK(w.play(R"([{"id":"a","when":{"battle":"start"},"do":[{"takeItem":"potion","from":"eirik"}]},
                   {"id":"b","when":{"turn":2},"do":[{"giveItem":"potion","to":"eirik"}]}])"));
  CHECK_EQ(count(w.battle, 0, Potion), 0);
  CHECK(w.act(Action::endTurn()) && w.act(Action::endTurn()));
  CHECK_EQ(count(w.battle, 0, Potion), 1);
}
TEST(takeitem_runs_in_a_defeated_event) {
  ItemWorld w;
  fixture::carry(w, 0, {Ring});
  CHECK(w.play(R"([{"id":"d","when":{"defeated":"gorm"},"do":[{"takeItem":"ring","from":"eirik"}]}])"));
  CHECK_EQ(count(w.battle, 0, Ring), 1);  // gorm still stands
}
TEST(takeitem_stays_gone_through_a_save) {
  ItemWorld w;
  fixture::carry(w, 0, {Potion, Potion});
  CHECK(w.play(startEvent(R"([{"takeItem":"potion","from":"eirik","all":true}])")));
  Json saved;
  saveBattleState(w.battle, saved);
  Battle r;
  std::string err;
  CHECK(r.init(w.db, w.map, w.formulas, 1, err));
  CHECK(restoreBattleState(r, saved, err));
  CHECK_EQ(count(r, 0, Potion), 0);
}
TEST(takeitem_loader_refusals) {
  auto err = [&](const char* cmd) {
    ItemWorld w;
    return w.load(startEvent(std::string("[") + cmd + "]"));
  };
  CHECK(has(err(R"({"takeItem":"nothing","from":"eirik"})"), "\"takeItem\" names unknown item \"nothing\""));
  CHECK(has(err(R"({"takeItem":"potion"})"), "missing required \"from\""));
  CHECK(has(err(R"({"takeItem":"potion","from":"nobody"})"), "unknown unit \"nobody\""));
  CHECK(has(err(R"({"takeItem":"potion","from":"eirik","all":"yes"})"), "\"all\""));
  CHECK(has(err(R"({"takeItem":"potion","from":"eirik","to":"lyra"})"), "unknown key"));
  CHECK_STR(err(R"({"takeItem":"potion","from":{"side":"red"},"all":true})"), "");
}

// ---- campaign scenes ----

TEST(takeitem_in_a_scene_is_still_gone_in_the_next_chapter) {
  const char* campaign = R"({
    "name": "Take", "rules": { "fallen": "gone", "onDefeat": "retry" },
    "chapters": [
      { "id": "ch1", "name": "One", "map": "ford", "after": [ {"takeItem":"potion","from":"eirik"}, {"takeItem":"ring","from":"nobody"} ] },
      { "id": "ch2", "name": "Two", "map": "mill" } ] })";
  camptest::Run r;
  CHECK(r.begin("camp_takeitem", campaign) && r.startBattle());
  CHECK(r.winAndFinish() == ChapterResult::Won);
  const int potion = r.g.db.findItem("potion"), ring = r.g.db.findItem("ring");
  auto has_item = [&](const RosterUnit* u, int item) {
    for (const ItemGrant& g : u->items)
      if (g.item == item) return true;
    return false;
  };
  CHECK(r.scene(SceneKind::After));
  const RosterUnit* u = r.pt.findUnit("eirik");
  CHECK(u && !has_item(u, potion) && has_item(u, ring));  // the potion is gone, the ring stays; "nobody" is not an error
  CHECK(r.startBattle(2));
  CHECK_EQ(count(r.b, r.unit("eirik"), potion), 0);
  CHECK_EQ(count(r.b, r.unit("eirik"), ring), 1);
}
TEST(takeitem_in_scenes_all_copies_and_loader_refusals) {
  camptest::Game g;
  auto bad = [&](const char* cmd) {
    return g.loadError("camp_takeitem_bad", std::string(R"({"chapters":[{"id":"c","name":"C","map":"ford","before":[)") + cmd + "]}]}");
  };
  CHECK(has(bad(R"({"takeItem":"nothing","from":"eirik"})"), "unknown item"));
  CHECK(has(bad(R"({"takeItem":"potion"})"), "missing required \"from\""));
  CHECK(has(bad(R"({"takeItem":"potion","from":"bad id"})"), "roster unit id"));
  CHECK_STR(bad(R"({"takeItem":"potion","from":"eirik","all":true})"), "");
}
