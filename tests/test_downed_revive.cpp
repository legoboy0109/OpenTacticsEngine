// Revive (docs/SRPG.md "Downed units and revive"): an ability, an area ability, an item, an "autoRevive" condition; saves, determinism and the
// computer. The rule itself: test_downed.cpp; staves: test_downed_staves.cpp. Game: tests/downed_fixture.h.
#include "core/battle/save.h"
#include "downed_fixture.h"

using namespace ote;
using namespace acf;
using namespace downedtest;

namespace {

const Event* revived(const World& w, int unit) {
  for (const Event& e : w.events)
    if (e.type == EventType::UnitRevived && e.a == unit) return &e;
  return nullptr;
}

// Blue (the computer) plays its turn to the end.
void blueAiTurn(World& w) {
  w.events.clear();
  int guard = 0;
  while (!w.b().over && w.b().activeSide == 0 && aiTakeStep(w.b(), w.events) && ++guard < 200) {
  }
}

}  // namespace

TEST(revive_an_ability_stands_a_downed_ally_up_with_its_amount) {
  World w(make("revive_ability", downedOpts()));
  lay(w, "pal");
  const int hero = w.id("hero"), pal = w.id("pal");
  // The menu offers it, and the downed ally is its one target (the player and the computer ask this way).
  std::vector<AbilityOption> menu;
  w.b().abilityOptions(hero, w.unit("hero").cell, menu);
  bool offered = false;
  for (const AbilityOption& o : menu) offered = offered || (o.ability == w.ability("raise") && o.ok);
  CHECK(offered);
  std::vector<int> targets;
  std::vector<Cell> cells;
  w.b().abilityTargets(hero, w.ability("raise"), w.unit("hero").cell, targets, cells);
  CHECK(targets == std::vector<int>({pal}));
  CHECK(w.run(Action::useAbility(hero, w.ability("raise"), pal)));
  const Unit& u = w.unit("pal");
  CHECK(u.alive && !u.downed && u.hp == 15);  // 50% of 30
  CHECK(u.moved && u.done);                   // revived in its own side's turn: that turn is over for it
  const Event* e = revived(w, pal);
  CHECK(e && e->b == 15 && e->c == hero && e->d == w.ability("raise") && e->flags == 0);
  CHECK(w.b().exists(u));
}

TEST(revive_an_area_revives_only_the_downed_units_in_it) {
  World w(make("revive_area", downedOpts()));
  lay(w, "pal");
  w.unit("hero").hp = 10;
  const int hero = w.id("hero"), pal = w.id("pal");
  AbilityAreaForecast f;
  CHECK(w.b().abilityAreaForecast(hero, w.ability("arise"), w.unit("hero").cell, w.unit("pal").cell, f));
  CHECK_EQ(f.hits.size(), (size_t)1);
  CHECK(f.hits.size() == 1 && f.hits[0].unit == pal && f.hits[0].revive == 5);
  CHECK(w.run(Action::useAbility(hero, w.ability("arise"), -1, w.unit("pal").cell)));
  CHECK(w.unit("pal").alive && w.unit("pal").hp == 5);
  CHECK_EQ(w.unit("hero").hp, 10);  // the standing user in the area is not touched
  CHECK_EQ(w.count(EventType::UnitRevived), 1);
}

TEST(revive_an_item_on_a_downed_neighbour) {
  World w(make("revive_item", downedOpts()));
  lay(w, "pal");
  const int hero = w.id("hero"), pal = w.id("pal");
  const int slot = slotOf(w, "hero", "phoenix");
  CHECK(slot >= 0);
  CHECK(w.why(Action::useItem(hero, slot, w.id("bandit"))) != "");  // a standing foe
  CHECK(w.run(Action::useItem(hero, slot, pal)));
  CHECK(w.unit("pal").alive && w.unit("pal").hp == 7);
  const Event* e = revived(w, pal);
  CHECK(e && (e->flags & kByItem) && e->c == hero);
  CHECK_EQ(slotOf(w, "hero", "phoenix"), -1);  // one use
}

TEST(revive_an_item_is_refused_on_a_standing_unit) {
  World w(make("revive_item_standing", downedOpts()));
  CHECK(w.why(Action::useItem(w.id("hero"), slotOf(w, "hero", "phoenix"), w.id("pal"))) != "");
}

TEST(revive_auto_revive_catches_the_fall_once) {
  World w(make("revive_auto", downedOpts()));
  Unit& bandit = w.unit("bandit");
  bandit.conditions.push_back(ConditionState{w.cond("burn"), 3});
  bandit.conditions.push_back(ConditionState{w.cond("reraise"), -1});
  const Cell cell = bandit.cell;
  CHECK(heroStrikes(w, 1));
  const Unit& u = w.unit("bandit");
  CHECK(u.alive && !u.downed && u.hp == 4 && u.cell == cell);  // 20% of 20
  CHECK(w.has("bandit", "burn") && !w.has("bandit", "reraise"));
  CHECK(w.first(EventType::UnitDied) == nullptr);
  const Event* e = withFlag(w, EventType::UnitRevived, kAuto);
  CHECK(e && e->a == w.id("bandit") && e->d == w.cond("reraise") && e->b == 4);
  const Event* ended = w.first(EventType::ConditionEnded);
  CHECK(ended && ended->c == kConditionRevived);
}

