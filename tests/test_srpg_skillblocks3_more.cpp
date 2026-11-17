// Docs/SRPG.md "Skill blocks, third set", part two: a steal condition that reads the item, a recoil that hurts the wielder, a light that shrinks, and
// the computer opponent's cure and refresh. Fixture tests/fixtures/srpg-skillblocks2 (see test_srpg_skillblocks3.cpp); the light tests use
// tests/fixtures/srpg-staves2 ("mist": fog, blue sage at 1,2, red g2 at 7,2).
#include <functional>
#include <memory>

#include "core/ai/ai.h"
#include "core/battle/save.h"
#include "json_edit.h"
#include "srpg_skills_fixture.h"
#include "srpg_staves2_fixture.h"

using namespace ote;
using skilltest::Copy;
using skilltest::World;
using skilltest::saysIn;

namespace {

const char* kFix = "srpg-skillblocks2";

std::string skill(const std::string& id, const std::string& when, const std::string& dos, const std::string& extra = "") {
  return "{ \"id\": \"" + id + "\", \"blocks\": [ { \"when\": \"" + when + "\"," + extra + " \"do\": [" + dos + "] } ] }";
}

struct Game {
  Copy c;
  std::unique_ptr<World> w;
  Game(const char* name, const std::string& skills, const std::vector<std::pair<const char*, const char*>>& learn,
       const std::function<void(Copy&)>& tweak = nullptr)
      : c(name, kFix) {
    if (!skills.empty()) c.skills(skills);
    if (tweak) tweak(c);
    w.reset(new World("field", c.dir));
    for (const auto& l : learn) w->unit(l.first).learned.push_back(w->skill(l.second));
  }
  int weapon(const char* unit) { return w->b().restingWeapon(w->unit(unit)); }
  Forecast forecast(const char* from, const char* to) {
    Forecast f;
    CHECK(w->b().forecast(w->id(from), w->id(to), weapon(from), w->unit(from).cell, f));
    return f;
  }
  bool fight(const char* from, const char* to) { return w->apply(Action::attack(w->id(from), w->id(to), weapon(from))); }
  std::vector<const Event*> of(EventType t) {
    std::vector<const Event*> out;
    for (const Event& e : w->events)
      if (e.type == t) out.push_back(&e);
    return out;
  }
};

Action stealOf(World& w, const char* ability, const char* from, const char* foe, int slot) {
  Action a = Action::useAbility(w.id(from), w.db().findAbility(ability), w.id(foe));
  a.slot = slot;
  return a;
}

// A thief who steals only what its strength can carry: the iron sword weighs 7, the potion 1 (weapons carry `weight` in weapons.json,
// a potion in its own "props"); the relic is marked unstealable.
void lightHands(Copy& c, const std::string& condition = "it.weight < a.str", const std::string& extra = ", \"reason\": \"too heavy\"") {
  c.swap("data/weapons.json", "\"group\": \"sword\", \"props\": { \"might\": 3 } }", "\"group\": \"sword\", \"props\": { \"might\": 3, \"weight\": 7 } }");
  c.swap("data/items.json", "{ \"id\": \"potion\", \"name\": \"Potion\", \"kind\": \"usable\",", "{ \"id\": \"potion\", \"name\": \"Potion\", \"kind\": \"usable\", \"props\": { \"weight\": 1 },");
  c.swap("data/abilities.json", "{ \"abilities\": [", "{ \"abilities\": [\n  { \"id\": \"light_hands\", \"name\": \"Light Hands\", \"target\": \"enemy\", \"effect\": { \"steal\": { \"if\": \"" + condition +
                                    "\", \"weapons\": true, \"equipped\": true" + extra + " } } },");
  c.swap("data/units.json", "\"abilities\": [\"steal\", ", "\"abilities\": [\"light_hands\", \"steal\", ");
}

}  // namespace

// ---- 4. steal conditions that read the item ----

