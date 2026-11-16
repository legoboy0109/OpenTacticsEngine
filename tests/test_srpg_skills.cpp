// Step K1 of docs/SRPG.md "Skills", in a battle: where a unit's skills come from, the standing stat bonuses and the
// condition evaluator. (Growths and learning: test_srpg_skills_learning.cpp.)
#include "srpg_skills_fixture.h"

using namespace ote;
using skilltest::Copy;
using skilltest::World;

namespace {

SkillCond cond(SkillCond::Kind kind) {
  SkillCond c;
  c.kind = kind;
  if (kind == SkillCond::Kind::Near) c.value = 1;  // the loader's default count
  return c;
}

}  // namespace

TEST(skills_sources_come_in_the_contract_order_and_count_once) {
  World w;
  // Eirik: type [steady_recovery]; learned at creation by level 3, oldest level first (type then character on a
  // tie): fast_learner (2), light_step (3), gifted (3); then the character's own bold.
  const Unit& e = w.unit("eirik");
  CHECK_STR(w.learnedIds(e), "fast_learner light_step gifted");
  CHECK_STR(w.skillIds(e), "steady_recovery fast_learner light_step gifted bold blade_focus");  // + the sword in hand
  // Accessories in inventory order, then the weapon; a skill that comes twice counts once, at its first place.
  Unit& u = w.unit("eirik");
  ItemStack charm, amulet;
  charm.item = w.db().findItem("charm");
  amulet.item = w.db().findItem("amulet");
  u.items = {charm, amulet};
  CHECK_STR(w.skillIds(u), "steady_recovery fast_learner light_step gifted bold lucky_charm tough blade_focus");
  u.items = {amulet, charm};
  CHECK_STR(w.skillIds(u), "steady_recovery fast_learner light_step gifted bold tough lucky_charm blade_focus");
  // The weapon in use: -1 = outside a combat (the type's first weapon), a weapon index, or -2 = none at all.
  CHECK_STR(w.skillIds(u, w.db().findWeapon("bow")), "steady_recovery fast_learner light_step gifted bold tough lucky_charm");
  CHECK_STR(w.skillIds(u, -2), "steady_recovery fast_learner light_step gifted bold tough lucky_charm");
  CHECK_EQ(w.b().restingWeapon(u), w.db().findWeapon("sword"));
  u.learned.push_back(w.skill("tough"));  // learned AND from an accessory: once, at the learned place
  CHECK_STR(w.skillIds(u, -2), "steady_recovery fast_learner light_step gifted tough bold lucky_charm");
  CHECK(w.b().hasSkill(u, w.skill("bold")) && !w.b().hasSkill(u, w.skill("rooted")));
  // A plain unit (no character) of the same type has the type's and what its level earned.
  CHECK_STR(w.skillIds(w.b().units[3]), "steady_recovery blade_focus");
  // Usable items give nothing.
  w.unit("gorm").items.clear();
  ItemStack potion;
  potion.item = w.db().findItem("potion");
  w.unit("gorm").items.push_back(potion);
  CHECK_STR(w.skillIds(w.unit("gorm")), "blade_focus");
}

TEST(skills_standing_stat_bonuses_and_current_hit_points) {
  World w;
  Unit& e = w.unit("eirik");  // hero 20 6 4 3 4, level 3: light_step learned (+1 move), sword gives blade_focus (+1 atk)
  CHECK_EQ(w.stat(e, "atk"), 7);
  CHECK_EQ(w.stat(e, "mov"), 5);
  CHECK_EQ(w.b().movePoints(e), 5);
  CHECK_EQ(w.stat(e, "hp"), 20);
  CHECK_EQ(w.stat(w.unit("gorm"), "atk"), 4);  // grunt 3 + blade_focus
  // "bold": +3 atk at half hit points or less, judged as hp * 100 <= 50 * maxhp.
  e.hp = 11;
  CHECK_EQ(w.stat(e, "atk"), 7);
  e.hp = 10;
  CHECK_EQ(w.stat(e, "atk"), 10);
  // A bonus to the HP stat counts in the maximum (no condition allowed): picking up a tough accessory.
  ItemStack amulet;
  amulet.item = w.db().findItem("amulet");  // hp +2 and the skills tough (+5 hp) and lucky_charm (+1 def)
  e.items = {amulet};
  CHECK_EQ(w.b().maxHp(e), 27);
  CHECK_EQ(w.stat(e, "def"), 4);
  CHECK_EQ(w.stat(e, "atk"), 10);  // 10 hp of 27 is still "half or less": 1000 <= 1350
  e.hp = 14;
  CHECK_EQ(w.stat(e, "atk"), 7);   // 1400 > 1350
}

