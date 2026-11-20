// docs/SRPG.md "Facing": where units look at the start, what turns them (a move, an attack, a Wait that chooses), the arc of an attack and what it does
// to the numbers (the formula variable `arc`, the forecast, the skill condition { "arc": ... }), area attacks, hexagons, saves, the loader and the events.
// Fixture tests/fixtures/facing (see tests/facing_fixture.h for its numbers).
#include "core/battle/save.h"
#include "facing_fixture.h"

using namespace ote;
using facingtest::Scene;
using facingtest::U;
using skilltest::saysIn;

namespace {

enum { N = 0, E = 1, S = 2, W = 3 };  // the square directions

int firstEvent(const std::vector<Event>& events, EventType t) {
  for (size_t i = 0; i < events.size(); ++i)
    if (events[i].type == t) return (int)i;
  return -1;
}

}  // namespace

// ---- the start ----

TEST(a_map_unit_may_say_which_way_it_looks_and_the_others_look_at_the_nearest_foe) {
  Scene s("fc_start", facingtest::plain(9, 5), {{"lancer", "blue", 1, 2, "lan"}, {"dummy", "red", 6, 2, "east", "south"}, {"dummy", "red", 1, 0, "near"}});
  CHECK(s.b().facingGame());
  CHECK_EQ(s.unit("east").facing, S);   // the map said so
  CHECK_EQ(s.unit("lan").facing, N);    // the nearest foe is the dummy two squares north
  CHECK_EQ(s.unit("near").facing, S);   // and for it, the lancer to the south
  // A unit with nobody to look at looks south.
  Scene t("fc_alone", facingtest::plain(5, 3), {{"lancer", "blue", 1, 1, "lan"}});
  CHECK_EQ(t.unit("lan").facing, S);
}

TEST(a_built_unit_and_a_summoned_unit_get_a_facing_too) {
  // (A built unit: the factory rules need production; the simple route is a story-free check that the helper gives the nearest-foe default.)
  Scene s("fc_default", facingtest::plain(9, 5), {{"lancer", "blue", 4, 2, "lan"}, {"dummy", "red", 4, 0, "foe"}});
  CHECK_EQ(s.b().defaultFacing(s.at(4, 4), 0), N);  // a square south of the lancer: the nearest foe is north of it
  CHECK_EQ(s.b().defaultFacing(s.at(4, 1), 1), S);  // a red unit on the square between them: the lancer is south
}

// ---- what turns a unit ----

TEST(a_move_turns_the_unit_toward_its_last_step) {
  Scene s("fc_move", facingtest::plain(9, 5), {{"lancer", "blue", 1, 2, "lan", "north"}, {"dummy", "red", 8, 4, "foe", "north"}});
  CHECK(s.go("lan", 4, 2));
  CHECK_EQ(s.unit("lan").facing, E);
  CHECK_STR(s.faced(), std::to_string(s.id("lan")) + ">1");
  const Event* e = nullptr;
  for (const Event& ev : s.w->events)
    if (ev.type == EventType::UnitFaced) e = &ev;
  CHECK(e && e->b == E && e->c == N);                  // the new direction and the old one
  CHECK(firstEvent(s.w->events, EventType::UnitMoved) < firstEvent(s.w->events, EventType::UnitFaced));
  // A walk that ends going round a corner looks where the LAST step went.
  Scene t("fc_move2", facingtest::plain(9, 5), {{"lancer", "blue", 1, 2, "lan"}, {"dummy", "red", 8, 4, "foe"}});
  CHECK(t.go("lan", 3, 0));  // 2 east, 2 north (the search is deterministic; whichever path it takes ends with a step in one of the two directions)
  CHECK(t.unit("lan").facing == N || t.unit("lan").facing == E);
  // No event when it already looked that way.
  Scene u("fc_move3", facingtest::plain(9, 5), {{"lancer", "blue", 1, 2, "lan", "east"}, {"dummy", "red", 8, 4, "foe"}});
  CHECK(u.go("lan", 4, 2));
  CHECK_EQ(u.count(EventType::UnitFaced), 0);
}

