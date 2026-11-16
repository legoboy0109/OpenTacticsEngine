// Weapon ranks in a battle (docs/SRPG.md section 7): starting ranks, the equip test, experience from use, promotion,
// formulas and the computer opponent.
#include <algorithm>

#include "core/ai/ai.h"
#include "srpg_ranks_fixture.h"

using namespace ote;
using namespace rankstest;

TEST(ranks_start_from_class_character_and_map) {
  World w("ranks_start", "duel");
  const int sword = w.group("sword"), bow = w.group("bow");
  // kell: class fighter "e" (0), character 4: the higher wins = experience 4, rank D (index 1).
  CHECK_EQ(w.unit("kell").weaponXp[(size_t)sword], 4);
  CHECK_EQ(w.b().weaponRank(w.unit("kell"), sword), 1);
  CHECK_EQ(w.unit("kell").weaponXp[(size_t)bow], 0);
  CHECK_EQ(w.unit("mara").weaponXp[(size_t)bow], 3);  // archer "d"
  CHECK_EQ(w.unit("g1").weaponXp[(size_t)sword], 3);  // grunt "d"
  World f("ranks_start_field", "field");
  // The map's own "ranks" replace the class's value of the groups it names.
  int e = 0, d = 0;
  for (const Unit& u : f.b().units)
    if (f.rig.db.unitTypes[(size_t)u.type].id == "grunt") (u.weaponXp[(size_t)f.group("sword")] == 0 ? e : d)++;
  CHECK_EQ(e, 1);
  CHECK_EQ(d, 1);
}

TEST(ranks_a_weapon_above_the_rank_is_carried_but_not_wielded) {
  World w("ranks_equip", "duel");
  Unit& kell = w.unit("kell");
  std::vector<int> weapons;
  w.b().weaponsOf(kell, weapons);
  CHECK_EQ((int)weapons.size(), 2);  // bronze and steel; the war sword needs B
  CHECK(std::find(weapons.begin(), weapons.end(), w.weapon("war_sword")) == weapons.end());
  CHECK_EQ(w.b().weaponSlot(kell, w.weapon("war_sword")), -1);
  std::string why;
  CHECK(!w.b().canWieldNow(kell, w.weapon("war_sword"), &why));
  CHECK_STR(why, "Needs rank B");
  CHECK(w.b().canWieldNow(kell, w.weapon("steel_sword"), &why));
  // Equip: refused with the reason, allowed for the steel sword (rank D is enough).
  CHECK_STR(w.why(Action::equip(kell.id, w.slotOf("kell", "war_sword"))), "Needs rank B");
  CHECK_STR(w.why(Action::equip(kell.id, w.slotOf("kell", "steel_sword"))), "");
  // A weapon that is first in the pack but too high is not "the equipped weapon".
  std::swap(kell.items[0], kell.items[2]);
  CHECK_EQ(w.b().equippedSlot(kell), 1);
  // Attack options and the forecast skip it too.
  std::vector<AttackOption> opts;
  w.b().attackOptions(kell.id, kell.cell, opts);
  for (const AttackOption& o : opts) CHECK(o.weapon != w.weapon("war_sword"));
  Forecast fc;
  CHECK(!w.b().forecast(kell.id, w.id("g1"), w.weapon("war_sword"), kell.cell, fc));
  CHECK(w.b().forecast(kell.id, w.id("g1"), w.weapon("steel_sword"), kell.cell, fc));
  // It can be traded (mara stands next to kell) and discarded.
  CHECK_STR(w.why(Action::giveItem(kell.id, w.slotOf("kell", "war_sword"), w.id("mara"))), "");
  CHECK_STR(w.why(Action::discardItem(kell.id, w.slotOf("kell", "war_sword"))), "");
}

