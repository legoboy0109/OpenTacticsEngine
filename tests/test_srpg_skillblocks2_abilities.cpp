// Docs/SRPG.md "Skill and ability blocks, second set": the ability effects steal, refresh, movePast and cure. Fixture
// tests/fixtures/srpg-skillblocks2: "thief" (blue rogue, 0,1) stands next to "mark" (red guard, 1,1: iron sword, potion, relic); "medic"
// (0,0) and "pal" (0,2) are blue allies; "far" (red guard) is out at 7,2.
#include "core/battle/save.h"
#include "srpg_skills_fixture.h"

using namespace ote;
using skilltest::Copy;
using skilltest::World;
using skilltest::saysIn;

namespace {

const char* kFix = "srpg-skillblocks2";

ItemStack stack(World& w, const char* item) {
  ItemStack s;
  s.item = w.db().findItem(item);
  s.uses = w.db().items[(size_t)s.item].uses;
  return s;
}

std::string itemIds(World& w, const Unit& u) {
  std::string out;
  for (const ItemStack& s : u.items) out += (out.empty() ? "" : " ") + w.db().items[(size_t)s.item].id;
  return out;
}

Action steal(World& w, const char* ability, const char* from, const char* foe, int slot) {
  Action a = Action::useAbility(w.id(from), w.db().findAbility(ability), w.id(foe));
  a.slot = slot;
  return a;
}

}  // namespace

// ---- 1. steal ----

TEST(steal_takes_the_chosen_item_into_the_pack) {
  World w("field", ote_test::sourceDir() + "/tests/fixtures/srpg-skillblocks2");
  CHECK_STR(itemIds(w, w.unit("mark")), "iron_sword potion relic");
  CHECK(w.apply(steal(w, "steal", "thief", "mark", 1)));
  CHECK_STR(itemIds(w, w.unit("mark")), "iron_sword relic");
  CHECK_STR(itemIds(w, w.unit("thief")), "iron_sword potion");
  CHECK(w.unit("thief").done);  // an ability's cost is "action": it ends the turn
  CHECK_EQ(w.count(EventType::ItemStolen), 1);
  const Event& e = w.events[1];  // AbilityUsed, ItemStolen, ItemGained
  CHECK(e.type == EventType::ItemStolen);
  CHECK_EQ(e.a, w.id("thief"));
  CHECK_EQ(e.b, w.id("mark"));
  CHECK_EQ(e.c, w.db().findItem("potion"));
  CHECK_EQ(w.count(EventType::ItemGained), 1);
}

TEST(steal_the_menu_and_the_refusals_have_plain_reasons) {
  World w("field", ote_test::sourceDir() + "/tests/fixtures/srpg-skillblocks2");
  std::vector<StealOption> o;
  w.b().stealOptions(w.id("thief"), w.db().findAbility("steal"), w.unit("thief").cell, w.id("mark"), o);
  CHECK_EQ(o.size(), (size_t)3);
  CHECK(!o[0].ok);
  CHECK_STR(o[0].why, "weapons cannot be stolen");  // weapons are off by default; the sword is a weapon
  CHECK_STR(w.refusal(steal(w, "steal", "thief", "mark", 0)), "that item weapons cannot be stolen");
  CHECK(o[1].ok);
  CHECK(!o[2].ok);
  CHECK_STR(o[2].why, "cannot be stolen");  // the relic is marked "stealable": false
  CHECK_STR(w.refusal(steal(w, "steal", "thief", "mark", 2)), "that item cannot be stolen");
  CHECK_STR(w.refusal(steal(w, "steal", "thief", "mark", -1)), "choose an item to steal");
  CHECK_STR(w.refusal(steal(w, "steal", "thief", "mark", 9)), "choose an item to steal");
}

