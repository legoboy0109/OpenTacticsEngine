// Looting and escaping (docs/AI.md "Looting and escaping"), never attacking (docs/AI.md "Never attacking") and the "last" drop rule
// (docs/ITEMS.md "Drops"). Game: tests/fixtures/srpg-doors, map "market" (11 x 5): unlocked chests c1 (4,1; potion) and c2 (4,3; 40 gold),
// blue's hamlet v1 (8,4); blue lord (10,0); red r1 (1,1, goesFor chests), r2 (1,3), r3 (9,3, goesFor villages). Raiders move 5.
#include "core/ai/ai_internal.h"
#include "core/battle/save_behaviour.h"
#include "srpg_doors_fixture.h"

using namespace ote;
using namespace doorstest;

namespace {

const char* kMarket = "maps/market.json";

// r1 a looter that never attacks, carrying a sword; the map gets one exit at its top-left corner.
void looter(DCopy& c) {
  c.swap(kMarket, R"("id": "r1", "side": "red", "x": 1, "y": 1, "items": [], "goesFor": ["chests"] })",
         R"("id": "r1", "side": "red", "x": 1, "y": 1, "items": ["iron_sword"], "goesFor": ["chests"], "behaviour": { "mode": "loot", "attacks": false } })");
  c.swap(kMarket, R"("units": [)", R"("exits": [ { "x": 0, "y": 0 } ], "units": [)");
}

bool says(const std::string& text, const char* part) { return text.find(part) != std::string::npos; }

int countOf(const std::vector<Event>& events, EventType t) {
  int n = 0;
  for (const Event& e : events) n += e.type == t;
  return n;
}

}  // namespace

TEST(ai_loot_a_looter_that_never_attacks_takes_both_chests_then_leaves_by_the_exit) {
  Doors w("ai_loot_run", "market", looter);
  CHECK(w.b().exits.size() == 1u);
  CHECK(w.unit("r1").behaviour.mode == AiMode::Loot && w.unit("r1").behaviour.noAttack);
  w.put("lord", 2, 1);  // right next to the looter, with a sword it could be hit with
  const int lordHp = w.unit("lord").hp;
  bool left = false;
  for (int t = 0; t < 8 && !left; ++t) {
    const std::vector<Event> turn = w.redTurn();
    CHECK_EQ(w.unit("lord").hp, lordHp);  // never once attacked
    left = countOf(turn, EventType::UnitRemoved) > 0 && !w.unit("r1").alive;
    if (!w.b().over && w.b().activeSide == 0) w.put("lord", 10, 0), w.b().refreshSight();  // out of everyone's way after the first turn
  }
  CHECK(left);
  CHECK_STR(w.kind("c1"), "chest_open");
  CHECK_STR(w.kind("c2"), "chest_open");
  CHECK(w.unit("r1").cell == kNoCell);
  CHECK(w.unit("r1").escaping);
  CHECK(w.unit("r2").alive && w.unit("r3").alive);  // the others play on
}

TEST(ai_loot_a_looter_that_may_attack_still_fights_when_it_can_like_any_marked_unit) {
  Doors w("ai_loot_fights", "market", [](DCopy& c) {
    looter(c);
    c.swap(kMarket, R"("behaviour": { "mode": "loot", "attacks": false })", R"("behaviour": { "mode": "loot" })");
  });
  w.put("lord", 2, 1);
  const int lordHp = w.unit("lord").hp;
  w.redTurn();
  CHECK(w.unit("lord").hp < lordHp || !w.unit("lord").alive);  // an attack scores above opening (as for every goesFor unit)
  CHECK(!w.unit("r1").escaping);
}