TEST(steal_condition_reads_the_item_and_greys_each_item_with_the_makers_reason) {
  Game g("sb3_steal_it", "", {}, [](Copy& c) { lightHands(c); });
  World& w = *g.w;
  std::vector<StealOption> o;
  w.b().stealOptions(w.id("thief"), w.db().findAbility("light_hands"), w.unit("thief").cell, w.id("mark"), o);
  CHECK_EQ((int)o.size(), 3);
  if (o.size() == 3) {
    CHECK(!o[0].ok);
    CHECK_STR(o[0].why, "too heavy");  // the iron sword: weight 7, strength 5
    CHECK(o[1].ok);                     // the potion: weight 1
    CHECK(!o[2].ok);
    CHECK_STR(o[2].why, "cannot be stolen");  // the relic: the item's own rule comes first
  }
  CHECK_STR(w.refusal(stealOf(w, "light_hands", "thief", "mark", 0)), "that item too heavy");
  // The foe as a whole is a valid target (something can be taken), and the item that can be taken moves over.
  std::vector<AbilityOption> menu;
  w.b().abilityOptions(w.id("thief"), w.unit("thief").cell, menu);
  bool found = false;
  for (const AbilityOption& m : menu)
    if (m.ability == w.db().findAbility("light_hands")) {
      found = true;
      CHECK(m.ok);
    }
  CHECK(found);
  CHECK(w.apply(stealOf(w, "light_hands", "thief", "mark", 1)));
  CHECK_EQ(w.unit("thief").items.back().item, w.db().findItem("potion"));
  // A stronger thief can take the sword too: the formula is judged with the stats as the rules see them.
  Game h("sb3_steal_it2", "", {}, [](Copy& c) { lightHands(c); });
  h.w->unit("thief").stats[(size_t)h.w->db().findStat("str")] = 9;
  std::vector<StealOption> p;
  h.w->b().stealOptions(h.w->id("thief"), h.w->db().findAbility("light_hands"), h.w->unit("thief").cell, h.w->id("mark"), p);
  CHECK(p[0].ok && p[1].ok && !p[2].ok);
}

TEST(steal_condition_item_fields_and_a_foe_with_nothing_light_enough) {
  // id, kind, uses and maxUses as text / numbers; the default reason when the maker gives none.
  Game g("sb3_steal_fields", "", {}, [](Copy& c) { lightHands(c, "it.kind == 'usable' and it.uses >= 2 and it.maxUses == 3 and it.id == 'potion'", ""); });
  World& w = *g.w;
  std::vector<StealOption> o;
  w.b().stealOptions(w.id("thief"), w.db().findAbility("light_hands"), w.unit("thief").cell, w.id("mark"), o);
  CHECK(!o[0].ok && o[1].ok && !o[2].ok);
  CHECK_STR(o[0].why, "cannot be taken");
  w.unit("mark").items[1].uses = 1;  // fewer uses left than the formula wants
  w.b().stealOptions(w.id("thief"), w.db().findAbility("light_hands"), w.unit("thief").cell, w.id("mark"), o);
  CHECK(!o[1].ok);
  // Nothing passes: the foe is not a target, with the plain reason of any foe with nothing to take.
  CHECK_STR(w.refusal(stealOf(w, "light_hands", "thief", "mark", 1)), "there is nothing it can take");
  // A formula without `it` still judges the foe as a whole, as before.
  Game h("sb3_steal_foe", "", {}, [](Copy& c) { lightHands(c, "a.spd > b.spd + 100", ""); });
  CHECK_STR(h.w->refusal(stealOf(*h.w, "light_hands", "thief", "mark", 1)), "cannot steal from that unit");
}

TEST(steal_condition_item_props_loader_rules) {
  Copy c("sb3_steal_props", kFix);
  lightHands(c);
  CHECK_STR(c.gameError(), "");
  Copy bad("sb3_steal_reason", kFix);
  bad.swap("data/abilities.json", "{ \"abilities\": [", "{ \"abilities\": [ { \"id\": \"x\", \"name\": \"X\", \"target\": \"enemy\", \"effect\": { \"steal\": { \"reason\": \"no\" } } },");
  CHECK(saysIn(bad.gameError(), "\"reason\" goes with an \"if\" formula"));
  Copy bad2("sb3_steal_reason2", kFix);
  bad2.swap("data/abilities.json", "{ \"abilities\": [", "{ \"abilities\": [ { \"id\": \"x\", \"name\": \"X\", \"target\": \"enemy\", \"effect\": { \"steal\": { \"if\": \"1\", \"reason\": \"\" } } },");
  CHECK(saysIn(bad2.gameError(), "\"reason\" must be a short plain text"));
  Copy bad3("sb3_item_props", kFix);
  bad3.swap("data/items.json", "{ \"id\": \"iron_sword\", \"name\": \"Iron Sword\", \"kind\": \"weapon\",", "{ \"id\": \"iron_sword\", \"name\": \"Iron Sword\", \"kind\": \"weapon\", \"props\": { \"weight\": 1 },");
  CHECK(saysIn(bad3.gameError(), "\"props\" belongs to usable, accessory and staff items"));
}

