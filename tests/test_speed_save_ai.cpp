// Docs/TURN_ORDER.md "Saves" and docs/AI.md "Speed turn order": a save in the middle of a charge round-trips and is checked, the computer opponent plays only
// the unit that has the turn, uses a charged weapon with a documented care, and a computer-against-computer game ends the same way twice (square
// and hexagon grids). Fixture tests/fixtures/speed-order.
#include "core/ai/ai.h"
#include "save_helpers.h"
#include "speed_fixture.h"

using namespace ote;
using speedtest::Scene;
using speedtest::plain;
using speedtest::U;

namespace {

const std::vector<U> kClump{{"mage", "blue", 3, 2, "mg"}, {"dummy", "red", 7, 2, "foe"}, {"dummy", "red", 8, 2, "foe2"}};
const std::vector<U> kArmies{{"knight", "blue", 1, 2, "kn"}, {"scout", "blue", 1, 3, "sc"}, {"mage", "blue", 0, 1, "mg"},
                             {"dummy", "red", 9, 2, "foe"}, {"dummy", "red", 10, 3, "foe2"}, {"brute", "red", 10, 1, "br"}};

// One saved battle state as JSON.
Json saved(Scene& s) {
  Json j;
  saveBattleState(s.b(), j);
  return j;
}

// The computer plays every unit's turn until the battle ends (or `turns` turns have passed); false when one turn would not end.
bool playAi(Scene& s, int turns, std::vector<Event>& all) {
  for (int t = 0; t < turns && !s.b().over; ++t) {
    long steps = 0;
    std::vector<Event> ev;
    while (aiTakeStep(s.b(), ev))
      if (++steps > 300) return false;
    all.insert(all.end(), ev.begin(), ev.end());
  }
  return true;
}

std::string dump(const std::vector<Event>& events) {
  std::string out;
  for (const Event& e : events)
    out += std::to_string((int)e.type) + ":" + std::to_string(e.a) + "," + std::to_string(e.b) + "," + std::to_string(e.c) + "," + std::to_string(e.d) + "," +
           std::to_string(e.flags) + ";";
  return out;
}

}  // namespace

// ---- saves ----

TEST(a_save_in_the_middle_of_a_charge_restores_the_clock_and_the_pending_action) {
  Scene s("sv_charge", plain(12, 5), kClump);
  s.shoot("mg", "fire", 7, 2);
  s.go("foe", 7, 4);
  const Json j = saved(s);
  const std::string text = j.dump(0);
  CHECK(text.find("\"clock\"") != std::string::npos);
  CHECK(text.find("\"pending\":[{") != std::string::npos);
  CHECK(text.find("\"ct\":") != std::string::npos);
  Scene t("sv_charge2", plain(12, 5), kClump);
  std::string err;
  CHECK(restoreBattleState(t.b(), j, err));
  CHECK_STR(savetest::stateText(t.b()), savetest::stateText(s.b()));
  CHECK_EQ(t.b().pending.size(), (size_t)1);
  CHECK_EQ(t.b().pending[0].counter, 0);
  CHECK_EQ(t.b().activeUnit, s.b().activeUnit);
  CHECK_EQ(t.unit("mg").ct, s.unit("mg").ct);
  CHECK_STR(t.preview(), s.preview());
  // Both go on the same way: the fire goes off with the foe out of the blast and the foe2 in it.
  for (Scene* x : {&s, &t}) {
    x->wait("foe");
    x->pass();
  }
  CHECK_STR(savetest::stateText(t.b()), savetest::stateText(s.b()));
  CHECK_EQ(t.unit("foe2").hp, 31);
  CHECK_EQ(t.unit("foe").hp, 40);
  CHECK(t.b().pending.empty());
}

TEST(a_save_of_a_speed_game_keeps_no_trace_in_a_side_by_side_game) {
  areatest::Scene s("sv_sides", plain(8, 3), {{"lancer", "blue", 1, 1, "a"}, {"dummy", "red", 6, 1, "c"}});
  Json j;
  saveBattleState(s.b(), j);
  const std::string text = j.dump(0);
  CHECK(text.find("\"clock\"") == std::string::npos);
  CHECK(text.find("\"ct\"") == std::string::npos);
  CHECK(text.find("turnActed") == std::string::npos);
}

