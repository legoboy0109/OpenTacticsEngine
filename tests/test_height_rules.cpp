// docs/SRPG.md "Height rules": what a step may climb or drop, what height does to the reach and the targets of a weapon, line of sight past a
// ridge, the vertical limit of an area, the enemy-range display, the refusals with a reason, saves and determinism, on squares and hexagons.
// Fixture tests/fixtures/height-rules (see tests/height_fixture.h for its numbers).
#include "core/battle/save.h"
#include "core/battle/threat_area.h"
#include "height_fixture.h"

using namespace ote;
using heighttest::Scene;
using heighttest::U;
using skilltest::saysIn;

namespace {

// A plateau of height 2 from column 5 on, three rows: a cliff of two levels between columns 4 and 5.
std::vector<std::string> cliff() { return {"00000222", "00000222", "00000222"}; }

}  // namespace

// ---- movement ----

TEST(a_cliff_stops_a_walker_with_too_little_jump) {
  Scene s("hr_cliff", cliff(), {{"lancer", "blue", 2, 1, "lan"}, {"goat", "blue", 2, 0, "goat"}, {"dummy", "red", 7, 2, "foe"}});
  CHECK(s.canStop("lan", 4, 1));
  CHECK(!s.canStop("lan", 5, 1));  // two levels up: its jump is 1
  CHECK(!s.canStop("lan", 6, 1));  // (and nothing beyond: the cliff is in the way)
  CHECK_STR(s.refusedMove("lan", 5, 1), "too steep to climb there");
  CHECK(s.canStop("goat", 5, 1));  // jump up 3
  CHECK(s.canStop("goat", 6, 1));
  CHECK(s.go("goat", 5, 1));
}

TEST(a_step_down_is_limited_by_the_down_jump_and_up_and_down_may_differ) {
  Scene s("hr_down", cliff(), {{"lancer", "blue", 6, 1, "lan"}, {"diver", "blue", 6, 0, "dv"}, {"goat", "blue", 6, 2, "goat"}, {"dummy", "red", 0, 2, "foe"}});
  CHECK(s.canStop("lan", 5, 1));    // along the top
  CHECK(!s.canStop("lan", 4, 1));   // two levels down: its jump down is 1
  CHECK(s.canStop("dv", 4, 0));     // the diver drops 5 ...
  CHECK(s.canStop("dv", 1, 0));
  CHECK(!s.canStop("goat", 4, 2));  // the goat climbs 3 but drops only 1
  // ... and the diver, at the foot of the cliff, cannot climb it: its jump up is the game's 1.
  Scene t("hr_down2", cliff(), {{"diver", "blue", 4, 0, "dv"}, {"dummy", "red", 0, 2, "foe"}});
  CHECK(!t.canStop("dv", 5, 0));
  CHECK(t.canStop("dv", 0, 0));
}

TEST(a_flier_ignores_the_jump_rule_and_a_jump_of_zero_means_flat_ground_only) {
  Scene s("hr_fly", {"00123333", "00123333", "00123333"}, {{"hawk", "blue", 0, 0, "hawk"}, {"stiff", "blue", 0, 1, "stiff"}, {"lancer", "blue", 0, 2, "lan"}, {"dummy", "red", 7, 2, "foe"}});
  CHECK(s.canStop("hawk", 4, 0));   // 3 levels up, straight over the ridge: it flies
  CHECK(s.canStop("lan", 3, 2));    // a ramp of single steps: the lancer climbs it
  CHECK(s.canStop("lan", 5, 2));    // and on along the top (5 squares of movement)
  CHECK(s.canStop("stiff", 1, 1));  // flat ground
  CHECK(!s.canStop("stiff", 2, 1)); // one level up: its jump is 0
}

TEST(without_the_height_rules_every_step_may_be_taken) {
  Scene s("hr_off", cliff(), {{"lancer", "blue", 2, 1, "lan"}, {"dummy", "red", 7, 2, "foe"}}, false, heighttest::dropHeights);
  CHECK(!s.b().heightGame());
  CHECK(s.canStop("lan", 5, 1));  // the map still has its heights; nothing limits the walk
  CHECK_STR(s.refusedMove("lan", 7, 0), "the unit cannot reach that cell");  // (too far: the plain refusal, no word about height)
}

