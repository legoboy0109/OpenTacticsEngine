// Downed units (docs/SRPG.md "Downed units and revive"): the loader and its refusals, the fall (the unit keeps its square), occupancy, targets,
// the win check, a defeated side's bodies, "removeAfter", and a game without the rule. Revives, saves, determinism and the computer:
// test_downed_revive.cpp. Game: tests/downed_fixture.h.
#include "downed_fixture.h"

using namespace ote;
using namespace acf;
using namespace downedtest;

namespace {

std::string withAbility(const char* name, const std::string& ability, const std::string& downed = "{}") {
  Opts o = downedOpts(downed);
  o.abilities += "," + ability;
  return error(make(name, o));
}

std::string withItem(const char* name, const std::string& item) {
  Opts o = downedOpts();
  o.moreItems += "," + item;
  return error(make(name, o));
}

std::string withCondition(const char* name, const std::string& cond, bool rules = true) {
  Opts o = downedOpts();
  if (!rules) {
    o.rules.clear();
    o.abilities = Opts().abilities;
    o.mageAbilities = Opts().mageAbilities;
    o.moreItems.clear();
    o.mageItems = Opts().mageItems;
    o.conditions = Opts().conditions;
  }
  o.conditions += "," + cond;
  return error(make(name, o));
}

}  // namespace

// ---- loader ----

TEST(downed_loads_the_rule_revive_abilities_items_and_auto_revive) {
  Database db;
  std::string err;
  CHECK(loadGame(make("downed_load", downedOpts("{\"afterBattle\":\"fallen\",\"removeAfter\":3}")), db, err));
  CHECK_STR(err, "");
  CHECK(db.rules.downed.on && db.rules.downed.fallenAfterBattle && db.rules.downed.removeAfter == 3);
  const AbilityDef& raise = db.abilities[(size_t)db.findAbility("raise")];
  CHECK(raise.effect == AbilityEffect::Revive && raise.healPercent == 50);
  const AbilityDef& arise = db.abilities[(size_t)db.findAbility("arise")];
  CHECK(arise.effect == AbilityEffect::Revive && arise.amount == 5 && arise.area.on());
  CHECK(db.items[(size_t)db.findItem("phoenix")].revive.fixed == 7);
  CHECK(db.conditions[(size_t)db.findCondition("reraise")].autoRevive.percent == 20);
  // The defaults: off, back after the battle, never removed.
  Database plain;
  CHECK(loadGame(make("downed_load_plain", Opts()), plain, err));
  CHECK(!plain.rules.downed.on && !plain.rules.downed.fallenAfterBattle && plain.rules.downed.removeAfter == 0);
  Database bare;
  CHECK(loadGame(make("downed_load_bare", downedOpts()), bare, err));
  CHECK(bare.rules.downed.on && !bare.rules.downed.fallenAfterBattle && bare.rules.downed.removeAfter == 0);
}