TEST(steal_weapons_and_the_equipped_weapon_are_separate_options) {
  World w("field", ote_test::sourceDir() + "/tests/fixtures/srpg-skillblocks2");
  w.unit("mark").items = {stack(w, "iron_sword"), stack(w, "steel_sword"), stack(w, "potion")};
  // "weapons": true, "equipped": false: the spare weapon, not the one in hand.
  CHECK_STR(w.refusal(steal(w, "steal_arms", "thief", "mark", 0)), "that item the equipped weapon");
  CHECK(w.apply(steal(w, "steal_arms", "thief", "mark", 1)));
  CHECK_STR(itemIds(w, w.unit("mark")), "iron_sword potion");
  CHECK_STR(itemIds(w, w.unit("thief")), "iron_sword steel_sword");
}

TEST(steal_the_equipped_weapon_leaves_the_next_one_in_hand) {
  World w("field", ote_test::sourceDir() + "/tests/fixtures/srpg-skillblocks2");
  w.unit("mark").items = {stack(w, "iron_sword"), stack(w, "steel_sword"), stack(w, "potion")};
  const int before = w.b().equippedSlot(w.unit("mark"));
  CHECK_EQ(before, 0);
  CHECK(w.apply(steal(w, "steal_all", "thief", "mark", 0)));
  CHECK_STR(itemIds(w, w.unit("mark")), "steel_sword potion");
  const int slot = w.b().equippedSlot(w.unit("mark"));
  CHECK_EQ(slot, 0);  // the fallback of every removed item: the first weapon it can still wield
  CHECK_EQ(w.unit("mark").items[(size_t)slot].item, w.db().findItem("steel_sword"));
}

TEST(steal_the_makers_formula_decides_whether_it_can_be_tried) {
  World w("field", ote_test::sourceDir() + "/tests/fixtures/srpg-skillblocks2");
  const int ab = w.db().findAbility("steal_fast");
  CHECK(w.b().validate(steal(w, "steal_fast", "thief", "mark", 1)));  // spd 8 against 3
  w.unit("thief").stats[(size_t)w.db().findStat("spd")] = 3;
  CHECK_STR(w.refusal(steal(w, "steal_fast", "thief", "mark", 1)), "cannot steal from that unit");
  std::vector<AbilityOption> menu;
  w.b().abilityOptions(w.id("thief"), w.unit("thief").cell, menu);
  bool found = false;
  for (const AbilityOption& m : menu)
    if (m.ability == ab) {
      found = true;
      CHECK(!m.ok);
    }
  CHECK(found);
}

TEST(steal_a_full_pack_follows_the_chest_rule) {
  {
    World w("field", ote_test::sourceDir() + "/tests/fixtures/srpg-skillblocks2");
    w.unit("thief").items = {stack(w, "iron_sword"), stack(w, "potion"), stack(w, "amulet")};  // inventorySize 3
    CHECK_STR(w.refusal(steal(w, "steal", "thief", "mark", 1)), "the unit cannot carry more: drop something first");
    std::vector<AbilityOption> menu;
    w.b().abilityOptions(w.id("thief"), w.unit("thief").cell, menu);
    for (const AbilityOption& m : menu)
      if (m.ability == w.db().findAbility("steal")) CHECK_STR(m.why, "the unit cannot carry more: drop something first");
  }
  {
    Copy c("sb2_steal_storage", kFix);
    c.swap("game.json", "\"inventorySize\": 3", "\"inventorySize\": 3, \"storage\": \"anywhere\"");
    World w("field", c.dir);
    w.unit("thief").items = {stack(w, "iron_sword"), stack(w, "potion"), stack(w, "amulet")};
    CHECK(w.apply(steal(w, "steal", "thief", "mark", 1)));  // the overflow goes to the side's storage, as a chest's does
    CHECK_EQ(w.b().sides[(size_t)w.unit("thief").side].storage.size(), (size_t)1);
    CHECK_STR(itemIds(w, w.unit("mark")), "iron_sword relic");
  }
}

