// The storeItems story command (docs/EVENTS.md "Sending a unit's items to storage"): in a battle (all, kept ones, the equipped weapon,
// computer units, a save round trip, no storage rule) and in a campaign scene (the roster's items, still there in the next chapter), and the
// loader's refusals.
#include "campaign_run.h"
#include "save_helpers.h"
#include "story_fixture.h"

using namespace ote;
using namespace story;
using namespace fixture::items;

namespace {

bool has(const std::string& hay, const char* needle) { return hay.find(needle) != std::string::npos; }
std::vector<std::string> field() { return {".........", ".........", "........."}; }
std::string startEvent(const std::string& commands) { return "[{\"id\":\"e\",\"when\":{\"battle\":\"start\"},\"do\":" + commands + "}]"; }

struct StoreWorld : StoryWorld {
  StoreWorld()
      : StoryWorld(field(), {{"soldier", 0, 0, 1, false}, {"soldier", 0, 1, 1, false}, {"soldier", 1, 8, 1, false}}, {"eirik", "lyra", "gorm"}) {
    fixture::addItems(*this);
    db.rules.items.storage = ItemStorage::Anywhere;
  }
  int stored(int item) const {
    int n = 0;
    for (const ItemStack& s : battle.sides[0].storage) n += s.item == item;
    return n;
  }
  int carried(int unit, int item) const {
    int n = 0;
    for (const ItemStack& s : battle.units[(size_t)unit].items) n += s.item == item;
    return n;
  }
};

}  // namespace

// ---- storeItems ----

TEST(storeitems_moves_everything_and_says_so) {
  StoreWorld w;
  fixture::carry(w, 0, {Potion, Ring, Potion});
  CHECK(w.play(startEvent(R"([{"storeItems":"eirik"}])")));
  CHECK_EQ(w.battle.units[0].items.size(), (size_t)0);
  CHECK_EQ(w.battle.sides[0].storage.size(), (size_t)3);
  CHECK_EQ(w.stored(Potion), 2);
  CHECK_EQ(w.count(EventType::ItemMoved), 3);  // the same entry as the Store action: unit -> storage (-1)
  const Event* e = w.last(EventType::ItemMoved);
  CHECK(e && e->a == 0 && e->b == -1);
}
TEST(storeitems_keep_leaves_those_items_and_an_empty_pack_is_silent) {
  StoreWorld w;
  fixture::carry(w, 0, {Potion, Ring, Potion});
  CHECK(w.play(startEvent(R"([{"storeItems":{"side":"blue"},"keep":["ring"]}])")));
  CHECK_EQ(w.carried(0, Ring), 1);
  CHECK_EQ(w.carried(0, Potion), 0);
  CHECK_EQ(w.stored(Potion), 2);
  CHECK_EQ(w.stored(Ring), 0);
  CHECK_EQ(w.battle.units[1].items.size(), (size_t)0);  // lyra had nothing
}
TEST(storeitems_the_equipped_weapon_goes_too_and_the_unit_falls_back) {
  StoreWorld w;
  w.db.rules.items.carriedWeapons = true;
  w.db.weapons[1].group = "bow";
  w.db.unitTypes[0].wields = {"bow"};
  fixture::carry(w, 0, {SwordItem, BowItem});
  CHECK(w.play(startEvent(R"([{"storeItems":"eirik","keep":["bow_item"]}])")));
  CHECK_EQ(w.stored(SwordItem), 1);
  CHECK(w.battle.units[0].items.size() == 1 && w.battle.units[0].items[0].item == BowItem);
  CHECK_EQ(w.battle.equippedSlot(w.battle.units[0]), 0);  // the kept bow is what it fights with now
  StoreWorld v;
  v.db.rules.items.carriedWeapons = true;
  fixture::carry(v, 0, {SwordItem});
  CHECK(v.play(startEvent(R"([{"storeItems":"eirik"}])")));
  CHECK_EQ(v.battle.equippedSlot(v.battle.units[0]), -1);  // unarmed
  CHECK_EQ(v.stored(SwordItem), 1);
}
TEST(storeitems_leaves_computer_units_alone_and_clears_the_drop_mark) {
  StoreWorld w;
  fixture::carry(w, 0, {Potion}, 0);  // marked to drop
  fixture::carry(w, 2, {Potion});
  CHECK(w.play(startEvent(R"([{"storeItems":{}}])")));
  CHECK_EQ(w.carried(2, Potion), 1);  // gorm is a computer unit: untouched
  CHECK_EQ(w.stored(Potion), 1);
  CHECK(!w.battle.sides[0].storage[0].drop);
}
TEST(storeitems_survives_a_save) {
  StoreWorld w;
  fixture::carry(w, 0, {Potion, Ring});
  CHECK(w.play(startEvent(R"([{"storeItems":"eirik"}])")));
  Json saved;
  saveBattleState(w.battle, saved);
  Battle r;
  std::string err;
  CHECK(r.init(w.db, w.map, w.formulas, 1, err));
  CHECK(restoreBattleState(r, saved, err));
  CHECK_EQ(r.units[0].items.size(), (size_t)0);
  CHECK_EQ(r.sides[0].storage.size(), (size_t)2);
}
TEST(storeitems_loader_refusals) {
  auto err = [&](const char* cmd) { StoreWorld w; return w.load(startEvent(std::string("[") + cmd + "]")); };
  CHECK(has(err(R"({"storeItems":"nobody"})"), "unknown unit \"nobody\""));
  CHECK(has(err(R"({"storeItems":"eirik","keep":["nothing"]})"), "\"keep\" names unknown item \"nothing\""));
  CHECK(has(err(R"({"storeItems":"eirik","keep":"ring"})"), "\"keep\" must be a list of item ids"));
  CHECK(has(err(R"({"storeItems":"eirik","keep":[3]})"), "unknown item"));
  CHECK(has(err(R"({"storeItems":"eirik","all":true})"), "unknown key"));
  CHECK(has(err(R"({"storeItems":"eirik","takeItem":"potion","from":"eirik"})"), "has two commands"));
  CHECK_STR(err(R"({"storeItems":{"side":"red"},"keep":[]})"), "");
  // A game without a shared storage cannot store anything: refused when the game loads, in words.
  StoreWorld none;
  none.db.rules.items.storage = ItemStorage::Off;
  CHECK(has(none.load(startEvent(R"([{"storeItems":"eirik"}])")), "\"storeItems\" needs a shared storage"));
}

