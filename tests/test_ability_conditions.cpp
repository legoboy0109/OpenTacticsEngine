// Ability conditions (docs/SRPG.md "Ability conditions"): a chance on an ability's "inflict" (number or formula, one roll per target, a miss
// event), harmful / helpful conditions and cure lists by kind, cures on enemies, formulas reading a unit's conditions, the doom and protect
// recipes, and the computer's use of status and cure abilities. Game: tests/ability_conditions_fixture.h.
#include "ability_conditions_fixture.h"

using namespace ote;
using namespace acf;

namespace {

std::string withAbility(const char* name, const std::string& ability) {
  Opts o;
  o.abilities += "," + ability;
  return error(make(name, o));
}

std::string withCondition(const char* name, const std::string& condition) {
  Opts o;
  o.conditions += "," + condition;
  return error(make(name, o));
}

void give(Unit& u, int condition, int left) { u.conditions.push_back(ConditionState{condition, left}); }

}  // namespace

// ---- loader ----

TEST(abcond_loads_kinds_chances_and_cure_kinds) {
  Database db;
  std::string err;
  CHECK(loadGame(make("load"), db, err));
  CHECK_STR(err, "");
  CHECK(!db.conditions[(size_t)db.findCondition("sleep")].helpful);  // the default is harmful
  CHECK(!db.conditions[(size_t)db.findCondition("burn")].helpful);
  CHECK(db.conditions[(size_t)db.findCondition("haste")].helpful);
  const AbilityDef& hex = db.abilities[(size_t)db.findAbility("hex")];
  CHECK_EQ(hex.inflictChance, 50);
  CHECK_EQ(hex.inflictChanceExpr, -1);
  CHECK(db.abilities[(size_t)db.findAbility("lull")].inflictChanceExpr >= 0);
  const AbilityDef& plain = db.abilities[(size_t)db.findAbility("plain")];
  CHECK(plain.inflictChance == 0 && plain.inflictChanceExpr == -1);
  // "harmful" = sleep, burn, doom (file order); "helpful" = haste, protect, ward.
  const AbilityDef& esuna = db.abilities[(size_t)db.findAbility("esuna")];
  CHECK(esuna.cureKind == 1 && esuna.cures == std::vector<int>({db.findCondition("sleep"), db.findCondition("burn"), db.findCondition("doom")}));
  const AbilityDef& dispel = db.abilities[(size_t)db.findAbility("dispel")];
  CHECK(dispel.cureKind == 2 && dispel.cures == std::vector<int>({db.findCondition("haste"), db.findCondition("protect"), db.findCondition("ward")}));
}

TEST(abcond_loader_refusals) {
  CHECK(says(withCondition("k1", "{\"id\":\"x\",\"kind\":\"good\",\"turns\":2,\"blocks\":[{\"when\":\"always\",\"do\":[{\"stats\":{\"atk\":1}}]}]}"),
             "\"kind\" must be \"harmful\""));
  CHECK(says(withCondition("k2", "{\"id\":\"x\",\"kind\":true,\"turns\":2,\"blocks\":[{\"when\":\"always\",\"do\":[{\"stats\":{\"atk\":1}}]}]}"),
             "\"kind\" must be \"harmful\""));
  CHECK(says(withAbility("c1", "{\"id\":\"x\",\"target\":\"enemy\",\"inflict\":{\"condition\":\"sleep\",\"chance\":0}}"), "\"chance\" must be a percent from 1 to 100"));
  CHECK(says(withAbility("c2", "{\"id\":\"x\",\"target\":\"enemy\",\"inflict\":{\"condition\":\"sleep\",\"chance\":101}}"), "\"chance\" must be a percent from 1 to 100"));
  CHECK(says(withAbility("c3", "{\"id\":\"x\",\"target\":\"enemy\",\"inflict\":{\"condition\":\"sleep\",\"chance\":\"\"}}"), "\"chance\" must be a percent"));
  CHECK(says(withAbility("c4", "{\"id\":\"x\",\"target\":\"enemy\",\"inflict\":{\"condition\":\"sleep\",\"chance\":\"a.mag +\"}}"), "chance"));
  CHECK(says(withAbility("c5", "{\"id\":\"x\",\"target\":\"enemy\",\"inflict\":{\"condition\":\"sleep\",\"odds\":5}}"), "unknown key"));
  CHECK(says(withAbility("u1", "{\"id\":\"x\",\"target\":\"unit\",\"effect\":{\"cure\":\"cursed\"}}"), "\"cure\" must be \"all\", \"harmful\", \"helpful\""));
  CHECK(says(withAbility("u2", "{\"id\":\"x\",\"target\":\"cell\",\"range\":[1,2],\"effect\":{\"cure\":\"harmful\"}}"), "\"cure\" needs a target unit"));
  CHECK_STR(withAbility("ok1", "{\"id\":\"x\",\"target\":\"enemy\",\"inflict\":{\"condition\":\"sleep\",\"chance\":100}}"), "");
  CHECK_STR(withAbility("ok2", "{\"id\":\"x\",\"target\":\"enemy\",\"effect\":{\"cure\":\"all\"}}"), "");
}

