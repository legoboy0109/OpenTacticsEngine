// Step F1 of docs/SRPG.md: the shared pieces every later step builds on. Expressions (Database::exprs,
// Formulas::evalExpr, FormulaCtx::battle and the bonus lists), Grid::opposite, the new actions (all refused until
// their step lands), the turn flags and Battle::exists.
#include "battle_fixture.h"
#include "core/battle/unit_stats.h"
#include "core/data/loader.h"

using namespace ote;
using fixture::World;

namespace {

bool has(const std::string& text, const char* needle) { return text.find(needle) != std::string::npos; }

// Two soldiers facing each other on open ground; blue (0) moves first.
World duel() { return World({".....", "....."}, {{"soldier", 0, 1, 0, false}, {"soldier", 1, 3, 0, false}}); }

}  // namespace

// ---- expressions ----

TEST(srpg_expr_syntax_error_names_the_place_it_was_written) {
  World w = duel();
  w.db.addExpr("a.atk +", "data/skills.json: skill \"heavy_blow\": block 1: \"chance\"");
  std::string err;
  CHECK(!w.formulas.init(w.db, err));
  CHECK(err.rfind("data/skills.json: skill \"heavy_blow\": block 1: \"chance\": ", 0) == 0);
  // An empty formula is a mistake too, not "no formula".
  w.db.exprs[0].text = "";
  CHECK(!w.formulas.init(w.db, err));
  CHECK(has(err, "the formula is empty"));
}

TEST(srpg_expr_evaluates_numbers_and_yes_no) {
  World w = duel();
  int sum = w.db.addExpr("a.atk + b.def + dist", "test: sum");
  int yes = w.db.addExpr("a.atk > b.def", "test: yes");
  int nil = w.db.addExpr("a.level >= 1 and b.tags.armor", "test: nil");  // a soldier has no tag: nil
  int bad = w.db.addExpr("a.nothing.at_all", "test: bad");
  int alone = w.db.addExpr("b == nil and a.character == nil", "test: alone");
  CHECK(w.begin());
  CHECK_EQ(w.formulas.exprCount(), 5);
  FormulaCtx ctx;
  ctx.battle = &w.battle;
  ctx.sides = &w.battle.sides;
  ctx.a = &w.battle.units[0];
  ctx.b = &w.battle.units[1];
  ctx.dist = 2;
  CHECK_EQ((int)w.formulas.evalExpr(sum, ctx, -1), 5 + 2 + 2);
  CHECK_EQ((int)w.formulas.evalExpr(yes, ctx, -1), 1);
  CHECK_EQ((int)w.formulas.evalExpr(nil, ctx, -1), 0);   // nil counts as false, without an error
  CHECK_EQ((int)w.formulas.evalExpr(bad, ctx, -7), -7);  // a run-time error gives the default
  CHECK_EQ((int)w.formulas.evalExpr(99, ctx, -3), -3);   // no such expression
  CHECK_EQ((int)w.formulas.evalExpr(-1, ctx, -3), -3);
  // Without a second unit `b` is nil; a unit without a character has no `character` field.
  ctx.b = nullptr;
  CHECK_EQ((int)w.formulas.evalExpr(alone, ctx, -1), 1);
}