TEST(skills_a_unit_made_with_a_tough_skill_starts_at_its_full_maximum) {
  Copy c("skills_tough_start");
  c.put("maps/field.json", R"({ "name": "Field", "grid": { "type": "square", "width": 6, "height": 3 }, "legend": { ".": "plain" },
    "tiles": ["......", "......", "......"], "sides": [ { "id": "blue", "controller": "human" }, { "id": "red", "controller": "ai" } ],
    "units": [ { "id": "tank", "type": "grunt", "side": "blue", "x": 0, "y": 0, "items": [ "amulet" ] },
               { "type": "grunt", "side": "red", "x": 5, "y": 0 } ] })");
  World w("field", c.dir);
  const Unit& u = w.unit("tank");
  CHECK_EQ(w.b().maxHp(u), 8 + 2 + 5);
  CHECK_EQ(u.hp, 15);
  // Losing the skill later cuts the hit points down to the new maximum (a story's forgetSkill, an item dropped).
  w.unit("tank").items.clear();
  CHECK_EQ(w.b().maxHp(w.unit("tank")), 8);
}

TEST(skills_a_stat_never_goes_below_zero_and_conditions_follow_the_square) {
  Copy c("skills_floor");
  c.skills(R"({ "id": "curse", "blocks": [ { "when": "always", "do": [ { "stats": { "def": -9 } } ] } ] })");
  c.swap("data/units.json", "\"skills\": [\"steady_recovery\"]", "\"skills\": [\"curse\", \"rooted\", \"fort_guard\"]");
  World w("woods", c.dir);
  Unit& s = w.unit("scout");  // hero def 3 - 9 -> 0; rooted on forest +2, fort_guard on a fort +3
  CHECK_EQ(w.stat(s, "def"), 0);
  s.cell = w.at(1, 1);  // a forest (tag "cover")
  CHECK_EQ(w.stat(s, "def"), 0);  // -6 + 2 = -4 -> still 0: the floor is the end result
  Copy d("skills_conditions_follow_the_square");
  d.swap("data/units.json", "\"skills\": [\"steady_recovery\"]", "\"skills\": [\"rooted\", \"fort_guard\"]");
  World v("woods", d.dir);
  Unit& u = v.unit("scout");
  CHECK_EQ(v.stat(u, "def"), 3);
  v.b().occupant[(size_t)u.cell] = -1;
  u.cell = v.at(1, 1);
  v.b().occupant[(size_t)u.cell] = u.id;
  CHECK_EQ(v.stat(u, "def"), 5);  // forest: +2
  v.b().occupant[(size_t)u.cell] = -1;
  u.cell = v.at(6, 0);
  v.b().occupant[(size_t)u.cell] = u.id;
  CHECK_EQ(v.stat(u, "def"), 6);  // the fort: +3
}