TEST(a_save_that_does_not_fit_the_clock_is_refused_in_the_usual_words) {
  Scene s("sv_bad", plain(12, 5), kClump);
  s.shoot("mg", "fire", 7, 2);
  const std::string text = saved(s).dump(0);
  auto refused = [&](const std::string& from, const std::string& to) {
    std::string t = text;
    const size_t at = t.find(from);
    CHECK(at != std::string::npos);
    if (at == std::string::npos) return std::string("(text missing: " + from + ")");
    t.replace(at, from.size(), to);
    Json bad;
    std::string perr;
    CHECK(Json::parse(t, bad, perr));
    Scene r("sv_bad_r", plain(12, 5), kClump);
    std::string err;
    return restoreBattleState(r.b(), bad, err) ? std::string("(accepted)") : err;
  };
  CHECK(refused("\"id\":\"fire\"", "\"id\":\"inferno\"").find("battle.clock.pending[0].id: \"inferno\" is not a weapon of this game") != std::string::npos);
  CHECK(refused("\"id\":\"fire\"", "\"id\":\"sword\"").find("\"sword\" does not charge") != std::string::npos);
  CHECK(refused("\"counter\":0", "\"counter\":100").find("battle.clock.pending[0].counter") != std::string::npos);
  CHECK(refused("\"kind\":\"weapon\"", "\"kind\":\"spell\"").find("battle.clock.pending[0].kind: must be") != std::string::npos);
  CHECK(refused("\"unit\":0,\"kind\"", "\"unit\":9,\"kind\"").find("battle.clock.pending[0].unit") != std::string::npos);
  CHECK(refused("\"activeUnit\":1", "\"activeUnit\":-1").find("battle.clock.activeUnit") != std::string::npos);
  CHECK(refused("\"activeUnit\":1", "\"activeUnit\":0").find("is not on the active side") != std::string::npos);
  CHECK(refused("\"tick\":", "\"tock\":").find("battle.clock.tick") != std::string::npos);
  CHECK(refused("\"clock\"", "\"clack\"").find("battle.clock: missing") != std::string::npos);
}

TEST(a_speed_key_in_a_game_without_the_speed_rule_is_refused) {
  areatest::Scene a("sv_bad_sides", plain(8, 3), {{"lancer", "blue", 1, 1, "a"}, {"dummy", "red", 6, 1, "c"}});
  Json j;
  saveBattleState(a.b(), j);
  std::string text = j.dump(0);
  const size_t at = text.find("\"leader\":");
  CHECK(at != std::string::npos);
  text.insert(at, "\"ct\":5,");
  Json bad;
  std::string perr;
  CHECK(Json::parse(text, bad, perr));
  areatest::Scene r("sv_bad_sides2", plain(8, 3), {{"lancer", "blue", 1, 1, "a"}, {"dummy", "red", 6, 1, "c"}});
  std::string err;
  CHECK(!restoreBattleState(r.b(), bad, err));
  CHECK(err.find("units[0].ct: this game has no speed turn order") != std::string::npos);
}

TEST(the_clock_of_a_saved_game_goes_on_exactly_as_the_live_one) {
  Scene s("sv_clock", plain(12, 5), kArmies, true);
  std::vector<Event> all;
  CHECK(playAi(s, 7, all));
  const Json j = saved(s);
  Scene t("sv_clock2", plain(12, 5), kArmies, true);
  std::string err;
  CHECK(restoreBattleState(t.b(), j, err));
  CHECK_STR(savetest::stateText(t.b()), savetest::stateText(s.b()));
  std::vector<Event> a, b;
  CHECK(playAi(s, 25, a));
  CHECK(playAi(t, 25, b));
  CHECK_STR(dump(a), dump(b));
  CHECK_STR(savetest::stateText(t.b()), savetest::stateText(s.b()));
}

// ---- the computer opponent ----