TEST(the_jump_rule_works_on_hexagons_too) {
  Scene s("hr_hex", {"0000222", "0000222", "0000222", "0000222"}, {{"lancer", "blue", 1, 1, "lan"}, {"goat", "blue", 1, 2, "goat"}, {"dummy", "red", 6, 3, "foe"}},
          false, nullptr, std::vector<std::string>(), "hex");
  CHECK(s.b().grid.isHex());
  CHECK(!s.canStop("lan", 4, 1));
  CHECK(s.canStop("lan", 3, 1));
  CHECK(s.canStop("goat", 4, 2));
  // Every square the lancer can end on has a path of steps no steeper than 1.
  Reach r;
  s.b().reachable(s.id("lan"), r);
  for (Cell c : r.stops) {
    std::vector<Cell> path;
    CHECK(s.b().pathTo(r, c, path));
    for (size_t i = 1; i < path.size(); ++i) CHECK(std::abs(s.b().heightAt(path[i]) - s.b().heightAt(path[i - 1])) <= 1);
  }
}

// ---- weapons: heightRange and the range bonus ----

TEST(a_weapon_with_a_height_range_refuses_a_target_too_high_or_too_low) {
  // The slinger stands at height 1 and cannot fire at anything higher; the pikeman reaches one level up and down.
  Scene s("hr_sling", {"01210", "01210", "01210"}, {{"slinger", "blue", 1, 1, "sl"}, {"pikeman", "blue", 3, 1, "pk"}, {"dummy", "red", 2, 1, "hi"}, {"dummy", "red", 0, 1, "lo"}, {"dummy", "red", 4, 1, "lo2"}});
  // (1,1) h1: the target at (2,1) h2 is higher; at (0,1) h0 lower.
  CHECK_STR(s.refusedHit("sl", "hi", "sling"), "too high to reach");
  CHECK_STR(s.refusedHit("sl", "lo", "sling"), "(allowed)");
  CHECK_STR(s.targets("sl", "sling"), std::to_string(s.id("lo")));  // only the low one is offered
  // The pikeman at (3,1) h1: (2,1) h2 is one level up: fine; (4,1) h0 one level down: fine.
  CHECK_STR(s.refusedHit("pk", "hi", "pike"), "(allowed)");
  CHECK_STR(s.refusedHit("pk", "lo2", "pike"), "(allowed)");
}

TEST(a_pike_cannot_reach_two_levels_either_way) {
  Scene s("hr_pike", {"0200", "0200", "0200", "0200"}, {{"pikeman", "blue", 1, 1, "pk"}, {"dummy", "red", 0, 1, "d0"}, {"dummy", "red", 2, 1, "d2"}});
  // The pikeman stands on height 2 (column 1); the dummies on both sides are two levels lower: too low.
  CHECK_STR(s.refusedHit("pk", "d0", "pike"), "too low to hit");
  CHECK_STR(s.refusedHit("pk", "d2", "pike"), "too low to hit");
  Scene t("hr_pike2", {"2000", "2000", "2000", "2000"}, {{"pikeman", "blue", 1, 1, "pk"}, {"dummy", "red", 0, 1, "d0"}, {"dummy", "red", 2, 1, "d2"}});
  CHECK_STR(t.refusedHit("pk", "d0", "pike"), "too high to reach");  // the dummy is two levels higher than the pikeman
  CHECK_STR(t.refusedHit("pk", "d2", "pike"), "(allowed)");
}

TEST(height_adds_one_square_of_range_per_two_levels_up_to_two) {
  auto reach = [](const char* digit, const char* weaponOf, int dist) {
    // A row: the shooter at column 0 standing at `digit`, a dummy `dist` squares along; every other square is level 0.
    std::string row(8, '0');
    row[0] = digit[0];
    Scene s(std::string("hr_range") + digit + weaponOf + std::to_string(dist), {row, row, row},
            {{weaponOf, "blue", 0, 1, "sh"}, {"dummy", "red", dist, 1, "foe"}});
    return s.refusedHit("sh", "foe", weaponOf[0] == 'a' ? "bow" : "sword") == "(allowed)";
  };
  // Archer, bow [2, 3]:
  CHECK(reach("0", "archer", 3));
  CHECK(!reach("0", "archer", 4));
  CHECK(!reach("1", "archer", 4));  // one level above: no bonus yet
  CHECK(reach("2", "archer", 4));   // two levels: +1
  CHECK(!reach("2", "archer", 5));
  CHECK(reach("4", "archer", 5));   // four levels: +2
  CHECK(!reach("4", "archer", 6));
  CHECK(!reach("9", "archer", 6));  // the bonus stops at 2
  CHECK(!reach("5", "archer", 1));  // the minimum range is untouched
  // A weapon that reaches one square gets no bonus.
  CHECK(!reach("9", "lancer", 2));
  CHECK(reach("9", "lancer", 1));  // (a drop of nine levels: the sword has no heightRange)
}