TEST(ai_loot_with_nothing_left_it_escapes_for_good) {
  Doors w("ai_loot_good", "market", looter);
  // Blue empties both chests before the looter gets there (red's turns are passed by hand, without the computer).
  w.put("lord", 4, 0);
  CHECK(w.run(w.open("lord", "c1")));
  CHECK(w.run(Action::endTurn()));
  CHECK(w.run(Action::endTurn()));
  w.put("lord", 4, 2);
  CHECK(w.run(w.open("lord", "c2")));
  CHECK(!w.unit("r1").escaping);
  w.redTurn();
  CHECK(w.unit("r1").escaping);  // nothing left anywhere: it makes for the exit from this turn on (the state is kept, not worked out again)
  CHECK(!w.unit("r1").alive || w.b().grid.distance(w.unit("r1").cell, w.at(0, 0)) < 2);
}

TEST(ai_loot_escape_mode_walks_to_the_nearest_exit_and_leaves_without_attacking) {
  Doors w("ai_loot_escape", "market", [](DCopy& c) {
    c.swap(kMarket, R"("id": "r2", "side": "red", "x": 1, "y": 3, "items": [] })",
           R"("id": "r2", "side": "red", "x": 1, "y": 3, "items": ["iron_sword"], "behaviour": "escape" })");
    c.swap(kMarket, R"("units": [)", R"("exits": [ { "x": 10, "y": 4 }, { "x": 0, "y": 4 } ], "units": [)");
  });
  w.put("lord", 1, 2);
  const int lordHp = w.unit("lord").hp;
  const std::vector<Event> turn = w.redTurn();
  CHECK_EQ(w.unit("lord").hp, lordHp);
  CHECK(!w.unit("r2").alive);  // 0,4 is one step away: there and out in the same turn
  CHECK_EQ(countOf(turn, EventType::UnitDied), 0);  // leaving is not a defeat
  CHECK(countOf(turn, EventType::UnitRemoved) >= 1);
}

TEST(ai_loot_never_attacking_holds_for_any_mode_and_counterattacks_still_happen) {
  Doors w("ai_loot_noattack", "market", [](DCopy& c) {
    c.swap(kMarket, R"("id": "r2", "side": "red", "x": 1, "y": 3, "items": [] })",
           R"("id": "r2", "side": "red", "x": 1, "y": 3, "items": ["iron_sword"], "behaviour": { "attacks": false } })");
  });
  w.put("lord", 2, 3);
  const int lordHp = w.unit("lord").hp;
  w.redTurn();
  CHECK_EQ(w.unit("lord").hp, lordHp);
  // Blue strikes it: it answers (a counterattack is a rule, not a choice).
  w.unit("lord").done = false, w.unit("lord").moved = false;
  if (w.unit("r2").cell != kNoCell && w.b().grid.distance(w.unit("r2").cell, w.unit("lord").cell) == 1) {
    CHECK(w.run(Action::attack(w.id("lord"), w.id("r2"), w.rig.db.findWeapon("iron_sword"))));
    CHECK(w.unit("lord").hp < lordHp || !w.unit("r2").alive);
  }
}

TEST(ai_loot_leaving_the_map_is_for_computer_units_on_an_exit_only) {
  Doors w("ai_loot_leave_rules", "market", looter);
  CHECK(!w.why(Action::leaveMap(w.id("r1"))).empty());  // not on an exit (and not its turn)
  w.put("lord", 0, 0);
  CHECK(says(w.why(Action::leaveMap(w.id("lord"))), "only a computer unit"));
}

TEST(ai_loot_the_escaping_state_is_saved_and_read_back) {
  Doors w("ai_loot_save", "market", looter);
  Unit& u = w.unit("r1");
  u.escaping = true;
  Json j = Json::makeObject();
  savejson::writeUnitBehaviour(u, j);
  CHECK(j["escaping"].asBool());
  CHECK_STR(behaviourToJson(u.behaviour).dump(0), R"({"mode":"loot","attacks":false})");
  Unit back;
  std::string err;
  savejson::Reader rd(err);
  CHECK(savejson::readUnitBehaviour(rd, w.db(), j, "u", back));
  CHECK(back.escaping && back.behaviour.mode == AiMode::Loot && back.behaviour.noAttack);
}

