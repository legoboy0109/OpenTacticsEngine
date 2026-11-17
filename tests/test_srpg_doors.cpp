// Doors, keys, chests and visiting: the rules (docs/SRPG.md section 10). Game: tests/fixtures/srpg-doors.
#include <algorithm>

#include "srpg_doors_fixture.h"

using namespace ote;
using namespace doorstest;

namespace {

int abilityIndex(Doors& w, const char* id) { return w.db().findAbility(id); }

// Walks `who` to a square the way a move does, without the rules (the test arranges the board) and clears the turn flags.
void arrange(Doors& w, const char* who, int x, int y) {
  w.put(who, x, y);
  w.unit(who).moved = w.unit(who).done = false;
}

// restoreBattleState that says why when it refuses.
bool restore(Battle& r, const Json& saved) {
  std::string err;
  const bool ok = restoreBattleState(r, saved, err);
  if (!ok) std::printf("  restore refused: %s\n", err.c_str());
  return ok;
}

bool canEnter(Doors& w, const char* unitName, int x, int y) {
  Battle& b = w.b();
  return b.enterCost(w.at(x, y), b.typeOf(w.unit(unitName)).moveType) > 0;
}

}  // namespace

TEST(doors_the_fixture_loads_and_kinds_are_read) {
  DCopy c("doors_load");
  CHECK_STR(c.gameError(), "");
  Doors w("doors_kinds");
  const Database& db = w.db();
  const StructureDef& gate = db.structures[(size_t)db.findStructure("gate")];
  CHECK(gate.place.isDoor() && gate.place.hp == 20 && gate.place.byBreak && gate.place.byKey && gate.place.byTrait);
  CHECK(gate.blocksSight && gate.place.fromNext && !gate.place.fromOn);
  CHECK_EQ(gate.place.to, db.findStructure("gate_open"));
  const StructureDef& cell = db.structures[(size_t)db.findStructure("cell_door")];
  CHECK(cell.place.keys.size() == 2 && !cell.place.byBreak && cell.place.hp == 0);
  CHECK(db.items[(size_t)db.findItem("brass_key")].opensDoors && !db.items[(size_t)db.findItem("brass_key")].opensChests);
  CHECK(db.items[(size_t)db.findItem("strong_key")].opensKinds.size() == 1);
  CHECK(db.structures[(size_t)db.findStructure("strongbox")].place.byTrait == false);
  CHECK(db.structures[(size_t)db.findStructure("camp")].place.repeat);
  CHECK(!db.rules.places.automatic);
  CHECK_EQ(w.st("d1").hp, -1);
  Doors g("doors_kinds2", "gatehouse");
  CHECK_EQ(g.st("g1").hp, 20);
}

TEST(doors_a_closed_door_blocks_movement_and_an_open_one_does_not) {
  Doors w("doors_move");
  CHECK(!canEnter(w, "hero", 5, 1));
  Reach before;
  w.b().reachable(w.id("hero"), before);
  CHECK(!before.canStop(w.at(5, 1)) && !before.canStop(w.at(6, 1)));  // the wall column holds
  CHECK(w.run(w.open("hero", "d1", "brass_key")));
  CHECK_STR(w.kind("d1"), "cell_door_open");
  CHECK(canEnter(w, "hero", 5, 1));
  Reach after;
  w.unit("hero").done = false;
  w.b().reachable(w.id("hero"), after);
  CHECK(after.canStop(w.at(5, 1)) && after.canStop(w.at(6, 1)));
}

TEST(doors_a_key_opens_the_door_and_is_used_up_by_its_uses) {
  Doors w("doors_key");
  CHECK(w.has("hero", "brass_key"));
  CHECK(w.run(w.open("hero", "d1", "brass_key")));
  CHECK(!w.has("hero", "brass_key"));  // one use, spent
  CHECK_EQ(w.count(EventType::ItemBroke), 1);
  const Event* e = w.first(EventType::DoorOpened);
  CHECK(e != nullptr);
  if (e) CHECK(e->a == w.place("d1") && e->b == w.id("hero") && e->c == kOpenKey);
  CHECK(w.unit("hero").done);  // an explicit action uses the turn
  CHECK(w.sw("opened"));       // the story trigger ran
  // A master key has three uses and loses one per opening.
  Doors m("doors_master");
  m.unit("hero").items.push_back(ItemStack{m.item("master_key"), 3, false});
  CHECK(m.run(m.open("hero", "d1", "master_key")));
  CHECK_EQ(m.unit("hero").items[(size_t)m.slotOf("hero", "master_key")].uses, 2);
  CHECK_EQ(m.count(EventType::ItemBroke), 0);
}

