// Guests (docs/SRPG.md "Guests"): a unit on a side a person plays that the computer moves, that does not keep its side in the battle and that leaves
// after it (the campaign part: test_guests_campaign.cpp). The loader's refusals, who acts when (side by side and in the speed turn order), the
// defeat rule, objectives and story conditions by side, "guestJoins", saves, the same battle twice. Game: tests/ability_conditions_fixture.h.
#include "ability_conditions_fixture.h"
#include "core/battle/save.h"

using namespace ote;
using namespace acf;

namespace {

// hero (mage) 1,1 and pal (knight) 0,1 on blue; the guest ritz (knight) 0,0 on blue; bandit (brigand) 3,1 on red. Ids 0..3 in that order.
const char* kUnits = "{\"type\":\"mage\",\"id\":\"hero\",\"side\":\"blue\",\"x\":1,\"y\":1},"
                     "{\"type\":\"knight\",\"id\":\"pal\",\"side\":\"blue\",\"x\":0,\"y\":1},"
                     "{\"type\":\"knight\",\"id\":\"ritz\",\"side\":\"blue\",\"x\":0,\"y\":0,\"guest\":true},"
                     "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":3,\"y\":1}";

Opts guestOpts(const std::string& units = kUnits) {
  Opts o;
  o.units = units;
  return o;
}

std::string guestError(const char* name, const std::string& units, const std::string& rules = "", const std::string& gameExtra = "",
                       const std::string& events = "") {
  Opts o = guestOpts(units);
  o.rules = rules;
  o.gameExtra = gameExtra;
  o.events = events;
  return error(make(name, o));
}

// Plays the guests of the side whose turn it is, as the player does before the person gets the turn. The number of decisions.
int playGuests(Battle& b, std::vector<Event>& events) {
  int n = 0;
  while (b.guestToAct() >= 0 && n < 50) {
    aiGuestStep(b, events);
    ++n;
  }
  return n;
}

// The whole battle with the computer on every side (a person's side: its guests first, then the rest as on --autoplay).
std::string playOut(const std::string& root, unsigned seed) {
  World w(root, seed);
  int guard = 0;
  while (!w.b().over && ++guard < 2000) {
    if (w.b().guestToAct() >= 0) aiGuestStep(w.b(), w.events);
    else aiTakeStep(w.b(), w.events);
  }
  Json saved;
  saveBattleState(w.b(), saved);
  return std::to_string(w.events.size()) + " " + std::to_string(w.b().winnerTeam) + " " + saved.dump(0);
}

}  // namespace

// ---- loader ----

TEST(guest_loads_from_the_map_and_from_add_unit) {
  World w(make("guest_load", guestOpts()));
  CHECK(w.unit("ritz").guest && !w.unit("hero").guest && !w.unit("bandit").guest);
  // An addUnit with "guest": true makes a guest too.
  Opts o = guestOpts();
  o.events = "[{\"id\":\"join\",\"when\":{\"battle\":\"start\"},\"do\":[{\"addUnit\":{\"type\":\"knight\",\"id\":\"marche\",\"side\":\"blue\",\"x\":0,\"y\":2,\"guest\":true}}]}]";
  World a(make("guest_load_add", o));
  CHECK(a.id("marche") >= 0 && a.unit("marche").guest);
}