TEST(steal_what_a_computer_unit_takes_it_drops_when_defeated) {
  World w("field", ote_test::sourceDir() + "/tests/fixtures/srpg-skillblocks2");
  Battle& b = w.b();
  // The computer side's turn: make "mark" the thief by giving it the ability through its type (a copy of the fixture would do too).
  b.units[(size_t)w.id("mark")].type = w.db().findUnitType("rogue");
  b.units[(size_t)w.id("mark")].items = {stack(w, "iron_sword")};
  b.units[(size_t)w.id("thief")].items = {stack(w, "iron_sword"), stack(w, "potion")};
  CHECK(w.apply(Action::endTurn()));  // red's turn
  CHECK(w.apply(steal(w, "steal", "mark", "thief", 1)));
  CHECK(w.unit("mark").items.back().drop);
}

TEST(steal_loader_refusals) {
  auto say = [](const std::string& ability) {
    Copy c("sb2_steal_bad", kFix);
    c.put("data/abilities.json", "{ \"abilities\": [" + ability + "] }");
    // the units name abilities that no longer exist: point them at nothing
    c.swap("data/units.json", "\"abilities\": [\"steal\", \"steal_fast\", \"steal_arms\", \"steal_all\", \"refresh\", \"refresh_two\", \"leap\", \"purge\", \"purge_self\"]", "\"abilities\": []");
    c.swap("data/units.json", "\"abilities\": [\"refresh\", \"refresh_two\", \"purge\"]", "\"abilities\": []");
    return c.gameError();
  };
  const std::string head = "{ \"id\": \"x\", \"target\": \"";
  CHECK(saysIn(say(head + "ally\", \"effect\": { \"steal\": true } }"), "\"steal\" needs \"target\": \"enemy\""));
  CHECK(saysIn(say(head + "enemy\", \"range\": [1, 2], \"effect\": { \"steal\": true } }"), "\"range\" must be [1, 1]"));
  CHECK(saysIn(say(head + "enemy\", \"effect\": { \"steal\": { \"weapons\": 1 } } }"), "\"weapons\""));
  CHECK(saysIn(say(head + "enemy\", \"effect\": { \"steal\": { \"equipped\": true } } }"), "\"equipped\": true needs \"weapons\": true"));
  CHECK(saysIn(say(head + "enemy\", \"effect\": { \"steal\": { \"when\": 1 } } }"), "unknown key \"when\""));
  CHECK(saysIn(say(head + "enemy\", \"effect\": { \"steal\": { \"if\": 3 } } }"), "\"if\" must be the text of a formula"));
  CHECK(saysIn(say(head + "enemy\", \"effect\": { \"steal\": 3 } }"), "\"steal\": must be true or"));
  Copy c("sb2_stealable", kFix);
  c.swap("data/items.json", "\"stealable\": false", "\"stealable\": \"no\"");
  CHECK(saysIn(c.gameError(), "\"stealable\" must be true or false"));
}

// ---- 2. refresh an ally ----

TEST(refresh_lets_an_ally_that_has_acted_act_again) {
  World w("field", ote_test::sourceDir() + "/tests/fixtures/srpg-skillblocks2");
  Battle& b = w.b();
  const Action refresh = Action::useAbility(w.id("medic"), w.db().findAbility("refresh"), w.id("thief"));
  CHECK_STR(w.refusal(refresh), "that unit has not acted yet");
  CHECK(w.apply(Action::wait(w.id("thief"))));
  CHECK(w.unit("thief").done);
  CHECK(w.apply(refresh));
  CHECK(!w.unit("thief").done);
  CHECK(!w.unit("thief").moved);
  CHECK(w.unit("medic").done);  // the cost is "action"
  CHECK_EQ(w.count(EventType::UnitRefreshed), 1);
  CHECK(b.canAct(w.id("thief")));
  CHECK(w.apply(Action::wait(w.id("thief"))));  // it acts again
  CHECK(w.unit("thief").done);
  // A second refresh of the same unit this turn: refused (once per turn is the default).
  CHECK_STR(w.refusal(Action::useAbility(w.id("pal"), w.db().findAbility("refresh"), w.id("thief"))), "that unit was already refreshed this turn");
  // The next turn of its side starts over.
  CHECK(w.apply(Action::endTurn()));
  CHECK(w.apply(Action::endTurn()));
  CHECK(w.unit("thief").refreshedBy.empty());
  CHECK(w.b().validate(Action::useAbility(w.id("pal"), w.db().findAbility("refresh"), w.id("thief"))) == false);  // it has not acted this turn
}