TEST(revive_saves_keep_a_downed_unit_and_its_turns) {
  World w(make("revive_save", downedOpts("{\"removeAfter\":3}")));
  CHECK(heroStrikes(w, 1));
  w.redTurn();
  CHECK_EQ(w.unit("bandit").downedTurns, 1);
  Json saved;
  saveBattleState(w.b(), saved);
  CHECK(saved.dump(0).find("\"downed\":true") != std::string::npos);
  savetest::Rig r;
  CHECK(r.load(w.rig.gameDir, "field", 11));
  std::string err;
  CHECK(restoreBattleState(r.battle, saved, err));
  CHECK_STR(err, "");
  const Unit& u = r.battle.units[(size_t)w.id("bandit")];
  CHECK(u.downed && !u.alive && u.downedTurns == 1 && u.cell == w.unit("bandit").cell);
  CHECK_EQ(r.battle.occupant[(size_t)u.cell], u.id);
  CHECK_STR(savetest::stateText(r.battle), savetest::stateText(w.b()));
  // A game without downed units writes no new key.
  World plain(make("revive_save_plain", Opts()));
  CHECK(savetest::stateText(plain.b()).find("downed") == std::string::npos);
}

TEST(revive_saves_refuse_a_downed_unit_with_hp) {
  World w(make("revive_save_bad", downedOpts()));
  CHECK(heroStrikes(w, 1));
  Json saved;
  saveBattleState(w.b(), saved);
  std::string text = saved.dump(0);
  savetest::Rig r;
  CHECK(r.load(w.rig.gameDir, "field", 11));
  // The bandit's "hp": 0 next to its "downed": true becomes 3.
  const size_t at = text.find("\"downed\":true");
  CHECK(at != std::string::npos);
  const size_t hp = text.rfind("\"hp\":0", at);
  CHECK(hp != std::string::npos);
  text.replace(hp, 6, "\"hp\":3");
  Json bad;
  std::string err;
  CHECK(Json::parse(text, bad, err));
  CHECK(!restoreBattleState(r.battle, bad, err));
  CHECK(says(err, "downed"));
}

TEST(revive_the_same_play_gives_the_same_events) {
  std::vector<Event> runs[2];
  std::string states[2];
  for (int i = 0; i < 2; ++i) {
    World w(make(i ? "revive_det_b" : "revive_det_a", downedOpts("{\"removeAfter\":2}")), 5);
    std::vector<Event> all;
    CHECK(heroStrikes(w, 1));
    all.insert(all.end(), w.events.begin(), w.events.end());
    w.redTurn();
    all.insert(all.end(), w.events.begin(), w.events.end());
    runs[i] = all;
    states[i] = savetest::stateText(w.b());
  }
  CHECK(savetest::sameEvents(runs[0], runs[1]));
  CHECK_STR(states[0], states[1]);
}

// ---- the computer ----

TEST(revive_the_computer_revives_a_downed_ally) {
  Opts o = downedOpts();
  o.blueAi = true;
  o.mageItems = "[\"iron_sword\"]";  // no Phoenix: the ability it is
  o.mageAbilities = "[\"raise\",\"hex\"]";
  World w(make("revive_ai", o));
  lay(w, "pal");
  blueAiTurn(w);
  CHECK(w.unit("pal").alive);
  CHECK(revived(w, w.id("pal")) != nullptr);
}

TEST(revive_the_computer_uses_a_revive_item) {
  Opts o = downedOpts();
  o.blueAi = true;
  o.mageAbilities = "[\"hex\"]";  // only the Phoenix revives
  World w(make("revive_ai_item", o));
  lay(w, "pal");
  blueAiTurn(w);
  CHECK(w.unit("pal").alive && w.unit("pal").hp == 7);
  const Event* e = revived(w, w.id("pal"));
  CHECK(e && (e->flags & kByItem));
}

TEST(revive_the_computer_keeps_its_revives_when_nobody_is_down) {
  Opts o = downedOpts();
  o.blueAi = true;
  World w(make("revive_ai_none", o));
  blueAiTurn(w);
  for (const Event& e : w.events) {
    CHECK(!(e.type == EventType::AbilityUsed && (e.b == w.ability("raise") || e.b == w.ability("arise"))));
    CHECK(e.type != EventType::ItemUsed);
  }
  CHECK(slotOf(w, "hero", "phoenix") >= 0);
}

TEST(revive_the_computer_leaves_a_downed_foe_alone) {
  Opts o = downedOpts();
  o.blueAi = true;
  World w(make("revive_ai_foe", o));
  lay(w, "bandit");
  blueAiTurn(w);
  CHECK(w.unit("bandit").downed);  // not revived (arise could reach it) and not struck
  CHECK(revived(w, w.id("bandit")) == nullptr);
}