// ---- 5. a strike that harms its wielder ----

TEST(recoil_hurts_the_wielder_by_a_number_or_a_formula_and_is_never_countered) {
  Game g("sb3_recoil", skill("bite", "strike", "{ \"recoil\": 2 }") + "," + skill("kick", "strike", "{ \"recoil\": \"w.might\" }") + "," +
                           skill("half", "strike", "{ \"recoil\": \"dealt / 2\" }"),
         {{"thief", "bite"}});
  CHECK(g.fight("thief", "mark"));
  CHECK_EQ(g.w->unit("mark").hp, 30 - 4);
  // The strike, the hurt, the answer: mark's counter is its one strike and the recoil adds none.
  CHECK_EQ((int)g.of(EventType::UnitHurt).size(), 1);
  CHECK(g.of(EventType::UnitHurt)[0]->a == g.w->id("thief") && g.of(EventType::UnitHurt)[0]->b == 2);
  int counters = 0;
  for (const Event& e : g.w->events) counters += e.type == EventType::Attack && (e.flags & kCounter);
  CHECK_EQ(counters, 1);
  CHECK_EQ(g.w->unit("thief").hp, 20 - 2 - (5 + 3 - 2));  // the recoil and mark's counter
  g.w->unit("thief").hp = 20;
  g.w->unit("thief").done = false;
  g.w->unit("thief").learned = {g.w->skill("kick")};
  CHECK(g.fight("thief", "mark"));
  CHECK_EQ(g.of(EventType::UnitHurt)[0]->b, 3);  // the weapon's own might
  g.w->unit("thief").hp = 20;
  g.w->unit("thief").done = false;
  g.w->unit("thief").learned = {g.w->skill("half")};
  CHECK(g.fight("thief", "mark"));
  CHECK_EQ(g.of(EventType::UnitHurt)[0]->b, 2);  // half the damage it dealt (4)
}

TEST(recoil_instead_turns_the_strike_on_the_wielder_and_the_foe_is_missed) {
  Game g("sb3_recoil_instead", skill("devil", "strike", "{ \"recoil\": { \"instead\": true } }"), {{"thief", "devil"}});
  const Forecast f = g.forecast("thief", "mark");
  CHECK_EQ(f.attack.damage, 4);  // the forecast numbers are those of the strike
  bool marked = false;
  for (const ForecastNote& n : f.notes) marked = marked || (n.unit == g.w->id("thief") && n.skill == g.w->skill("devil") && n.chance == 100 && n.recoil);
  CHECK(marked);  // the forecast shows the weapon bites
  CHECK(g.fight("thief", "mark"));
  CHECK_EQ(g.w->unit("mark").hp, 30);  // the foe took nothing
  const Event* strike = nullptr;
  for (const Event& e : g.w->events)
    if (e.type == EventType::Attack && e.a == g.w->id("thief")) strike = &e;
  CHECK(strike && !(strike->flags & kHit) && strike->d == 0);
  CHECK(g.of(EventType::UnitHurt).size() == 1 && g.of(EventType::UnitHurt)[0]->a == g.w->id("thief") && g.of(EventType::UnitHurt)[0]->b == 4);
  // A formula for the amount: "dealt" is what the strike would have done.
  Game h("sb3_recoil_instead2", skill("devil", "strike", "{ \"recoil\": { \"instead\": true, \"amount\": \"dealt * 2\" } }"), {{"thief", "devil"}});
  CHECK(h.fight("thief", "mark"));
  CHECK_EQ(h.of(EventType::UnitHurt)[0]->b, 8);
}

