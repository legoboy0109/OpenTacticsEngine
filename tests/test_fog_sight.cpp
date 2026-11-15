// Fog of war: who sees what (docs/ECONOMY.md "Fog of war"): vision by grid distance, bonuses, structures' sight, hiding
// squares, allied teams, and that sight follows every change (move, death, capture, build, story changes, upgrades,
// hand-made changes with refreshSight).
#include "fog_fixture.h"

using namespace ote;
using namespace fogtest;

TEST(fog_sight_a_unit_sees_squares_within_its_vision_by_grid_distance) {
  FogWorld w(field(12, 5), {{"soldier", 0, 4, 2, false}, {"soldier", 1, 11, 4, false}});
  CHECK(w.begin());
  CHECK(w.sees(0, 4, 2));    // its own square
  CHECK(w.sees(0, 6, 2));    // distance 2
  CHECK(w.sees(0, 5, 3));    // distance 2 (two steps, not "one diagonal")
  CHECK(w.sees(0, 3, 1));
  CHECK(!w.sees(0, 7, 2));   // distance 3
  CHECK(!w.sees(0, 6, 3));   // distance 3
  CHECK(!w.sees(0, 11, 4));
  CHECK(w.sees(1, 11, 4));
  CHECK(!w.sees(1, 4, 2));
}

TEST(fog_sight_vision_is_per_unit_type_and_zero_sees_only_its_own_square) {
  FogWorld w(field(12, 5), {{"soldier", 0, 2, 2, false}, {"archer", 0, 9, 2, false}, {"tank", 1, 11, 4, false}});
  w.vision("soldier", 4);
  w.vision("archer", 0);
  CHECK(w.begin());
  CHECK(w.sees(0, 6, 2));    // soldier: 4 squares
  CHECK(!w.sees(0, 7, 2));
  CHECK(w.sees(0, 9, 2));    // the archer's own square
  CHECK(!w.sees(0, 10, 2));  // vision 0: nothing beyond it
  CHECK(!w.sees(0, 9, 3));
}

TEST(fog_sight_default_vision_is_two) {
  FogWorld w(field(), {{"soldier", 0, 0, 0, false}, {"soldier", 1, 11, 2, false}});
  CHECK_EQ(w.db.unitTypes[0].vision, 2);
  CHECK(w.begin());
}

TEST(fog_sight_the_square_a_unit_stands_on_adds_its_vision_bonus) {
  FogWorld w({".....", "..f..", "....."}, {{"soldier", 0, 2, 1, false}, {"soldier", 1, 4, 2, false}});
  w.db.terrains[(size_t)w.db.findTerrain("forest")].visionBonus = 2;
  CHECK(w.begin());
  CHECK(w.sees(0, 0, 0));    // distance 3 from (2,1)
  CHECK(w.sees(0, 4, 2));    // distance 3: the red soldier is within the bonus
  CHECK(w.seesUnit(0, 1));
  CHECK(w.sees(0, 0, 1));
  // Off the forest, the bonus is gone.
  CHECK(w.act(Action::move(0, w.at(1, 1))));
  CHECK(!w.sees(0, 4, 2));
  CHECK(!w.seesUnit(0, 1));
}

TEST(fog_sight_a_structures_vision_bonus_is_added_to_the_terrains) {
  FogWorld w({"......", "......", "......"}, {{"soldier", 0, 1, 1, false}, {"soldier", 1, 5, 2, false}});
  w.place("tower", 1, 1);   // a neutral tower under the blue soldier: bonus 3
  w.db.terrains[(size_t)w.db.findTerrain("plain")].visionBonus = 1;
  CHECK(w.begin());
  // 2 (type) + 1 (terrain) + 3 (structure) = 6 squares: (5,2) is 5 away
  CHECK(w.sees(0, 5, 2));
  CHECK(w.seesUnit(0, 1));
}

