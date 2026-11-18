// Map weapons: saves and the computer opponent (docs/SRPG.md "Map weapons", docs/SAVES.md, docs/AI.md). Game: tests/fixtures/srpg-mapweapon.
#include "srpg_mapweapon_fixture.h"

using namespace ote;
using namespace mwtest;

namespace {

bool restore(Battle& r, const Json& saved, std::string* why = nullptr) {
  std::string err;
  const bool ok = restoreBattleState(r, saved, err);
  if (why) *why = err;
  else if (!ok) std::printf("  restore refused: %s\n", err.c_str());
  return ok;
}

Json savedOf(MapWorld& w) {
  Json j;
  saveBattleState(w.b(), j);
  return j;
}

// A copy of the saved JSON where structure `index` has `key` set to `value`.
Json withKey(const Json& saved, size_t index, const char* key, double value) {
  Json out = saved;
  Json list = Json::makeArray();
  for (size_t i = 0; i < saved["structures"].size(); ++i) {
    Json s = Json::makeObject();
    for (const Json::Member& m : saved["structures"][i].members()) s.set(m.first, m.second);
    if (i == index) s.set(key, Json::makeNumber(value));
    list.push(s);
  }
  out.set("structures", list);
  return out;
}

int blueLost(MapWorld& w) { return 60 - w.unit("kn").hp; }

}  // namespace

TEST(mapweapon_save_a_battle_in_the_middle_comes_back_with_the_uses_left) {
  MapWorld w("mapweapon_save");
  w.run(Action::move(w.id("ar"), w.at(4, 1)));
  CHECK(w.run(Action::attack(w.id("ar"), w.id("g1"), w.bolt())));
  CHECK_EQ(w.st("b1").weaponUses, 2);
  const Json saved = savedOf(w);
  Battle r;
  std::string err;
  CHECK(r.init(w.rig.db, w.rig.map, w.rig.formulas, 1, err));
  CHECK(restore(r, saved));
  CHECK_EQ(r.mapWeaponUses(w.place("b1")), 2);
  CHECK_EQ(r.mapWeaponUses(w.place("b2")), 1);  // the placement's own number
  CHECK_EQ(r.mapWeaponUses(w.place("c1")), -1);
  Json again;
  saveBattleState(r, again);
  CHECK_STR(again.dump(0), saved.dump(0));  // saving the restored battle gives the same text
  // And it goes on from there: two more, then spent and the kind swaps.
  Battle& b = r;
  std::vector<Event> ev;
  b.units[(size_t)w.id("ar")].moved = b.units[(size_t)w.id("ar")].done = false;
  CHECK(b.apply(Action::attack(w.id("ar"), w.id("g1"), w.bolt()), ev, &err));
  CHECK_EQ(b.mapWeaponUses(w.place("b1")), 1);
}

TEST(mapweapon_save_a_spent_weapon_is_saved_as_zero_and_the_kind_swap_as_before) {
  MapWorld w("mapweapon_save_spent", "field", [](MCopy& c) { c.swap("data/structures.json", ", \"becomes\": \"ballista_wreck\"", ""); });
  w.put("g1", 7, 1);
  w.put("ar", 4, 2);  // b2: one use
  CHECK(w.run(Action::attack(w.id("ar"), w.id("g1"), w.bolt())));
  const Json saved = savedOf(w);
  CHECK(saved["structures"][(size_t)w.place("b2")].has("weaponUses"));
  CHECK_EQ(saved["structures"][(size_t)w.place("b2")]["weaponUses"].asInt(), 0);
  Battle r;
  std::string err;
  CHECK(r.init(w.rig.db, w.rig.map, w.rig.formulas, 1, err));
  CHECK(restore(r, saved));
  CHECK_EQ(r.mapWeaponUses(w.place("b2")), 0);
  // With the kind swapped (the default fixture) the wreck is saved as its kind and has no uses key.
  MapWorld v("mapweapon_save_wreck");
  v.put("g1", 7, 1);
  v.unit("g1").stats[0] = 500;
  v.unit("g1").hp = 500;
  v.put("ar", 4, 2);
  CHECK(v.run(Action::attack(v.id("ar"), v.id("g1"), v.bolt())));
  CHECK_STR(v.kind("b2"), "ballista_wreck");
  const Json sv = savedOf(v);
  CHECK(!sv["structures"][(size_t)v.place("b2")].has("weaponUses"));
  Battle r2;
  CHECK(r2.init(v.rig.db, v.rig.map, v.rig.formulas, 1, err));
  CHECK(restore(r2, sv));
  CHECK_STR(r2.db->structures[(size_t)r2.structures[(size_t)v.place("b2")].type].id, "ballista_wreck");
}

TEST(mapweapon_save_without_a_used_weapon_only_a_placed_number_is_written) {
  MapWorld w("mapweapon_save_fresh");
  const Json saved = savedOf(w);
  CHECK(!saved["structures"][(size_t)w.place("b1")].has("weaponUses"));  // what the kind starts with: nothing to say
  CHECK(!saved["structures"][(size_t)w.place("c1")].has("weaponUses"));
  CHECK(saved["structures"][(size_t)w.place("b2")].has("weaponUses"));    // the placement said 1, the kind 3
  CHECK_EQ(saved["structures"][(size_t)w.place("b2")]["weaponUses"].asInt(), 1);
  // A game with the same kinds but no usable weapon at all writes no such key anywhere.
  MapWorld x("mapweapon_save_none", "field", [](MCopy& c) {
    c.swap("data/structures.json", "\"weapon\": \"ballista_bolt\", \"uses\": 3, \"by\": { \"tags\": [\"archer\"] }, \"becomes\": \"ballista_wreck\"", "\"width\": 1");
    c.swap("data/structures.json", "\"weapon\": \"ballista_bolt\" }", "\"width\": 1 }");
    c.swap("maps/field.json", ", \"uses\": 1", "");
  });
  CHECK(x.db().structures[(size_t)x.db().findStructure("ballista")].mapWeapon.has() == false);
  CHECK(savedOf(x).dump(0).find("weaponUses") == std::string::npos);
}