TEST(abcond_cure_kinds_in_items_staves_and_skills) {
  Opts o;
  o.abilities += ",{\"id\":\"x\",\"target\":\"self\",\"effect\":{\"cure\":[\"sleep\"]}}";
  const std::string root = make("kinds_elsewhere", o);
  // A usable item and a skill block (turnStart) with the kinds load and expand to the same lists.
  CHECK(writeTextFile(root + "/data/items.json",
                      "{ \"items\": [{\"id\":\"iron_sword\",\"name\":\"Iron Sword\",\"kind\":\"weapon\",\"weapon\":\"sword\"},"
                      "{\"id\":\"remedy\",\"name\":\"Remedy\",\"kind\":\"usable\",\"cure\":\"harmful\"}] }"));
  CHECK(writeTextFile(root + "/data/skills.json",
                      "{ \"skills\": [{\"id\":\"purity\",\"name\":\"Purity\",\"blocks\":[{\"when\":\"turnStart\",\"do\":[{\"cure\":\"helpful\"}]}]}] }"));
  Database db;
  std::string err;
  CHECK(loadGame(root, db, err));
  CHECK_STR(err, "");
  const ItemDef& remedy = db.items[(size_t)db.findItem("remedy")];
  CHECK(!remedy.cureAll && remedy.cures.size() == 3 && remedy.cures[0] == db.findCondition("sleep"));
  const SkillDef& s = db.skills[(size_t)db.findSkill("purity")];
  CHECK(s.blocks[0].effects[0].cureKind == 2 && s.blocks[0].effects[0].conds.size() == 3);
}

// ---- the chance ----

TEST(abcond_a_chance_rolls_once_per_use_and_a_miss_says_so) {
  int took = 0, missed = 0;
  std::vector<std::string> first;
  for (unsigned seed = 1; seed <= 40; ++seed) {
    World w(make("roll"), seed);
    const Rng before = w.b().rng;
    CHECK(w.run(Action::useAbility(w.id("hero"), w.ability("hex"), w.id("bandit"))));
    CHECK(!w.sameRng(before));  // one roll
    Rng one = before;
    (void)one.range(100);
    CHECK(w.sameRng(one));  // exactly one
    if (w.has("bandit", "sleep")) {
      ++took;
      CHECK_EQ(w.count(EventType::ConditionApplied), 1);
      CHECK_EQ(w.count(EventType::ConditionResisted), 0);
    } else {
      ++missed;
      const Event* e = w.first(EventType::ConditionResisted);
      CHECK(e != nullptr);
      if (e) {
        CHECK(e->flags & kMissed);
        CHECK_EQ(e->a, w.id("bandit"));
        CHECK_EQ(e->c, w.id("hero"));
        CHECK_EQ(e->d, 50);
      }
    }
    CHECK(w.unit("hero").done);  // a miss still uses the turn
  }
  CHECK(took > 5 && missed > 5);
}