TEST(fog_sight_an_owned_structure_sees_one_square_around_every_square_of_its_footprint) {
  FogWorld w({"..........", "..........", "..........", "..........", ".........."},
             {{"soldier", 0, 9, 4, false}, {"soldier", 1, 0, 4, false}});
  w.place("castle", 3, 1, 0);  // 2 x 2 at (3,1)-(4,2), blue's
  CHECK(w.begin());
  CHECK(w.sees(0, 3, 1) && w.sees(0, 4, 2));    // the footprint itself
  CHECK(w.sees(0, 2, 1) && w.sees(0, 5, 2));    // one to the side
  CHECK(w.sees(0, 3, 0) && w.sees(0, 4, 3));    // one above / below
  CHECK(!w.sees(0, 1, 1) && !w.sees(0, 6, 2));  // two away
  CHECK(!w.sees(0, 2, 0));                       // diagonal corner: two steps away
  CHECK(!w.sees(1, 3, 1));                       // not visible to the other team
}

TEST(fog_sight_a_neutral_structure_sees_nothing) {
  FogWorld w(field(10, 3), {{"soldier", 0, 9, 2, false}, {"soldier", 1, 0, 2, false}});
  w.place("castle", 3, 0);
  CHECK(w.begin());
  CHECK(!w.sees(0, 3, 0) && !w.sees(1, 3, 0));
}

TEST(fog_sight_a_hiding_square_shows_its_unit_only_to_adjacent_units) {
  FogWorld w({"......", "...f..", "......"}, {{"soldier", 0, 1, 1, false}, {"soldier", 1, 3, 1, false}});
  w.forestHides();
  CHECK(w.begin());
  CHECK(w.sees(0, 3, 1));          // the terrain itself is in sight (distance 2) ...
  CHECK(!w.seesUnit(0, 1));        // ... but the unit on it is not
  CHECK(w.seesUnit(1, 0));         // red on the hideout still sees blue, who stands on plain two squares away
}

TEST(fog_sight_a_unit_on_plain_two_squares_away_is_seen_and_a_hidden_one_next_to_it_is_too) {
  FogWorld w({"......", "...f..", "......"}, {{"soldier", 0, 1, 1, false}, {"soldier", 1, 3, 1, false}});
  w.forestHides();
  CHECK(w.begin());
  CHECK(w.seesUnit(1, 0));         // blue on plain, red's vision 2
  CHECK(!w.seesUnit(0, 1));        // red on the hideout
  CHECK(w.act(Action::move(0, w.at(2, 1))));
  CHECK(w.seesUnit(0, 1));         // adjacent now
}

TEST(fog_sight_a_unit_always_sees_its_own_team_even_in_a_hideout) {
  FogWorld w({"......", "...f..", "......"}, {{"soldier", 0, 0, 0, false}, {"soldier", 0, 3, 1, false}, {"soldier", 1, 5, 2, false}});
  w.forestHides();
  CHECK(w.begin());
  CHECK(w.seesUnit(0, 1));
  CHECK(!w.seesUnit(1, 1));        // red's unit is 3 away from the hideout
}

TEST(fog_sight_a_structure_can_hide_its_unit_too) {
  FogWorld w({".....", ".....", "....."}, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 2, 1, false}});
  w.place("hut", 2, 1);
  CHECK(w.begin());
  CHECK(w.sees(0, 2, 1));
  CHECK(!w.seesUnit(0, 1));
  CHECK(w.act(Action::move(0, w.at(1, 1))));
  CHECK(w.seesUnit(0, 1));
}

TEST(fog_sight_a_unit_with_no_vision_still_notices_a_neighbour_in_a_hideout) {
  FogWorld w({"...", ".f.", "..."}, {{"soldier", 0, 0, 1, false}, {"soldier", 1, 1, 1, false}});
  w.forestHides();
  w.vision("soldier", 0);
  CHECK(w.begin());
  CHECK(w.sees(0, 1, 1));
  CHECK(w.seesUnit(0, 1));
}

TEST(fog_sight_allied_sides_of_a_team_share_their_sight) {
  FogWorld w(field(12, 3), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 10, 1, false}});
  w.addAlly("blue");
  w.addUnit("soldier", 2, 8, 1);   // green, in blue's team
  CHECK(w.begin());
  CHECK_EQ(w.battle.sides[2].team, 0);
  CHECK(w.sees(0, 10, 1));          // green's eyes serve blue
  CHECK(w.seesUnit(0, 1));          // red at (10,1) is two squares from green: blue sees it
  CHECK(w.seesUnit(0, 2));          // and always sees its ally, eight squares from its own unit
}

