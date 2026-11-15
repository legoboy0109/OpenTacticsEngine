// Saves of fog of war battles (docs/SAVES.md): the last seen structure owners round-trip, sight itself is recomputed,
// a battle without fog writes nothing new, and every damaged field is refused with a readable message.
#include "core/ai/ai.h"
#include "fog_fixture.h"
#include "json_edit.h"
#include "save_helpers.h"

using namespace ote;
using namespace fogtest;
using namespace jsonedit;

namespace {

// Blue (0,1) on the left, red (11,0) next to a city it has just taken in blue's absence; a second city is red's from
// the start. Blue's picture of the first one is stale.
struct Mid : FogWorld {
  Mid() : FogWorld({"c.........c.", "............", "............"}, {{"soldier", 0, 3, 1, false}, {"soldier", 1, 10, 0, false}}) {
    own(0, 0, 1);
  }
  bool ok() {
    if (!begin()) return false;
    return act(Action::move(0, at(5, 1))) && act(Action::endTurn()) && act(Action::capture(1));
  }
  Json save() const {
    Json j;
    saveBattleState(battle, j);
    return j;
  }
};

std::string restoreError(Mid& m, const Json& saved, Battle* into = nullptr) {
  Battle local;
  Battle& r = into ? *into : local;
  std::string err;
  if (!r.init(m.db, m.map, m.formulas, 1, err)) return "init failed";
  std::string before = savetest::stateText(r);
  bool ok = restoreBattleState(r, saved, err);
  if (!ok && savetest::stateText(r) != before) return "refused but changed the battle: " + err;
  return ok ? "" : err;
}

}  // namespace

TEST(fog_save_round_trip_keeps_what_each_team_last_saw) {
  Mid m;
  CHECK(m.ok());
  const int far = m.structure(10, 0), near = m.structure(0, 0);
  CHECK_EQ(m.battle.seenOwner(0, far), -1);          // blue never saw the capture
  CHECK_EQ(m.battle.seenOwner(1, far), 1);
  Json reread;
  std::string err;
  CHECK(Json::parse(m.save().dump(0), reread, err));  // through text, like a file
  Battle r;
  CHECK_STR(restoreError(m, reread, &r), "");
  CHECK_STR(savetest::stateText(r), savetest::stateText(m.battle));
  for (int team = 0; team < 2; ++team)
    for (int s : {far, near}) CHECK_EQ(r.seenOwner(team, s), m.battle.seenOwner(team, s));
  CHECK_EQ(r.structures[(size_t)far].owner, 1);       // the truth is not in the memory
  CHECK_EQ(r.seenOwner(0, far), -1);                  // and the stale belief survived
}

TEST(fog_save_sight_is_recomputed_after_loading) {
  Mid m;
  CHECK(m.ok());
  Json saved = m.save();
  Battle r;
  CHECK_STR(restoreError(m, saved, &r), "");
  for (int team = 0; team < 2; ++team)
    for (Cell c = 0; c < r.grid.cellCount(); ++c) CHECK_EQ(r.visibleTo(team, c), m.battle.visibleTo(team, c));
  for (int u = 0; u < 2; ++u)
    for (int team = 0; team < 2; ++team) CHECK_EQ(r.unitVisibleTo(team, u), m.battle.unitVisibleTo(team, u));
}

TEST(fog_save_playing_on_from_a_loaded_save_gives_the_same_game_and_masks) {
  Mid m;
  CHECK(m.ok());
  Battle r;
  CHECK_STR(restoreError(m, m.save(), &r), "");
  std::vector<Event> a, b;
  for (int i = 0; i < 40 && !m.battle.over; ++i) {
    aiTakeStep(m.battle, a);
    aiTakeStep(r, b);
  }
  CHECK(a.size() > 5);
  CHECK(savetest::sameEvents(a, b));
  CHECK_STR(savetest::stateText(r), savetest::stateText(m.battle));
}

TEST(fog_save_writes_the_key_only_for_fog_battles) {
  Mid m;
  CHECK(m.ok());
  Json s = m.save();
  CHECK(s.has("seenOwners"));
  CHECK_EQ(s["seenOwners"].size(), size_t(2));       // one list per team
  CHECK_EQ(s["seenOwners"][size_t(0)].size(), size_t(m.battle.structures.size()));
  Mid off;
  off.map.fog = false;
  CHECK(off.ok());
  CHECK(!off.save().has("seenOwners"));
}

TEST(fog_save_a_battle_without_fog_refuses_the_key) {
  Mid fog;
  CHECK(fog.ok());
  Mid off;
  off.map.fog = false;
  CHECK(off.ok());
  Json saved = withValue(off.save(), {"seenOwners"}, Json::makeArray());
  CHECK(restoreError(off, saved).find("seenOwners") != std::string::npos);
  // And a fog battle needs it.
  CHECK(restoreError(fog, withoutAt(fog.save(), {"seenOwners"})).find("seenOwners") != std::string::npos);
}

#define DAMAGED(testName, edit, needle)                                                    \
  TEST(testName) {                                                                         \
    Mid m;                                                                                 \
    CHECK(m.ok());                                                                         \
    std::string err = restoreError(m, edit);                                               \
    if (err.find(needle) == std::string::npos) {                                           \
      std::printf("  expected \"%s\" in: %s\n", needle, err.c_str());                      \
      CHECK(false);                                                                        \
    }                                                                                      \
  }

DAMAGED(fog_save_damaged_seen_owners_not_a_list, withValue(m.save(), {"seenOwners"}, num(1)), "battle.seenOwners")
DAMAGED(fog_save_damaged_seen_owners_wrong_team_count, withValue(m.save(), {"seenOwners"}, Json::makeArray()), "has 0 entries, expected 2")
DAMAGED(fog_save_damaged_seen_owners_team_not_a_list, withValue(m.save(), {"seenOwners", "#1"}, num(3)), "seenOwners[1]: not a list")
DAMAGED(fog_save_damaged_seen_owners_wrong_structure_count, withValue(m.save(), {"seenOwners", "#0"}, list({-1})), "seenOwners[0]: has 1 entries, expected 2")
DAMAGED(fog_save_damaged_seen_owner_not_a_number, withValue(m.save(), {"seenOwners", "#0", "#1"}, str("red")), "seenOwners[0][1]: not a side number")
DAMAGED(fog_save_damaged_seen_owner_fraction, withValue(m.save(), {"seenOwners", "#1", "#0"}, num(0.5)), "seenOwners[1][0]: not a side number")
DAMAGED(fog_save_damaged_seen_owner_below_range, withValue(m.save(), {"seenOwners", "#0", "#0"}, num(-2)), "seenOwners[0][0]: not a side number")
DAMAGED(fog_save_damaged_seen_owner_a_side_that_does_not_exist, withValue(m.save(), {"seenOwners", "#0", "#0"}, num(2)), "seenOwners[0][0]: not a side number")

TEST(fog_save_a_refused_save_leaves_the_battle_untouched) {
  Mid m;
  CHECK(m.ok());
  Battle r;
  std::string err;
  CHECK(r.init(m.db, m.map, m.formulas, 1, err));
  std::string before = savetest::stateText(r);
  CHECK(!restoreBattleState(r, withValue(m.save(), {"seenOwners", "#0", "#0"}, num(9)), err));
  CHECK_STR(savetest::stateText(r), before);
}