TEST(abcond_the_same_seed_gives_the_same_result) {
  for (unsigned seed = 1; seed <= 10; ++seed) {
    World a(make("det_a"), seed), b(make("det_b"), seed);
    CHECK(a.run(Action::useAbility(a.id("hero"), a.ability("hex"), a.id("bandit"))));
    CHECK(b.run(Action::useAbility(b.id("hero"), b.ability("hex"), b.id("bandit"))));
    CHECK_EQ(a.has("bandit", "sleep"), b.has("bandit", "sleep"));
  }
}

TEST(abcond_a_formula_chance_sees_both_units_and_rolls_only_inside_1_to_99) {
  World w(make("formula"));
  const int hero = w.id("hero"), bandit = w.id("bandit"), lull = w.ability("lull");
  CHECK_EQ(w.b().abilityInflictChance(hero, lull, bandit, w.unit("hero").cell), 50);  // 5 * 10 - 0
  CHECK_EQ(w.b().abilityInflictChance(hero, w.ability("hex"), bandit, w.unit("hero").cell), 50);
  CHECK_EQ(w.b().abilityInflictChance(hero, w.ability("plain"), bandit, w.unit("hero").cell), 100);
  CHECK_EQ(w.b().abilityInflictChance(hero, w.ability("esuna"), bandit, w.unit("hero").cell), -1);
  CHECK(!w.b().abilityInflictRolls(w.ability("plain")));
  // Resistance 5 makes it 0: no roll, a miss.
  w.unit("bandit").stats[(size_t)w.rig.db.findStat("res")] = 5;
  CHECK_EQ(w.b().abilityInflictChance(hero, lull, bandit, w.unit("hero").cell), 0);
  Rng before = w.b().rng;
  CHECK(w.run(Action::useAbility(hero, lull, bandit)));
  CHECK(w.sameRng(before));
  CHECK(!w.has("bandit", "sleep"));
  CHECK(w.first(EventType::ConditionResisted) && (w.first(EventType::ConditionResisted)->flags & kMissed));
}

TEST(abcond_no_chance_key_never_rolls) {
  World w(make("plain"));
  const Rng before = w.b().rng;
  CHECK(w.run(Action::useAbility(w.id("hero"), w.ability("plain"), w.id("bandit"))));
  CHECK(w.sameRng(before));
  CHECK(w.has("bandit", "sleep"));
}

TEST(abcond_immune_or_has_it_gives_no_roll) {
  World w(make("immune"));
  give(w.unit("bandit"), w.cond("ward"), -1);  // immune to sleep
  CHECK(says(w.why(Action::useAbility(w.id("hero"), w.ability("hex"), w.id("bandit"))), "immune"));
  // Through a strike the condition meets immunity: ConditionResisted without kMissed, no roll for it.
  give(w.unit("bandit"), w.cond("burn"), 3);
  const Rng before = w.b().rng;
  CHECK(w.run(Action::useAbility(w.id("hero"), w.ability("scorch"), w.id("bandit"))));
  CHECK(w.sameRng(before));  // hit 100, crit 0 and "has it for as long already": nothing was rolled
  CHECK_EQ(w.count(EventType::ConditionResisted), 0);
  CHECK_EQ(w.count(EventType::ConditionApplied), 0);
}

TEST(abcond_a_strike_rolls_for_the_unit_it_hit) {
  int took = 0;
  for (unsigned seed = 1; seed <= 30; ++seed) {
    World w(make("strike"), seed);
    CHECK(w.run(Action::useAbility(w.id("hero"), w.ability("scorch"), w.id("bandit"))));
    CHECK_EQ(w.count(EventType::Attack), 1);
    CHECK_EQ(w.unit("bandit").hp, 15);  // 5 - 0 damage
    const bool burnt = w.has("bandit", "burn");
    took += burnt;
    CHECK_EQ(w.count(EventType::ConditionApplied) + w.count(EventType::ConditionResisted), 1);
  }
  CHECK(took > 3 && took < 27);
}

