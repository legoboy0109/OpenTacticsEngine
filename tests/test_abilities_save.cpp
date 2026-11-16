// Step A1 of docs/SRPG.md "Menu abilities and summoning": Saves, events. The abilities a unit used and the summons
// round-trip through a battle save, only when not empty; damaged saves are refused with a path and a reason.
#include "abilities_files_fixture.h"
#include "json_edit.h"

using namespace ote;
using jsonedit::num;

namespace {

struct Played {
  savetest::Rig rig;
  std::vector<Event> events;
  int ability(const char* id) { return rig.db.findAbility(id); }
  bool use(int unit, const char* id, int target, Cell cell = kNoCell) {
    return rig.battle.apply(Action::useAbility(unit, ability(id), target, cell), events);
  }
  bool endTurn() { return rig.battle.apply(Action::endTurn(), events); }
  Cell at(int x, int y) { return rig.battle.grid.at(x, y); }
  // The fixture's battle after blue's first turn: eirik (0) pulled gorm with the free "yank" and called a wisp (4).
  bool firstTurn() {
    if (!rig.load(abfiles::source(), "field")) return false;
    rig.battle.start(events);
    return use(0, "yank", 2) && use(0, "call_wisp", -1, at(0, 0));
  }
  Json saved() {
    Json j;
    saveBattleState(rig.battle, j);
    return j;
  }
};

Json goodSave(Played& p) {
  CHECK(p.firstTurn());
  return p.saved();
}

// The state restored into a fresh battle of the same game equals the one saved.
void roundTrips(Played& p) {
  const Json j = p.saved();
  savetest::Rig fresh;
  CHECK(fresh.load(abfiles::source(), "field"));
  std::string err;
  CHECK(restoreBattleState(fresh.battle, j, err));
  CHECK_STR(err, "");
  CHECK_STR(savetest::stateText(fresh.battle), savetest::stateText(p.rig.battle));
}

}  // namespace

TEST(abilities_save_round_trip_after_free_and_summon_abilities) {
  Played p;
  CHECK(p.firstTurn());
  const Unit& eirik = p.rig.battle.units[0];
  CHECK_EQ(eirik.abilityUses.size(), (size_t)2);
  CHECK(eirik.abilityUses[0].ability == p.ability("yank") && eirik.abilityUses[0].turn);
  roundTrips(p);
  const Json j = p.saved();
  CHECK(j["units"][0].has("abilities") && !j["units"][0].has("summon"));
  const Json& e0 = j["units"][0]["abilities"][0];
  CHECK(e0["ability"].asInt() == p.ability("yank") && e0["used"].asInt() == 1 && e0["wait"].asInt() == 0 && e0["turn"].asBool());
  CHECK(!j["units"][0]["abilities"][1].has("turn"));  // only when true
  CHECK(!j["units"][1].has("abilities") && !j["units"][2].has("abilities"));  // only when not empty
  const Json& s = j["units"][4]["summon"];
  CHECK(s["by"].asInt() == 0 && s["ability"].asInt() == p.ability("call_wisp") && s["left"].asInt() == 3);
}

TEST(abilities_save_round_trip_later_with_cooldowns_lifetimes_and_two_kinds_of_summon) {
  Played p;
  CHECK(p.firstTurn());
  CHECK(p.endTurn() && p.endTurn());  // blue's second turn: the wisp has used one turn, the yank is ready again
  CHECK_EQ(p.rig.battle.units[4].summonLeft, 2);
  CHECK(p.use(0, "call_hound", -1, p.at(1, 0)));
  roundTrips(p);
  const Json j = p.saved();
  CHECK(!j["units"][5]["summon"].has("left"));  // the hound has no lifetime
  CHECK(j["units"][4]["summon"]["left"].asInt() == 2);
  const Unit& eirik = p.rig.battle.units[0];
  int wait = -1;
  for (const AbilityUse& u : eirik.abilityUses)
    if (u.ability == p.ability("call_hound")) wait = u.wait;
  CHECK_EQ(wait, 2);
  // A restored battle goes on the same way: the wisp leaves after its lifetime (it stays when its summoner falls).
  savetest::Rig fresh;
  CHECK(fresh.load(abfiles::source(), "field"));
  std::string err;
  CHECK(restoreBattleState(fresh.battle, j, err));
  std::vector<Event> a, b;
  for (int i = 0; i < 6; ++i) {
    CHECK(p.rig.battle.apply(Action::endTurn(), a));
    CHECK(fresh.battle.apply(Action::endTurn(), b));
  }
  CHECK(savetest::sameEvents(a, b));
  CHECK_STR(savetest::stateText(fresh.battle), savetest::stateText(p.rig.battle));
  CHECK(!p.rig.battle.units[4].alive);  // lifetime 3: gone after its three turns
}