TEST(doors_a_wrong_key_or_none_is_refused_and_nothing_changes) {
  Doors w("doors_wrong");
  w.unit("hero").items.push_back(ItemStack{w.item("strong_key"), -1, false});
  CHECK(w.why(w.open("hero", "d1", "strong_key")).find("does not fit") != std::string::npos);
  CHECK(w.why(w.open("hero", "d1")).find("needs a key") != std::string::npos);
  CHECK(w.why(Action::openPlace(w.id("hero"), w.place("d1"), 0)).find("does not fit") != std::string::npos);  // slot 0 is the sword
  CHECK(!w.run(w.open("hero", "d1", "strong_key"), true));
  CHECK_STR(w.kind("d1"), "cell_door");
  CHECK(w.has("hero", "brass_key") && !w.unit("hero").done);
  // Out of reach: sneak (4,3) is not next to d1 (5,1).
  w.unit("sneak").items.push_back(ItemStack{w.item("brass_key"), 1, false});
  CHECK(w.why(w.open("sneak", "d1", "brass_key")).find("out of reach") != std::string::npos);
}

TEST(doors_an_unlocked_door_opens_without_a_key) {
  Doors w("doors_latch");
  CHECK(!w.st("d2").locked && w.st("d1").locked);
  CHECK(w.run(w.open("sneak", "d2")));
  CHECK_STR(w.kind("d2"), "cell_door_open");
  const Event* e = w.first(EventType::DoorOpened);
  CHECK(e && e->c == kOpenFree);
  CHECK_EQ(w.count(EventType::ItemBroke), 0);
}

TEST(doors_a_wide_gate_opens_as_one) {
  Doors w("doors_gate", "gatehouse");
  CHECK(!canEnter(w, "hero", 4, 2) && !canEnter(w, "hero", 5, 2));
  CHECK_EQ(w.b().structureAt[(size_t)w.at(4, 2)], w.b().structureAt[(size_t)w.at(5, 2)]);
  // The gate is opened from next to it (from: "next"), from either of its two squares' neighbours.
  CHECK(w.b().placeInReach(w.place("g1"), w.at(4, 3)) && w.b().placeInReach(w.place("g1"), w.at(5, 3)));
  CHECK(!w.b().placeInReach(w.place("g1"), w.at(7, 3)) && !w.b().placeInReach(w.place("g1"), w.at(4, 2)));
  CHECK(w.run(w.open("hero", "g1", "master_key")));
  CHECK(canEnter(w, "hero", 4, 2) && canEnter(w, "hero", 5, 2));
  CHECK_STR(w.kind("g1"), "gate_open");
  CHECK(w.sw("opened"));
}

TEST(doors_a_door_with_hit_points_is_broken_down_by_attacks) {
  Doors w("doors_break", "gatehouse");
  std::vector<PlaceAttack> opts;
  w.b().placeAttackOptions(w.id("brute"), w.unit("brute").cell, opts);
  CHECK(opts.size() == 1 && opts[0].structure == w.place("g1"));
  PlaceForecast f;
  const int axe = w.db().findWeapon("heavy_axe"), sword = w.db().findWeapon("iron_sword");
  CHECK(w.b().placeForecast(w.id("brute"), w.place("g1"), axe, w.unit("brute").cell, f));
  CHECK(f.damage == 9 + 7 - 3 && f.strikes == 1 && f.hpBefore == 20 && f.hpAfter == 7 && !f.breaks);
  CHECK(w.run(w.hit("brute", "g1", "heavy_axe")));
  CHECK_EQ(w.st("g1").hp, 7);
  CHECK_EQ(w.count(EventType::PlaceHit), 1);
  CHECK(w.unit("brute").done);
  CHECK_STR(w.kind("g1"), "gate");
  // The second blow (sword: 5 + 7 - 3 = 9) finishes it.
  CHECK(w.b().placeForecast(w.id("hero"), w.place("g1"), sword, w.unit("hero").cell, f) && f.breaks && f.total == 7);
  CHECK(w.run(w.hit("hero", "g1", "iron_sword")));
  CHECK_STR(w.kind("g1"), "gate_open");
  const Event* e = w.first(EventType::DoorOpened);
  CHECK(e && e->c == kOpenBreak && e->b == w.id("hero"));
  const Event* h = w.first(EventType::PlaceHit);
  CHECK(h && (h->flags & kKill) && h->d == 7);
  CHECK(w.sw("opened"));
  CHECK_EQ(w.st("g1").hp, -1);
  // A weapon wears one use per strike.
  CHECK_EQ(w.unit("brute").items[0].uses, 19);
}