TEST(downed_loader_refusals) {
  // The rule block.
  CHECK(says(withAbility("d1", "{\"id\":\"x\",\"target\":\"ally\",\"effect\":{\"heal\":1}}", "{\"afterBattle\":\"gone\"}"), "\"afterBattle\" must be \"return\""));
  CHECK(says(withAbility("d2", "{\"id\":\"x\",\"target\":\"ally\",\"effect\":{\"heal\":1}}", "{\"removeAfter\":100}"), "\"removeAfter\" must be a whole number from 0 to 99"));
  CHECK(says(withAbility("d3", "{\"id\":\"x\",\"target\":\"ally\",\"effect\":{\"heal\":1}}", "{\"lingers\":1}"), "lingers"));
  CHECK(says(withAbility("d4", "{\"id\":\"x\",\"target\":\"ally\",\"effect\":{\"heal\":1}}", "true"), "downed"));
  // Revive abilities.
  Opts off;
  off.abilities += ",{\"id\":\"x\",\"target\":\"ally\",\"range\":[1,1],\"effect\":{\"revive\":5}}";
  CHECK(says(error(make("a0", off)), "\"revive\" needs \"rules\": { \"downed\": {} }"));
  CHECK(says(withAbility("a1", "{\"id\":\"x\",\"target\":\"enemy\",\"effect\":{\"revive\":5}}"), "\"revive\" needs \"target\" \"ally\""));
  CHECK(says(withAbility("a2", "{\"id\":\"x\",\"target\":\"self\",\"effect\":{\"revive\":5}}"), "\"revive\" needs \"target\" \"ally\""));
  CHECK(says(withAbility("a3", "{\"id\":\"x\",\"target\":\"ally\",\"effect\":{\"revive\":5},\"inflict\":{\"condition\":\"haste\"}}"),
             "\"revive\" cannot go with \"inflict\""));
  CHECK(says(withAbility("a4", "{\"id\":\"x\",\"target\":\"ally\",\"effect\":{\"revive\":0}}"), "\"revive\" must be a whole number of HP from 1 to 999"));
  CHECK(says(withAbility("a5", "{\"id\":\"x\",\"target\":\"ally\",\"effect\":{\"revive\":\"most\"}}"), "is not one"));
  CHECK(says(withAbility("a6", "{\"id\":\"x\",\"target\":\"ally\",\"effect\":{\"revive\":{\"percent\":0}}}"), "\"revive\" must be"));
  CHECK(says(withAbility("a7", "{\"id\":\"x\",\"target\":\"ally\",\"effect\":{\"revive\":5,\"heal\":5}}"), "x"));
  CHECK_STR(withAbility("ok1", "{\"id\":\"x\",\"target\":\"ally\",\"effect\":{\"revive\":\"full\"}}"), "");
  CHECK_STR(withAbility("ok2", "{\"id\":\"x\",\"target\":\"unit\",\"effect\":{\"revive\":\"b.hp_max / 4\"}}"), "");
  // Revive items.
  CHECK(says(withItem("i1", "{\"id\":\"x\",\"name\":\"X\",\"kind\":\"usable\",\"revive\":5}"), "\"revive\" needs \"target\": \"ally\""));
  CHECK(says(withItem("i2", "{\"id\":\"x\",\"name\":\"X\",\"kind\":\"usable\",\"target\":\"ally\",\"revive\":5,\"heal\":5}"), "\"revive\" cannot go with \"heal\""));
  CHECK(says(withItem("i3", "{\"id\":\"x\",\"name\":\"X\",\"kind\":\"weapon\",\"weapon\":\"sword\",\"revive\":5}"), "belongs to items of kind \"usable\""));
  CHECK(says(withItem("i4", "{\"id\":\"x\",\"name\":\"X\",\"kind\":\"usable\",\"target\":\"ally\",\"revive\":\"a.mag\"}"), "\"revive\" must be"));
  // Auto-revive conditions.
  CHECK(says(withCondition("c1", "{\"id\":\"x\",\"name\":\"X\",\"until\":\"cured\",\"autoRevive\":5}", false), "\"autoRevive\" needs \"rules\""));
  CHECK(says(withCondition("c2", "{\"id\":\"x\",\"name\":\"X\",\"until\":\"cured\",\"autoRevive\":\"a.hp\"}"), "\"autoRevive\" must be"));
  CHECK(says(withCondition("c3", "{\"id\":\"x\",\"name\":\"X\",\"until\":\"cured\",\"autoRevive\":0}"), "\"autoRevive\" must be"));
  CHECK_STR(withCondition("ok3", "{\"id\":\"x\",\"name\":\"X\",\"until\":\"cured\",\"autoRevive\":\"full\"}"), "");
}

// ---- the fall ----

TEST(downed_a_fallen_unit_lies_on_its_square) {
  World w(make("downed_fall", downedOpts()));
  const Cell cell = w.unit("bandit").cell;
  CHECK(heroStrikes(w, 1));
  const Unit& u = w.unit("bandit");
  CHECK(!u.alive && u.downed && u.hp == 0 && u.cell == cell);
  CHECK_EQ(w.b().occupant[(size_t)cell], w.id("bandit"));
  CHECK_EQ(w.b().unitAt(cell), w.id("bandit"));
  const Event* died = w.first(EventType::UnitDied);
  CHECK(died && (died->flags & kDowned) && died->a == w.id("bandit") && died->b == cell);
  CHECK(!w.b().over);  // red still has the thug
  CHECK(!w.b().exists(u));
}