TEST(ranks_experience_from_use_and_the_rank_up_event) {
  World w("ranks_use", "duel", [](Copy& c) { c.swap("game.json", "\"hit\": \"90\"", "\"hit\": \"100\""); });
  Unit& kell = w.unit("kell");
  const int sword = w.group("sword");
  // Steel sword: rankXp 2. Experience 4 -> 6: no new rank (C needs 7). The grunt's counter trains it too (bronze: 1).
  CHECK(w.run(Action::attack(kell.id, w.id("g1"), w.weapon("steel_sword"))));
  CHECK_EQ(w.unit("kell").weaponXp[(size_t)sword], 6);
  CHECK_EQ(w.unit("g1").weaponXp[(size_t)sword], 4);
  CHECK_EQ(w.count(EventType::RankUp), 0);
  // Again: 6 + 2 = 8, rank C.
  w.unit("kell").done = w.unit("kell").moved = false;  // the next turn (the opponent's move is not what is tested)
  CHECK(w.run(Action::attack(w.id("kell"), w.id("g1"), w.weapon("steel_sword"))));
  CHECK_EQ(w.unit("kell").weaponXp[(size_t)sword], 8);
  const Event* up = w.first(EventType::RankUp);
  CHECK(up != nullptr);
  if (up) {
    CHECK_EQ(up->a, w.id("kell"));
    CHECK_EQ(up->b, sword);
    CHECK_EQ(up->c, 2);  // C
    CHECK_EQ(up->d, 1);  // from D
  }
}

TEST(ranks_experience_stops_at_the_top_and_a_weapon_without_a_group_trains_nothing) {
  World w("ranks_top", "duel", [](Copy& c) { c.swap("game.json", "\"hit\": \"90\"", "\"hit\": \"100\""); });
  const int sword = w.group("sword");
  w.unit("kell").weaponXp[(size_t)sword] = 11;
  CHECK(w.run(Action::attack(w.id("kell"), w.id("g1"), w.weapon("steel_sword"))));
  CHECK_EQ(w.unit("kell").weaponXp[(size_t)sword], 12);  // 11 + 2 stops at B's 12
  CHECK_EQ(w.b().weaponRank(w.unit("kell"), sword), 3);
  // A stick has no group: nothing is gained, nothing breaks.
  World s("ranks_stick", "duel", [](Copy& c) {
    c.swap("game.json", "\"hit\": \"90\"", "\"hit\": \"100\"");
    c.swap("maps/duel.json", "\"character\": \"kell\", \"side\": \"blue\", \"x\": 1, \"y\": 1, \"leader\": true",
           "\"character\": \"kell\", \"side\": \"blue\", \"x\": 1, \"y\": 1, \"leader\": true, \"items\": [\"stick\"]");
  });
  CHECK(s.run(Action::attack(s.id("kell"), s.id("g1"), s.weapon("stick"))));
  CHECK_EQ(s.unit("kell").weaponXp[(size_t)s.group("sword")], 4);
  CHECK_EQ(s.count(EventType::RankUp), 0);
}

TEST(ranks_fixed_growth_gains_nothing_from_use) {
  World w("ranks_fixed", "duel", [](Copy& c) {
    c.swap("data/ranks.json", "\"growth\": \"use\"", "\"growth\": \"fixed\"");
    c.swap("game.json", "\"hit\": \"90\"", "\"hit\": \"100\"");
  });
  CHECK(w.run(Action::attack(w.id("kell"), w.id("g1"), w.weapon("steel_sword"))));
  CHECK_EQ(w.unit("kell").weaponXp[(size_t)w.group("sword")], 4);
  CHECK_EQ(w.count(EventType::RankUp), 0);
}

TEST(ranks_a_unit_that_dies_gains_nothing) {
  World w("ranks_dead", "duel", [](Copy& c) {
    c.swap("game.json", "\"hit\": \"90\"", "\"hit\": \"100\"");
    c.swap("maps/duel.json", "\"stats\": { \"hp\": 60, \"str\": 1, \"def\": 1 }", "\"stats\": { \"hp\": 1, \"str\": 1, \"def\": 0 }");
  });
  CHECK(w.run(Action::attack(w.id("kell"), w.id("g1"), w.weapon("bronze_sword"))));
  CHECK(!w.unit("g1").alive);
  CHECK_EQ(w.unit("kell").weaponXp[(size_t)w.group("sword")], 5);  // the killer: 4 + 1
  CHECK_EQ(w.unit("g1").weaponXp[(size_t)w.group("sword")], 3);    // the fallen: unchanged
}

TEST(ranks_promotion_raises_to_the_new_class_and_keeps_more) {
  World w("ranks_promo", "duel");
  const int sword = w.group("sword");
  // kell: 4 (D). Knight starts at C = 7: raised, with a RankUp.
  CHECK(w.run(Action::promote(w.id("kell"))));
  CHECK_EQ(w.unit("kell").weaponXp[(size_t)sword], 7);
  const Event* up = w.first(EventType::RankUp);
  CHECK(up != nullptr);
  if (up) { CHECK_EQ(up->c, 2); CHECK_EQ(up->d, 1); }
  // A unit that earned more keeps it.
  World k("ranks_promo2", "duel");
  k.unit("kell").weaponXp[(size_t)sword] = 10;
  CHECK(k.run(Action::promote(k.id("kell"))));
  CHECK_EQ(k.unit("kell").weaponXp[(size_t)sword], 10);
  CHECK_EQ(k.count(EventType::RankUp), 0);
}