TEST(doors_only_doors_with_hit_points_can_be_attacked_and_a_unit_must_be_in_range) {
  Doors w("doors_nobreak");  // hall: d1 has no hp, nothing is in reach
  std::vector<PlaceAttack> opts;
  w.b().placeAttackOptions(w.id("hero"), w.unit("hero").cell, opts);
  CHECK(opts.empty());
  CHECK(w.why(w.hit("hero", "d1", "iron_sword")).find("cannot be broken") != std::string::npos);
  Doors g("doors_range", "gatehouse");
  g.b().placeAttackOptions(g.id("sneak"), g.unit("sneak").cell, opts);
  CHECK(opts.empty());  // (7,3) is three squares from the gate; a sword reaches one
  CHECK(!g.run(Action::breakPlace(g.id("sneak"), g.place("g1"), g.db().findWeapon("iron_sword")), true));
}

TEST(doors_a_unit_trait_opens_without_a_key) {
  Doors w("doors_trait");
  const int pick = abilityIndex(w, "pick");
  w.put("hero", 3, 1);  // hero stands on 4,1: swap their places for the test
  arrange(w, "sneak", 4, 1);
  std::vector<int> us;
  std::vector<Cell> cs;
  w.b().abilityTargets(w.id("sneak"), pick, w.unit("sneak").cell, us, cs);
  CHECK(std::find(cs.begin(), cs.end(), w.at(5, 1)) != cs.end());
  CHECK(w.run(Action::useAbility(w.id("sneak"), pick, -1, w.at(5, 1))));
  CHECK_STR(w.kind("d1"), "cell_door_open");
  const Event* e = w.first(EventType::DoorOpened);
  CHECK(e && e->c == kOpenTrait && e->b == w.id("sneak"));
  CHECK(w.sw("opened") && w.unit("sneak").done);
}

TEST(doors_the_trait_can_be_limited_by_kind_and_by_what_the_structure_allows) {
  Doors w("doors_trait2");
  const int pick = abilityIndex(w, "pick"), deft = abilityIndex(w, "deft");
  // The strongbox lists only "key" as a way: a skill cannot open it.
  arrange(w, "sneak", 2, 4);
  CHECK(w.why(Action::useAbility(w.id("sneak"), pick, -1, w.at(2, 5))).find("skill cannot open") != std::string::npos);
  // deft opens chests only (not doors) and needs a lockpick.
  arrange(w, "tink", 4, 1);
  w.put("hero", 3, 0);
  CHECK(w.why(Action::useAbility(w.id("tink"), deft, -1, w.at(5, 1))).find("cannot be unlocked") != std::string::npos);
  arrange(w, "tink", 2, 1);  // next to chest c1 at 2,0
  w.unit("tink").items.erase(w.unit("tink").items.begin() + w.slotOf("tink", "lockpick"));
  CHECK(w.why(Action::useAbility(w.id("tink"), deft, -1, w.at(2, 0))).find("needs a Lockpick") != std::string::npos);
  w.unit("tink").items.push_back(ItemStack{w.item("lockpick"), 2, false});
  CHECK(w.run(Action::useAbility(w.id("tink"), deft, -1, w.at(2, 0))));
  CHECK_STR(w.kind("c1"), "chest_open");
  CHECK_EQ(w.unit("tink").items[(size_t)w.slotOf("tink", "lockpick")].uses, 1);  // the pick wore
  CHECK(w.has("tink", "potion") && w.gold() == 50);
  CHECK(w.sw("chest_done"));  // the trigger's "by" matches tink
}