TEST(srpg_expr_battle_stats_equal_unit_stats_when_nothing_is_added) {
  // demo-srpg has accessories; the formulas must see the same numbers through the battle as without it.
  Database db;
  MapDef map;
  Formulas formulas;
  Battle b;
  std::string err;
  CHECK(loadGame(ote_test::sourceDir() + "/games/demo-srpg", db, err) && loadMap(db, "bridge", map, err));
  std::vector<int> exprs;
  for (const StatDef& s : db.stats) exprs.push_back(db.addExpr("a." + s.id, "test: " + s.id));
  int maxhp = db.addExpr("a.maxhp", "test: maxhp");
  CHECK(formulas.init(db, err) && b.init(db, map, formulas, 3, err));
  std::vector<Event> ev;
  b.start(ev);
  for (const Unit& u : b.units) {
    FormulaCtx with, without;
    with.a = without.a = &u;
    with.sides = without.sides = &b.sides;
    with.battle = &b;
    std::vector<int> skills;
    b.skillsOf(u, -1, skills);
    for (size_t s = 0; s < db.stats.size(); ++s) {
      // The demo has skills since D1: the only thing the battle adds to the raw numbers are the standing stat bonuses
      // of the unit's skills (none of the demo's has a condition).
      int expected = unitStat(db, u, (int)s, &b.sides);
      for (int k : skills)
        for (const SkillBlock& blk : db.skills[(size_t)k].blocks)
          if (blk.when == SkillWhen::Always && blk.conds.empty())
            for (const SkillEffect& e : blk.effects)
              if (e.kind == SkillEffect::Kind::Stats) expected += e.stats[s];
      CHECK_EQ(b.stat(u, (int)s), expected);
      CHECK_EQ((int)formulas.evalExpr(exprs[s], with, -1), expected);
    }
    CHECK_EQ((int)formulas.evalExpr(maxhp, with, -1), b.maxHp(u));
  }
}

TEST(srpg_expr_bonus_lists_add_to_the_stats_of_a_and_b) {
  World w = duel();
  int diff = w.db.addExpr("a.atk - b.def", "test: diff");
  int def = w.db.addExpr("b.def", "test: def");
  CHECK(w.begin());
  FormulaCtx ctx;
  ctx.battle = &w.battle;
  ctx.a = &w.battle.units[0];
  ctx.b = &w.battle.units[1];
  CHECK_EQ((int)w.formulas.evalExpr(diff, ctx, -1), 5 - 2);
  std::vector<int> plus = {0, 3, 0, 0}, minus = {0, 0, -9, 0};
  ctx.bonusA = &plus;
  CHECK_EQ((int)w.formulas.evalExpr(diff, ctx, -1), 8 - 2);
  ctx.bonusB = &minus;
  CHECK_EQ((int)w.formulas.evalExpr(def, ctx, -1), 0);  // a stat with a bonus does not go below 0
  // The rules formulas see the same context: the damage formula of the fixture is a.atk - b.def - defense.
  CHECK_EQ((int)w.formulas.eval(Formulas::Damage, ctx, -1), 8);
}

// ---- Grid::opposite ----

static void checkOpposite(const Grid& g) {
  const int n = g.directionCount();
  CHECK_EQ(g.opposite(-1), -1);
  CHECK_EQ(g.opposite(n), -1);
  for (int d = 0; d < n; ++d) {
    CHECK(g.opposite(d) != d);
    CHECK_EQ(g.opposite(g.opposite(d)), d);
  }
  for (Cell c = 0; c < g.cellCount(); ++c)
    for (int d = 0; d < n; ++d) {
      // One step out and one step straight back ends where it began.
      Cell out = g.neighbor(c, d);
      if (out != kNoCell) CHECK_EQ(g.neighbor(out, g.opposite(d)), c);
      // A straight line: two steps one way are at distance 2, and the two ends of a line through c at distance 2.
      Cell two = out == kNoCell ? kNoCell : g.neighbor(out, d);
      if (two != kNoCell) CHECK_EQ(g.distance(c, two), 2);
      Cell back = g.neighbor(c, g.opposite(d));
      if (out != kNoCell && back != kNoCell) CHECK_EQ(g.distance(out, back), 2);
    }
}

TEST(srpg_grid_opposite_on_squares) {
  Grid g(GridType::Square, 5, 4);
  checkOpposite(g);
  CHECK_EQ(g.opposite(0), 2);  // N - S
  CHECK_EQ(g.opposite(1), 3);  // E - W
}
TEST(srpg_grid_opposite_on_pointy_hexagons) { checkOpposite(Grid(GridType::Hex, 6, 5, HexLayout::Pointy)); }
TEST(srpg_grid_opposite_on_flat_hexagons) { checkOpposite(Grid(GridType::Hex, 6, 5, HexLayout::Flat)); }

// ---- the new actions ----