TEST(skills_every_condition_true_and_false_for_the_owner_and_the_foe) {
  World w;
  Battle& b = w.b();
  const int eirik = w.id("eirik"), gorm = w.id("gorm"), lyra = w.id("lyra");
  SkillCtx ctx;
  ctx.unit = eirik;
  ctx.foe = gorm;
  ctx.weapon = w.db().findWeapon("sword");
  ctx.foeWeapon = w.db().findWeapon("sword");

  // hp: hp * 100 <op> percent * maxhp, for the owner and for the foe ("who": "foe").
  SkillCond hp = cond(SkillCond::Kind::Hp);
  hp.op = CmpOp::Le;
  hp.value = 50;
  CHECK(!b.skillCondHolds(hp, ctx));
  w.unit("eirik").hp = 10;
  CHECK(b.skillCondHolds(hp, ctx));
  hp.foe = true;
  CHECK(!b.skillCondHolds(hp, ctx));
  w.unit("gorm").hp = 4;  // 4 of 8
  CHECK(b.skillCondHolds(hp, ctx));
  hp.op = CmpOp::Lt;
  CHECK(!b.skillCondHolds(hp, ctx));
  SkillCtx alone = ctx;
  alone.foe = -1;  // "who": "foe" with no foe never holds
  CHECK(!b.skillCondHolds(hp, alone));

  // near: ally / enemy counts within a distance; "enemy" only counts what the team sees; match = type id or tag.
  SkillCond near = cond(SkillCond::Kind::Near);
  near.within = 1;
  CHECK(!b.skillCondHolds(near, ctx));  // lyra is two away
  near.within = 2;
  CHECK(b.skillCondHolds(near, ctx));
  near.op = CmpOp::Eq;
  near.value = 0;
  CHECK(!b.skillCondHolds(near, ctx));
  near.within = 1;
  CHECK(b.skillCondHolds(near, ctx));  // "nobody next to me"
  near = cond(SkillCond::Kind::Near);
  near.within = 10;
  near.key = "lord";
  CHECK(b.skillCondHolds(near, ctx));  // lyra is a hero, tagged lord
  near.key = "grunt";
  CHECK(!b.skillCondHolds(near, ctx));
  near.enemy = true;
  CHECK(b.skillCondHolds(near, ctx));  // gorm
  near.key = "lord";
  near.within = 6;
  CHECK(!b.skillCondHolds(near, ctx));  // the red hero is seven away
  near.within = 7;
  CHECK(b.skillCondHolds(near, ctx));
  ctx.cell = w.at(4, 0);  // where it would stand: next to gorm
  near.key = "grunt";
  near.within = 1;
  CHECK(b.skillCondHolds(near, ctx));
  ctx.cell = kNoCell;

  // weapon, unit, carrying, round
  SkillCond weapon = cond(SkillCond::Kind::Weapon);
  weapon.key = "sword";
  CHECK(b.skillCondHolds(weapon, ctx));
  weapon.key = "blade";  // a group
  CHECK(b.skillCondHolds(weapon, ctx));
  weapon.key = "bow";
  CHECK(!b.skillCondHolds(weapon, ctx));
  weapon.key = "blade";
  ctx.weapon = -1;
  CHECK(!b.skillCondHolds(weapon, ctx));
  weapon.foe = true;
  CHECK(b.skillCondHolds(weapon, ctx));  // the foe's weapon
  ctx.weapon = w.db().findWeapon("sword");
  SkillCond unit = cond(SkillCond::Kind::Unit);
  unit.key = "lord";
  CHECK(b.skillCondHolds(unit, ctx));
  unit.foe = true;
  CHECK(!b.skillCondHolds(unit, ctx));  // gorm is a grunt
  unit.key = "grunt";
  CHECK(b.skillCondHolds(unit, ctx));
  SkillCond carrying = cond(SkillCond::Kind::Carrying);
  carrying.flag = true;
  CHECK(!b.skillCondHolds(carrying, ctx));
  w.unit("eirik").carrying = lyra;
  CHECK(b.skillCondHolds(carrying, ctx));
  carrying.flag = false;
  CHECK(!b.skillCondHolds(carrying, ctx));
  w.unit("eirik").carrying = -1;
  SkillCond round = cond(SkillCond::Kind::Round);
  round.value = 1;
  CHECK(b.skillCondHolds(round, ctx));
  round.op = CmpOp::Gt;
  CHECK(!b.skillCondHolds(round, ctx));
  b.round = 3;
  CHECK(b.skillCondHolds(round, ctx));
  b.round = 1;

  // initiating and distance: only where there is a combat.
  SkillCond init = cond(SkillCond::Kind::Initiating);
  CHECK(!b.skillCondHolds(init, ctx));
  ctx.combat = true;
  ctx.initiating = true;
  CHECK(b.skillCondHolds(init, ctx));
  init.flag = false;
  CHECK(!b.skillCondHolds(init, ctx));
  ctx.initiating = false;
  CHECK(b.skillCondHolds(init, ctx));
  SkillCond dist = cond(SkillCond::Kind::Distance);
  dist.op = CmpOp::Eq;
  dist.value = 5;
  CHECK(b.skillCondHolds(dist, ctx));
  dist.value = 1;
  CHECK(!b.skillCondHolds(dist, ctx));
  ctx.cell = w.at(4, 0);
  CHECK(b.skillCondHolds(dist, ctx));
  ctx.cell = kNoCell;

  // formula: `a` = the owner, `b` = the foe.
  std::string err;
  SkillCond f1 = cond(SkillCond::Kind::Formula), f2 = cond(SkillCond::Kind::Formula), f3 = cond(SkillCond::Kind::Formula);
  f1.expr = w.rig.db.addExpr("a.atk > b.def and dist == 5", "test: f1");
  f2.expr = w.rig.db.addExpr("a.level >= 10", "test: f2");
  f3.expr = w.rig.db.addExpr("b.tags.flier", "test: f3");  // nil counts as false
  CHECK(w.rig.formulas.init(w.rig.db, err));
  CHECK(b.skillCondHolds(f1, ctx));
  CHECK(!b.skillCondHolds(f2, ctx));
  CHECK(!b.skillCondHolds(f3, ctx));
  ctx.foe = -1;
  CHECK(!b.skillCondHolds(f1, ctx));  // b is nil: the run-time error counts as false
}

TEST(skills_terrain_and_structure_conditions) {
  World w("woods");
  Battle& b = w.b();
  SkillCtx ctx;
  ctx.unit = w.id("scout");
  SkillCond terrain = cond(SkillCond::Kind::Terrain);
  terrain.key = "cover";  // a tag
  CHECK(!b.skillCondHolds(terrain, ctx));
  ctx.cell = w.at(1, 1);
  CHECK(b.skillCondHolds(terrain, ctx));
  terrain.key = "forest";  // an id
  CHECK(b.skillCondHolds(terrain, ctx));
  terrain.key = "plain";
  CHECK(!b.skillCondHolds(terrain, ctx));
  SkillCond st = cond(SkillCond::Kind::Structure);
  CHECK(!b.skillCondHolds(st, ctx));  // any structure
  ctx.cell = w.at(6, 0);
  CHECK(b.skillCondHolds(st, ctx));
  st.key = "fort";
  CHECK(b.skillCondHolds(st, ctx));
  st.key = "stronghold";
  CHECK(b.skillCondHolds(st, ctx));
  st.key = "keep";
  CHECK(!b.skillCondHolds(st, ctx));
  st.key.clear();
  st.flag = false;  // "not on a structure"
  CHECK(!b.skillCondHolds(st, ctx));
  ctx.cell = w.at(0, 0);
  CHECK(b.skillCondHolds(st, ctx));
}