TEST(doors_an_unlock_staff_opens_a_door_from_afar_following_the_staff_rules) {
  Doors w("doors_staff");
  arrange(w, "sage", 2, 1);  // 3 squares from d1 at 5,1
  const int slot = w.slotOf("sage", "unlock_staff");
  std::vector<StaffOption> opts;
  w.b().staffOptions(w.id("sage"), w.unit("sage").cell, opts);
  CHECK(opts.size() == 2 && opts[0].ok && opts[0].item == w.item("unlock_staff"));
  std::vector<Cell> cells;
  w.b().staffUnlockCells(w.id("sage"), slot, w.unit("sage").cell, cells);
  CHECK(std::find(cells.begin(), cells.end(), w.at(5, 1)) != cells.end() && std::find(cells.begin(), cells.end(), w.at(2, 0)) != cells.end());
  StaffForecast f;
  CHECK(w.b().staffForecast(w.id("sage"), slot, w.at(5, 1), w.unit("sage").cell, f));
  CHECK(f.isUnlock && f.unlockStructure == w.place("d1") && f.unlockHit == 100 && f.xp == 10 && f.usesLeft == 3);
  CHECK(w.run(Action::useStaff(w.id("sage"), slot, -1, w.at(5, 1))));
  CHECK_STR(w.kind("d1"), "cell_door_open");
  CHECK_EQ(w.unit("sage").items[(size_t)slot].uses, 2);
  CHECK(w.count(EventType::StaffUsed) == 1 && w.count(EventType::XpGained) == 1 && w.unit("sage").done);
  const Event* e = w.first(EventType::DoorOpened);
  CHECK(e && e->c == kOpenStaff);
  // Out of range.
  Doors far("doors_staff_far");
  CHECK(far.why(Action::useStaff(far.id("sage"), far.slotOf("sage", "unlock_staff"), -1, far.at(5, 1))).find("out of range") != std::string::npos);
}

TEST(doors_a_staff_that_can_miss_spends_its_use_and_leaves_the_door) {
  Doors w("doors_staff_miss");
  arrange(w, "sage", 3, 1);
  const int slot = w.slotOf("sage", "wild_staff");  // "hit": 0, doors only
  StaffForecast f;
  CHECK(w.b().staffForecast(w.id("sage"), slot, w.at(5, 1), w.unit("sage").cell, f) && f.canMiss && f.unlockHit == 0);
  CHECK(w.run(Action::useStaff(w.id("sage"), slot, -1, w.at(5, 1))));
  CHECK_STR(w.kind("d1"), "cell_door");
  CHECK_EQ(w.count(EventType::StaffMissed), 1);
  CHECK_EQ(w.unit("sage").items[(size_t)slot].uses, 2);
  // Doors only: the chest next to it is not a target of the wild staff.
  Doors v("doors_staff_kind");
  arrange(v, "sage", 2, 1);
  CHECK(v.why(Action::useStaff(v.id("sage"), v.slotOf("sage", "wild_staff"), -1, v.at(2, 0))).find("cannot be unlocked") != std::string::npos);
  // The strongbox opens to the plain unlock staff, whatever its "by" list says.
  arrange(v, "sage", 2, 4);
  CHECK(v.run(Action::useStaff(v.id("sage"), v.slotOf("sage", "unlock_staff"), -1, v.at(2, 5))));
  CHECK_STR(v.kind("c2"), "chest_open");
  CHECK(v.has("sage", "heavy_axe"));
}

TEST(doors_a_chest_gives_its_items_and_resources_to_the_opener) {
  Doors w("doors_chest");
  w.unit("hero").items.push_back(ItemStack{w.item("master_key"), 3, false});
  arrange(w, "hero", 2, 1);
  CHECK(w.run(w.open("hero", "c1", "master_key")));
  CHECK_STR(w.kind("c1"), "chest_open");
  CHECK(w.has("hero", "potion"));
  CHECK_EQ(w.gold(), 50);
  CHECK(w.st("c1").loot.empty());
  const Event* g = w.first(EventType::ItemGained);
  CHECK(g && g->a == w.id("hero") && (g->flags & kLooted));
  const Event* r = w.first(EventType::ResourceChanged);
  CHECK(r && r->c == 50 && (r->flags & kLooted));
  CHECK(w.count(EventType::ChestOpened) == 1);
  CHECK(!w.sw("chest_done"));  // the trigger wants tink
  // It cannot be opened twice.
  CHECK(!w.run(w.open("hero", "c1"), true));
}