TEST(fog_sight_allies_are_not_shared_with_the_enemy_team) {
  FogWorld w(field(12, 3), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 10, 1, false}});
  w.addAlly("blue");
  w.addUnit("soldier", 2, 8, 1);
  CHECK(w.begin());
  CHECK(!w.seesUnit(1, 0));         // blue's own soldier is far from red
  CHECK(w.seesUnit(1, 2));          // green stands two squares from red's soldier
}

TEST(fog_sight_off_everything_is_visible_and_owners_are_true) {
  FogWorld w({"c...", "....", "...."}, {{"soldier", 0, 3, 2, false}, {"soldier", 1, 0, 2, false}});
  w.map.fog = false;
  w.own(0, 0, 1);
  CHECK(w.begin());
  CHECK(!w.battle.fogOn());
  CHECK(w.sees(0, 0, 0) && w.sees(1, 3, 2) && w.sees(0, 3, 0));
  CHECK(w.seesUnit(0, 1) && w.seesUnit(1, 0));
  CHECK_EQ(w.battle.seenOwner(0, 0), 1);       // the true owner, whoever asks
  CHECK(w.battle.structureVisibleTo(0, 0));
  CHECK(w.battle.visibleToSide(0, w.at(0, 0)) && w.battle.unitVisibleToSide(1, 0));
  CHECK_EQ(w.battle.seenOwnerForSide(0, 0), 1);
  for (const Event& e : w.events) CHECK(e.seen == kSeenByAll && e.seenSteps.empty());
}

TEST(fog_sight_dead_and_unknown_units_are_never_seen) {
  FogWorld w(field(6, 3), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 1, 1, false}});
  CHECK(w.begin());
  CHECK(!w.seesUnit(0, 7) && !w.seesUnit(0, -1));
  w.battle.units[1].hp = 1;
  CHECK(w.act(Action::attack(0, 1, 0)));
  CHECK(!w.battle.units[1].alive);
  CHECK(!w.seesUnit(0, 1));
  CHECK(!w.battle.visibleTo(0, kNoCell) && !w.battle.visibleTo(7, w.at(0, 0)));
}

TEST(fog_sight_follows_a_move) {
  FogWorld w(field(12, 3), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 7, 1, false}});
  CHECK(w.begin());
  CHECK(!w.seesUnit(0, 1));
  CHECK(w.act(Action::move(0, w.at(3, 1))));
  CHECK(w.sees(0, 5, 1));
  CHECK(!w.sees(0, 0, 0));
  CHECK(!w.seesUnit(0, 1));         // (7,1) is four away from (3,1)
  CHECK(w.act(Action::move(0, w.at(3, 1))) == false);
}

TEST(fog_sight_follows_a_death_and_takes_the_dead_units_eyes_away) {
  FogWorld w(field(12, 3), {{"soldier", 0, 0, 1, false}, {"soldier", 0, 5, 1, false}, {"soldier", 1, 6, 1, false},
                            {"soldier", 1, 11, 1, false}});
  CHECK(w.begin());
  CHECK(w.sees(0, 7, 1));
  CHECK(w.sees(1, 6, 1));
  w.battle.units[2].hp = 1;
  CHECK(w.act(Action::attack(1, 2, 0)));
  CHECK(!w.battle.units[2].alive);
  CHECK(w.sees(0, 5, 1));
  CHECK(w.sees(1, 11, 1));
  CHECK(!w.sees(1, 6, 1));          // red's eyes at (6,1) are gone
}

TEST(fog_sight_follows_a_capture_the_new_owner_sees_with_the_structure) {
  FogWorld w({".......", "...H...", "......."}, {{"soldier", 0, 3, 1, false}, {"soldier", 1, 6, 2, false}});
  w.vision("soldier", 0);
  CHECK(w.begin());
  CHECK(!w.sees(0, 4, 1));
  CHECK(w.act(Action::capture(0)));
  CHECK_EQ(w.structureAt(3, 1).owner, 0);
  CHECK(w.sees(0, 4, 1) && w.sees(0, 3, 0) && w.sees(0, 2, 1));   // one around the hq
  CHECK(!w.sees(0, 5, 1));
}