TEST(refresh_a_unit_that_refreshed_you_cannot_be_refreshed_back_this_turn) {
  World w("field", ote_test::sourceDir() + "/tests/fixtures/srpg-skillblocks2");
  CHECK(w.apply(Action::wait(w.id("pal"))));
  CHECK(w.apply(Action::useAbility(w.id("thief"), w.db().findAbility("refresh"), w.id("pal"))));  // thief refreshed pal; thief is done
  // pal tries to refresh thief (done, never refreshed): refused because thief refreshed pal this turn.
  CHECK_STR(w.refusal(Action::useAbility(w.id("pal"), w.db().findAbility("refresh"), w.id("thief"))), "that unit refreshed you this turn");
  // Another unit may refresh the thief (its own count is zero).
  CHECK(w.apply(Action::wait(w.id("medic"))));
  CHECK(w.unit("medic").done);
}

TEST(refresh_per_turn_option_allows_more) {
  World w("field", ote_test::sourceDir() + "/tests/fixtures/srpg-skillblocks2");
  const int two = w.db().findAbility("refresh_two");
  CHECK(w.apply(Action::wait(w.id("thief"))));
  CHECK(w.apply(Action::useAbility(w.id("pal"), two, w.id("thief"))));
  CHECK(w.apply(Action::wait(w.id("thief"))));
  CHECK(w.b().validate(Action::useAbility(w.id("medic"), two, w.id("thief"))));  // a second one this turn: perTurn 2
  CHECK(w.apply(Action::useAbility(w.id("medic"), two, w.id("thief"))));
  CHECK(w.apply(Action::wait(w.id("thief"))));
  CHECK_EQ(w.unit("thief").refreshedBy.size(), (size_t)2);
}

TEST(refresh_a_unit_that_cannot_act_is_not_refreshed) {
  World w("field", ote_test::sourceDir() + "/tests/fixtures/srpg-skillblocks2");
  ConditionState st;
  st.condition = w.db().findCondition("stun");
  st.left = 2;
  w.unit("pal").conditions.push_back(st);
  w.unit("pal").done = true;
  CHECK_STR(w.refusal(Action::useAbility(w.id("thief"), w.db().findAbility("refresh"), w.id("pal"))), "that unit cannot act");
}

TEST(refresh_state_is_saved_and_old_saves_load) {
  World w("field", ote_test::sourceDir() + "/tests/fixtures/srpg-skillblocks2");
  CHECK(w.apply(Action::wait(w.id("pal"))));
  CHECK(w.apply(Action::useAbility(w.id("thief"), w.db().findAbility("refresh"), w.id("pal"))));
  Json j;
  saveBattleState(w.b(), j);
  CHECK(j.dump(0).find("refreshedBy") != std::string::npos);
  World v("field", ote_test::sourceDir() + "/tests/fixtures/srpg-skillblocks2", false);
  std::string err;
  CHECK(restoreBattleState(v.b(), j, err));
  CHECK_EQ(v.unit("pal").refreshedBy.size(), (size_t)1);
  // The guard still holds after loading.
  CHECK_STR(v.refusal(Action::useAbility(v.id("pal"), v.db().findAbility("refresh"), v.id("thief"))), "that unit refreshed you this turn");
  // A fresh state has no key, and a save without the key (an old save) loads.
  World f("field", ote_test::sourceDir() + "/tests/fixtures/srpg-skillblocks2");
  Json k;
  saveBattleState(f.b(), k);
  CHECK(k.dump(0).find("refreshedBy") == std::string::npos);
  World g("field", ote_test::sourceDir() + "/tests/fixtures/srpg-skillblocks2", false);
  CHECK(restoreBattleState(g.b(), k, err));
  // A game without a refresh ability refuses the key.
  Copy c("sb2_refresh_nokey", kFix);
  c.skills("");
  Json bad = j;
  c.put("data/abilities.json", "{ \"abilities\": [ { \"id\": \"steal\", \"target\": \"enemy\", \"effect\": { \"steal\": true } } ] }");
  c.swap("data/units.json", "\"abilities\": [\"steal\", \"steal_fast\", \"steal_arms\", \"steal_all\", \"refresh\", \"refresh_two\", \"leap\", \"purge\", \"purge_self\"]", "\"abilities\": [\"steal\"]");
  c.swap("data/units.json", "\"abilities\": [\"refresh\", \"refresh_two\", \"purge\"]", "\"abilities\": []");
  World n("field", c.dir, false);
  std::string text = j.dump(0);
  Json parsed;
  std::string perr;
  CHECK(Json::parse(text, parsed, perr));
  CHECK(!restoreBattleState(n.b(), parsed, err));
}