// ---- campaign scenes ----

TEST(storeitems_in_a_scene_moves_the_roster_units_items_to_the_campaign_storage) {
  const char* campaign = R"({
    "name": "Store", "rules": { "fallen": "gone", "onDefeat": "retry" },
    "chapters": [
      { "id": "ch1", "name": "One", "map": "ford", "after": [ {"storeItems":"eirik","keep":["ring"]}, {"storeItems":"nobody"} ] },
      { "id": "ch2", "name": "Two", "map": "mill" } ] })";
  camptest::Run r;
  CHECK(r.begin("camp_storeitems", campaign) && r.startBattle());
  CHECK(r.winAndFinish() == ChapterResult::Won);
  const int potion = r.g.db.findItem("potion"), ring = r.g.db.findItem("ring");
  CHECK(r.scene(SceneKind::After));
  const RosterUnit* u = r.pt.findUnit("eirik");
  CHECK(u != nullptr);
  if (!u) return;
  CHECK(u->items.size() == 1 && u->items[0].item == ring);  // the ring was kept, the potion went (the unknown "nobody" is no error)
  int inStorage = 0;
  for (const CarriedSide& s : r.pt.sides)
    if (s.id == "blue")
      for (const ItemGrant& g : s.storage) inStorage += g.item == potion;
  CHECK(inStorage >= 1);
  CHECK(r.startBattle(2));
  int atStart = 0;
  for (const ItemStack& s : r.b.sides[0].storage) atStart += s.item == potion;
  CHECK(atStart >= 1);  // still there in the next chapter
}
TEST(storeitems_in_scenes_loader_refusals) {
  camptest::Game g;
  auto bad = [&](const char* cmd) {
    return g.loadError("camp_storeitems_bad", std::string(R"({"chapters":[{"id":"c","name":"C","map":"ford","before":[)") + cmd + "]}]}");
  };
  CHECK(has(bad(R"({"storeItems":"eirik","keep":["nothing"]})"), "\"keep\" names unknown item"));
  CHECK(has(bad(R"({"storeItems":"bad id"})"), "roster unit id"));
  CHECK(has(bad(R"({"storeItems":{"side":"blue"}})"), "roster unit id"));
  CHECK(has(bad(R"({"storeItems":"eirik","all":true})"), "unknown key"));
  CHECK_STR(bad(R"({"storeItems":"eirik","keep":["ring"]})"), "");
}
