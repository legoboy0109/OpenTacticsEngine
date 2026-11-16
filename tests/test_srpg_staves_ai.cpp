// Staves and the computer opponent, saves and the campaign roster (docs/SRPG.md section 8 "AI", "Saves, hash, campaign").
// Game: tests/fixtures/srpg-staves; the blue side is played by the AI here (aiTakeStep plays the active side).
#include <algorithm>
#include <memory>

#include "core/ai/ai.h"
#include "srpg_staves_fixture.h"

using namespace ote;
using namespace stavestest;

namespace {

// The blue AI plays its whole turn; returns the staff uses it made (the StaffUsed events).
std::vector<Event> blueTurn(Clinic& w) {
  std::vector<Event> all;
  for (int i = 0; i < 40 && !w.b().over && w.b().activeSide == 0; ++i) aiTakeStep(w.b(), all);
  std::vector<Event> uses;
  for (const Event& e : all)
    if (e.type == EventType::StaffUsed) uses.push_back(e);
  return uses;
}

// One healer with one staff and nobody who can fight: a turn then holds nothing but moves and the healer's choice.
// `staves` = the items of Wen, as JSON text of the list.
void onlyWen(Copy& c, const std::string& staves) {
  c.swap("data/characters.json", "\"items\": [\"mend_staff\", \"rally_staff\", \"high_staff\", \"kin_staff\", \"chorus_staff\"]",
         "\"items\": " + staves);
  c.swap("maps/clinic.json", "\"items\": [\"kin_staff\", \"mend_staff\", \"rest_staff\"]", "\"items\": []");
  std::string units = c.read("data/units.json");
  const std::string from = "\"items\": [\"short_sword\"]";
  for (size_t at = units.find(from); at != std::string::npos; at = units.find(from, at)) units.replace(at, from.size(), "\"items\": []");
  c.put("data/units.json", units);
}

}  // namespace

TEST(staves_ai_heals_the_most_valuable_wounded_ally_in_reach) {
  Clinic w("staves_ai1", "clinic", [](Copy& c) { onlyWen(c, "[\"mend_staff\"]"); });
  w.hurt("ola", 5);   // 8 of 24 would come back
  w.hurt("brae", 5);  // 8 of 30: less than ola's share
  const std::vector<Event> uses = blueTurn(w);
  CHECK(!uses.empty());
  CHECK(uses[0].a == w.id("wen") && uses[0].c == w.id("ola"));
  CHECK_EQ(w.unit("ola").hp, 13);
  CHECK_EQ(w.unit("brae").hp, 5);  // one staff, one order per turn
}

TEST(staves_ai_never_spends_a_use_on_an_unhurt_unit_nor_on_a_scratch) {
  Clinic w("staves_ai2", "clinic", [](Copy& c) { onlyWen(c, "[\"mend_staff\"]"); });
  CHECK(blueTurn(w).empty());  // everybody whole
  CHECK_EQ(w.unit("wen").items[0].uses, 6);
  Clinic s("staves_ai3", "clinic", [](Copy& c) { onlyWen(c, "[\"mend_staff\"]"); });
  s.hurt("tarn", 23);  // a scratch: 1 HP of 24, above half, a staff that gives 8: not worth a use
  CHECK(blueTurn(s).empty());
  CHECK_EQ(s.unit("wen").items[0].uses, 6);
  Clinic m("staves_ai4", "clinic", [](Copy& c) { onlyWen(c, "[\"mend_staff\"]"); });
  m.hurt("tarn", 10);  // below half: worth it
  CHECK(!blueTurn(m).empty());
}

TEST(staves_ai_does_not_use_a_staff_it_cannot_wield) {
  // The only healer carries just the high staff (rank B needed, she has C): she never uses it.
  Clinic w("staves_ai5", "clinic", [](Copy& c) { onlyWen(c, "[\"high_staff\"]"); });
  w.hurt("tarn", 5);
  CHECK(blueTurn(w).empty());
}

TEST(staves_ai_plays_a_whole_skirmish_the_same_way_twice_and_never_stalls) {
  int used = 0;
  for (unsigned seed = 1; seed <= 6; ++seed) {
    std::string state[2];
    for (int run = 0; run < 2; ++run) {
      Clinic w(run ? "staves_sim_b" : "staves_sim_a", "skirmish");
      w.b().rng.seed(seed);
      std::vector<Event> ev;
      int steps = 0;
      while (!w.b().over && steps++ < 4000) aiTakeStep(w.b(), ev);
      CHECK(w.b().over);  // every turn ends: no stall
      state[run] = savetest::stateText(w.b());
      if (run == 0)
        for (const Event& e : ev) used += e.type == EventType::StaffUsed;
    }
    CHECK_STR(state[0], state[1]);
  }
  CHECK(used > 0);  // the healers do heal
}

TEST(staves_uses_and_experience_survive_a_save) {
  Clinic w("staves_save");
  w.hurt("tarn", 5);
  CHECK(w.use("wen", "mend_staff", "tarn"));
  Json j;
  saveBattleState(w.b(), j);
  CHECK_EQ((int)j["units"][(size_t)w.id("wen")]["ranks"]["staff"].asNumber(), 27);
  CHECK_EQ((int)j["units"][(size_t)w.id("wen")]["items"][(size_t)0]["uses"].asNumber(), 5);
  savetest::Rig again;
  CHECK(again.load(w.copy.dir, "clinic"));
  std::string err;
  CHECK(restoreBattleState(again.battle, j, err));
  CHECK_STR(savetest::stateText(again.battle), savetest::stateText(w.b()));
  CHECK_EQ(again.battle.units[(size_t)w.id("wen")].items[0].uses, 5);
  // A damaged save is refused by the item reader as before.
  std::string text = j.dump(0);
  const size_t at = text.find("\"uses\":5");
  CHECK(at != std::string::npos);
  text.replace(at, 8, "\"uses\":9");  // the mend staff has 6
  Json bad;
  std::string perr;
  CHECK(Json::parse(text, bad, perr));
  CHECK(!restoreBattleState(again.battle, bad, err));
}

TEST(staves_the_roster_carries_a_staffs_uses_like_a_weapons) {
  Clinic w("staves_roster");
  auto roster = std::make_shared<Roster>();
  RosterUnit r;
  r.id = "wen";
  r.type = w.rig.db.findUnitType("cleric");
  r.name = "Wen";
  r.level = 5;
  r.stats = w.unit("wen").stats;
  ItemGrant g;
  g.item = w.item("mend_staff");
  g.uses = 2;
  r.items.push_back(g);
  r.weaponXp = {0, 31};
  roster->push_back(r);
  savetest::Rig fresh;
  CHECK(fresh.load(w.copy.dir, "clinic"));
  fresh.battle.setStartingRoster(roster);
  const Unit& wen = fresh.battle.units[(size_t)w.id("wen")];
  CHECK_EQ((int)wen.items.size(), 1);
  CHECK_EQ(wen.items[0].uses, 2);
  CHECK_EQ(wen.weaponXp[(size_t)w.rig.db.ranks.findGroup("staff")], 31);
}