TEST(guest_loader_refusals) {
  // On a computer side.
  CHECK(says(guestError("guest_r1", "{\"type\":\"mage\",\"id\":\"hero\",\"side\":\"blue\",\"x\":1,\"y\":1},"
                                    "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":3,\"y\":1,\"guest\":true}"),
             "\"guest\" is for a unit on a side a person plays; side \"red\" is the computer's"));
  // Not true / false.
  CHECK(says(guestError("guest_r2", "{\"type\":\"mage\",\"id\":\"hero\",\"side\":\"blue\",\"x\":1,\"y\":1,\"guest\":1},"
                                    "{\"type\":\"brigand\",\"side\":\"red\",\"x\":3,\"y\":1}"),
             "\"guest\""));
  // With action points (one pool for the side's turn).
  CHECK(says(guestError("guest_r3", kUnits, "\"actionPoints\":{\"resource\":\"ap\"},", "\"resources\":[{\"id\":\"ap\",\"name\":\"AP\"}],"),
             "\"guest\" cannot be used with \"rules\": { \"actionPoints\" }"));
  // An addUnit guest on a computer side.
  CHECK(says(guestError("guest_r4", kUnits, "", "",
                        "[{\"id\":\"e\",\"when\":{\"battle\":\"start\"},\"do\":[{\"addUnit\":{\"type\":\"brigand\",\"side\":\"red\",\"x\":5,\"y\":1,\"guest\":true}}]}]"),
             "\"guest\" is for a unit on a side a person plays"));
  // A behaviour is the computer's: fine on a guest, still refused on a unit the person moves.
  CHECK_STR(guestError("guest_r5", "{\"type\":\"mage\",\"id\":\"hero\",\"side\":\"blue\",\"x\":1,\"y\":1},"
                                   "{\"type\":\"knight\",\"id\":\"ritz\",\"side\":\"blue\",\"x\":0,\"y\":0,\"guest\":true,\"behaviour\":\"hold\"},"
                                   "{\"type\":\"brigand\",\"side\":\"red\",\"x\":3,\"y\":1}"),
            "");
  CHECK(says(guestError("guest_r6", "{\"type\":\"mage\",\"id\":\"hero\",\"side\":\"blue\",\"x\":1,\"y\":1,\"behaviour\":\"hold\"},"
                                    "{\"type\":\"brigand\",\"side\":\"red\",\"x\":3,\"y\":1}"),
             "\"behaviour\" tells the computer opponent what to do"));
  // guestJoins needs a unit filter.
  CHECK(says(guestError("guest_r7", kUnits, "", "", "[{\"id\":\"j\",\"when\":{\"battle\":\"start\"},\"do\":[{\"guestJoins\":5}]}]"), "guestJoins"));
  CHECK(says(guestError("guest_r8", kUnits, "", "", "[{\"id\":\"j\",\"when\":{\"battle\":\"start\"},\"do\":[{\"guestJoins\":\"ritz\",\"side\":\"blue\"}]}]"),
             "unknown key \"side\""));
}

// ---- who acts ----

TEST(guest_acts_first_and_never_ends_the_persons_turn) {
  World w(make("guest_first", guestOpts()));
  const int blue = w.b().activeSide;
  CHECK(!w.b().sides[(size_t)blue].ai);
  CHECK_EQ(w.b().guestToAct(), w.id("ritz"));
  w.events.clear();
  CHECK(playGuests(w.b(), w.events) >= 1);
  CHECK_EQ(w.b().guestToAct(), -1);
  CHECK(w.unit("ritz").done);                             // the guest has had its go ...
  CHECK(!w.unit("hero").done && !w.unit("pal").done);     // ... the person's units are untouched ...
  CHECK_EQ(w.b().activeSide, blue);                        // ... and the turn is still the person's
  for (const Event& e : w.events) CHECK(e.type != EventType::TurnStarted);
  CHECK(!aiGuestStep(w.b(), w.events));                    // nothing left to do
  // Next blue turn: it acts again.
  CHECK(w.run(Action::endTurn()));
  int guard = 0;
  while (!w.b().over && w.b().sides[(size_t)w.b().activeSide].ai && aiTakeStep(w.b(), w.events) && ++guard < 100) {
  }
  if (!w.b().over && w.unit("ritz").alive) CHECK_EQ(w.b().guestToAct(), w.id("ritz"));
}

TEST(guest_a_computer_side_and_a_game_without_guests_have_nothing_to_play) {
  World w(make("guest_none", Opts()));
  CHECK_EQ(w.b().guestToAct(), -1);
  std::vector<Event> ev;
  CHECK(!aiGuestStep(w.b(), ev));
  CHECK(ev.empty());
  World g(make("guest_red", guestOpts()));
  CHECK(g.run(Action::endTurn()));  // red's turn: its units are the computer's, never "guests"
  CHECK_EQ(g.b().guestToAct(), -1);
}