TEST(fog_sight_follows_a_build) {
  FogWorld w({".......", "..F....", "......."}, {{"soldier", 0, 6, 0, false}, {"soldier", 1, 6, 2, false}});
  w.own(2, 1, 0);
  w.vision("soldier", 3);
  w.map.sides[0].resources = {{0, 1000}};
  CHECK(w.begin());
  CHECK(w.sees(0, 2, 1));                         // the factory's own sight covers its square ...
  CHECK(!w.sees(0, 0, 0));                        // ... and one around it, no more
  CHECK(w.act(Action::build(w.at(2, 1), 0)));
  CHECK_EQ(w.unitIdAt(2, 1), 2);
  CHECK(w.sees(0, 0, 0));                         // the new soldier sees 3 squares
}

TEST(fog_sight_follows_units_the_story_adds_removes_moves_and_changes_side) {
  FogWorld w(field(12, 3), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 11, 1, false}, {"soldier", 1, 8, 0, false}},
             {"hero", "", "scout"});
  CHECK(w.play(R"([
    {"id":"join","when":{"turn":1,"side":"blue"},"do":[{"addUnit":{"type":"soldier","side":"blue","x":6,"y":1,"id":"pal"}}]},
    {"id":"swap","when":{"turn":2,"side":"blue"},"do":[{"changeSide":{"unit":"scout"},"side":"blue"}]},
    {"id":"away","when":{"turn":3,"side":"blue"},"do":[{"moveUnit":"pal","x":11,"y":2}]},
    {"id":"gone","when":{"turn":4,"side":"blue"},"do":[{"removeUnit":{"unit":"pal"}}]}])"));
  // Round 1: pal joined at (6,1) and sees as far as (8,1); red's scout stands at (8,0), three away.
  CHECK(w.sees(0, 8, 1));
  CHECK(!w.sees(0, 11, 1));
  CHECK(w.nextTurn());   // round 2: red's scout (unit 2) joins blue; its eyes at (8,0) reach (10,0)
  CHECK(w.sees(0, 10, 0));
  CHECK(w.seesUnit(0, 2));
  CHECK(w.nextTurn());   // round 3: pal is moved to (11,2): it now sees red's soldier at (11,1)
  CHECK(w.seesUnit(0, 1));
  CHECK(w.sees(0, 11, 0));
  CHECK(w.nextTurn());   // round 4: pal is removed; the red soldier is out of sight again (scout at (8,0): 4 away)
  CHECK(!w.seesUnit(0, 1));
  CHECK(!w.sees(0, 11, 2));
}

TEST(fog_sight_follows_a_structure_upgrade_for_the_unit_standing_on_it) {
  FogWorld w({"....c....", ".........", "........."}, {{"soldier", 0, 4, 0, false}, {"soldier", 1, 8, 2, false}});
  w.own(4, 0, 0);
  StructureDef big = w.db.structures[(size_t)w.db.findStructure("city")];
  big.id = big.name = "megacity";
  big.visionBonus = 4;
  w.db.structures.push_back(big);
  UpgradeDef up;
  up.to = (int)w.db.structures.size() - 1;
  w.db.structures[(size_t)w.db.findStructure("city")].upgrades.push_back(up);
  CHECK(w.begin());
  CHECK(!w.sees(0, 8, 2));
  CHECK(w.act(Action::upgrade(w.structure(4, 0), up.to)));
  CHECK(w.sees(0, 8, 2));   // 2 + 4 = 6 squares: (8,2) is 6 away
  CHECK(w.seesUnit(0, 1));
}

TEST(fog_sight_is_recomputed_at_the_start_and_after_hand_made_changes_with_refreshSight) {
  FogWorld w(field(12, 3), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 11, 1, false}});
  CHECK(w.begin());
  CHECK(!w.seesUnit(0, 1));
  w.battle.units[0].cell = w.at(9, 1);     // as a test or tool might: not through apply
  w.battle.occupant[(size_t)w.at(0, 1)] = -1;
  w.battle.occupant[(size_t)w.at(9, 1)] = 0;
  w.battle.refreshSight();
  CHECK(w.seesUnit(0, 1));
}

TEST(fog_sight_copies_of_a_battle_carry_their_own_sight) {
  FogWorld w(field(12, 3), {{"soldier", 0, 0, 1, false}, {"soldier", 1, 3, 1, false}});
  CHECK(w.begin());
  CHECK(!w.seesUnit(0, 1));
  Battle copy = w.battle;
  CHECK(w.act(Action::move(0, w.at(1, 1))));
  CHECK(w.seesUnit(0, 1));
  CHECK(!copy.unitVisibleTo(0, 1));          // the copy still has the old picture
  std::vector<Event> ev;
  CHECK(copy.apply(Action::move(0, w.at(1, 1)), ev));
  CHECK(copy.unitVisibleTo(0, 1));
}