TEST(an_attack_turns_the_attacker_but_never_the_defender_and_a_counter_hits_the_front) {
  // The lancer stands west of the dummy, which looks east, away from it: a back attack. (8 + 5 = 13; the counter, 3 + 4 - 2 = 5, meets the front.)
  Scene s("fc_attack", facingtest::plain(9, 5), {{"lancer", "blue", 3, 2, "lan", "west"}, {"dummy", "red", 4, 2, "foe", "east"}});
  const Forecast f = s.forecast("lan", "foe", "sword");
  CHECK_EQ(f.arc, (int)kArcBack);
  CHECK_EQ(f.attack.damage, 13);
  CHECK(f.counter.can);
  CHECK_EQ(f.counter.damage, 5);
  CHECK(s.hit("lan", "foe", "sword"));
  CHECK_EQ(s.unit("foe").hp, 60 - 13);
  CHECK_EQ(s.unit("lan").hp, 40 - 5);
  CHECK_EQ(s.unit("lan").facing, E);   // it turned to its target ...
  CHECK_EQ(s.unit("foe").facing, E);   // ... and the defender did not turn
  CHECK_STR(s.faced(), std::to_string(s.id("lan")) + ">1");
  CHECK(firstEvent(s.w->events, EventType::UnitFaced) < firstEvent(s.w->events, EventType::Attack));
}

TEST(the_arc_of_an_attack_is_front_side_or_back_against_the_defenders_facing) {
  auto damageFrom = [](const char* facing, int& arc) {
    Scene s(std::string("fc_arc_") + facing, facingtest::plain(9, 5), {{"lancer", "blue", 3, 2, "lan"}, {"dummy", "red", 4, 2, "foe", facing}});
    const Forecast f = s.forecast("lan", "foe", "sword");
    arc = f.arc;
    return f.attack.damage;
  };
  int arc = -1;
  CHECK_EQ(damageFrom("west", arc), 8);   // looking at the attacker: front
  CHECK_EQ(arc, (int)kArcFront);
  CHECK_EQ(damageFrom("north", arc), 10);  // looking along: side (+2)
  CHECK_EQ(arc, (int)kArcSide);
  CHECK_EQ(damageFrom("south", arc), 10);
  CHECK_EQ(damageFrom("east", arc), 13);  // looking away: back (+5)
  CHECK_EQ(arc, (int)kArcBack);
}

TEST(a_ranged_attacker_not_on_a_line_comes_from_the_dominant_direction_and_a_diagonal_is_the_milder_arc) {
  // The archer at (1,4) shoots a dummy at (3,3): 2 east, 1 north: from the dummy it lies to the west and a little south: west.
  Scene s("fc_ranged", facingtest::plain(9, 5), {{"archer", "blue", 1, 4, "arc"}, {"dummy", "red", 3, 3, "foe", "west"}});
  CHECK_EQ(s.forecast("arc", "foe", "bow").arc, (int)kArcFront);
  CHECK(s.hit("arc", "foe", "bow"));
  CHECK_EQ(s.unit("arc").facing, E);  // it turned toward the target by the dominant axis
  // A true diagonal: the dummy at (1,3), the archer at (0,4): one across and one down. Facing east it has the archer at south (side) or west (back): side.
  Scene t("fc_diag", facingtest::plain(9, 5), {{"archer", "blue", 0, 4, "arc"}, {"dummy", "red", 1, 3, "foe", "east"}});
  CHECK_EQ(t.forecast("arc", "foe", "bow").arc, (int)kArcSide);
}

// ---- Wait ----