TEST(a_defender_on_high_ground_counters_from_further_away) {
  // The gunner (rifle [2, 6], line fire) shoots the archer four squares away; the archer stands four levels up and so answers with range 5.
  auto answers = [](const char* archerLevel) {
    std::string row = std::string("0000") + archerLevel;
    Scene s(std::string("hr_counter") + archerLevel, {row, row, row}, {{"gunner", "blue", 0, 1, "gun"}, {"archer", "red", 4, 1, "arc"}});
    Forecast f;
    return s.b().forecast(s.id("gun"), s.id("arc"), s.weapon("rifle"), s.unit("gun").cell, f) && f.counter.can;
  };
  CHECK(answers("4"));
  CHECK(!answers("0"));
  CHECK(!answers("1"));
}

// ---- line of sight ----

TEST(a_ridge_higher_than_both_ends_stops_line_fire) {
  Scene s("hr_los", {"00300000", "00000000", "00000000"}, {{"gunner", "blue", 0, 0, "gun"}, {"dummy", "red", 4, 0, "foe"}, {"dummy", "red", 4, 2, "other"}});
  CHECK_STR(s.refusedHit("gun", "foe", "rifle"), "the high ground is in the way");
  CHECK_EQ((int)s.targets("gun", "rifle").size(), 0);  // (the other dummy is not on a straight line)
  std::vector<Cell> aim;
  s.b().aimCells(s.weapon("rifle"), s.at(0, 0), s.id("gun"), aim);
  CHECK(std::find(aim.begin(), aim.end(), s.at(2, 0)) != aim.end());  // the ridge itself: nothing is between
  CHECK(std::find(aim.begin(), aim.end(), s.at(3, 0)) == aim.end());  // behind it
  CHECK(std::find(aim.begin(), aim.end(), s.at(4, 0)) == aim.end());
  CHECK(std::find(aim.begin(), aim.end(), s.at(0, 2)) != aim.end());  // the other line is open
}

TEST(a_target_as_high_as_the_ridge_can_be_shot_and_the_rule_can_be_off) {
  // The dummy stands on level 3 as the ridge does: the ridge is not higher than BOTH ends.
  Scene s("hr_los2", {"00300000", "00000000", "00000000"}, {{"gunner", "blue", 0, 0, "gun"}, {"dummy", "red", 4, 0, "foe"}});
  s.b().cells[(size_t)s.at(4, 0)].height = 3;
  CHECK_STR(s.refusedHit("gun", "foe", "rifle"), "(allowed)");
  s.b().cells[(size_t)s.at(4, 0)].height = 2;
  CHECK_STR(s.refusedHit("gun", "foe", "rifle"), "the high ground is in the way");
  Scene off("hr_los3", {"00300000", "00000000", "00000000"}, {{"gunner", "blue", 0, 0, "gun"}, {"dummy", "red", 4, 0, "foe"}}, false,
            [](skilltest::Copy& c) { c.swap("game.json", "\"lineOfSight\": true", "\"lineOfSight\": false"); });
  CHECK_STR(off.refusedHit("gun", "foe", "rifle"), "(allowed)");
}

TEST(line_of_sight_on_hexagons_walks_the_same_straight_line) {
  Scene s("hr_los_hex", {"000000000", "000000000", "000000000", "000000000"}, {{"gunner", "blue", 0, 1, "gun"}, {"dummy", "red", 4, 1, "foe"}}, false, nullptr,
          std::vector<std::string>(), "hex");
  CHECK_STR(s.refusedHit("gun", "foe", "rifle"), "(allowed)");
  s.b().cells[(size_t)s.at(2, 1)].height = 4;  // the middle of the straight line along the row
  CHECK_STR(s.refusedHit("gun", "foe", "rifle"), "the high ground is in the way");
}

// ---- area attacks ----