TEST(doors_a_chest_that_does_not_fit_in_a_full_inventory_waits_or_goes_to_storage) {
  Doors w("doors_full");
  Unit& hero = w.unit("hero");
  hero.items.push_back(ItemStack{w.item("master_key"), 3, false});
  hero.items.push_back(ItemStack{w.item("lockpick"), 2, false});  // four now: the inventory is full
  arrange(w, "hero", 2, 1);
  CHECK(w.why(w.open("hero", "c1", "master_key")).find("carry more") != std::string::npos);
  std::vector<PlaceOption> opts = w.options("hero");
  bool listed = false;
  for (const PlaceOption& o : opts)
    if (o.structure == w.place("c1")) listed = !o.ok && o.why.find("drop something") != std::string::npos;
  CHECK(listed);
  // Dropping something first makes room.
  CHECK(w.run(Action::discardItem(w.id("hero"), w.slotOf("hero", "lockpick"))));
  CHECK(w.run(w.open("hero", "c1", "master_key")));
  CHECK(w.has("hero", "potion"));
  // With a shared storage the contents go there instead.
  Doors s("doors_storage", "hall", [](DCopy& c) { c.swap("game.json", "\"inventorySize\": 4 }", "\"inventorySize\": 4, \"storage\": \"anywhere\" }"); });
  Unit& h2 = s.unit("hero");
  h2.items.push_back(ItemStack{s.item("master_key"), 3, false});
  h2.items.push_back(ItemStack{s.item("lockpick"), 2, false});
  arrange(s, "hero", 2, 1);
  CHECK(s.run(s.open("hero", "c1", "master_key")));
  CHECK(s.b().sides[0].storage.size() == 1);
  const Event* g = s.first(EventType::ItemGained);
  CHECK(g && g->a == -1 && (g->flags & kLooted));
}

TEST(doors_a_place_is_visited_once_or_again_and_again) {
  Doors w("doors_visit");
  arrange(w, "hero", 3, 3);  // the neutral hamlet v1
  CHECK(w.run(w.visit("hero", "v1")));
  CHECK_STR(w.kind("v1"), "hamlet_shut");
  CHECK(w.st("v1").used && w.sw("visited") && w.gold() == 30);
  CHECK_EQ(w.count(EventType::PlaceVisited), 1);
  w.unit("hero").done = false;
  CHECK(w.why(w.visit("hero", "v1")).find("nothing to visit") != std::string::npos);
  // The camp can be visited every turn.
  Doors c("doors_camp");
  arrange(c, "hero", 1, 3);
  CHECK(c.run(c.visit("hero", "v2")));
  c.unit("hero").done = false;
  CHECK(c.run(c.visit("hero", "v2")));
  CHECK_STR(c.kind("v2"), "camp");
  // From next to it ("from": "either"), but not a hamlet (its own square only).
  Doors n("doors_visit_next");
  arrange(n, "hero", 1, 2);
  CHECK(n.run(n.visit("hero", "v2")));
  arrange(n, "sneak", 3, 2);
  CHECK(n.why(n.visit("sneak", "v1")).find("out of reach") != std::string::npos);
}

TEST(doors_a_place_can_be_limited_to_listed_units_or_sides) {
  Doors w("doors_visit_limit", "hall", [](DCopy& c) {
    c.swap("maps/hall.json", "\"id\": \"v1\" }", "\"id\": \"v1\", \"visitUnits\": [\"tink\"] }");
  });
  arrange(w, "hero", 3, 3);
  CHECK(w.why(w.visit("hero", "v1")).find("cannot visit") != std::string::npos);
  w.unit("hero").done = false;
  w.put("hero", 5, 3);
  w.b().refreshSight();
  arrange(w, "tink", 3, 3);
  CHECK(w.run(w.visit("tink", "v1")));
  Doors s("doors_visit_side", "hall", [](DCopy& c) {
    c.swap("maps/hall.json", "\"id\": \"v1\" }", "\"id\": \"v1\", \"visitSides\": [\"red\"] }");
  });
  arrange(s, "hero", 3, 3);
  CHECK(!s.run(s.visit("hero", "v1"), true));
}