TEST(ai_loot_the_loader_explains_what_is_missing_or_wrong) {
  auto err = [](const char* name, const std::function<void(DCopy&)>& tweak) {
    DCopy c(name);
    tweak(c);
    Database db;
    MapDef m;
    std::string e;
    return loadGame(c.dir, db, e) && loadMap(db, "market", m, e) ? std::string() : e;
  };
  CHECK(says(err("ai_loot_e1", [](DCopy& c) {
              c.swap(kMarket, R"("id": "r2", "side": "red", "x": 1, "y": 3, "items": [] })", R"("id": "r2", "side": "red", "x": 1, "y": 3, "items": [], "behaviour": "escape" })");
            }),
             "needs squares to leave by"));
  CHECK(says(err("ai_loot_e2", [](DCopy& c) {
              c.swap(kMarket, R"("id": "r2", "side": "red", "x": 1, "y": 3, "items": [] })", R"("id": "r2", "side": "red", "x": 1, "y": 3, "items": [], "behaviour": "loot" })");
            }),
             "mode \"loot\" needs \"goesFor\""));
  CHECK(says(err("ai_loot_e3", [](DCopy& c) { c.swap(kMarket, R"("units": [)", R"("exits": [ { "x": 11, "y": 0 } ], "units": [)"); }), "outside the map"));
  CHECK(says(err("ai_loot_e4", [](DCopy& c) { c.swap(kMarket, R"("units": [)", R"("exits": [ { "x": 1, "y": 0 }, { "x": 1, "y": 0 } ], "units": [)"); }),
             "listed twice"));
  CHECK(says(err("ai_loot_e5", [](DCopy& c) { c.swap(kMarket, R"("units": [)", R"("exits": [], "units": [)"); }), "\"exits\" must be a list of 1 to"));
  CHECK(says(err("ai_loot_e6", [](DCopy& c) {
              c.swap(kMarket, R"("id": "r2", "side": "red", "x": 1, "y": 3, "items": [] })", R"("id": "r2", "side": "red", "x": 1, "y": 3, "items": [], "behaviour": { "attacks": "no" } })");
            }),
             "\"attacks\" must be true or false"));
  CHECK(says(err("ai_loot_e7", [](DCopy& c) {
              c.swap("game.json", R"("items": { "weapons": "carried", "inventorySize": 4 })", R"("items": { "weapons": "carried", "inventorySize": 4, "drops": "first" })");
            }),
             "\"drops\" must be \"marked\""));
  CHECK_STR(err("ai_loot_ok", looter), "");
}

TEST(ai_loot_drops_last_gives_only_the_last_item_that_is_not_the_weapon_in_hand) {
  for (int last = 0; last < 2; ++last) {
    Doors w(last ? "ai_loot_drops_last" : "ai_loot_drops_marked", "market", [last](DCopy& c) {
      if (last) c.swap("game.json", R"("items": { "weapons": "carried", "inventorySize": 4 })", R"("items": { "weapons": "carried", "inventorySize": 4, "drops": "last" })");
    });
    CHECK_EQ(w.db().rules.items.dropLast, last == 1);
    Unit& u = w.unit("r1");
    const int sword = w.item("iron_sword"), potion = w.item("potion"), key = w.item("brass_key");
    std::vector<ItemStack> out;
    // Nothing marked: nothing drops, under both rules.
    u.items = {ItemStack{sword, 30, false}, ItemStack{potion, 1, false}, ItemStack{key, 1, false}};
    w.b().dropsOf(u, out);
    CHECK(out.empty());
    // The potion is marked: "marked" gives the potion, "last" the key (the last item, marked or not).
    u.items[1].drop = true;
    w.b().dropsOf(u, out);
    CHECK_EQ((int)out.size(), 1);
    CHECK_EQ(out[0].item, last ? key : potion);
    // The last item is the sword in its hand: "last" gives the one before it.
    u.items = {ItemStack{potion, 1, true}, ItemStack{sword, 30, false}};
    w.b().dropsOf(u, out);
    CHECK_EQ((int)out.size(), 1);
    CHECK_EQ(out[0].item, potion);
  }
}
