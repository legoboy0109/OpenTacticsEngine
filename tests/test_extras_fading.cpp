// A status condition whose effects fade (docs/SRPG.md "Effects that fade"): "scale": "turnsLeft" makes the numbers of its "always" blocks
// count as many times as it has turns left (stats, vision), shown in the unit's numbers, the forecast and the saves.
// Fixture tests/fixtures/srpg-skillblocks2 (carried iron swords, 100 % hit, one strike): "thief" (blue rogue, str 5, def 2, 0,1) attacking "mark"
// (red guard, def 4, hp 30, 1,1) with an iron sword (might 3) does 5 + 3 - 4 = 4.
#include <functional>
#include <memory>

#include "srpg_skills_fixture.h"

using namespace ote;
using skilltest::Copy;
using skilltest::World;
using skilltest::saysIn;

namespace {

const char* kFix = "srpg-skillblocks2";

// The fixture's own conditions (its abilities cure some of them) plus these.
std::string conditions(const std::string& list) {
  std::string base;
  readTextFile(ote_test::sourceDir() + "/tests/fixtures/" + kFix + "/data/conditions.json", base);
  base = base.substr(base.find('[') + 1, base.rfind(']') - base.find('[') - 1);
  return "{ \"conditions\": [" + base + "," + list + "] }";
}

// A fading condition: `ward` (4 turns) gives +1 defence for each turn it has left; `might` gives +1 strength per turn left; `torch` gives
// +1 vision per turn left; `bane` does not fade (-2 strength).
const char* kFading = R"(
  { "id": "ward", "name": "Ward", "tag": "Wd", "turns": 4, "scale": "turnsLeft", "blocks": [ { "when": "always", "do": [ { "stats": { "def": 1 } } ] } ] },
  { "id": "might", "name": "Might", "tag": "Mi", "turns": 4, "scale": "turnsLeft", "blocks": [ { "when": "always", "do": [ { "stats": { "str": 1 } } ] } ] },
  { "id": "torch", "name": "Torch", "tag": "To", "turns": 4, "scale": "turnsLeft", "blocks": [ { "when": "always", "do": [ { "vision": 1 } ] } ] },
  { "id": "bane", "name": "Bane", "tag": "Ba", "turns": 4, "blocks": [ { "when": "always", "do": [ { "stats": { "str": -2 } } ] } ] })";

struct Game {
  Copy c;
  std::unique_ptr<World> w;
  Game(const char* name, const std::string& skills, const std::vector<std::pair<const char*, const char*>>& learn,
       const std::function<void(Copy&)>& tweak = nullptr)
      : c(name, kFix) {
    c.skills(skills);
    c.put("data/conditions.json", conditions(kFading));
    if (tweak) tweak(c);
    w.reset(new World("field", c.dir));
    for (const auto& l : learn) w->unit(l.first).learned.push_back(w->skill(l.second));
  }
  void give(const char* unit, const char* cond, int left) {
    ConditionState cs;
    cs.condition = w->db().findCondition(cond);
    cs.left = left;
    w->unit(unit).conditions.push_back(cs);
  }
  int weapon(const char* unit) { return w->b().restingWeapon(w->unit(unit)); }
  Forecast forecast(const char* from, const char* to) {
    Forecast f;
    CHECK(w->b().forecast(w->id(from), w->id(to), weapon(from), w->unit(from).cell, f));
    return f;
  }
  std::vector<int> strikes(const char* from) {
    std::vector<int> out;
    for (const Event& e : w->events)
      if (e.type == EventType::Attack && e.a == w->id(from)) out.push_back(e.d);
    return out;
  }
  bool fight(const char* from, const char* to) { return w->apply(Action::attack(w->id(from), w->id(to), weapon(from))); }
  void twoRounds() { CHECK(w->apply(Action::endTurn()) && w->apply(Action::endTurn())); }  // blue, red: blue's next turn starts
};

}  // namespace

// ---- a condition whose effects fade ----