TEST(guest_with_hold_behaviour_stays) {
  World w(make("guest_hold", guestOpts("{\"type\":\"mage\",\"id\":\"hero\",\"side\":\"blue\",\"x\":1,\"y\":1},"
                                       "{\"type\":\"knight\",\"id\":\"ritz\",\"side\":\"blue\",\"x\":0,\"y\":0,\"guest\":true,\"behaviour\":\"hold\"},"
                                       "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":7,\"y\":1}")));
  const Cell at = w.unit("ritz").cell;
  playGuests(w.b(), w.events);
  CHECK_EQ(w.unit("ritz").cell, at);
  CHECK(w.unit("ritz").done);
}

TEST(guest_in_the_speed_turn_order_plays_its_own_turn) {
  Opts o = guestOpts();
  o.rules = "\"turnOrder\":{\"mode\":\"speed\",\"stat\":\"mov\"},";
  World w(make("guest_speed", o));
  int guard = 0, guestTurns = 0;
  while (!w.b().over && ++guard < 400) {
    const int g = w.b().guestToAct();
    if (g >= 0) {
      CHECK_EQ(g, w.b().activeUnit);
      CHECK(w.b().units[(size_t)g].guest);
      const int turn = w.b().turnNo;
      while (w.b().guestToAct() >= 0 && w.b().turnNo == turn && aiGuestStep(w.b(), w.events)) {
      }
      ++guestTurns;
      continue;
    }
    if (w.b().activeUnit >= 0) CHECK(!w.b().units[(size_t)w.b().activeUnit].guest);
    aiTakeStep(w.b(), w.events);
  }
  CHECK(guestTurns >= 1);
}

// ---- the defeat rule, objectives, conditions ----

TEST(guest_does_not_keep_its_side_in_the_battle) {
  // pal (1 HP) stands next to the bandit; the guest is far away. When pal falls, blue has only its guest: it has lost.
  World w(make("guest_defeat", guestOpts("{\"type\":\"knight\",\"id\":\"pal\",\"side\":\"blue\",\"x\":2,\"y\":1,\"stats\":{\"hp\":1}},"
                                         "{\"type\":\"knight\",\"id\":\"ritz\",\"side\":\"blue\",\"x\":8,\"y\":0,\"guest\":true},"
                                         "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":3,\"y\":1}")));
  playGuests(w.b(), w.events);
  CHECK(w.run(Action::endTurn()));
  CHECK(w.run(Action::attack(w.id("bandit"), w.id("pal"), w.rig.db.findWeapon("sword"))));
  CHECK(!w.unit("pal").alive);
  CHECK(w.b().over);
  CHECK_EQ(w.b().winnerTeam, w.b().sides[1].team);
}

TEST(guest_falling_does_not_lose_the_battle) {
  World w(make("guest_falls", guestOpts("{\"type\":\"mage\",\"id\":\"hero\",\"side\":\"blue\",\"x\":8,\"y\":2},"
                                        "{\"type\":\"knight\",\"id\":\"ritz\",\"side\":\"blue\",\"x\":2,\"y\":1,\"guest\":true,\"stats\":{\"hp\":1},\"behaviour\":\"hold\"},"
                                        "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":3,\"y\":1}")));
  playGuests(w.b(), w.events);
  CHECK(w.run(Action::endTurn()));
  CHECK(w.run(Action::attack(w.id("bandit"), w.id("ritz"), w.rig.db.findWeapon("sword"))));
  CHECK(!w.unit("ritz").alive);
  CHECK(!w.b().over);
}

