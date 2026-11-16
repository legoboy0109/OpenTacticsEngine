// Step R1 of docs/SRPG.md: carried and paired units in battle saves. "carrying" is written by the unit that stays on
// the board only; the carried unit is saved as it is; every check of the contract.
#include "json_edit.h"
#include "rescue_fixture.h"
#include "save_helpers.h"

using namespace ote;
using namespace rescuetest;

namespace {

// A second, identical battle that is initialised but not started: what restoreBattleState needs.
struct Twin {
  RescueWorld w;
  explicit Twin(RescueMode mode) : w(line(mode)) {}
  bool ready() {
    std::string err;
    return w.formulas.init(w.db, err) && w.battle.init(w.db, w.map, w.formulas, 1, err);
  }
};

bool refusedAs(const Json& saved, RescueMode mode, const char* where, const char* why) {
  Twin t(mode);
  if (!t.ready()) return false;
  std::string err;
  if (restoreBattleState(t.w.battle, saved, err)) {
    std::printf("  restored although it should be refused: %s\n", where);
    return false;
  }
  if (err.find(where) == std::string::npos || err.find(why) == std::string::npos) {
    std::printf("  got: %s\n  wanted: %s ... %s\n", err.c_str(), where, why);
    return false;
  }
  return true;
}

}  // namespace

TEST(rescue_save_a_carry_round_trips_and_plays_on_the_same) {
  RescueWorld live = line();
  live.carrierStat(Atk, 50, 0);
  CHECK(live.begin());
  CHECK(live.act(Action::rescue(0, 1)));
  Json saved;
  saveBattleState(live.battle, saved);
  // Only the unit that stays writes the key; the carried unit is saved as it is.
  CHECK(saved["units"][0].has("carrying") && saved["units"][0]["carrying"].asInt() == 1);
  CHECK(!saved["units"][1].has("carrying") && !saved["units"][2].has("carrying"));
  CHECK(!saved["units"][1]["alive"].asBool() && saved["units"][1]["cell"].asInt() == -1 && saved["units"][1]["hp"].asInt() == 8);

  Twin t(RescueMode::Carry);
  t.w.carrierStat(Atk, 50, 0);
  CHECK(t.ready());
  std::string err;
  CHECK(restoreBattleState(t.w.battle, saved, err));
  CHECK_STR(err, "");
  CHECK(t.w.carriedBy(1, 0));
  CHECK_EQ(t.w.stat(0, Atk), 2);  // the penalty is back
  CHECK_STR(savetest::stateText(t.w.battle), savetest::stateText(live.battle));
  // Both play on identically: the carrier puts the unit down next turn.
  live.pass();
  t.w.pass();
  CHECK(live.act(Action::drop(0, live.at(2, 1))));
  CHECK(t.w.act(Action::drop(0, t.w.at(2, 1))));
  CHECK_STR(savetest::stateText(t.w.battle), savetest::stateText(live.battle));
  CHECK(t.w.stands(1, 2, 1));
}

TEST(rescue_save_a_pair_round_trips_with_its_bonus) {
  RescueWorld live = line(RescueMode::PairUp);
  live.pairBonus({0, 2, 2, 0});
  CHECK(live.begin());
  CHECK(live.act(Action::pairUp(1, 0)));
  Json saved;
  saveBattleState(live.battle, saved);
  Twin t(RescueMode::PairUp);
  t.w.pairBonus({0, 2, 2, 0});
  CHECK(t.ready());
  std::string err;
  CHECK(restoreBattleState(t.w.battle, saved, err));
  CHECK_STR(err, "");
  CHECK(t.w.carriedBy(1, 0));
  CHECK_EQ(t.w.stat(0, Atk), 7);
  CHECK_STR(savetest::stateText(t.w.battle), savetest::stateText(live.battle));
  CHECK(t.w.battle.validate(Action::move(0, t.w.at(1, 0))));  // the unit in front can still act
  CHECK(t.w.refused(Action::wait(1), "that unit cannot act now"));
}

TEST(rescue_save_a_game_with_carrying_off_writes_no_key) {
  RescueWorld w = line();
  CHECK(w.begin());
  Json saved;
  saveBattleState(w.battle, saved);
  for (size_t i = 0; i < saved["units"].size(); ++i) CHECK(!saved["units"][i].has("carrying"));
}

TEST(rescue_save_refuses_a_damaged_carry_link) {
  RescueWorld live = line();
  CHECK(live.begin());
  CHECK(live.act(Action::rescue(0, 1)));
  Json good;
  saveBattleState(live.battle, good);
  using jsonedit::num;
  const RescueMode carry = RescueMode::Carry;
  const std::vector<std::string> carrying = {"units", "#0", "carrying"};
  // The id names a unit that does not exist, the carrier itself, a unit on the board, a unit of another side.
  CHECK(refusedAs(jsonedit::withValue(good, carrying, num(9)), carry, "units[0].carrying", "unit 9 does not exist"));
  CHECK(refusedAs(jsonedit::withValue(good, carrying, num(0)), carry, "units[0].carrying", "cannot carry itself"));
  CHECK(refusedAs(jsonedit::withValue(good, carrying, num(2)), carry, "units[0].carrying", "unit 2 is on the board"));
  CHECK(refusedAs(jsonedit::withValue(good, carrying, num(-1)), carry, "units[0].carrying", "outside"));
  CHECK(refusedAs(jsonedit::withValue(good, {"units", "#1", "side"}, num(1)), carry, "units[0].carrying", "is on another side"));
  // The carried unit has no hit points: a defeated unit, not a carried one.
  CHECK(refusedAs(jsonedit::withValue(good, {"units", "#1", "hp"}, num(0)), carry, "units[0].carrying", "is defeated"));
  // Two carriers name the same unit.
  CHECK(refusedAs(jsonedit::withValue(good, {"units", "#2", "carrying"}, num(1)), carry, "units[2].carrying", "is carried by unit 0 too"));
  // A carrier that is itself off the board.
  Json offBoard = jsonedit::withValue(jsonedit::withValue(good, {"units", "#0", "alive"}, Json::makeBool(false)), {"units", "#0", "cell"}, num(-1));
  offBoard = jsonedit::withValue(offBoard, {"units", "#0", "hp"}, num(0));
  CHECK(refusedAs(offBoard, carry, "units[0].carrying", "cannot carry anybody"));
  // A unit that is not alive but has hit points, and nobody carries it.
  CHECK(refusedAs(jsonedit::withoutAt(good, carrying), carry, "units[1].hp", "nobody carries it"));
  // The key in a game without rescue.
  CHECK(refusedAs(good, RescueMode::Off, "units[0].carrying", "this game has no rescue or pair up"));
}

TEST(rescue_save_a_dead_unit_with_hit_points_is_refused_in_any_game) {
  savetest::Rig live;
  CHECK(live.loadPlain("twin_rivers"));
  live.start();
  Json good;
  saveBattleState(live.battle, good);
  Json bad = jsonedit::withValue(good, {"units", "#0", "alive"}, Json::makeBool(false));
  bad = jsonedit::withValue(bad, {"units", "#0", "cell"}, jsonedit::num(-1));
  savetest::Rig fresh;
  CHECK(fresh.loadPlain("twin_rivers"));
  std::string err;
  CHECK(!restoreBattleState(fresh.battle, bad, err));
  CHECK(err.find("units[0].hp") != std::string::npos && err.find("nobody carries it") != std::string::npos);
}