TEST(an_area_leaves_out_what_stands_further_than_vertical_from_the_aimed_square) {
  // Aimed at (3,1) (level 0): the dummy at (4,1) is one level up (hit), the dummy at (3,0) two levels up (not hit).
  Scene s("hr_area", {"00020000", "00001000", "00000000"}, {{"bomber", "blue", 0, 1, "bomb"}, {"dummy", "red", 3, 1, "foe"}, {"dummy", "red", 4, 1, "near"}, {"dummy", "red", 3, 0, "far"}});
  AreaForecast f;
  CHECK(s.b().areaForecast(s.id("bomb"), s.weapon("bomb"), s.at(0, 1), s.at(3, 1), f));
  std::vector<int> ids;
  for (const AreaVictim& v : f.victims) ids.push_back(v.unit);
  CHECK_EQ((int)ids.size(), 2);
  CHECK(std::find(ids.begin(), ids.end(), s.id("foe")) != ids.end());
  CHECK(std::find(ids.begin(), ids.end(), s.id("near")) != ids.end());
  CHECK(std::find(ids.begin(), ids.end(), s.id("far")) == ids.end());
  CHECK(std::find(f.cells.begin(), f.cells.end(), s.at(3, 0)) == f.cells.end());  // not even covered
  // And the attack really leaves the far one alone.
  Action shot = Action::attack(s.id("bomb"), s.id("foe"), s.weapon("bomb"));
  shot.cell = s.at(3, 1);
  const int before = s.unit("far").hp;
  CHECK(s.w->apply(shot));
  CHECK_EQ(s.unit("far").hp, before);
  CHECK(s.unit("near").hp < s.b().maxHp(s.unit("near")));
}

// ---- the enemy-range display ----

TEST(the_enemy_range_follows_the_jump_and_the_height_range) {
  // A red lancer below the cliff: it cannot climb it, so the plateau squares are not in its reach (nor in what it hits: they are 2 away from its
  // last square at column 4 ... one square, in reach of its sword from column 4).
  Scene s("hr_threat", cliff(), {{"dummy", "blue", 0, 0, "x"}, {"lancer", "red", 2, 1, "foe"}});
  ThreatArea area;
  Reach scratch;
  threatArea(s.b(), s.id("foe"), 0, scratch, area);
  CHECK(area.move[(size_t)s.at(4, 1)]);
  CHECK(!area.move[(size_t)s.at(5, 1)]);
  CHECK(area.attack[(size_t)s.at(5, 1)]);   // a sword from the foot of the cliff still reaches the edge square
  CHECK(!area.attack[(size_t)s.at(6, 1)]);
  // A slinger at level 1 facing a plateau of level 3 (too high to climb): the plateau is not painted as its target; the low squares are.
  Scene t("hr_threat2", {"01300", "01300", "01300"}, {{"dummy", "blue", 0, 0, "x"}, {"slinger", "red", 1, 1, "foe"}});
  ThreatArea a2;
  threatArea(t.b(), t.id("foe"), 0, scratch, a2);
  CHECK(!a2.move[(size_t)t.at(2, 1)]);
  CHECK(!a2.attack[(size_t)t.at(2, 1)]);
  CHECK(a2.attack[(size_t)t.at(0, 1)]);
  CHECK(a2.attack[(size_t)t.at(3, 1)]);
}

// ---- the loader ----

namespace {
std::string loadError(const char* name, const char* from, const char* to, const char* file = "game.json") {
  skilltest::Copy c(name, "height-rules");
  c.swap(file, from, to);
  return c.gameError();
}
}  // namespace