TEST(abcond_formulas_read_conditions) {
  World w(make("sees"));
  const int hero = w.id("hero"), bandit = w.id("bandit"), sure = w.ability("sure");
  CHECK_EQ(w.b().abilityInflictChance(hero, sure, bandit, w.unit("hero").cell), 0);
  give(w.unit("bandit"), w.cond("burn"), 2);
  CHECK_EQ(w.b().abilityInflictChance(hero, sure, bandit, w.unit("hero").cell), 100);
}

// ---- cures by kind and on enemies ----

TEST(abcond_esuna_takes_harmful_and_dispel_takes_helpful) {
  World w(make("cure"));
  give(w.unit("pal"), w.cond("sleep"), 2);
  give(w.unit("pal"), w.cond("haste"), 2);
  give(w.unit("bandit"), w.cond("haste"), 2);
  give(w.unit("bandit"), w.cond("burn"), 2);
  CHECK(w.run(Action::useAbility(w.id("hero"), w.ability("esuna"), w.id("pal"))));
  CHECK(!w.has("pal", "sleep") && w.has("pal", "haste"));
  CHECK(w.run(Action::endTurn()));
  CHECK(w.run(Action::endTurn()));
  // Dispel on a foe: the blessing goes, the curse stays.
  CHECK_STR(w.why(Action::useAbility(w.id("hero"), w.ability("dispel"), w.id("bandit"))), "");
  CHECK(w.run(Action::useAbility(w.id("hero"), w.ability("dispel"), w.id("bandit"))));
  CHECK(!w.has("bandit", "haste") && w.has("bandit", "burn"));
  CHECK_EQ(w.count(EventType::ConditionEnded), 1);
}

TEST(abcond_a_cure_with_nothing_of_its_kind_is_refused) {
  World w(make("cure_none"));
  give(w.unit("pal"), w.cond("haste"), 2);
  CHECK(says(w.why(Action::useAbility(w.id("hero"), w.ability("esuna"), w.id("pal"))), "nothing to cure"));
}

// ---- recipes the gap list calls "already possible" ----

TEST(abcond_doom_defeats_when_its_count_runs_out) {
  World w(make("doom"));
  give(w.unit("bandit"), w.cond("doom"), 3);  // as a fresh "turns": 3
  // Red's turn starts: 2 left, then 1, then 0 = the block's formula holds and it is defeated.
  for (int round = 0; round < 2; ++round) {
    CHECK(w.run(Action::endTurn()));
    CHECK(w.unit("bandit").alive);
    CHECK(w.run(Action::endTurn()));
  }
  CHECK(w.run(Action::endTurn()));
  CHECK(!w.unit("bandit").alive);
  CHECK_EQ(w.count(EventType::UnitDied), 1);
}

TEST(abcond_protect_halves_physical_but_not_magic) {
  World w(make("protect"));
  const int hero = w.id("hero"), bandit = w.id("bandit");
  give(w.unit("bandit"), w.cond("protect"), 3);
  w.unit("bandit").cell = w.b().grid.at(2, 1);  // (a neighbour for the sword)
  w.b().occupant[(size_t)w.b().grid.at(3, 1)] = -1;
  w.b().occupant[(size_t)w.b().grid.at(2, 1)] = bandit;
  Forecast f;
  CHECK(w.b().forecast(hero, bandit, w.rig.db.findWeapon("sword"), w.unit("hero").cell, f));
  CHECK_EQ(f.attack.damage, 2);  // (6 - 1) * 50%, rounded down
  CHECK(w.b().abilityStrikeForecast(hero, w.ability("scorch"), bandit, w.unit("hero").cell, f));
  CHECK_EQ(f.attack.damage, 5);  // magic: unchanged
}

// ---- the computer ----