TEST(recoil_can_defeat_the_wielder_with_the_usual_defeat_flow) {
  Game g("sb3_recoil_death", skill("bite", "strike", "{ \"recoil\": 5 }"), {{"thief", "bite"}}, [](Copy& c) { c.swap("game.json", "\"strikes\": \"1\"", "\"strikes\": \"2\""); });
  g.w->unit("thief").hp = 3;
  CHECK(g.fight("thief", "mark"));
  CHECK(!g.w->unit("thief").alive && g.w->unit("thief").hp == 0);
  CHECK_EQ((int)g.of(EventType::UnitDied).size(), 1);
  CHECK_EQ(g.of(EventType::UnitDied)[0]->a, g.w->id("thief"));
  int attacks = 0;
  for (const Event& e : g.w->events) attacks += e.type == EventType::Attack;
  CHECK_EQ(attacks, 1);  // no second strike, no answer from mark
  CHECK_EQ(g.w->unit("mark").hp, 30 - 4);
  CHECK(!g.w->b().over);  // blue has other units
  // The loss is capped at what is left: a recoil never goes below zero.
  CHECK_EQ(g.of(EventType::UnitHurt)[0]->b, 3);
}

TEST(recoil_in_a_weapon_and_with_a_chance_shows_in_the_forecast_notes) {
  Game g("sb3_recoil_weapon", skill("cursed", "strike", "{ \"recoil\": 1 }", " \"chance\": 35,"), {}, [](Copy& c) {
    c.swap("data/weapons.json", "\"group\": \"sword\", \"props\": { \"might\": 3 } }", "\"group\": \"sword\", \"props\": { \"might\": 3 }, \"skills\": [\"cursed\"] }");
  });
  const Forecast f = g.forecast("thief", "mark");
  CHECK_EQ(f.attack.damage, 4);
  // The weapon is the same iron sword for both fighters: one note for each (the foe's answer bites it too).
  CHECK_EQ((int)f.notes.size(), 2);
  for (const ForecastNote& n : f.notes) CHECK(n.chance == 35 && n.recoil && (n.unit == g.w->id("thief") || n.unit == g.w->id("mark")));
}

TEST(recoil_loader_refusals) {
  auto say = [](const std::string& when, const std::string& effect) {
    Copy c("sb3_recoil_bad", kFix);
    c.skills(skill("x", when, effect));
    return c.gameError();
  };
  CHECK(say("strike", "{ \"recoil\": 3 }").empty());
  CHECK(say("strike", "{ \"recoil\": \"dealt\" }").empty());
  CHECK(say("strike", "{ \"recoil\": { \"instead\": true } }").empty());
  CHECK(saysIn(say("strike", "{ \"recoil\": 0 }"), "\"recoil\" must be a whole number of 1 or more"));
  CHECK(saysIn(say("strike", "{ \"recoil\": true }"), "\"recoil\" must be a whole number of 1 or more"));
  CHECK(saysIn(say("strike", "{ \"recoil\": { \"instead\": 3 } }"), "\"recoil\".\"instead\" must be true or false"));
  CHECK(saysIn(say("strike", "{ \"recoil\": { \"extra\": 3 } }"), "unknown key \"extra\""));
  CHECK(saysIn(say("combat", "{ \"recoil\": 3 }"), "\"recoil\" cannot be used in a \"combat\" block (it belongs to: strike)"));
  CHECK(saysIn(say("struck", "{ \"recoil\": 3 }"), "\"recoil\" cannot be used in a \"struck\" block"));
}

// ---- 9. a light that shrinks ----

namespace {

// "mist" with a torch (radius 4, shrinks 2 a turn: lasts until nothing is left) in the sage's pack.
void torch(rankstest::Copy& c, const std::string& light = "{ \"radius\": 4, \"shrink\": 2 }") {
  c.swap("data/items.json", "{ \"id\": \"spark_staff\"", "{ \"id\": \"torch_staff\", \"name\": \"Torch\", \"kind\": \"staff\", \"group\": \"staff\", \"uses\": 3, \"range\": 6, \"effects\": [ { \"light\": " + light + " } ] },\n  { \"id\": \"spark_staff\"");
  c.swap("maps/mist.json", "\"items\": [\"lantern_staff\", ", "\"items\": [\"torch_staff\", \"lantern_staff\", ");
}

Json saveOf(staves2test::Yard& w) {
  Json j;
  saveBattleState(w.b(), j);
  return j;
}

}  // namespace