TEST(a_wait_may_choose_the_direction_and_without_one_the_facing_stays) {
  Scene s("fc_wait", facingtest::plain(9, 5), {{"lancer", "blue", 1, 2, "lan", "north"}, {"archer", "blue", 1, 3, "arc", "north"}, {"dummy", "red", 8, 4, "foe"}});
  CHECK(s.wait("lan", W));
  CHECK_EQ(s.unit("lan").facing, W);
  CHECK(s.unit("lan").done);
  CHECK_STR(s.faced(), std::to_string(s.id("lan")) + ">3");
  CHECK(s.wait("arc"));  // no direction: it keeps its own
  CHECK_EQ(s.unit("arc").facing, N);
  CHECK_EQ(s.count(EventType::UnitFaced), 0);
  Scene t("fc_wait2", facingtest::plain(9, 5), {{"lancer", "blue", 1, 2, "lan", "north"}, {"dummy", "red", 8, 4, "foe"}});
  CHECK_STR(t.refused(Action::wait(t.id("lan"), 4)), "that is not a direction");  // a square has four
  CHECK_STR(t.refused(Action::wait(t.id("lan"), 3)), "(allowed)");
  CHECK_EQ(t.unit("lan").facing, N);  // (a refusal changes nothing)
  // The same direction again is no event.
  CHECK(t.wait("lan", N));
  CHECK_EQ(t.count(EventType::UnitFaced), 0);
}

TEST(a_unit_that_has_moved_may_still_choose_its_facing_when_it_waits) {
  Scene s("fc_move_wait", facingtest::plain(9, 5), {{"lancer", "blue", 1, 2, "lan"}, {"dummy", "red", 8, 4, "foe"}});
  CHECK(s.go("lan", 3, 2));
  CHECK_EQ(s.unit("lan").facing, E);
  CHECK(s.wait("lan", N));
  CHECK_EQ(s.unit("lan").facing, N);
}

TEST(a_game_without_facing_has_none_of_it) {
  Scene s("fc_off", facingtest::plain(9, 5), {{"lancer", "blue", 3, 2, "lan"}, {"dummy", "red", 4, 2, "foe"}}, false, facingtest::dropFacing);
  CHECK(!s.b().facingGame());
  CHECK_EQ(s.unit("lan").facing, -1);
  CHECK_STR(s.refused(Action::wait(s.id("lan"), 1)), "this game has no facing");
  const Forecast f = s.forecast("lan", "foe", "sword");
  CHECK_EQ(f.arc, (int)kArcFront);
  CHECK_EQ(f.attack.damage, 8);  // the formula never sees an arc
  CHECK(s.hit("lan", "foe", "sword"));
  CHECK_EQ(s.count(EventType::UnitFaced), 0);
  CHECK_EQ(s.unit("lan").facing, -1);
  CHECK_EQ(s.b().arcAt(s.at(4, 2), 1, s.at(3, 2)), (int)kArcFront);
}

// ---- skills and formulas ----

TEST(the_arc_condition_of_a_skill_sees_the_owner_in_the_foes_back) {
  // The assassin (a skill: +4 damage when it stands in the foe's back arc) at the dummy's back: 8 + 5 + 4 = 17; from the front 8.
  Scene s("fc_skill", facingtest::plain(9, 5), {{"assassin", "blue", 3, 2, "ass"}, {"dummy", "red", 4, 2, "foe", "east"}});
  CHECK_EQ(s.forecast("ass", "foe", "sword").attack.damage, 17);
  s.unit("foe").facing = W;
  CHECK_EQ(s.forecast("ass", "foe", "sword").attack.damage, 8);
  s.unit("foe").facing = N;
  CHECK_EQ(s.forecast("ass", "foe", "sword").attack.damage, 10);  // a side attack: the formula's +2, not the skill's
  // The same assassin attacked: the foe has turned to look at it before it strikes, so the counter is a front attack whatever the assassin faced.
  Scene t("fc_skill2", facingtest::plain(9, 5), {{"dummy", "blue", 3, 2, "dum", "west"}, {"assassin", "red", 4, 2, "ass", "east"}});
  const Forecast f = t.forecast("dum", "ass", "sword");
  CHECK_EQ(f.arc, (int)kArcBack);  // the dummy hits the assassin's back
  CHECK_EQ(f.counter.damage, 6 + 4 - 2 + 0);  // the assassin's counter meets the dummy's front: no arc, no skill
}