TEST(abcond_ai_puts_a_curse_on_a_foe_not_twice) {
  Opts o;
  o.brigandAbilities = "[\"plain\"]";
  o.units = "{\"type\":\"knight\",\"id\":\"pal\",\"side\":\"blue\",\"x\":0,\"y\":1},"
            "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":6,\"y\":1}";
  World w(make("ai_curse", o));
  // The knight is 6 squares away: the bandit (move 3, the spell's range 1..3) reaches it with the spell after walking, not with its sword.
  w.redTurn();
  CHECK(w.has("pal", "sleep"));
  int used = 0;
  for (const Event& e : w.events) used += e.type == EventType::AbilityUsed;
  CHECK_EQ(used, 1);
  // Next turn it does not use it again on a unit that has it for as long.
  if (!w.unit("pal").conditions.empty()) w.unit("pal").conditions[0].left = 3;
  w.redTurn();
  int again = 0;
  for (const Event& e : w.events) again += e.type == EventType::AbilityUsed;
  CHECK_EQ(again, 0);
}

TEST(abcond_ai_gives_a_blessing_to_its_own_side_only) {
  Opts o;
  o.brigandAbilities = "[\"quicken\"]";
  o.units = "{\"type\":\"knight\",\"id\":\"pal\",\"side\":\"blue\",\"x\":0,\"y\":1},"
            "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":8,\"y\":1},"
            "{\"type\":\"brigand\",\"id\":\"mate\",\"side\":\"red\",\"x\":8,\"y\":0}";
  World w(make("ai_bless", o));
  w.redTurn();
  CHECK(!w.has("pal", "haste"));
  CHECK(w.has("bandit", "haste") || w.has("mate", "haste"));
}

TEST(abcond_ai_cures_harmful_on_allies_and_strips_helpful_from_foes) {
  Opts o;
  o.brigandAbilities = "[\"esuna\"]";
  o.units = "{\"type\":\"knight\",\"id\":\"pal\",\"side\":\"blue\",\"x\":0,\"y\":1},"
            "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":7,\"y\":1},"
            "{\"type\":\"brigand\",\"id\":\"mate\",\"side\":\"red\",\"x\":8,\"y\":1}";
  {
    World w(make("ai_esuna", o));
    give(w.unit("mate"), w.cond("haste"), 3);  // only a blessing: nothing to cure on it
    give(w.unit("pal"), w.cond("burn"), 3);    // a foe's curse: left alone
    w.redTurn();
    CHECK(w.has("mate", "haste") && w.has("pal", "burn"));
    int used = 0;
    for (const Event& e : w.events) used += e.type == EventType::AbilityUsed;
    CHECK_EQ(used, 0);
  }
  {
    World w(make("ai_esuna2", o));
    give(w.unit("mate"), w.cond("burn"), 3);
    w.redTurn();
    CHECK(!w.has("mate", "burn"));
  }
  o.brigandAbilities = "[\"dispel\"]";
  o.units = "{\"type\":\"knight\",\"id\":\"pal\",\"side\":\"blue\",\"x\":4,\"y\":1},"
            "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":7,\"y\":1}";
  {
    World w(make("ai_dispel", o));
    give(w.unit("pal"), w.cond("haste"), 3);
    give(w.unit("bandit"), w.cond("protect"), 3);  // its own blessing stays
    w.redTurn();
    CHECK(!w.has("pal", "haste") && w.has("bandit", "protect"));
  }
}

TEST(abcond_ai_play_is_the_same_twice) {
  Opts o;
  o.blueAi = true;
  o.brigandAbilities = "[\"hex\",\"esuna\"]";
  o.mageAbilities = "[\"hex\",\"quicken\",\"esuna\",\"dispel\",\"scorch\"]";
  auto play = [&](unsigned seed) {
    World w(make("ai_twice", o), seed);
    std::string log;
    int guard = 0;
    while (!w.b().over && w.b().round < 12 && ++guard < 2000) {
      std::vector<Event> ev;
      aiTakeStep(w.b(), ev);
      for (const Event& e : ev) log += std::to_string((int)e.type) + ":" + std::to_string(e.a) + "," + std::to_string(e.b) + "," + std::to_string(e.c) + ";";
    }
    return log;
  };
  for (unsigned seed = 1; seed <= 5; ++seed) {
    const std::string a = play(seed), b = play(seed);
    CHECK(!a.empty());
    CHECK(a == b);
  }
}