TEST(light_shrink_the_radius_goes_down_each_turn_until_gone) {
  staves2test::Yard w("sb3_torch", "mist", [](rankstest::Copy& c) { torch(c); });
  CHECK(w.use("sage", "torch_staff", nullptr, 7, 2));
  CHECK_EQ((int)w.b().lights.size(), 1);
  const Event* lit = nullptr;
  for (const Event& e : w.events)
    if (e.type == EventType::AreaLit) lit = &e;
  CHECK(lit && lit->c == 4 && lit->d == 2);  // radius 4, and 2 turns: the time it takes to burn down
  CHECK(w.blueSees("g2") && w.blueSees(3, 2) && w.blueSees(10, 2) && w.blueSees(11, 2));
  CHECK(w.run(Action::endTurn()));  // red's turn: nothing changes for blue's light
  CHECK_EQ(w.b().lights[0].radius, 4);
  CHECK(w.run(Action::endTurn()));  // blue's turn starts: 4 -> 2
  CHECK_EQ(w.b().lights[0].radius, 2);
  CHECK(w.blueSees("g2") && w.blueSees(9, 2) && w.blueSees(5, 2));
  CHECK(!w.blueSees(10, 2) && !w.blueSees(11, 2));  // shrunk away
  CHECK(w.run(Action::endTurn()));
  CHECK(w.run(Action::endTurn()));  // the next one: gone
  CHECK(w.b().lights.empty() && !w.blueSees("g2"));
}

TEST(light_shrink_ends_by_turns_or_by_radius_whichever_comes_first) {
  staves2test::Yard a("sb3_torch_turns", "mist", [](rankstest::Copy& c) { torch(c, "{ \"radius\": 4, \"shrink\": 1, \"turns\": 2 }"); });
  CHECK(a.use("sage", "torch_staff", nullptr, 7, 2));
  CHECK(a.run(Action::endTurn()) && a.run(Action::endTurn()));
  CHECK_EQ(a.b().lights[0].radius, 3);
  CHECK(a.run(Action::endTurn()) && a.run(Action::endTurn()));
  CHECK(a.b().lights.empty());  // the turns ran out with radius 2 left
  staves2test::Yard b("sb3_torch_radius", "mist", [](rankstest::Copy& c) { torch(c, "{ \"radius\": 3, \"shrink\": 2, \"turns\": 5 }"); });
  CHECK(b.use("sage", "torch_staff", nullptr, 7, 2));
  CHECK(b.run(Action::endTurn()) && b.run(Action::endTurn()));
  CHECK_EQ(b.b().lights[0].radius, 1);
  CHECK(b.run(Action::endTurn()) && b.run(Action::endTurn()));
  CHECK(b.b().lights.empty());  // 1 - 2: burnt down before the 5 turns
  // Without "turns" it lasts as long as it takes to burn down: ceil(radius / shrink), at most 10.
  staves2test::Yard c("sb3_torch_default", "mist", [](rankstest::Copy& cc) { torch(cc, "{ \"radius\": 5, \"shrink\": 2 }"); });
  CHECK(c.use("sage", "torch_staff", nullptr, 7, 2));
  int turns = 0;
  for (; turns < 8 && !c.b().lights.empty(); ++turns) CHECK(c.run(Action::endTurn()) && c.run(Action::endTurn()));
  CHECK_EQ(turns, 3);  // 5, 3, 1, gone
}

