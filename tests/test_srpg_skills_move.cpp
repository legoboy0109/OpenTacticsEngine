// Step K1 of docs/SRPG.md "Skills", movement and turn state: maxMoveCost and pass on square and hexagon maps (with
// and without fog), "move again" as a state machine (budget, refusals, flags, save round trip), xpPercent stacking.
#include "core/battle/save_srpg.h"
#include "json_edit.h"
#include "srpg_skills_fixture.h"

using namespace ote;
using skilltest::Copy;
using skilltest::saysIn;
using skilltest::World;

namespace {

// The fixture copy where the hero type has exactly these skills (a JSON list body).
void heroSkills(Copy& c, const std::string& list) {
  std::string text = c.read("data/units.json");
  const size_t open = text.find("\"skills\": [") + 11, close = text.find(']', open);  // the hero is the first entry
  c.put("data/units.json", text.substr(0, open) + list + text.substr(close));
}

// A one-row map (a corridor, on squares or hexagons): `runner` (blue) at x 0 and a red grunt at `foeX`.
void corridor(Copy& c, bool hex, int foeX, bool fog = false) {
  if (hex) c.swap("game.json", "\"formatVersion\": 1,", "\"formatVersion\": 1, \"grid\": { \"type\": \"hex\", \"layout\": \"pointy\" },");
  c.put("maps/field.json", std::string("{ \"name\": \"Corridor\", \"grid\": { \"type\": \"") + (hex ? "hex" : "square") +
                               "\", \"width\": 8, \"height\": 1 }, \"legend\": { \".\": \"plain\" }, \"tiles\": [\"........\"]," +
                               (fog ? "\"fog\": true," : "") +
                               R"( "sides": [ { "id": "blue", "controller": "human" }, { "id": "red", "controller": "ai" } ],
    "units": [ { "id": "runner", "type": "hero", "side": "blue", "x": 0, "y": 0 },
               { "type": "grunt", "side": "red", "x": )" + std::to_string(foeX) + ", \"y\": 0 } ] }");
}

Reach reachOf(World& w, const char* id) {
  Reach r;
  w.b().reachable(w.id(id), r);
  return r;
}

}  // namespace

TEST(skills_move_a_cap_on_the_cost_of_a_square_and_more_movement) {
  World base("woods");
  Reach r = reachOf(base, "scout");  // hero move 4 from (0,1); forest costs 3
  CHECK_EQ(r.cost[(size_t)base.at(1, 1)], 3);
  CHECK_EQ(r.cost[(size_t)base.at(2, 1)], -1);

  Copy a("skills_move_cap");
  heroSkills(a, "\"sure_footing\"");
  World capped("woods", a.dir);
  r = reachOf(capped, "scout");
  CHECK_EQ(r.cost[(size_t)capped.at(1, 1)], 1);  // forest costs 1
  CHECK_EQ(r.cost[(size_t)capped.at(4, 1)], 4);
  CHECK_EQ(r.cost[(size_t)capped.at(5, 1)], -1);
  CHECK_EQ(capped.b().enterCost(capped.at(1, 1), 0), 3);  // the terrain itself is unchanged
  CHECK(r.cost[(size_t)capped.at(6, 2)] == -1);            // a wall stays a wall

  // The lowest cap wins.
  Copy b("skills_move_cap_lowest");
  b.skills(R"({ "id": "slog", "blocks": [ { "when": "always", "do": [ { "maxMoveCost": 2 } ] } ] })");
  heroSkills(b, "\"slog\"");
  World slog("woods", b.dir);
  CHECK_EQ(reachOf(slog, "scout").cost[(size_t)slog.at(1, 1)], 2);
  heroSkills(b, "\"slog\", \"sure_footing\"");
  World both("woods", b.dir);
  CHECK_EQ(reachOf(both, "scout").cost[(size_t)both.at(1, 1)], 1);

  // A move stat bonus reaches further.
  Copy c("skills_move_stat");
  heroSkills(c, "\"light_step\"");
  World fast("woods", c.dir);
  CHECK_EQ(fast.b().movePoints(fast.unit("scout")), 5);
  CHECK_EQ(reachOf(fast, "scout").cost[(size_t)fast.at(2, 0)], 5);
  CHECK_EQ(reachOf(base, "scout").cost[(size_t)base.at(2, 0)], -1);
}

TEST(skills_move_pass_through_enemies_but_never_stop_on_them) {
  for (bool hex : {false, true}) {
    Copy plain(hex ? "skills_pass_hex_plain" : "skills_pass_plain");
    corridor(plain, hex, 2);
    World w0("field", plain.dir);
    Reach r = reachOf(w0, "runner");
    CHECK_EQ(r.cost[(size_t)w0.at(1, 0)], 1);
    CHECK_EQ(r.cost[(size_t)w0.at(3, 0)], -1);  // an enemy in a corridor is a wall

    Copy c(hex ? "skills_pass_hex" : "skills_pass");
    corridor(c, hex, 2);
    heroSkills(c, "\"slip_through\"");
    World w("field", c.dir);
    r = reachOf(w, "runner");
    CHECK_EQ(r.cost[(size_t)w.at(3, 0)], 3);
    CHECK_EQ(r.cost[(size_t)w.at(4, 0)], 4);
    CHECK(r.canStop(w.at(3, 0)) && r.canStop(w.at(4, 0)));
    CHECK(!r.canStop(w.at(2, 0)));  // the enemy's own square is never a stop
    CHECK(w.apply(Action::move(w.id("runner"), w.at(4, 0))));
    CHECK_EQ(w.unit("runner").cell, w.at(4, 0));
    CHECK_EQ(w.unit("runner").moveSpent, 4);
    CHECK_EQ(w.count(EventType::MoveBlocked), 0);
    CHECK(w.b().units[1].alive && w.b().units[1].cell == w.at(2, 0));
    CHECK(!w.b().validate(Action::move(w.id("runner"), w.at(2, 0))));
  }
}

TEST(skills_move_pass_with_fog_an_unseen_enemy_on_the_way_does_not_stop_the_unit) {
  // The grunt stands three squares away: outside the hero's sight of 2, in the middle of its path to (4, 0).
  Copy plain("skills_pass_fog_plain");
  corridor(plain, false, 3, true);
  World w0("field", plain.dir);
  CHECK(w0.apply(Action::move(w0.id("runner"), w0.at(4, 0))));
  CHECK_EQ(w0.count(EventType::MoveBlocked), 1);  // without the skill it runs into the enemy it did not see
  CHECK_EQ(w0.unit("runner").cell, w0.at(2, 0));

  Copy c("skills_pass_fog");
  corridor(c, false, 3, true);
  heroSkills(c, "\"slip_through\"");
  World w("field", c.dir);
  CHECK(w.apply(Action::move(w.id("runner"), w.at(4, 0))));
  CHECK_EQ(w.count(EventType::MoveBlocked), 0);
  CHECK_EQ(w.unit("runner").cell, w.at(4, 0));
}

TEST(skills_move_again_is_a_state_machine_with_a_budget) {
  Copy c("skills_move_again");
  heroSkills(c, "\"strike_and_move\"");
  World w("field", c.dir);
  const int eirik = w.id("eirik"), gorm = w.id("gorm"), lyra = w.id("lyra"), sword = w.db().findWeapon("sword");

  // Eirik (move 5) walks 4, attacks: the action is used up, one point of movement is left.
  CHECK(w.apply(Action::move(eirik, w.at(4, 0))));
  CHECK_EQ(w.unit("eirik").moveSpent, 4);
  CHECK(w.apply(Action::attack(eirik, gorm, sword)));
  CHECK(w.unit("eirik").acted && !w.unit("eirik").done);
  CHECK_STR(w.refusal(Action::attack(eirik, gorm, sword)), "that unit can only move or wait now");
  CHECK_STR(w.refusal(Action::capture(eirik)), "that unit can only move or wait now");
  CHECK(w.b().canAct(eirik));
  Reach r = reachOf(w, "eirik");  // the budget is movement minus what was spent
  CHECK_EQ(r.cost[(size_t)w.at(3, 0)], 1);
  CHECK_EQ(r.cost[(size_t)w.at(2, 0)], -1);
  CHECK_STR(w.refusal(Action::move(eirik, w.at(2, 0))), "the unit cannot reach that cell");

  // Mid-turn save: the flags are written, the restored battle is the same and the move is still on offer.
  Json saved;
  saveBattleState(w.b(), saved);
  CHECK(saved["units"][(size_t)eirik]["acted"].asBool());
  CHECK_EQ(saved["units"][(size_t)eirik]["moveSpent"].asInt(), 4);
  CHECK(!saved["units"][(size_t)gorm].has("moveSpent") && !saved["units"][(size_t)gorm].has("acted"));
  World again("field", c.dir, false);
  std::string err;
  CHECK(restoreBattleState(again.b(), saved, err));
  CHECK_STR(savetest::stateText(again.b()), savetest::stateText(w.b()));
  CHECK(again.unit("eirik").acted && again.unit("eirik").moveSpent == 4);
  CHECK(again.apply(Action::move(again.id("eirik"), again.at(3, 0))));

  // The second move is the last: it ends the order.
  CHECK(w.apply(Action::move(eirik, w.at(3, 0))));
  CHECK(w.unit("eirik").done && w.unit("eirik").moveSpent == 5);
  CHECK_STR(w.refusal(Action::move(eirik, w.at(2, 0))), "that unit has already acted");

  // Lyra (move 4) uses all of it before she attacks: nothing is left, so she is done at once.
  CHECK(w.apply(Action::move(lyra, w.at(4, 2))));
  CHECK(w.apply(Action::attack(lyra, 3, sword)));
  CHECK(w.unit("lyra").done && !w.unit("lyra").acted);

  // Only the character has the skill here: Wait after the action ends the order, and a unit without the skill is
  // done after its attack even with movement to spare. The next turn starts everything over.
  Copy d("skills_move_again_character");
  heroSkills(d, "");
  d.swap("data/characters.json", "\"skills\": [\"bold\"]", "\"skills\": [\"strike_and_move\"]");
  World v("field", d.dir);
  CHECK(v.apply(Action::move(v.id("eirik"), v.at(4, 0))));
  CHECK(v.apply(Action::attack(v.id("eirik"), v.id("gorm"), sword)));
  CHECK(v.unit("eirik").acted);
  CHECK(v.apply(Action::wait(v.id("eirik"))));
  CHECK(v.unit("eirik").done);
  CHECK(v.apply(Action::move(v.id("lyra"), v.at(4, 2))));  // 4 of her 4 points: attack from here
  CHECK(v.apply(Action::attack(v.id("lyra"), 3, sword)));
  CHECK(v.unit("lyra").done && !v.unit("lyra").acted);
  CHECK(v.apply(Action::endTurn()));  // red's turn
  CHECK(v.b().activeSide == 1);
  CHECK(v.apply(Action::endTurn()));  // blue again
  CHECK(!v.unit("eirik").acted && !v.unit("eirik").done && v.unit("eirik").moveSpent == 0 && !v.unit("eirik").moved);
}

TEST(skills_move_again_flags_are_refused_in_a_game_without_such_skills) {
  Copy c("skills_flags_refused");
  c.swap("data/skills.json", "{ \"moveAgain\": true }", "{ \"pass\": \"enemies\" }");
  World live("field", c.dir);
  Json good;
  saveBattleState(live.b(), good);
  for (const char* key : {"moveSpent", "acted", "refreshed"}) {
    World fresh("field", c.dir, false);
    std::string err;
    const Json bad = jsonedit::withValue(good, {"units", "#0", key}, std::string(key) == "moveSpent" ? Json::makeNumber(2) : Json::makeBool(true));
    CHECK(!restoreBattleState(fresh.b(), bad, err));
    CHECK(saysIn(err, std::string("units[0].") + key + ": this game has no \"moveAgain\" or \"actAgain\" skills"));
  }
  // Nothing of it is written either.
  live.unit("eirik").moveSpent = 3;
  Json again;
  saveBattleState(live.b(), again);
  CHECK(!again["units"][0].has("moveSpent"));
}

TEST(skills_xp_percent_multiplies_in_skill_list_order) {
  // xpHit 3 shows the rounding: whole-number division after each percent.
  auto xpOfEirikHit = [](Copy& c) {
    c.swap("game.json", "\"xpHit\": \"30\"", "\"xpHit\": \"3\"");
    World w("field", c.dir);
    CHECK(w.apply(Action::move(w.id("eirik"), w.at(4, 0))));
    CHECK(w.apply(Action::attack(w.id("eirik"), w.id("gorm"), w.db().findWeapon("sword"))));
    for (const Event& e : w.events)
      if (e.type == EventType::XpGained && e.a == w.id("eirik")) return e.b;
    return -1;
  };
  Copy plain("skills_xp_plain");  // fast_learner only (learned at level 2): 3 * 150 / 100 = 4
  CHECK_EQ(xpOfEirikHit(plain), 4);
  Copy slowFirst("skills_xp_slow_first");  // the type's slow_learner, then the learned fast_learner: 1, then 1
  heroSkills(slowFirst, "\"slow_learner\"");
  CHECK_EQ(xpOfEirikHit(slowFirst), 1);
  Copy slowLast("skills_xp_slow_last");  // learned fast_learner, then the character's slow_learner: 4, then 2
  heroSkills(slowLast, "");
  slowLast.swap("data/characters.json", "\"skills\": [\"bold\"]", "\"skills\": [\"slow_learner\"]");
  CHECK_EQ(xpOfEirikHit(slowLast), 2);
  Copy none("skills_xp_none");  // no skill: untouched (and the unit without it keeps 3)
  heroSkills(none, "");
  none.swap("data/units.json", "{ \"level\": 3, \"skill\": \"light_step\" }, { \"level\": 2, \"skill\": \"fast_learner\" }", "{ \"level\": 3, \"skill\": \"light_step\" }");
  CHECK_EQ(xpOfEirikHit(none), 3);
}

TEST(skills_save_learned_skills_round_trip_and_damaged_saves_are_refused) {
  World w;
  Json saved;
  saveBattleState(w.b(), saved);
  const Json& e = saved["units"][(size_t)w.id("eirik")];
  CHECK_EQ(e["skills"].size(), (size_t)3);
  CHECK_EQ(e["skills"][(size_t)0].asInt(), w.skill("fast_learner"));
  CHECK(!saved["units"][(size_t)w.id("gorm")].has("skills"));  // nothing learned, no key
  World fresh("field", "", false);
  std::string err;
  CHECK(restoreBattleState(fresh.b(), saved, err));
  CHECK_STR(savetest::stateText(fresh.b()), savetest::stateText(w.b()));
  CHECK_STR(fresh.learnedIds(fresh.unit("eirik")), "fast_learner light_step gifted");

  const std::vector<std::string> at = {"units", "#0", "skills"};
  struct Bad { Json value; const char* why; };
  Json twice = Json::makeArray(), wild = Json::makeArray();
  twice.push(Json::makeNumber(1));
  twice.push(Json::makeNumber(1));
  wild.push(Json::makeNumber(99));
  const Bad bad[] = {{twice, "units[0].skills[1]: skill 1 is listed twice"},
                     {wild, "units[0].skills[0]: not a skill of this game"},
                     {Json::makeNumber(1), "units[0].skills: missing or not a list"}};
  for (const Bad& b : bad) {
    World f("field", "", false);
    CHECK(!restoreBattleState(f.b(), jsonedit::withValue(saved, at, b.value), err));
    CHECK(saysIn(err, b.why));
  }
  // The turn flags are checked as well: a moveSpent of 0 is written as nothing, so 0 is refused.
  World f("field", "", false);
  CHECK(!restoreBattleState(f.b(), jsonedit::withValue(saved, {"units", "#0", "moveSpent"}, Json::makeNumber(0)), err));
  CHECK(saysIn(err, "units[0].moveSpent"));
  CHECK(!restoreBattleState(f.b(), jsonedit::withValue(saved, {"units", "#0", "acted"}, Json::makeNumber(1)), err));
  CHECK(saysIn(err, "units[0].acted: missing or not true/false"));
}

TEST(skills_save_a_roster_entry_keeps_the_learned_skills) {
  World w;
  RosterUnit r;
  r.skills = {w.skill("tough"), w.skill("bold")};
  Json j = Json::makeObject();
  savejson::writeRosterSkills(w.db(), r, j);
  CHECK_EQ(j["skills"].size(), (size_t)2);
  RosterUnit back;
  std::string err;
  savejson::Reader rd(err);
  CHECK(savejson::readRosterSkills(rd, w.db(), j, "playthrough.roster[0]", back));
  CHECK(back.skills == r.skills);
  Json empty = Json::makeObject();
  savejson::writeRosterSkills(w.db(), RosterUnit(), empty);
  CHECK(!empty.has("skills"));  // nothing learned, no key
  Json wild = Json::makeObject();
  Json list = Json::makeArray();
  list.push(Json::makeNumber(40));
  wild.set("skills", list);
  CHECK(!savejson::readRosterSkills(rd, w.db(), wild, "playthrough.roster[0]", back));
  CHECK(saysIn(err, "playthrough.roster[0].skills[0]: not a skill of this game"));
}

TEST(skills_the_data_hash_follows_what_a_skill_does_not_what_it_is_called) {
  auto hashOf = [](Copy& c) {
    Database db;
    std::string err, out;
    return loadGame(c.dir, db, err) && computeDataHash(db, "field", out, err) ? out : err;
  };
  Copy a("skills_hash_a"), b("skills_hash_b"), c("skills_hash_c");
  b.swap("data/skills.json", "\"name\": \"Tough\"", "\"name\": \"Sturdy\"");
  b.swap("data/skills.json", "\"description\": \"A chance of double damage.\"", "\"description\": \"Hits hard.\"");
  c.swap("data/skills.json", "{ \"stats\": { \"hp\": 5 } }", "{ \"stats\": { \"hp\": 6 } }");
  CHECK_STR(hashOf(a), hashOf(b));
  CHECK(hashOf(a) != hashOf(c));
  CHECK(hashOf(a).size() == 16);
}