TEST(the_arc_condition_with_who_foe_asks_whether_the_foe_is_in_the_owners_back) {
  // The sentry's skill: it takes 4 more damage when its foe stands in ITS back arc. Hit from behind: 8 + 5 + 4 = 17; from the front 8.
  Scene s("fc_skill3", facingtest::plain(9, 5), {{"lancer", "blue", 3, 2, "lan"}, {"sentry", "red", 4, 2, "sen", "east"}});
  CHECK_EQ(s.forecast("lan", "sen", "sword").attack.damage, 17);
  s.unit("sen").facing = W;
  CHECK_EQ(s.forecast("lan", "sen", "sword").attack.damage, 8);
}

TEST(a_skill_condition_arc_needs_the_facing_rules) {
  skilltest::Copy c("fc_skill_off", "facing");
  facingtest::dropFacing(c);
  c.put("data/skills.json", "{ \"skills\": [ { \"id\": \"x\", \"name\": \"X\", \"blocks\": [ { \"when\": \"combat\", \"if\": [ { \"arc\": \"back\" } ], \"do\": [ { \"damage\": 4 } ] } ] } ] }");
  CHECK(saysIn(c.gameError(), "\"arc\" needs \"rules\": { \"facing\": ... } in game.json"));
  skilltest::Copy d("fc_skill_bad", "facing");
  d.swap("data/skills.json", "\"arc\": \"back\" }", "\"arc\": \"behind\" }");
  CHECK(saysIn(d.gameError(), "\"arc\" must be \"front\", \"side\" or \"back\""));
  skilltest::Copy e("fc_skill_story", "facing");
  e.swap("data/skills.json", "\"when\": \"combat\", \"if\": [ { \"arc\": \"back\" } ]", "\"when\": \"always\", \"if\": [ { \"arc\": \"back\" } ]");
  CHECK(saysIn(e.gameError(), "cannot be used"));  // outside a combat there is no foe to be behind
}

// ---- area attacks ----

TEST(an_area_attack_turns_the_attacker_toward_the_aimed_square_and_every_victim_keeps_its_facing) {
  // The bomber at (1,2) aims at (4,2): the dummy there looks east (its back to the bomber), the one at (4,1) looks west (toward it).
  Scene s("fc_area", facingtest::plain(9, 5), {{"bomber", "blue", 1, 2, "bomb", "north"}, {"dummy", "red", 4, 2, "back", "east"}, {"dummy", "red", 4, 1, "front", "west"}});
  Action shot = Action::attack(s.id("bomb"), s.id("back"), s.weapon("bomb"));
  shot.cell = s.at(4, 2);
  CHECK(s.w->apply(shot));
  CHECK_EQ(s.unit("bomb").facing, E);
  CHECK_EQ(s.unit("back").facing, E);
  CHECK_EQ(s.unit("front").facing, W);
  CHECK_EQ(s.unit("back").hp, 60 - 12);   // bomb 3 + str 6 - def 2 = 7, +5 for the back
  CHECK_EQ(s.unit("front").hp, 60 - 7);   // the front: no bonus
  CHECK(firstEvent(s.w->events, EventType::UnitFaced) < firstEvent(s.w->events, EventType::Attack));
}

// ---- hexagons ----