TEST(light_shrink_is_saved_and_restored_and_other_lights_save_as_they_did) {
  staves2test::Yard w("sb3_torch_save", "mist", [](rankstest::Copy& c) { torch(c); });
  CHECK(w.use("sage", "torch_staff", nullptr, 7, 2));
  CHECK(w.run(Action::endTurn()) && w.run(Action::endTurn()));  // radius 2, 1 turn left
  const Json saved = saveOf(w);
  CHECK_EQ(saved["lights"][(size_t)0]["radius"].asInt(), 2);
  CHECK_EQ(saved["lights"][(size_t)0]["shrink"].asInt(), 2);
  CHECK_EQ(saved["lights"][(size_t)0]["turns"].asInt(), 1);
  Json reread;
  std::string err;
  CHECK(Json::parse(saved.dump(0), reread, err));
  Battle r;
  CHECK(r.init(w.rig.db, w.rig.map, w.rig.formulas, 1, err));
  CHECK(restoreBattleState(r, reread, err));
  CHECK_STR(savetest::stateText(r), savetest::stateText(w.b()));
  CHECK(r.lights.size() == 1 && r.lights[0].shrink == 2 && r.lights[0].radius == 2);
  // The restored battle burns the same way.
  std::vector<Event> ev;
  CHECK(r.apply(Action::endTurn(), ev) && r.apply(Action::endTurn(), ev));
  CHECK(r.lights.empty());
  // A lantern that does not shrink has no "shrink" key, so its saves are as they were.
  staves2test::Yard v("sb3_lantern_save", "mist", [](rankstest::Copy& c) { torch(c); });
  CHECK(v.use("sage", "lantern_staff", nullptr, 7, 2));
  CHECK(!saveOf(v)["lights"][(size_t)0].has("shrink"));
  // A damaged value is refused with its path.
  Json bad = jsonedit::withValue(saved, {"lights", "#0", "shrink"}, Json::makeNumber(99));
  Battle r2;
  CHECK(r2.init(w.rig.db, w.rig.map, w.rig.formulas, 1, err));
  CHECK(!restoreBattleState(r2, bad, err));
  CHECK(saysIn(err, "battle.lights[0].shrink"));
}

TEST(light_shrink_loader_refusals) {
  auto say = [](const std::string& light) {
    rankstest::Copy c("sb3_torch_bad", "srpg-staves2");
    torch(c, light);
    return c.gameError();
  };
  CHECK(say("{ \"radius\": 4, \"shrink\": 1 }").empty());
  CHECK(saysIn(say("{ \"radius\": 4, \"shrink\": 0 }"), "\"shrink\" must be a whole number of 1 or more and less than the radius"));
  CHECK(saysIn(say("{ \"radius\": 4, \"shrink\": 4 }"), "\"shrink\" must be a whole number of 1 or more and less than the radius"));
  CHECK(saysIn(say("{ \"radius\": 4, \"shrink\": \"2\" }"), "\"shrink\" must be a whole number"));
  CHECK(saysIn(say("{ \"radius\": 4, \"fade\": 1 }"), "unknown key \"fade\" (expected \"radius\", \"turns\" or \"shrink\")"));
}

// ---- 10. the computer opponent cures and refreshes ----

namespace {

// The fixture's red side is the computer. Red's turn, `far` (a rogue, with every ability) next to `mark`; `far` has not moved, so it waits where it is.
struct RedTurn {
  Game g;
  RedTurn(const char* name, const std::function<void(Copy&)>& tweak)
      : g(name, "", {}, [&](Copy& c) {
          // far (a rogue: purge, purge_self, refresh) stands next to mark; the blue thief stays next to mark.
          c.swap("maps/field.json", "{ \"type\": \"guard\", \"side\": \"red\", \"x\": 7, \"y\": 2, \"id\": \"far\" }", "{ \"type\": \"rogue\", \"side\": \"red\", \"x\": 2, \"y\": 1, \"id\": \"far\" }");
          if (tweak) tweak(c);
        }) {
    World& w = *g.w;
    CHECK(w.apply(Action::endTurn()));  // blue ends its turn: the computer's turn
    CHECK_EQ(w.b().activeSide, 1);
    w.unit("far").moved = true;  // it stays where it is
  }
  World& w() { return *g.w; }
  void poison(const char* who) {
    ConditionState p;
    p.condition = w().db().findCondition("poison");
    p.left = 3;
    w().unit(who).conditions.push_back(p);
  }
  // One decision of the computer opponent.
  void step() {
    w().events.clear();
    aiTakeStep(w().b(), w().events);
  }
  int count(EventType t) { return w().count(t); }
};

}  // namespace