TEST(guest_is_not_counted_by_a_side_filter_but_by_its_name) {
  // "defeated" { "side": "red" } wins; { "side": "blue" } loses. Blue's only unit of its own is hero, the guest ritz does not count.
  Opts o = guestOpts("{\"type\":\"mage\",\"id\":\"hero\",\"side\":\"blue\",\"x\":2,\"y\":1,\"stats\":{\"hp\":1}},"
                     "{\"type\":\"knight\",\"id\":\"ritz\",\"side\":\"blue\",\"x\":8,\"y\":0,\"guest\":true},"
                     "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":3,\"y\":1}");
  o.rules = "";
  o.mapExtra = "\"objectives\":{\"lose\":[{\"defeated\":{\"side\":\"blue\"}}]},";
  World w(make("guest_obj", o));
  playGuests(w.b(), w.events);
  CHECK(w.run(Action::endTurn()));
  CHECK(w.run(Action::attack(w.id("bandit"), w.id("hero"), w.rig.db.findWeapon("sword"))));
  CHECK(w.b().over && w.b().winnerTeam == w.b().sides[1].team);
  // Named, it counts: losing the guest loses.
  Opts p = guestOpts("{\"type\":\"mage\",\"id\":\"hero\",\"side\":\"blue\",\"x\":8,\"y\":2},"
                     "{\"type\":\"knight\",\"id\":\"ritz\",\"side\":\"blue\",\"x\":2,\"y\":1,\"guest\":true,\"stats\":{\"hp\":1},\"behaviour\":\"hold\"},"
                     "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":3,\"y\":1}");
  p.mapExtra = "\"objectives\":{\"lose\":[{\"defeated\":{\"unit\":\"ritz\"}}]},";
  World v(make("guest_obj2", p));
  playGuests(v.b(), v.events);
  CHECK(v.run(Action::endTurn()));
  CHECK(v.run(Action::attack(v.id("bandit"), v.id("ritz"), v.rig.db.findWeapon("sword"))));
  CHECK(v.b().over && v.b().winnerTeam == v.b().sides[1].team);
}

// ---- guestJoins ----

TEST(guest_joins_and_becomes_the_persons) {
  Opts o = guestOpts();
  o.events = "[{\"id\":\"j\",\"when\":{\"battle\":\"start\"},\"do\":[{\"guestJoins\":\"ritz\"}]}]";
  World w(make("guest_joins", o));
  CHECK(!w.unit("ritz").guest);
  CHECK_EQ(w.b().guestToAct(), -1);  // the person moves it now
  CHECK(w.run(Action::wait(w.id("ritz"), -1)));
}

// ---- saves ----

TEST(guest_is_saved_and_restored_only_when_there_is_one) {
  const std::string root = make("guest_save", guestOpts());
  World w(root);
  Json saved;
  saveBattleState(w.b(), saved);
  const Json& units = saved["units"];
  CHECK(units[2].has("guest") && units[2]["guest"].asBool());
  CHECK(!units[0].has("guest") && !units[3].has("guest"));
  {
    savetest::Rig r;
    CHECK(r.load(root, "field", 11));
    std::string err;
    CHECK(restoreBattleState(r.battle, saved, err));
    CHECK(r.battle.units[2].guest && !r.battle.units[0].guest);
  }
  {  // a save that calls a computer unit a guest is refused
    Json bad = saved;
    Json u = bad["units"][3];
    u.set("guest", Json::makeBool(true));
    Json list = Json::makeArray();
    for (size_t i = 0; i < bad["units"].size(); ++i) list.push(i == 3 ? u : bad["units"][i]);
    bad.set("units", list);
    savetest::Rig r;
    CHECK(r.load(root, "field", 11));
    std::string err;
    CHECK(!restoreBattleState(r.battle, bad, err));
    CHECK(says(err, "a guest belongs to a side a person plays"));
  }
  // A game without guests writes no key at all.
  World plain(make("guest_save_none", Opts()));
  Json p;
  saveBattleState(plain.b(), p);
  CHECK(p.dump(0).find("guest") == std::string::npos);
}

// ---- the same battle twice ----

TEST(guest_battle_plays_the_same_twice) {
  const std::string root = make("guest_twice", guestOpts());
  const std::string a = playOut(root, 5), b = playOut(root, 5);
  CHECK(a == b);
  CHECK(a.size() > 10);
}

TEST(guest_with_advance_walks_towards_the_foe) {
  World w(make("guest_walk", guestOpts("{\"type\":\"mage\",\"id\":\"hero\",\"side\":\"blue\",\"x\":1,\"y\":2},"
                                       "{\"type\":\"knight\",\"id\":\"ritz\",\"side\":\"blue\",\"x\":1,\"y\":0,\"guest\":true,\"behaviour\":\"advance\"},"
                                       "{\"type\":\"brigand\",\"id\":\"bandit\",\"side\":\"red\",\"x\":8,\"y\":0}")));
  const int before = w.b().grid.x(w.unit("ritz").cell);
  playGuests(w.b(), w.events);
  CHECK(w.b().grid.x(w.unit("ritz").cell) > before);
}