TEST(the_computer_plays_only_the_unit_that_has_the_turn) {
  Scene s("ai_one_unit", plain(12, 5), kArmies, true);
  int turns = 0;
  for (; turns < 60 && !s.b().over; ++turns) {
    const int uid = s.b().activeUnit;
    CHECK(uid >= 0);
    std::vector<Event> ev;
    long steps = 0;
    while (aiTakeStep(s.b(), ev))
      if (++steps > 300) break;
    CHECK(steps <= 300);
    std::vector<int> resolved;  // casters whose charged action went off during this call (they strike without a turn)
    for (const Event& e : ev) {
      if (e.type == EventType::ChargeResolved) resolved.push_back(e.a);
      if (e.type == EventType::UnitMoved) CHECK_EQ(e.a, uid);
      if (e.type == EventType::Attack && !(e.flags & kCounter) && std::find(resolved.begin(), resolved.end(), e.a) == resolved.end()) CHECK_EQ(e.a, uid);
    }
  }
  CHECK(turns > 10);
}

TEST(an_ai_battle_with_a_clock_ends_the_same_way_twice_on_a_square_grid) {
  auto run = [](const char* name) {
    Scene s(name, plain(12, 5), kArmies, true);
    std::vector<Event> all;
    CHECK(playAi(s, 400, all));
    CHECK(s.b().over);
    return dump(all) + "|" + savetest::stateText(s.b());
  };
  CHECK_STR(run("ai_det_a"), run("ai_det_b"));
}

TEST(an_ai_battle_with_a_clock_ends_the_same_way_twice_on_a_hexagon_grid) {
  auto run = [](const char* name) {
    Scene s(name, plain(12, 5), kArmies, true, nullptr, "hex");
    std::vector<Event> all;
    CHECK(playAi(s, 400, all));
    CHECK(s.b().over);
    return dump(all) + "|" + savetest::stateText(s.b());
  };
  CHECK_STR(run("ai_det_hex_a"), run("ai_det_hex_b"));
}

TEST(the_computer_charges_an_area_weapon_at_enemies_that_cannot_move_away_before_it_goes_off) {
  // Two enemies that never act (speed 0) stand next to each other: the fire (4 ticks) cannot miss them.
  const std::vector<U> units{{"mage", "blue", 3, 2, "mg"}, {"stiff", "red", 7, 2, "s1"}, {"stiff", "red", 8, 2, "s2"}};
  Scene s("ai_charge", plain(12, 5), units, true);
  std::vector<Event> all;
  CHECK_STR(s.active(), "mg");
  std::vector<Event> ev;
  while (aiTakeStep(s.b(), ev)) {}
  bool charging = false;
  for (const Event& e : ev)
    if (e.type == EventType::ActionCharging && e.a == s.id("mg")) charging = e.b == 1 && e.c == s.weapon("fire");
  CHECK(charging);
  bool wentOff = false;  // (the clock ran on inside the same step: 4 ticks later the fire went off, before the mage's next turn)
  for (const Event& e : ev) wentOff = wentOff || e.type == EventType::ChargeResolved;
  CHECK(wentOff);
  CHECK(s.unit("s1").hp < 40);
  CHECK(s.unit("s2").hp < 40);
}

TEST(the_computer_does_not_aim_a_square_at_enemies_that_will_have_moved_before_it_goes_off) {
  // The same geometry, but the enemies are fast (speed 20): each has a turn before the fire (4 ticks) lands, so the square is not worth aiming at.
  // (The mage is faster still, so that it has the first turn.)
  const std::vector<U> units{{"mage", "blue", 3, 2, "mg"}, {"scout", "red", 7, 2, "s1"}, {"scout", "red", 8, 2, "s2"}};
  Scene s("ai_charge_fast", plain(12, 5), units, true, [](skilltest::Copy& c) {
    c.swap("data/units.json", "\"mov\": 3, \"spd\": 10 }, \"weapons\": [\"fire\", \"bolt\", \"sword\"]", "\"mov\": 3, \"spd\": 30 }, \"weapons\": [\"fire\", \"bolt\", \"sword\"]");
  });
  CHECK_STR(s.active(), "mg");
  std::vector<Event> ev;
  while (aiTakeStep(s.b(), ev)) {}
  bool aimedAtSquare = false, acted = false;
  for (const Event& e : ev) {
    if (e.type == EventType::ActionCharging) acted = true;
    if (e.type == EventType::ActionCharging && e.b == 1 && e.c == s.weapon("fire")) aimedAtSquare = true;
  }
  CHECK(!aimedAtSquare);
  CHECK(acted);  // it still charged something it can trust: the bolt follows its target
}