TEST(ranks_formulas_see_the_rank) {
  World w("ranks_formula", "duel", [](Copy& c) { c.swap("game.json", "\"hit\": \"90\"", "\"hit\": \"50 + 10 * a.rank.sword\""); });
  Forecast fc;
  CHECK(w.b().forecast(w.id("kell"), w.id("g1"), w.weapon("bronze_sword"), w.unit("kell").cell, fc));
  CHECK_EQ(fc.attack.hit, 60);  // rank D = index 1
}

TEST(ranks_the_computer_never_picks_a_weapon_it_cannot_wield) {
  World w("ranks_ai", "duel", [](Copy& c) {
    c.swap("maps/duel.json", "\"id\": \"g2\", \"side\": \"red\", \"x\": 5, \"y\": 2",
           "\"id\": \"g2\", \"side\": \"red\", \"x\": 5, \"y\": 2, \"items\": [\"war_sword\"]");
  });
  std::vector<int> weapons;
  w.b().weaponsOf(w.unit("g2"), weapons);
  CHECK(weapons.empty());  // it carries a war sword and has rank D
  // Played through, the AI never attacks with it and every turn ends.
  std::vector<Event> ev;
  long steps = 0;
  w.b().apply(Action::endTurn(), ev);
  while (w.b().activeSide != 0 && ++steps < 200) aiTakeStep(w.b(), ev);
  CHECK(steps < 200);
  for (const Event& e : ev)
    if (e.type == EventType::Attack && e.a == w.id("g2")) CHECK(false);
}

TEST(ranks_survive_a_save_and_the_roster) {
  World w("ranks_save", "duel");
  w.unit("kell").weaponXp[(size_t)w.group("sword")] = 9;
  Json j;
  saveBattleState(w.b(), j);
  CHECK(j["units"][(size_t)w.id("kell")].has("ranks"));
  CHECK_EQ((int)j["units"][(size_t)w.id("kell")]["ranks"]["sword"].asNumber(), 9);
  CHECK(!j["units"][(size_t)w.id("g2")]["ranks"].has("bow"));  // only groups above 0
  savetest::Rig again;
  CHECK(again.load(w.copy.dir, "duel"));
  std::string err;
  CHECK(restoreBattleState(again.battle, j, err));
  CHECK_EQ(again.battle.units[(size_t)w.id("kell")].weaponXp[(size_t)w.group("sword")], 9);
  CHECK_STR(savetest::stateText(again.battle), savetest::stateText(w.b()));
  // Damaged saves are refused with a path.
  auto edited = [&](const std::string& to) {
    std::string text = j.dump(0);
    const std::string from = "\"ranks\":{\"sword\":9}";
    const size_t at = text.find(from);
    CHECK(at != std::string::npos);
    text.replace(at, from.size(), to);
    Json out;
    std::string perr;
    CHECK(Json::parse(text, out, perr));
    return out;
  };
  CHECK(!restoreBattleState(again.battle, edited("\"ranks\":{\"axe\":3}"), err));
  CHECK(saysIn(err, "units[" + std::to_string(w.id("kell")) + "].ranks.axe: not a weapon group of this game"));
  CHECK(!restoreBattleState(again.battle, edited("\"ranks\":{\"sword\":13}"), err));
  CHECK(saysIn(err, "must be a whole number from 0 to 12"));
  // A roster hands a unit's experience on and never lowers what the class gives.
  auto roster = std::make_shared<Roster>();
  RosterUnit r;
  r.id = "kell";
  r.type = w.rig.db.findUnitType("fighter");
  r.name = "Kell";
  r.level = 3;
  r.stats = w.unit("kell").stats;
  r.weaponXp = {9, 0};
  roster->push_back(r);
  savetest::Rig fresh;
  CHECK(fresh.load(w.copy.dir, "duel"));
  fresh.battle.setStartingRoster(roster);
  CHECK_EQ(fresh.battle.units[(size_t)w.id("kell")].weaponXp[(size_t)w.group("sword")], 9);
}