TEST(srpg_every_new_action_is_refused_with_its_message) {
  World w = duel();
  CHECK(w.begin());
  const std::string before = [&] { std::string s; for (const Unit& u : w.battle.units) s += std::to_string(u.cell) + (u.done ? "d" : "-"); return s; }();
  struct Case { Action action; const char* why; };
  const Case cases[] = {
      {Action::promote(0), "this game has no promotions"},
      {Action::promote(0, 1), "this game has no promotions"},
      {Action::useAbility(0, 0, 1), "this game has no abilities"},
      {Action::useAbility(0, 0, -1, w.at(2, 0)), "this game has no abilities"},
      {Action::rescue(0, 1), "this game has no rescue or pair up"},
      {Action::drop(0, w.at(1, 1)), "this game has no rescue or pair up"},
      {Action::giveUnit(0, 1), "this game has no rescue or pair up"},
      {Action::takeUnit(0, 1), "this game has no rescue or pair up"},
      {Action::pairUp(0, 1), "this game has no rescue or pair up"},
      {Action::switchPair(0), "this game has no rescue or pair up"},
      {Action::support(0, 1), "this game has no supports"},
  };
  for (const Case& c : cases) {
    std::string why;
    size_t events = w.events.size();
    CHECK(!w.battle.validate(c.action, &why));
    CHECK_STR(why, c.why);
    why.clear();
    CHECK(!w.battle.apply(c.action, w.events, &why));
    CHECK_STR(why, c.why);
    CHECK_EQ(w.events.size(), events);
  }
  // They are unit orders: the usual checks come first.
  std::string why;
  CHECK(!w.battle.validate(Action::rescue(1, 0), &why));
  CHECK_STR(why, "that unit cannot act now");
  std::string after;
  for (const Unit& u : w.battle.units) after += std::to_string(u.cell) + (u.done ? "d" : "-");
  CHECK_STR(after, before);
}

TEST(srpg_factories_fill_the_action) {
  Action a = Action::useAbility(3, 2, 7, 11);
  CHECK(a.type == ActionType::Ability);
  CHECK_EQ(a.unit, 3);
  CHECK_EQ(a.ability, 2);
  CHECK_EQ(a.target, 7);
  CHECK_EQ(a.cell, 11);
  a = Action::promote(4, 6);
  CHECK(a.type == ActionType::Promote);
  CHECK_EQ(a.unitType, 6);
  CHECK_EQ(Action::promote(4).unitType, -1);
  a = Action::drop(1, 9);
  CHECK(a.type == ActionType::Drop);
  CHECK_EQ(a.cell, 9);
  CHECK_EQ(a.target, -1);
  CHECK(Action::support(1, 2).type == ActionType::Support);
  CHECK_EQ(Action::support(1, 2).target, 2);
}

TEST(srpg_queries_answer_nothing_in_a_game_without_the_features) {
  World w = duel();
  CHECK(w.begin());
  const Unit& u = w.battle.units[0];
  std::vector<int> ints = {1, 2};
  std::vector<Cell> cells = {3};
  std::vector<AbilityOption> options(1);
  w.battle.skillsOf(u, -1, ints);
  CHECK(ints.empty());
  ints = {1};
  w.battle.abilitiesOf(u, ints);
  CHECK(ints.empty());
  w.battle.abilityOptions(0, u.cell, options);
  CHECK(options.empty());
  ints = {1};
  w.battle.abilityTargets(0, 0, u.cell, ints, cells);
  CHECK(ints.empty() && cells.empty());
  ints = {1};
  w.battle.promotionOptions(0, -1, ints);
  CHECK(ints.empty());
  ints = {1};
  w.battle.rescueTargets(0, u.cell, ints);
  CHECK(ints.empty());
  cells = {1};
  w.battle.dropCells(0, u.cell, cells);
  CHECK(cells.empty());
  ints = {1};
  w.battle.giveTargets(0, u.cell, ints);
  CHECK(ints.empty());
  ints = {1};
  w.battle.takeTargets(0, u.cell, ints);
  CHECK(ints.empty());
  ints = {1};
  w.battle.supportOptions(0, u.cell, ints);
  CHECK(ints.empty());
  CHECK(!w.battle.canSwitchPair(0));
  CHECK(!w.battle.hasSkill(u, 0));
  CHECK_EQ(w.battle.characterOf(u), -1);
  CHECK_EQ(u.character, -1);
  CHECK(w.battle.supports.points.empty() && w.battle.supports.level.empty());
  // Growths are the unit type's.
  for (int s = 0; s < 4; ++s) CHECK_EQ(w.battle.growthOf(u, s), w.db.unitTypes[0].growths[(size_t)s]);
  CHECK_EQ(w.battle.growthOf(u, 99), 0);
}