TEST(downed_conditions_end_when_a_unit_falls) {
  World w(make("downed_conds", downedOpts()));
  w.unit("bandit").conditions.push_back(ConditionState{w.cond("burn"), 3});
  CHECK(heroStrikes(w, 1));
  CHECK(w.unit("bandit").conditions.empty());
}

TEST(downed_nobody_stops_on_a_body_but_anyone_walks_past) {
  World w(make("downed_occupancy", downedOpts()));
  CHECK(heroStrikes(w, 1));
  const int pal = w.id("pal");
  CHECK(w.why(Action::move(pal, w.b().grid.at(2, 1))) != "");  // the body's square
  CHECK(w.run(Action::move(pal, w.b().grid.at(3, 1))));        // past the hero (ally) and the body (a foe's)
  CHECK(w.unit("pal").cell == w.b().grid.at(3, 1));
}

TEST(downed_only_a_revive_targets_a_downed_unit) {
  World w(make("downed_targets", downedOpts()));
  CHECK(heroStrikes(w, 1));
  w.unit("hero").done = w.unit("hero").moved = false;
  const int hero = w.id("hero"), bandit = w.id("bandit");
  CHECK(w.why(Action::attack(hero, bandit, 0)) != "");
  CHECK(says(w.why(Action::useAbility(hero, w.ability("hex"), bandit)), "that unit is downed"));
  CHECK(w.why(Action::useAbility(hero, w.ability("raise"), bandit)) != "");  // an enemy: "raise" is for allies
  // "raise" on a standing ally is refused too.
  CHECK(says(w.why(Action::useAbility(hero, w.ability("raise"), w.id("pal"))), "that unit is not downed"));
}

TEST(downed_every_unit_of_a_side_downed_is_its_defeat_and_its_bodies_go) {
  Opts o = downedOpts();
  o.units = "{\"type\":\"mage\",\"id\":\"hero\",\"side\":\"blue\",\"x\":1,\"y\":1},"
            "{\"type\":\"knight\",\"id\":\"pal\",\"side\":\"blue\",\"x\":0,\"y\":1},"
            "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":2,\"y\":1}";
  World w(make("downed_defeat", o));
  const Cell cell = w.unit("bandit").cell;
  CHECK(heroStrikes(w, 1));
  CHECK(w.b().over);
  CHECK_EQ(w.b().winnerTeam, w.b().teamOf(w.unit("hero")));
  const Unit& u = w.unit("bandit");
  CHECK(!u.alive && !u.downed);  // a defeated side's bodies are removed: nobody is left to revive them
  CHECK_EQ(w.b().occupant[(size_t)cell], -1);
  CHECK(withFlag(w, EventType::UnitRemoved, kDowned) != nullptr);
}

TEST(downed_remove_after_takes_the_body_away_at_its_sides_turn) {
  World w(make("downed_remove", downedOpts("{\"removeAfter\":1}")));
  const Cell cell = w.unit("bandit").cell;
  CHECK(heroStrikes(w, 1));
  CHECK(w.unit("bandit").downed);
  w.redTurn();
  CHECK(!w.unit("bandit").downed && w.unit("bandit").cell == kNoCell);
  CHECK(w.b().occupant[(size_t)cell] != w.id("bandit"));
  const Event* removed = withFlag(w, EventType::UnitRemoved, kDowned);
  CHECK(removed && removed->a == w.id("bandit"));
}

TEST(downed_without_remove_after_a_body_stays) {
  World w(make("downed_stays", downedOpts()));
  CHECK(heroStrikes(w, 1));
  w.redTurn();
  CHECK(w.run(Action::endTurn()));
  w.redTurn();
  CHECK(w.unit("bandit").downed);
  CHECK_EQ(w.unit("bandit").downedTurns, 0);
}

TEST(downed_a_game_without_the_rule_removes_the_fallen_as_before) {
  Opts o;
  o.units = downedOpts().units;
  World w(make("downed_off", o));
  const Cell cell = w.unit("bandit").cell;
  CHECK(heroStrikes(w, 1));
  const Unit& u = w.unit("bandit");
  CHECK(!u.alive && !u.downed);
  CHECK_EQ(w.b().occupant[(size_t)cell], -1);
  const Event* died = w.first(EventType::UnitDied);
  CHECK(died && died->flags == 0);
  CHECK(w.first(EventType::UnitRevived) == nullptr);
}