TEST(ai_a_cure_ability_cures_the_unit_next_to_it_or_itself) {
  RedTurn t("sb3_ai_cure", nullptr);
  t.w().unit("mark").done = true;  // mark has nothing left to do; far is the one acting
  t.poison("mark");
  CHECK(!t.w().unit("mark").conditions.empty());
  t.step();
  CHECK(t.w().unit("mark").conditions.empty());  // the ally was cured
  CHECK_EQ(t.count(EventType::AbilityUsed), 1);
  CHECK_EQ(t.count(EventType::ConditionEnded), 1);
  // Itself.
  RedTurn s("sb3_ai_cure_self", nullptr);
  s.w().unit("mark").done = true;
  s.poison("far");
  s.step();
  CHECK(s.w().unit("far").conditions.empty());
  // Nothing to cure: nothing is cured (far refreshes mark instead, which is another test).
  RedTurn n("sb3_ai_cure_none", nullptr);
  n.w().unit("mark").done = true;
  n.step();
  CHECK_EQ(n.count(EventType::ConditionEnded), 0);
}

TEST(ai_a_cure_item_is_used_on_itself_instead_of_a_plain_attack) {
  auto antidote = [](Copy& c) {
    c.swap("data/items.json", "{ \"id\": \"potion\"", "{ \"id\": \"antidote\", \"name\": \"Antidote\", \"kind\": \"usable\", \"uses\": 1, \"cure\": [\"poison\"] },\n  { \"id\": \"potion\"");
    c.swap("maps/field.json", "\"id\": \"mark\"", "\"id\": \"mark\", \"items\": [\"iron_sword\", \"antidote\"]");
  };
  RedTurn t("sb3_ai_antidote", antidote);
  t.poison("mark");
  t.w().unit("far").done = true;
  t.step();
  CHECK(t.w().unit("mark").conditions.empty());
  CHECK_EQ(t.count(EventType::Attack), 0);  // it drank instead of striking
  CHECK_EQ(t.w().unit("mark").items.size(), (size_t)1);
  // Not poisoned: it just attacks, as before.
  RedTurn u("sb3_ai_antidote2", antidote);
  u.w().unit("far").done = true;
  u.step();
  CHECK(u.count(EventType::Attack) >= 1);
  CHECK_EQ(u.w().unit("mark").items.size(), (size_t)2);
}

TEST(ai_refresh_an_ally_that_has_acted_and_still_has_a_foe_in_reach) {
  RedTurn t("sb3_ai_refresh", nullptr);
  t.w().unit("mark").done = true;  // it has acted; the thief is next to it
  t.step();
  CHECK_EQ(t.count(EventType::UnitRefreshed), 1);
  CHECK(!t.w().unit("mark").done);
  // The refreshed unit then acts (it attacks the thief) and nobody refreshes it again.
  t.step();
  CHECK(t.count(EventType::Attack) >= 1);
  for (int i = 0; i < 6 && t.w().b().activeSide == 1; ++i) t.step();
  CHECK_EQ(t.w().unit("mark").refreshedBy.size(), (size_t)1);
  // No foe in reach for the acted unit (the thief is far away): no refresh.
  RedTurn n("sb3_ai_refresh_none", [](Copy& c) {
    c.swap("maps/field.json", "\"x\": 0, \"y\": 1, \"id\": \"thief\"", "\"x\": 5, \"y\": 0, \"id\": \"thief\"");
  });
  n.w().unit("mark").done = true;
  n.step();
  CHECK_EQ(n.count(EventType::UnitRefreshed), 0);
}

TEST(ai_cure_and_refresh_turns_are_deterministic_and_end) {
  std::string first;
  for (int run = 0; run < 2; ++run) {
    RedTurn t(run == 0 ? "sb3_ai_det_a" : "sb3_ai_det_b", nullptr);
    t.poison("mark");
    t.poison("far");
    t.w().unit("mark").done = true;
    int steps = 0;
    while (t.w().b().activeSide == 1 && !t.w().b().over && steps++ < 40) t.step();
    CHECK(steps < 40);  // the turn ends by itself: no loop of refreshing
    const std::string text = savetest::stateText(t.w().b());
    if (run == 0) first = text;
    else CHECK_STR(text, first);
  }
}