TEST(mapweapon_save_a_damaged_save_is_refused) {
  MapWorld w("mapweapon_save_bad");
  const Json saved = savedOf(w);
  Battle r;
  std::string err, why;
  CHECK(r.init(w.rig.db, w.rig.map, w.rig.formulas, 1, err));
  // Uses on a tower, a number out of range, a text.
  const size_t tower = w.b().structures.size() - 1;
  CHECK(!restore(r, withKey(saved, tower, "weaponUses", 1), &why) && why.find("has no weapon") != std::string::npos);
  Battle r2;
  CHECK(r2.init(w.rig.db, w.rig.map, w.rig.formulas, 1, err));
  CHECK(!restore(r2, withKey(saved, (size_t)w.place("b1"), "weaponUses", -1), &why) && why.find("weaponUses") != std::string::npos);
  Battle r3;
  CHECK(r3.init(w.rig.db, w.rig.map, w.rig.formulas, 1, err));
  CHECK(!restore(r3, withKey(saved, (size_t)w.place("b1"), "weaponUses", 10000), &why) && why.find("weaponUses") != std::string::npos);
  Battle r4;
  CHECK(r4.init(w.rig.db, w.rig.map, w.rig.formulas, 1, err));
  CHECK(restore(r4, withKey(saved, (size_t)w.place("b1"), "weaponUses", 2)));
  CHECK_EQ(r4.mapWeaponUses(w.place("b1")), 2);
}

TEST(mapweapon_ai_a_unit_standing_on_one_fires_it_without_being_marked) {
  MapWorld w("mapweapon_ai_on", "siege");
  CHECK_EQ(w.unit("g1").aiGoals, 0);
  w.put("g1", 5, 1);  // on the catapult; the knight is 5 squares away
  CHECK_EQ(w.options("g1"), 1);
  const std::vector<Event> red = w.redTurn();
  bool fired = false;
  for (const Event& e : red) fired = fired || (e.type == EventType::Attack && (e.flags & kMapWeapon) && e.a == w.id("g1"));
  CHECK(fired);
  CHECK_EQ(blueLost(w), 9);  // 4 + 10 - 5
}

TEST(mapweapon_ai_it_walks_to_a_free_one_only_when_marked) {
  // Unmarked: the grunt cannot reach the knight with its sword this turn, and a free catapult is nothing to it.
  MapWorld w("mapweapon_ai_unmarked", "siege");
  const std::vector<Event> red = w.redTurn();
  for (const Event& e : red) CHECK(!(e.type == EventType::Attack && (e.flags & kMapWeapon)));
  CHECK_EQ(blueLost(w), 0);
  // Marked: it steps onto the catapult (3 squares) and fires.
  MapWorld m("mapweapon_ai_marked", "siege", [](MCopy& c) {
    c.swap("maps/siege.json", "\"id\": \"g1\", \"side\": \"red\"", "\"id\": \"g1\", \"side\": \"red\", \"goesFor\": [\"weapons\"]");
  });
  CHECK_EQ(m.unit("g1").aiGoals, kAiWeapons);
  const std::vector<Event> red2 = m.redTurn();
  bool fired = false;
  for (const Event& e : red2) fired = fired || (e.type == EventType::Attack && (e.flags & kMapWeapon));
  CHECK(fired);
  CHECK_EQ(m.unit("g1").cell, m.at(5, 1));
  CHECK_EQ(blueLost(m), 9);
}

TEST(mapweapon_ai_a_marked_unit_prefers_a_real_attack_over_walking_to_a_weapon) {
  // The knight stands where the grunt's sword reaches it: the marked grunt hits with the sword, not the catapult.
  MapWorld m("mapweapon_ai_prefers", "siege", [](MCopy& c) {
    c.swap("maps/siege.json", "\"id\": \"g1\", \"side\": \"red\"", "\"id\": \"g1\", \"side\": \"red\", \"goesFor\": [\"weapons\"]");
  });
  m.put("kn", 6, 1);
  m.put("g1", 8, 1);
  m.unit("g1").stats[1] = 12;  // strong enough, and the knight hurt enough, that hitting it is worth it
  m.unit("kn").hp = 10;
  const std::vector<Event> red = m.redTurn();
  bool mapShot = false, swordHit = false;
  for (const Event& e : red) {
    if (e.type != EventType::Attack || e.a != m.id("g1")) continue;
    mapShot = mapShot || (e.flags & kMapWeapon);
    swordHit = swordHit || !(e.flags & kMapWeapon);
  }
  CHECK(swordHit);
  CHECK(!mapShot);
}

TEST(mapweapon_ai_the_turn_always_ends_with_a_marked_unit_and_a_spent_weapon) {
  MapWorld m("mapweapon_ai_spent", "siege", [](MCopy& c) {
    c.swap("maps/siege.json", "\"id\": \"g1\", \"side\": \"red\"", "\"id\": \"g1\", \"side\": \"red\", \"goesFor\": [\"weapons\"]");
    c.swap("maps/siege.json", "\"id\": \"c1\"", "\"id\": \"c1\", \"uses\": 1");
  });
  for (int turn = 0; turn < 5 && !m.b().over; ++turn) {
    m.redTurn();
    CHECK(m.b().over || m.b().activeSide == 0);  // red handed the turn back every time
  }
  CHECK_EQ(m.b().mapWeaponUses(m.place("c1")) <= 0, true);  // it fired its one use
}