TEST(on_hexagons_the_front_is_three_directions_and_a_move_turns_toward_the_last_step) {
  Scene s("fc_hex", facingtest::plain(9, 7), {{"lancer", "blue", 2, 3, "lan", "east"}, {"dummy", "red", 7, 3, "foe", "east"}}, false, nullptr, "hex");
  CHECK(s.b().grid.isHex());
  CHECK_EQ(s.unit("lan").facing, 1);  // "east" of a pointy hexagon is direction 1
  // The dummy stands on a hexagon; put the lancer next to it from the west and turn the dummy through every direction.
  Scene t("fc_hex2", facingtest::plain(9, 7), {{"lancer", "blue", 3, 3, "lan"}, {"dummy", "red", 4, 3, "foe", "east"}}, false, nullptr, "hex");
  const int toLancer = t.b().grid.directionTo(t.at(4, 3), t.at(3, 3));  // the direction from the dummy to the lancer: west (4)
  CHECK_EQ(toLancer, 4);
  for (int facing = 0; facing < 6; ++facing) {
    t.unit("foe").facing = facing;
    const int rel = (toLancer - facing + 6) % 6;
    const int want = (rel == 0 || rel == 1 || rel == 5) ? kArcFront : rel == 3 ? kArcBack : kArcSide;
    CHECK_EQ(t.forecast("lan", "foe", "sword").arc, want);
  }
  // A move turns toward the last step on a hexagon grid too.
  CHECK(t.go("lan", 3, 1));
  const int dir = t.unit("lan").facing;
  CHECK(dir >= 0 && dir < 6);
  CHECK(dir == 0 || dir == 5 || dir == 4 || dir == 1);  // (a short walk north-ish: the last step went one of these ways)
}

// ---- saves ----

TEST(a_save_in_a_facing_game_keeps_every_facing_and_other_games_save_none) {
  Scene s("fc_save", facingtest::plain(9, 5), {{"lancer", "blue", 1, 2, "lan", "north"}, {"dummy", "red", 6, 2, "foe", "west"}});
  CHECK(s.go("lan", 4, 2));
  Json j;
  saveBattleState(s.b(), j);
  CHECK(j.dump(0).find("\"facing\":1") != std::string::npos);
  Scene t("fc_save2", facingtest::plain(9, 5), {{"lancer", "blue", 1, 2, "lan", "north"}, {"dummy", "red", 6, 2, "foe", "west"}});
  std::string err;
  CHECK(restoreBattleState(t.b(), j, err));
  CHECK_STR(savetest::stateText(t.b()), savetest::stateText(s.b()));
  CHECK_EQ(t.unit("lan").facing, E);
  CHECK_EQ(t.unit("foe").facing, W);
  // A game without facing writes no such key.
  Scene off("fc_save3", facingtest::plain(9, 5), {{"lancer", "blue", 1, 2, "lan"}, {"dummy", "red", 6, 2, "foe"}}, false, facingtest::dropFacing);
  Json o;
  saveBattleState(off.b(), o);
  CHECK(o.dump(0).find("facing") == std::string::npos);
}

TEST(a_save_with_a_wrong_or_missing_facing_is_handled) {
  Scene s("fc_save_bad", facingtest::plain(9, 5), {{"lancer", "blue", 1, 2, "lan", "north"}, {"dummy", "red", 6, 2, "foe", "west"}});
  Json j;
  saveBattleState(s.b(), j);
  std::string text = j.dump(0);
  const size_t at = text.find("\"facing\":0");
  CHECK(at != std::string::npos);
  std::string bad = text;
  bad.replace(at, 10, "\"facing\":9");
  Json bj;
  std::string perr;
  CHECK(Json::parse(bad, bj, perr));
  Scene t("fc_save_bad2", facingtest::plain(9, 5), {{"lancer", "blue", 1, 2, "lan"}, {"dummy", "red", 6, 2, "foe"}});
  std::string err;
  CHECK(!restoreBattleState(t.b(), bj, err));
  CHECK(err.find("facing") != std::string::npos);
  // Without the keys (a save from before the game had facing) the units look at the nearest foe.
  std::string none = text;
  while (true) {
    const size_t k = none.find(",\"facing\":");
    if (k == std::string::npos) break;
    size_t end = k + 10;
    while (end < none.size() && none[end] >= '0' && none[end] <= '9') ++end;
    none.erase(k, end - k);
  }
  Json nj;
  CHECK(Json::parse(none, nj, perr));
  Scene u("fc_save_none", facingtest::plain(9, 5), {{"lancer", "blue", 1, 2, "lan", "south"}, {"dummy", "red", 6, 2, "foe", "south"}});
  CHECK(restoreBattleState(u.b(), nj, err));
  CHECK_EQ(u.unit("lan").facing, E);
  CHECK_EQ(u.unit("foe").facing, W);
  // A "facing" in the save of a game without facing is refused.
  Scene off("fc_save_off", facingtest::plain(9, 5), {{"lancer", "blue", 1, 2, "lan"}, {"dummy", "red", 6, 2, "foe"}}, false, facingtest::dropFacing);
  CHECK(!restoreBattleState(off.b(), j, err));
}