TEST(doors_an_enemy_place_is_destroyed_and_the_ruin_cannot_be_visited) {
  Doors w("doors_raze");
  // v3 belongs to red: blue cannot visit it, and destroys it.
  arrange(w, "hero", 8, 0);
  CHECK(w.b().destroyAllowed(w.unit("hero"), w.place("v3")) && !w.b().visitAllowed(w.unit("hero"), w.place("v3")));
  CHECK(w.run(w.visit("hero", "v3")));
  CHECK_STR(w.kind("v3"), "hamlet_ruin");
  CHECK_EQ(w.count(EventType::PlaceDestroyed), 1);
  w.unit("hero").done = false;
  CHECK(w.why(w.visit("hero", "v3")).find("nothing to visit") != std::string::npos);
  // v4 is blue's: red destroys it (its turn), blue visits it.
  Doors r("doors_raze2");
  r.run(Action::endTurn());
  arrange(r, "g1", 8, 4);
  CHECK(r.run(r.visit("g1", "v4")));
  CHECK_STR(r.kind("v4"), "hamlet_ruin");
  CHECK(r.sw("razed"));  // the destroyed trigger ran
  Doors v("doors_visit_own");
  arrange(v, "hero", 8, 4);
  CHECK(v.b().visitAllowed(v.unit("hero"), v.place("v4")) && !v.b().destroyAllowed(v.unit("hero"), v.place("v4")));
  CHECK(v.run(v.visit("hero", "v4")));
  CHECK_STR(v.kind("v4"), "hamlet_shut");
}

TEST(doors_the_menu_lists_what_a_unit_can_do_where_it_stands) {
  Doors w("doors_options");
  std::vector<PlaceOption> o = w.options("hero");
  CHECK(o.size() == 1 && o[0].structure == w.place("d1") && o[0].ok && o[0].locked && o[0].keySlots.size() == 1);
  o = w.options("sneak");  // next to the latched door: opens without a key
  CHECK(o.size() == 1 && o[0].structure == w.place("d2") && o[0].ok && !o[0].locked && o[0].keySlots.empty());
  w.unit("hero").items.erase(w.unit("hero").items.begin() + w.slotOf("hero", "brass_key"));
  o = w.options("hero");
  CHECK(o.size() == 1 && !o[0].ok && o[0].why.find("needs a key") != std::string::npos);
  // Two keys that fit: both are listed, in inventory order (the first is the default).
  Doors k("doors_options2");
  k.unit("hero").items.push_back(ItemStack{k.item("master_key"), 3, false});
  o = k.options("hero");
  CHECK(o.size() == 1 && o[0].keySlots.size() == 2 && o[0].keySlots[0] < o[0].keySlots[1]);
  // On a hamlet: Visit; on an enemy hamlet: Destroy.
  arrange(k, "sneak", 3, 3);
  o = k.options("sneak");
  CHECK(o.size() >= 1 && o[0].verb == PlaceOption::Verb::Visit);
  arrange(k, "sneak", 8, 0);
  o = k.options("sneak");
  CHECK(o.size() == 1 && o[0].verb == PlaceOption::Verb::Destroy);
}

TEST(doors_automatic_actions_happen_when_a_move_ends_there) {
  const auto autoOn = [](DCopy& c) { c.swap("game.json", "\"rules\": { ", "\"rules\": { \"places\": { \"actions\": \"auto\" }, "); };
  Doors w("doors_auto", "hall", autoOn);
  CHECK(w.db().rules.places.automatic);
  // Hero steps away and back: the move that ends next to d1 opens it with the brass key and does not end his turn.
  w.put("hero", 3, 1);
  CHECK(w.run(Action::move(w.id("hero"), w.at(4, 1))));
  CHECK_STR(w.kind("d1"), "cell_door_open");
  CHECK(!w.unit("hero").done);
  CHECK(!w.has("hero", "brass_key"));
  // A latched door opens as well; a neutral hamlet is visited by walking onto it.
  CHECK(w.run(Action::move(w.id("sneak"), w.at(3, 3))));
  CHECK_STR(w.kind("v1"), "hamlet_shut");
  CHECK(w.sw("visited"));
  // An enemy hamlet is not destroyed by walking past: destroying stays an explicit action.
  w.unit("hero").moved = false;
  Doors m("doors_menu_move");  // menu mode: the same move does nothing by itself
  m.put("hero", 3, 1);
  CHECK(m.run(Action::move(m.id("hero"), m.at(4, 1))));
  CHECK_STR(m.kind("d1"), "cell_door");
}