TEST(abilities_save_a_battle_without_abilities_writes_none) {
  savetest::Rig rig;
  CHECK(rig.load(abfiles::source(), "field"));
  rig.start();
  Json j;
  saveBattleState(rig.battle, j);
  for (size_t i = 0; i < j["units"].size(); ++i) CHECK(!j["units"][i].has("abilities") && !j["units"][i].has("summon"));
}

TEST(abilities_save_damaged_saves_are_refused_with_a_path_and_a_reason) {
  Played p;
  const Json good = goodSave(p);
  struct Case {
    std::vector<std::string> path;
    Json value;
    const char* where;
    const char* why;
  };
  Json notBool = Json::makeString("yes");
  const int hound = p.ability("call_hound");
  const Case cases[] = {
      {{"units", "#0", "abilities", "#0", "ability"}, num(99), "units[0].abilities[0].ability", "outside 0..7"},
      {{"units", "#0", "abilities", "#0", "ability"}, num(-1), "units[0].abilities[0].ability", "outside"},
      {{"units", "#0", "abilities", "#0", "used"}, num(0), "units[0].abilities[0].used", "outside 1..999"},
      {{"units", "#0", "abilities", "#0", "used"}, num(1000), "units[0].abilities[0].used", "outside 1..999"},
      {{"units", "#0", "abilities", "#0", "wait"}, num(100), "units[0].abilities[0].wait", "outside 0..99"},
      {{"units", "#0", "abilities", "#0", "wait"}, num(-1), "units[0].abilities[0].wait", "outside 0..99"},
      {{"units", "#0", "abilities", "#0", "turn"}, notBool, "units[0].abilities[0].turn", "not true/false"},
      {{"units", "#0", "abilities", "#1", "ability"}, num(p.ability("yank")), "units[0].abilities[1].ability", "listed twice"},
      {{"units", "#4", "summon", "by"}, num(4), "units[4].summon.by", "lower id"},
      {{"units", "#4", "summon", "by"}, num(9), "units[4].summon.by", "lower id"},
      {{"units", "#4", "summon", "ability"}, num(p.ability("shove")), "units[4].summon.ability", "does not summon"},
      {{"units", "#4", "summon", "ability"}, num(40), "units[4].summon.ability", "outside"},
      {{"units", "#4", "summon", "left"}, num(100), "units[4].summon.left", "outside 0..99"},
      {{"units", "#4", "summon", "ability"}, num(hound), "units[4].summon.left", "no lifetime"},  // the hound has none
      {{"units", "#2", "summon"}, Json::makeObject(), "units[2].summon.by", "missing"},
  };
  for (const Case& c : cases) {
    savetest::Rig fresh;
    CHECK(fresh.load(abfiles::source(), "field"));
    std::string err;
    CHECK(!restoreBattleState(fresh.battle, jsonedit::withValue(good, c.path, c.value), err));
    CHECK(abfiles::saysIn(err, c.where));
    CHECK(abfiles::saysIn(err, c.why));
  }
  // A summon of a lifetime that lost its "left".
  savetest::Rig fresh;
  CHECK(fresh.load(abfiles::source(), "field"));
  std::string err;
  CHECK(!restoreBattleState(fresh.battle, jsonedit::withoutAt(good, {"units", "#4", "summon", "left"}), err));
  CHECK(abfiles::saysIn(err, "units[4].summon.left"));
}