TEST(fog_grid_within_lists_the_squares_by_distance_and_clips_at_the_edge) {
  Grid g(GridType::Square, 5, 4);
  std::vector<Cell> out;
  g.within(g.at(0, 0), 2, out);
  CHECK_EQ(out.size(), size_t(6));   // (0,0) (1,0) (2,0) (0,1) (1,1) (0,2)
  for (Cell c : out) CHECK(g.distance(g.at(0, 0), c) <= 2);
  g.within(g.at(2, 1), 0, out);
  CHECK_EQ(out.size(), size_t(1));
  g.within(g.at(2, 1), 1, out);
  CHECK_EQ(out.size(), size_t(5));
  g.within(kNoCell, 3, out);
  CHECK(out.empty());
}

// ---- a sight bonus for some unit types only (docs/ECONOMY.md "Fog of war") ----

namespace {

// A blue unit of `type` on the forest square (0,0) of a long field, a red soldier `far` squares to its right on the
// same row. Reports whether blue sees the red soldier.
bool seesAcross(const char* type, int far, const std::vector<std::string>& only, int amount = 2) {
  FogWorld w({"f.............", "..............", ".............."}, {{type, 0, 0, 0, false}, {"soldier", 1, far, 0, false}});
  TerrainDef& forest = w.db.terrains[(size_t)w.db.findTerrain("forest")];
  forest.visionBonus = amount;
  forest.visionUnits = only;
  if (!w.begin()) return false;
  return w.seesUnit(0, 1);
}

}  // namespace

TEST(fog_sight_a_limited_bonus_applies_to_the_listed_unit_type_id) {
  CHECK(seesAcross("soldier", 4, {"soldier"}));    // vision 2 + 2
  CHECK(!seesAcross("soldier", 5, {"soldier"}));
  CHECK(!seesAcross("archer", 3, {"soldier"}));    // an archer standing there gets nothing
  CHECK(seesAcross("archer", 2, {"soldier"}));     // its own vision of 2 still works
}

TEST(fog_sight_a_limited_bonus_applies_to_a_tag) {
  CHECK(seesAcross("tank", 4, {"armor"}));
  CHECK(!seesAcross("soldier", 3, {"armor"}));
  CHECK(seesAcross("soldier", 4, {"armor", "soldier"}));   // any entry matches
}

TEST(fog_sight_the_plain_form_is_for_every_unit) {
  CHECK(seesAcross("archer", 4, {}));
  CHECK(seesAcross("tank", 4, {}));
  CHECK(!seesAcross("tank", 5, {}));
}

TEST(fog_sight_a_limited_bonus_of_zero_changes_nothing) {
  CHECK(!seesAcross("soldier", 3, {"soldier"}, 0));
}

TEST(fog_sight_a_structures_limited_bonus_is_added_only_for_its_unit_types) {
  for (const char* type : {"soldier", "archer"}) {
    FogWorld w({"..............", "..............", ".............."}, {{type, 0, 0, 0, false}, {"soldier", 1, 5, 0, false}});
    w.place("tower", 0, 0);   // bonus 3
    StructureDef& tower = w.db.structures[(size_t)w.db.findStructure("tower")];
    tower.visionUnits = {"archer"};
    CHECK(w.begin());
    CHECK_EQ(w.seesUnit(0, 1), std::string(type) == "archer");   // 2 + 3 = 5
  }
}

TEST(fog_sight_terrain_and_structure_limits_are_independent) {
  FogWorld w({"..............", "..............", ".............."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 7, 0, false}});
  w.place("tower", 0, 0);   // 3, for archers only
  w.db.structures[(size_t)w.db.findStructure("tower")].visionUnits = {"archer"};
  TerrainDef& plain = w.db.terrains[(size_t)w.db.findTerrain("plain")];
  plain.visionBonus = 2;   // for soldiers only
  plain.visionUnits = {"soldier"};
  CHECK(w.begin());
  CHECK(!w.seesUnit(0, 1));   // 2 + 2 = 4: the tower adds nothing for a soldier
  CHECK(w.sees(0, 4, 0));
  CHECK(!w.sees(0, 5, 0));
}