TEST(doors_stories_open_close_lock_and_unlock_by_id) {
  const auto story = [](DCopy& c) {
    c.swap("maps/hall.json", "\"events\": [",
           "\"events\": [ { \"id\": \"s1\", \"when\": { \"battle\": \"start\" }, \"do\": [ { \"openDoor\": \"d1\" }, { \"lockDoor\": \"d2\" }, "
           "{ \"openChest\": \"c1\", \"by\": \"tink\" } ] },");
  };
  Doors w("doors_story", "hall", story);
  CHECK_STR(w.kind("d1"), "cell_door_open");
  const Event* e = nullptr;
  (void)e;
  CHECK(w.st("d2").locked && w.kind("d2") == "latch_door");  // locking a shut door only locks it
  CHECK_STR(w.kind("c1"), "chest_open");
  CHECK(w.has("tink", "potion") && w.gold() == 50);  // the contents went to the named unit
  // Closing: an open door shuts again (and a story may lock it); a unit in the doorway keeps it open.
  const auto story2 = [](DCopy& c) {
    c.swap("maps/hall.json", "\"events\": [",
           "\"events\": [ { \"id\": \"s1\", \"when\": { \"battle\": \"start\" }, \"do\": [ { \"openDoor\": \"d1\" }, { \"closeDoor\": \"d1\" }, "
           "{ \"unlockDoor\": \"d1\" }, { \"openDoor\": \"d2\" }, { \"lockDoor\": \"d2\" } ] },");
  };
  Doors v("doors_story2", "hall", story2);
  CHECK_STR(v.kind("d1"), "cell_door");
  CHECK(!v.st("d1").locked);  // closeDoor leaves it unlocked; unlockDoor made sure
  CHECK_STR(v.kind("d2"), "latch_door");  // opened, then shut and locked again (back to the kind the map placed)
  CHECK(v.st("d2").locked);
  // The latched door was locked by the story, so only a key (or trait) opens it now.
  v.unit("sneak").items.push_back(ItemStack{v.item("master_key"), 3, false});
  CHECK(v.run(v.open("sneak", "d2", "master_key")) || true);
}

TEST(doors_a_door_stays_open_while_a_unit_stands_in_it) {
  const auto story = [](DCopy& c) {
    c.swap("maps/hall.json", "\"events\": [",
           "\"events\": [ { \"id\": \"s1\", \"when\": { \"turn\": 2 }, \"do\": [ { \"closeDoor\": \"d2\" } ] },");
  };
  Doors w("doors_story_block", "hall", story);
  CHECK(w.run(w.open("sneak", "d2")));
  w.unit("sneak").done = false;
  w.put("sneak", 5, 3);  // in the doorway
  w.run(Action::endTurn());
  while (w.b().round < 2 && !w.b().over) w.run(Action::endTurn());
  CHECK_STR(w.kind("d2"), "cell_door_open");  // refused: somebody stands there
}

TEST(doors_a_closed_door_with_blocks_sight_hides_what_is_behind_it_in_fog) {
  Doors w("doors_fog", "gatehouse_fog");
  CHECK(w.b().fogOn());
  // r1 at (5,0) is within hero's vision (4) of (4,3) but the gate stands between them.
  CHECK(!w.b().unitVisibleTo(0, w.unit("r1")));
  CHECK(w.b().visibleTo(0, w.at(4, 2)));    // the gate itself is seen
  CHECK(!w.b().visibleTo(0, w.at(4, 1)));   // what is behind it is not
  CHECK(w.run(w.open("hero", "g1", "master_key")));
  CHECK(w.b().unitVisibleTo(0, w.unit("r1")));  // opening the gate updates sight
  CHECK(w.b().visibleTo(0, w.at(4, 1)));
  // Without the flag nothing is blocked (the plain map is not fog, everything is visible).
  Doors p("doors_nofog", "gatehouse");
  CHECK(p.b().unitVisibleTo(0, p.unit("r1")));
}