TEST(refresh_loader_refusals) {
  auto say = [](const std::string& ability) {
    Copy c("sb2_refresh_bad", kFix);
    c.put("data/abilities.json", "{ \"abilities\": [" + ability + "] }");
    c.swap("data/units.json", "\"abilities\": [\"steal\", \"steal_fast\", \"steal_arms\", \"steal_all\", \"refresh\", \"refresh_two\", \"leap\", \"purge\", \"purge_self\"]", "\"abilities\": []");
    c.swap("data/units.json", "\"abilities\": [\"refresh\", \"refresh_two\", \"purge\"]", "\"abilities\": []");
    return c.gameError();
  };
  const std::string head = "{ \"id\": \"x\", \"target\": \"";
  CHECK(saysIn(say(head + "enemy\", \"effect\": { \"refresh\": true } }"), "\"refresh\" needs \"target\": \"ally\""));
  CHECK(saysIn(say(head + "ally\", \"effect\": { \"refresh\": { \"perTurn\": 0 } } }"), "\"refresh\" must be true or { \"perTurn\": 1 }"));
  CHECK(saysIn(say(head + "ally\", \"effect\": { \"refresh\": { \"times\": 1 } } }"), "\"refresh\" must be true or"));
  CHECK(say(head + "ally\", \"range\": [1, 2], \"effect\": { \"refresh\": { \"perTurn\": 2 } } }").empty());
}

// ---- 3. move past a neighbour ----

TEST(movepast_puts_the_user_on_the_far_side) {
  World w("field", ote_test::sourceDir() + "/tests/fixtures/srpg-skillblocks2");
  const int leap = w.db().findAbility("leap");
  CHECK(w.apply(Action::useAbility(w.id("thief"), leap, w.id("mark"))));  // an enemy
  CHECK_EQ(w.unit("thief").cell, w.at(2, 1));
  CHECK_EQ(w.unit("mark").cell, w.at(1, 1));  // the other unit stays
  CHECK_EQ(w.count(EventType::UnitRelocated), 1);
  const Event& e = w.events[1];
  CHECK(e.type == EventType::UnitRelocated);
  CHECK_EQ(e.a, w.id("thief"));
  CHECK_EQ(e.b, w.at(0, 1));
  CHECK_EQ(e.c, w.at(2, 1));
  CHECK(w.unit("thief").done);
}

TEST(movepast_an_ally_works_too_and_needs_room_beyond) {
  World w("field", ote_test::sourceDir() + "/tests/fixtures/srpg-skillblocks2");
  const int leap = w.db().findAbility("leap");
  // pal (0,2) and medic (0,0) are above and below thief, not in line with anything free: the far side of medic is off the map.
  CHECK_STR(w.refusal(Action::useAbility(w.id("thief"), leap, w.id("medic"))), "there is no room beyond that unit");
  // Put the guard behind the mark: nothing free beyond it.
  w.unit("far").cell = w.at(2, 1);
  w.b().occupant[(size_t)w.at(7, 2)] = -1;
  w.b().occupant[(size_t)w.at(2, 1)] = w.id("far");
  w.b().refreshSight();
  CHECK_STR(w.refusal(Action::useAbility(w.id("thief"), leap, w.id("mark"))), "there is no room beyond that unit");
}