// ---- turn flags and the end of an order ----

TEST(srpg_orders_end_as_before_and_the_turn_flags_start_over) {
  World w({".....", "....."}, {{"soldier", 0, 1, 0, false}, {"soldier", 0, 0, 1, false}, {"soldier", 1, 3, 0, false}});
  CHECK(w.begin());
  CHECK(w.act(Action::move(0, w.at(2, 0))));
  CHECK_EQ(w.battle.units[0].moveSpent, 1);  // the movement points of the squares walked
  CHECK(w.act(Action::attack(0, 2, 0)));
  CHECK(w.battle.units[0].done && !w.battle.units[0].acted && !w.battle.units[0].refreshed);
  CHECK(w.act(Action::wait(1)));
  CHECK(w.battle.units[1].done && !w.battle.units[1].acted);
  CHECK(w.act(Action::endTurn()));
  CHECK(w.act(Action::endTurn()));
  CHECK_EQ(w.battle.activeSide, 0);
  CHECK_EQ(w.battle.units[0].moveSpent, 0);
  CHECK(!w.battle.units[0].done && !w.battle.units[0].moved);
}

TEST(srpg_a_unit_that_has_acted_may_only_move_or_wait) {
  // No skill can set `acted` yet; the rule itself is in place for the step that adds "move again".
  World w({".....", "....."}, {{"soldier", 0, 1, 0, false}, {"soldier", 1, 3, 0, false}});
  CHECK(w.begin());
  CHECK(w.act(Action::move(0, w.at(2, 0))));
  w.battle.units[0].acted = true;
  std::string why;
  CHECK(!w.battle.validate(Action::attack(0, 1, 0), &why));
  CHECK_STR(why, "that unit can only move or wait now");
  CHECK(!w.battle.validate(Action::capture(0), &why));
  CHECK_STR(why, "that unit can only move or wait now");
  // It has moved already, and may still make the move that is left; after it the unit is done.
  CHECK(w.battle.validate(Action::wait(0)));
  CHECK(w.act(Action::move(0, w.at(2, 1))));
  CHECK(w.battle.units[0].done);
  CHECK_EQ(w.battle.units[0].moveSpent, 2);
  CHECK(!w.battle.validate(Action::wait(0), &why));
  CHECK_STR(why, "that unit has already acted");
}

// ---- "does this unit still exist" ----

TEST(srpg_exists_is_alive_or_carried) {
  World w = duel();
  CHECK(w.begin());
  Unit u = w.battle.units[0];
  CHECK(w.battle.exists(u));
  u.alive = false;
  u.hp = 0;
  CHECK(!w.battle.exists(u));  // defeated
  u.hp = 4;
  u.carriedBy = 1;
  CHECK(w.battle.exists(u));   // off the board, attached to another unit
}

// ---- wording of support conversations ----

TEST(srpg_story_text_reads_support_lines_from_their_own_table) {
  World w = duel();
  w.db.supports.texts = {"You never miss.", "I miss plenty."};
  CHECK(w.begin());
  CHECK_STR(w.battle.storyText(kSupportTextBase), "You never miss.");
  CHECK_STR(w.battle.storyText(kSupportTextBase + 1), "I miss plenty.");
  CHECK_STR(w.battle.storyText(kSupportTextBase + 2), "");
  CHECK_STR(w.battle.storyText(0), "");  // the map has no story
}