TEST(fading_stats_count_the_turns_left_and_the_unit_info_follows) {
  Game g("ex_fade_stats", "", {});
  g.give("thief", "ward", 4);
  CHECK_EQ(g.w->stat(g.w->unit("thief"), "def"), 2 + 4);  // 4 turns left: +1 four times
  g.twoRounds();                                           // its own turn starts: one turn less
  CHECK_EQ(g.w->stat(g.w->unit("thief"), "def"), 2 + 3);
  g.twoRounds();
  CHECK_EQ(g.w->stat(g.w->unit("thief"), "def"), 2 + 2);
  g.twoRounds();
  CHECK_EQ(g.w->stat(g.w->unit("thief"), "def"), 2 + 1);
  g.twoRounds();  // the last turn: nothing is left of it, and it ends with the turn
  CHECK_EQ(g.w->stat(g.w->unit("thief"), "def"), 2 + 0);
  CHECK_EQ(g.w->unit("thief").conditions.size(), (size_t)1);
}
TEST(fading_a_plain_condition_is_not_scaled) {
  Game g("ex_fade_plain", "", {});
  g.give("thief", "bane", 4);
  CHECK_EQ(g.w->stat(g.w->unit("thief"), "str"), 5 - 2);
  g.twoRounds();
  CHECK_EQ(g.w->stat(g.w->unit("thief"), "str"), 5 - 2);  // always -2 while it lasts
}
TEST(fading_the_forecast_and_the_fight_use_the_current_value) {
  Game g("ex_fade_fc", "", {});
  g.give("thief", "might", 4);
  CHECK_EQ(g.forecast("thief", "mark").attack.damage, 4 + 4);
  g.twoRounds();
  CHECK_EQ(g.forecast("thief", "mark").attack.damage, 4 + 3);
  CHECK(g.fight("thief", "mark"));
  CHECK_EQ(g.strikes("thief")[0], 4 + 3);
}
TEST(fading_vision_is_the_turns_left) {
  Game g("ex_fade_vis", "", {});
  CHECK(g.w->db().visionSkills);
  CHECK_EQ(g.w->b().skillVision(g.w->unit("thief")), 0);
  g.give("thief", "torch", 4);
  CHECK_EQ(g.w->b().skillVision(g.w->unit("thief")), 4);
  g.twoRounds();
  CHECK_EQ(g.w->b().skillVision(g.w->unit("thief")), 3);
}
TEST(fading_two_conditions_add_up_and_the_count_is_saved) {
  Game g("ex_fade_save", "", {});
  g.give("thief", "might", 3);
  g.give("thief", "ward", 2);
  CHECK_EQ(g.w->stat(g.w->unit("thief"), "str"), 5 + 3);
  CHECK_EQ(g.w->stat(g.w->unit("thief"), "def"), 2 + 2);
  Json saved;
  saveBattleState(g.w->b(), saved);
  Battle r;
  std::string err;
  CHECK(r.init(g.w->rig.db, g.w->rig.map, g.w->rig.formulas, 7, err));
  CHECK(restoreBattleState(r, saved, err));
  const Unit& u = r.units[(size_t)g.w->id("thief")];
  CHECK_EQ(r.stat(u, g.w->db().findStat("str")), 5 + 3);
  CHECK_EQ(r.stat(u, g.w->db().findStat("def")), 2 + 2);
}
TEST(fading_a_usable_item_inflicts_it_on_its_user) {
  Game g("ex_fade_item", "", {}, [](Copy& c) {
    c.swap("data/items.json", "{ \"id\": \"relic\"",
           "{ \"id\": \"ward_tonic\", \"name\": \"Ward Tonic\", \"kind\": \"usable\", \"uses\": 1, \"inflict\": { \"condition\": \"ward\", \"turns\": 3 } },\n  { \"id\": \"relic\"");
  });
  ItemStack tonic;
  tonic.item = g.w->db().findItem("ward_tonic");
  tonic.uses = 1;
  g.w->unit("thief").items.push_back(tonic);
  const int slot = (int)g.w->unit("thief").items.size() - 1;
  CHECK(g.w->apply(Action::useItem(g.w->id("thief"), slot, g.w->id("thief"))));
  CHECK_EQ(g.w->unit("thief").conditions.size(), (size_t)1);
  CHECK_EQ(g.w->unit("thief").conditions[0].left, 3);
  CHECK_EQ(g.w->stat(g.w->unit("thief"), "def"), 2 + 3);
}
TEST(fading_loader_refusals) {
  auto err = [](const char* name, const std::string& cond) {
    Copy c(name, kFix);
    c.put("data/conditions.json", conditions(cond));
    return c.gameError();
  };
  CHECK(saysIn(err("ex_fn1", R"({ "id": "x", "turns": 4, "scale": "always", "blocks": [ { "when": "always", "do": [ { "stats": { "def": 1 } } ] } ] })"),
               "\"scale\" must be \"turnsLeft\""));
  CHECK(saysIn(err("ex_fn2", R"({ "id": "x", "until": "cured", "scale": "turnsLeft", "blocks": [ { "when": "always", "do": [ { "stats": { "def": 1 } } ] } ] })"),
               "needs a condition that counts turns"));
  CHECK(saysIn(err("ex_fn3", R"({ "id": "x", "turns": 3, "scale": "turnsLeft", "blocks": [ { "when": "combat", "do": [ { "hit": 5 } ] } ] })"),
               "has nothing to scale"));
  CHECK(saysIn(err("ex_fn4", R"({ "id": "x", "turns": 3, "scale": 3, "blocks": [ { "when": "always", "do": [ { "stats": { "def": 1 } } ] } ] })"),
               "\"scale\" must be \"turnsLeft\""));
  CHECK_STR(err("ex_fn5", kFading), "");
}