TEST(movepast_loader_refusals_and_target_kinds) {
  Copy c("sb2_leap_bad", kFix);
  c.put("data/abilities.json", "{ \"abilities\": [ { \"id\": \"leap\", \"target\": \"cell\", \"effect\": { \"movePast\": true } } ] }");
  c.swap("data/units.json", "\"abilities\": [\"steal\", \"steal_fast\", \"steal_arms\", \"steal_all\", \"refresh\", \"refresh_two\", \"leap\", \"purge\", \"purge_self\"]", "\"abilities\": []");
  c.swap("data/units.json", "\"abilities\": [\"refresh\", \"refresh_two\", \"purge\"]", "\"abilities\": []");
  CHECK(saysIn(c.gameError(), "\"movePast\" needs \"target\" \"ally\", \"enemy\" or \"unit\""));
  c.put("data/abilities.json", "{ \"abilities\": [ { \"id\": \"leap\", \"target\": \"ally\", \"range\": [1, 2], \"effect\": { \"movePast\": true } } ] }");
  CHECK(saysIn(c.gameError(), "\"movePast\" moves a neighbour: \"range\" must be [1, 1]"));
  // "enemy" only: an ally is not a target.
  c.put("data/abilities.json", "{ \"abilities\": [ { \"id\": \"leap\", \"target\": \"enemy\", \"effect\": { \"movePast\": true } } ] }");
  CHECK(c.gameError().empty() || saysIn(c.gameError(), "leap"));
}

// ---- 8. cure as an ability ----

TEST(cure_ability_cures_an_ally_that_has_something_to_cure) {
  World w("field", ote_test::sourceDir() + "/tests/fixtures/srpg-skillblocks2");
  const int purge = w.db().findAbility("purge");
  CHECK_STR(w.refusal(Action::useAbility(w.id("medic"), purge, w.id("thief"))), "that unit has nothing to cure");
  ConditionState poison, curse;
  poison.condition = w.db().findCondition("poison");
  poison.left = 3;
  curse.condition = w.db().findCondition("curse");
  curse.left = 3;
  w.unit("thief").conditions = {poison, curse};
  CHECK(w.apply(Action::useAbility(w.id("medic"), purge, w.id("thief"))));
  CHECK(!w.b().hasCondition(w.unit("thief"), poison.condition));
  CHECK(w.b().hasCondition(w.unit("thief"), curse.condition));  // only the listed one
  CHECK_EQ(w.count(EventType::ConditionEnded), 1);
  // A target with only a condition the ability does not list is no target.
  CHECK_STR(w.refusal(Action::useAbility(w.id("pal"), purge, w.id("thief"))), "that unit has nothing to cure");
}

TEST(cure_self_ability_and_all) {
  World w("field", ote_test::sourceDir() + "/tests/fixtures/srpg-skillblocks2");
  const int self = w.db().findAbility("purge_self");
  CHECK_STR(w.refusal(Action::useAbility(w.id("thief"), self, -1)), "nothing to cure");
  ConditionState poison, curse;
  poison.condition = w.db().findCondition("poison");
  poison.left = 3;
  curse.condition = w.db().findCondition("curse");
  curse.left = 3;
  w.unit("thief").conditions = {poison, curse};
  std::vector<AbilityOption> menu;
  w.b().abilityOptions(w.id("thief"), w.unit("thief").cell, menu);
  for (const AbilityOption& m : menu)
    if (m.ability == self) CHECK(m.ok);
  CHECK(w.apply(Action::useAbility(w.id("thief"), self, -1)));
  CHECK(w.unit("thief").conditions.empty());
  CHECK_EQ(w.count(EventType::ConditionEnded), 2);
}