// ---- the loader ----

namespace {
std::string loadError(const char* name, const char* from, const char* to, const char* file = "game.json") {
  skilltest::Copy c(name, "facing");
  c.swap(file, from, to);
  return c.gameError();
}
}  // namespace

TEST(the_loader_refuses_wrong_facing_keys_naming_the_file_and_the_key) {
  CHECK_STR(loadError("fcl0", "\"name\": \"Backstab\"", "\"name\": \"Backstab\""), "");
  CHECK(saysIn(loadError("fcl1", "\"arcs\"", "\"arcs\": {}, \"speed\": 3, \"x\""), "rules.facing: unknown key \"speed\""));
  CHECK(saysIn(loadError("fcl2", "\"back\": { \"name\": \"Backstab\" }", "\"behind\": { \"name\": \"Backstab\" }"), "rules.facing, \"arcs\": unknown key \"behind\""));
  CHECK(saysIn(loadError("fcl3", "\"name\": \"Backstab\"", "\"name\": \"\""), "\"name\" must be 1 to 16 characters"));
  CHECK(saysIn(loadError("fcl4", "\"name\": \"Backstab\"", "\"title\": \"Backstab\""), "unknown key \"title\""));
  CHECK(saysIn(loadError("fcl5", "\"name\": \"Backstab\"", "\"name\": 7"), "\"name\" must be a string"));
}

TEST(a_map_unit_facing_must_be_a_direction_of_the_grid_and_needs_the_rules) {
  auto mapError = [](const char* name, const char* facing, const char* grid) {
    skilltest::Copy c(name, "facing");
    if (std::string(grid) == "hex") c.swap("game.json", "\"formatVersion\": 1,", "\"formatVersion\": 1, \"grid\": { \"type\": \"hex\", \"layout\": \"pointy\" },");
    c.put("maps/field.json", facingtest::mapJson(facingtest::plain(6, 4), {{"lancer", "blue", 1, 1, "lan", facing}, {"dummy", "red", 4, 2, "foe"}}, false, grid));
    return c.mapError("field");
  };
  CHECK_STR(mapError("fcm1", "south", "square"), "");
  CHECK(saysIn(mapError("fcm2", "northeast", "square"), "\"facing\" must be one of \"north\", \"east\", \"south\", \"west\" on this map's grid (not \"northeast\")"));
  CHECK_STR(mapError("fcm3", "northeast", "hex"), "");
  CHECK(saysIn(mapError("fcm4", "north", "hex"), "\"facing\" must be one of \"northeast\", \"east\", \"southeast\", \"southwest\", \"west\", \"northwest\""));
  skilltest::Copy c("fcm5", "facing");
  facingtest::dropFacing(c);
  c.put("maps/field.json", facingtest::mapJson(facingtest::plain(6, 4), {{"lancer", "blue", 1, 1, "lan", "south"}, {"dummy", "red", 4, 2, "foe"}}));
  CHECK(saysIn(c.mapError("field"), "\"facing\" needs \"rules\": { \"facing\": ... } in game.json"));
}

// ---- determinism ----

TEST(the_same_orders_give_the_same_facing_battle) {
  auto play = [](const char* name) {
    Scene s(name, facingtest::plain(9, 5), {{"lancer", "blue", 1, 2, "lan"}, {"dummy", "red", 5, 2, "foe", "east"}});
    s.go("lan", 4, 2);
    s.hit("lan", "foe", "sword");
    s.w->apply(Action::endTurn());
    return std::make_pair(savetest::stateText(s.b()), s.w->events.size());
  };
  CHECK(play("fc_det1") == play("fc_det2"));
}