TEST(the_loader_refuses_wrong_height_keys_naming_the_file_and_the_key) {
  CHECK_STR(loadError("hrl0", "\"perLevels\": 2", "\"perLevels\": 2"), "");  // the fixture itself loads
  CHECK(saysIn(loadError("hrl1", "\"perLevels\": 2", "\"perLevels\": 0"), "rules.height, \"rangeBonus\": \"perLevels\" must be a whole number from 1 to 999"));
  CHECK(saysIn(loadError("hrl2", "\"max\": 2", "\"max\": 20"), "rules.height, \"rangeBonus\": \"max\" must be a whole number from 0 to 8"));
  CHECK(saysIn(loadError("hrl3", "\"lineOfSight\": true", "\"lineOfSight\": 1"), "\"lineOfSight\" must be true or false"));
  CHECK(saysIn(loadError("hrl4", "\"flyingMoveTypes\": [\"air\"]", "\"flyingMoveTypes\": [\"wing\"]"), "\"flyingMoveTypes\" names unknown move type \"wing\""));
  CHECK(saysIn(loadError("hrl5", "\"jump\": { \"default\": 1 }", "\"jump\": { \"default\": -1 }"), "\"default\" must be a whole number from 0 to 999"));
  CHECK(saysIn(loadError("hrl6", "\"lineOfSight\": true", "\"lineOfSight\": true, \"fall\": 3"), "unknown key \"fall\""));
  CHECK(saysIn(loadError("hrl7", "\"heightRange\": { \"up\": 0 }", "\"heightRange\": { \"up\": -2 }", "data/weapons.json"), "\"heightRange\": \"up\" must be a whole number from 0 to 999"));
  CHECK(saysIn(loadError("hrl8", "\"heightRange\": { \"up\": 0 }", "\"heightRange\": {}", "data/weapons.json"), "\"heightRange\": needs \"up\" and / or \"down\""));
  CHECK(saysIn(loadError("hrl9", "\"heightRange\": { \"up\": 0 }", "\"heightRange\": { \"sideways\": 1 }", "data/weapons.json"), "unknown key \"sideways\""));
  CHECK(saysIn(loadError("hrl10", "\"vertical\": 1", "\"vertical\": -1", "data/weapons.json"), "\"area\": \"vertical\" must be a whole number from 0 to 999"));
  CHECK(saysIn(loadError("hrl11", "\"jump\": 0", "\"jump\": \"high\"", "data/units.json"), "\"jump\" must be a whole number from 0 to 999 or { \"up\": n, \"down\": m }"));
  CHECK(saysIn(loadError("hrl12", "\"jump\": { \"down\": 5 }", "\"jump\": { \"left\": 5 }", "data/units.json"), "unknown key \"left\""));
}

TEST(the_height_keys_need_the_height_rules_in_game_json) {
  auto without = [](const char* name, const char* file, const char* from, const char* to) {
    skilltest::Copy c(name, "height-rules");
    heighttest::dropHeights(c);
    c.swap(file, from, to);
    return c.gameError();
  };
  CHECK_STR(without("hrn0", "game.json", "\"formatVersion\": 1", "\"formatVersion\": 1"), "");
  CHECK(saysIn(without("hrn1", "data/weapons.json", "\"id\": \"sling\"", "\"heightRange\": { \"up\": 0 }, \"id\": \"sling\""), "\"heightRange\" needs \"rules\": { \"height\": ... }"));
  CHECK(saysIn(without("hrn2", "data/weapons.json", "\"shape\": \"burst\"", "\"vertical\": 1, \"shape\": \"burst\""), "\"area\".\"vertical\" needs \"rules\": { \"height\": ... }"));
  CHECK(saysIn(without("hrn3", "data/units.json", "\"id\": \"lancer\"", "\"jump\": 1, \"id\": \"lancer\""), "\"jump\" needs \"rules\": { \"height\": ... }"));
}

// ---- saves and determinism ----

TEST(a_battle_in_a_height_game_saves_and_restores_unchanged) {
  Scene s("hr_save", cliff(), {{"lancer", "blue", 2, 1, "lan"}, {"goat", "blue", 2, 0, "goat"}, {"dummy", "red", 7, 2, "foe"}});
  CHECK(s.go("goat", 5, 1));
  Json j;
  saveBattleState(s.b(), j);
  Scene t("hr_save2", cliff(), {{"lancer", "blue", 2, 1, "lan"}, {"goat", "blue", 2, 0, "goat"}, {"dummy", "red", 7, 2, "foe"}});
  std::string err;
  CHECK(restoreBattleState(t.b(), j, err));
  CHECK_STR(savetest::stateText(t.b()), savetest::stateText(s.b()));
  CHECK(!t.canStop("lan", 5, 1));  // the rules are the game's, the same after a restore
  CHECK(t.canStop("goat", 6, 0));
}

TEST(the_same_orders_give_the_same_height_battle) {
  auto play = [](const char* name) {
    Scene s(name, cliff(), {{"lancer", "blue", 2, 1, "lan"}, {"goat", "blue", 2, 0, "goat"}, {"dummy", "red", 6, 1, "foe"}});
    s.go("goat", 5, 1);
    s.hit("goat", "foe", "sword");
    s.w->apply(Action::endTurn());
    return std::make_pair(savetest::stateText(s.b()), s.w->events.size());
  };
  CHECK(play("hr_det1") == play("hr_det2"));
}