TEST(cure_loader_refusals) {
  auto say = [](const std::string& ability) {
    Copy c("sb2_cure_bad", kFix);
    c.put("data/abilities.json", "{ \"abilities\": [" + ability + "] }");
    c.swap("data/units.json", "\"abilities\": [\"steal\", \"steal_fast\", \"steal_arms\", \"steal_all\", \"refresh\", \"refresh_two\", \"leap\", \"purge\", \"purge_self\"]", "\"abilities\": []");
    c.swap("data/units.json", "\"abilities\": [\"refresh\", \"refresh_two\", \"purge\"]", "\"abilities\": []");
    return c.gameError();
  };
  const std::string head = "{ \"id\": \"x\", \"target\": \"";
  CHECK(saysIn(say(head + "enemy\", \"effect\": { \"cure\": \"all\" } }"), "\"cure\" needs \"target\": \"self\" or \"ally\""));
  CHECK(saysIn(say(head + "ally\", \"effect\": { \"cure\": [] } }"), "\"cure\" must be \"all\" or a list of condition ids"));
  CHECK(saysIn(say(head + "ally\", \"effect\": { \"cure\": [\"gout\"] } }"), "\"cure\" names unknown condition \"gout\""));
  CHECK(say(head + "ally\", \"effect\": { \"cure\": [\"poison\", \"curse\"] } }").empty());
}

// ---- the computer opponent: only a unit marked "steal" steals ----

namespace {

// Red's turn, played by the computer until it hands the turn back (false = it stalled).
bool redTurn(World& w) {
  CHECK(w.apply(Action::endTurn()));  // blue ends: red's turn begins
  for (int i = 0; i < 60 && w.b().activeSide == 1 && !w.b().over; ++i) aiTakeStep(w.b(), w.events);
  return w.b().activeSide != 1 || w.b().over;
}

}  // namespace

TEST(steal_ai_a_marked_unit_steals_and_an_unmarked_one_does_not) {
  for (const bool marked : {true, false}) {
    Copy c(marked ? "sb2_ai_steal_marked" : "sb2_ai_steal_plain", kFix);
    // The red guard becomes a rogue (it has the steal abilities) next to the blue thief; marked or not.
    c.swap("maps/field.json", "{ \"type\": \"guard\", \"side\": \"red\", \"x\": 1, \"y\": 1, \"id\": \"mark\" }",
           std::string("{ \"type\": \"rogue\", \"side\": \"red\", \"x\": 1, \"y\": 1, \"id\": \"mark\"") + (marked ? ", \"goesFor\": [\"steal\"]" : "") + " }");
    c.swap("data/units.json", "\"items\": [\"iron_sword\"], \"tags\": [\"thief\"]", "\"items\": [\"iron_sword\", \"potion\"], \"tags\": [\"thief\"]");
    World w("field", c.dir);
    CHECK_EQ(w.unit("mark").aiGoals, marked ? kAiSteal : 0);
    auto blueItems = [&]() {
      size_t n = 0;
      for (const Unit& u : w.b().units) n += u.side == 0 ? u.items.size() : 0;
      return n;
    };
    const size_t before = blueItems();
    CHECK(redTurn(w));
    bool stole = false;
    for (const Event& e : w.events) stole |= e.type == EventType::ItemStolen;
    CHECK_EQ(stole, marked);
    if (marked) {
      CHECK_EQ(blueItems(), before - 1);  // it may walk to any blue unit with something it can take
      CHECK(w.unit("mark").items.back().drop);  // what a computer unit takes it drops when defeated
    }
    CHECK(redTurn(w));  // later turns never stall either
  }
}

TEST(steal_ai_goesfor_steal_is_refused_on_a_human_side) {
  Copy c("sb2_ai_steal_human", kFix);
  c.swap("maps/field.json", "\"x\": 0, \"y\": 1, \"id\": \"thief\"", "\"x\": 0, \"y\": 1, \"id\": \"thief\", \"goesFor\": [\"steal\"]");
  CHECK(saysIn(c.mapError("field"), "\"goesFor\" tells the computer opponent what to do"));
}