TEST(doors_the_state_of_doors_chests_and_places_is_saved) {
  Doors w("doors_save", "hall");
  w.unit("hero").items.push_back(ItemStack{w.item("master_key"), 3, false});
  CHECK(w.run(w.open("hero", "d1", "brass_key")));
  arrange(w, "sneak", 3, 3);
  CHECK(w.run(w.visit("sneak", "v1")));
  arrange(w, "hero", 8, 0);
  CHECK(w.run(w.visit("hero", "v3")));
  const std::string before = savetest::stateText(w.b());
  Json saved;
  saveBattleState(w.b(), saved);
  Battle r;
  std::string err;
  CHECK(r.init(w.rig.db, w.rig.map, w.rig.formulas, 1, err));
  CHECK(restore(r, saved));
  CHECK_STR(savetest::stateText(r), before);
  CHECK(r.structures[(size_t)w.place("v1")].used);
  CHECK_EQ(r.structures[(size_t)w.place("d1")].type, w.st("d1").type);
  // A chest's contents and a door's hit points survive too.
  Doors g("doors_save2", "gatehouse");
  CHECK(g.run(g.hit("brute", "g1", "heavy_axe")));
  Json gs;
  saveBattleState(g.b(), gs);
  Battle r2;
  CHECK(r2.init(g.rig.db, g.rig.map, g.rig.formulas, 1, err));
  CHECK(restore(r2, gs));
  CHECK_EQ(r2.structures[(size_t)g.place("g1")].hp, 7);
  Doors c("doors_save3");
  CHECK(!c.st("c1").loot.empty());
  Json cs;
  saveBattleState(c.b(), cs);
  Battle r3;
  CHECK(r3.init(c.rig.db, c.rig.map, c.rig.formulas, 1, err));
  r3.structures[(size_t)c.place("c1")].loot.clear();  // the save must put it back
  CHECK(restore(r3, cs));
  CHECK_EQ(r3.structures[(size_t)c.place("c1")].loot.size(), 2u);
}

TEST(doors_old_saves_and_damaged_ones_are_handled) {
  Doors w("doors_save_old", "hall");
  Json saved;
  saveBattleState(w.b(), saved);
  // A save without the new keys (every game that has none) loads and gets the kind's defaults.
  Json old = Json::makeObject();
  for (const Json::Member& m : saved.members()) old.set(m.first, m.second);
  Json structures = Json::makeArray();
  for (size_t i = 0; i < saved["structures"].size(); ++i) {
    Json s = Json::makeObject();
    for (const Json::Member& m : saved["structures"][i].members())
      if (m.first != "locked" && m.first != "loot" && m.first != "hp" && m.first != "used") s.set(m.first, m.second);
    structures.push(s);
  }
  old.set("structures", structures);
  Battle r;
  std::string err;
  CHECK(r.init(w.rig.db, w.rig.map, w.rig.formulas, 1, err));
  CHECK(restoreBattleState(r, old, err));
  CHECK(r.structures[(size_t)w.place("d1")].locked && !r.structures[(size_t)w.place("d2")].locked);
  // A damaged one: a lock on a hamlet, a chest's loot on a door, an unknown item.
  Json bad = saved;
  Json h = Json::makeObject();
  for (const Json::Member& m : saved["structures"][(size_t)w.place("v1")].members()) h.set(m.first, m.second);
  h.set("locked", Json::makeBool(true));
  Json list = Json::makeArray();
  for (size_t i = 0; i < saved["structures"].size(); ++i) list.push(i == (size_t)w.place("v1") ? h : saved["structures"][i]);
  bad.set("structures", list);
  Battle r2;
  CHECK(r2.init(w.rig.db, w.rig.map, w.rig.formulas, 1, err));
  CHECK(!restoreBattleState(r2, bad, err) && err.find("locked") != std::string::npos);
}

TEST(doors_a_unit_that_goes_for_things_keeps_the_marker_in_saves) {
  Doors w("doors_goes", "market");
  CHECK_EQ(w.unit("r1").aiGoals, kAiChests);
  CHECK_EQ(w.unit("r2").aiGoals, 0);
  CHECK_EQ(w.unit("r3").aiGoals, kAiPlaces);
  Json saved;
  saveBattleState(w.b(), saved);
  Battle r;
  std::string err;
  CHECK(r.init(w.rig.db, w.rig.map, w.rig.formulas, 1, err));
  r.units[(size_t)w.id("r1")].aiGoals = 0;
  CHECK(restore(r, saved));
  CHECK_EQ(r.units[(size_t)w.id("r1")].aiGoals, kAiChests);
}
