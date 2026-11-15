// Fog of war hides structure upgrades (docs/ECONOMY.md "Fog of war"): every team remembers the last seen KIND of
// each structure next to its last seen owner, refreshed under exactly the same conditions. An upgrade done out of
// sight stays unknown to a team until one of its units (or structures) sees the structure again, and that
// sighting updates the picture without any event. Rules stay on the truth. Saves keep the memory in "seenKinds",
// written only when some team believes a kind other than the map's.
#include "core/ai/ai.h"
#include "core/ai/ai_internal.h"
#include "json_edit.h"
#include "research_fixture.h"
#include "save_helpers.h"

using namespace ote;
using namespace restest;
using namespace jsonedit;

namespace {

// The research map with fog on: blue's factory at (2,0) is far from red's soldier at (5,2) (vision 2).
struct Hidden : ResearchWorld {
  Hidden() { map.fog = true; }
  int factory() const { return structure(2, 0); }
  int kindFor(int team) const { return battle.seenKind(team, factory()); }
  Json save() const {
    Json j;
    saveBattleState(battle, j);
    return j;
  }
  // Red's soldier sees far enough to take in the factory.
  void redSeesEverything() {
    for (UnitTypeDef& t : db.unitTypes) t.vision = 9;
    battle.refreshSight();
  }
};

std::string restoreError(Hidden& m, const Json& saved) {
  Battle r;
  std::string err;
  if (!r.init(m.db, m.map, m.formulas, 1, err)) return "init failed";
  return restoreBattleState(r, saved, err) ? "" : err;
}

}  // namespace

TEST(fog_kinds_an_upgrade_out_of_sight_is_not_known_to_the_other_team) {
  Hidden w;
  CHECK(w.begin());
  const int factory = w.db.findStructure("factory");
  CHECK_EQ(w.kindFor(0), factory);
  CHECK_EQ(w.kindFor(1), factory);
  CHECK(!w.battle.structureVisibleTo(1, w.factory()));
  CHECK(w.act(Action::upgrade(w.factory(), Foundry)));
  CHECK_EQ(w.kindAt(2, 0), (int)Foundry);  // the truth
  CHECK_EQ(w.kindFor(0), (int)Foundry);    // its owner sees it
  CHECK_EQ(w.kindFor(1), factory);         // red still believes the old kind
  CHECK_EQ(w.battle.seenKindForSide(1, w.factory()), factory);
  CHECK(&w.battle.seenDef(1, w.factory()) == &w.db.structures[(size_t)factory]);
  // The event is not red's.
  std::vector<Event> up;
  for (const Event& e : w.events)
    if (e.type == EventType::StructureUpgraded) up.push_back(e);
  CHECK_EQ(up.size(), size_t(1));
  CHECK((up[0].seen & 2u) == 0);
  CHECK((up[0].seen & 1u) != 0);
}

TEST(fog_kinds_a_later_sighting_updates_the_picture_without_an_event) {
  Hidden w;
  CHECK(w.begin());
  CHECK(w.act(Action::upgrade(w.factory(), Foundry)));
  CHECK_EQ(w.kindFor(1), w.db.findStructure("factory"));
  CHECK(w.act(Action::endTurn()));  // red's turn
  w.redSeesEverything();
  CHECK(w.battle.structureVisibleTo(1, w.factory()));
  CHECK_EQ(w.kindFor(1), (int)Foundry);
  // Nothing was announced to red for it: the upgrade event is still blue's alone.
  for (const Event& e : w.events)
    if (e.type == EventType::StructureUpgraded) CHECK((e.seen & 2u) == 0);
  // And once learned, it stays even when the structure drops out of sight again.
  for (UnitTypeDef& t : w.db.unitTypes) t.vision = 0;
  w.battle.refreshSight();
  CHECK(!w.battle.structureVisibleTo(1, w.factory()));
  CHECK_EQ(w.kindFor(1), (int)Foundry);
}

TEST(fog_kinds_an_upgrade_in_sight_is_known_at_once) {
  Hidden w;
  CHECK(w.begin());
  w.redSeesEverything();
  CHECK(w.act(Action::upgrade(w.factory(), Foundry)));
  CHECK_EQ(w.kindFor(1), (int)Foundry);
  bool redSaw = false;
  for (const Event& e : w.events)
    if (e.type == EventType::StructureUpgraded) redSaw = (e.seen & 2u) != 0;
  CHECK(redSaw);
}

TEST(fog_kinds_without_fog_every_team_knows_the_true_kind) {
  Hidden w;
  w.map.fog = false;
  CHECK(w.begin());
  CHECK(w.act(Action::upgrade(w.factory(), Foundry)));
  CHECK_EQ(w.kindFor(0), (int)Foundry);
  CHECK_EQ(w.kindFor(1), (int)Foundry);
  CHECK(!w.save().has("seenKinds"));
}

TEST(fog_kinds_the_rules_stay_on_the_true_kind) {
  Hidden w;
  CHECK(w.begin());
  CHECK(w.act(Action::upgrade(w.structure(3, 0), Keep)));  // the hq becomes a keep: 40 capture points
  const StructureState& hq = w.battle.structures[(size_t)w.structure(3, 0)];
  CHECK_EQ(w.battle.defOf(hq).capturePoints, 40);                          // the rule reads the truth ...
  CHECK_EQ(w.db.structures[(size_t)w.battle.seenKind(1, w.structure(3, 0))].capturePoints,
           w.db.structures[(size_t)w.db.findStructure("hq")].capturePoints);  // ... red's picture is the old kind
  CHECK_EQ(hq.type, (int)Keep);
  // The footprint cannot change with a kind.
  CHECK_EQ(w.db.structures[(size_t)w.battle.seenKind(1, w.structure(3, 0))].width, w.db.structures[Keep].width);
}

TEST(fog_kinds_the_ai_values_a_target_by_the_kind_it_believes) {
  Hidden w;
  CHECK(w.begin());
  // Blue's factory becomes a foundry (it produces more); red has not seen it.
  const int factory = w.db.findStructure("factory");
  CHECK(&ai::knownDef(w.battle, 1, w.factory()) == &w.db.structures[(size_t)factory]);
  CHECK(w.act(Action::upgrade(w.factory(), Foundry)));
  CHECK(&ai::knownDef(w.battle, 1, w.factory()) == &w.db.structures[(size_t)factory]);  // still the old kind
  CHECK(&ai::knownDef(w.battle, 0, w.factory()) == &w.db.structures[Foundry]);
  w.redSeesEverything();
  CHECK(&ai::knownDef(w.battle, 1, w.factory()) == &w.db.structures[Foundry]);
}

TEST(fog_kinds_save_is_byte_identical_when_nothing_was_upgraded) {
  Hidden w;
  CHECK(w.begin());
  CHECK(w.act(Action::endTurn()));
  Json s = w.save();
  CHECK(s.has("seenOwners"));
  CHECK(!s.has("seenKinds"));
  CHECK_STR(restoreError(w, s), "");
}

TEST(fog_kinds_save_round_trips_a_stale_picture) {
  Hidden w;
  CHECK(w.begin());
  CHECK(w.act(Action::upgrade(w.factory(), Foundry)));
  Json saved = w.save();
  CHECK(saved.has("seenKinds"));
  CHECK_EQ(saved["seenKinds"].size(), size_t(2));
  Json reread;
  std::string err;
  CHECK(Json::parse(saved.dump(0), reread, err));
  Battle r;
  CHECK(r.init(w.db, w.map, w.formulas, 1, err));
  CHECK(restoreBattleState(r, reread, err));
  CHECK_STR(savetest::stateText(r), savetest::stateText(w.battle));
  CHECK_EQ(r.seenKind(0, w.factory()), (int)Foundry);
  CHECK_EQ(r.seenKind(1, w.factory()), w.db.findStructure("factory"));
  // A save from before this existed (no key) restores to the map kinds.
  Json old = withoutAt(saved, {"seenKinds"});
  Battle r2;
  CHECK(r2.init(w.db, w.map, w.formulas, 1, err));
  CHECK(restoreBattleState(r2, old, err));
  CHECK_EQ(r2.seenKind(0, w.factory()), (int)Foundry);  // blue sees it, so the memory refreshes at once
  CHECK_EQ(r2.seenKind(1, w.factory()), w.db.findStructure("factory"));
}

TEST(fog_kinds_save_playing_on_after_loading_gives_the_same_knowledge) {
  Hidden w;
  CHECK(w.begin());
  CHECK(w.act(Action::upgrade(w.factory(), Foundry)));
  Battle r;
  std::string err;
  CHECK(r.init(w.db, w.map, w.formulas, 1, err));
  CHECK(restoreBattleState(r, w.save(), err));
  std::vector<Event> a, b;
  CHECK(w.battle.apply(Action::endTurn(), a));
  CHECK(r.apply(Action::endTurn(), b));
  for (int t = 0; t < 2; ++t) CHECK_EQ(r.seenKind(t, w.factory()), w.battle.seenKind(t, w.factory()));
}

TEST(fog_kinds_save_without_fog_refuses_the_key) {
  Hidden w;
  CHECK(w.begin());
  CHECK(w.act(Action::upgrade(w.factory(), Foundry)));
  Json saved = w.save();
  Hidden off;
  off.map.fog = false;
  CHECK(off.begin());
  Json s2 = withValue(off.save(), {"seenKinds"}, saved["seenKinds"]);
  CHECK(restoreError(off, s2).find("seenKinds") != std::string::npos);
}

#define DAMAGED(testName, edit, needle)                                                    \
  TEST(testName) {                                                                         \
    Hidden m;                                                                              \
    CHECK(m.begin());                                                                      \
    CHECK(m.act(Action::upgrade(m.factory(), Foundry)));                                   \
    std::string err = restoreError(m, edit);                                               \
    if (err.find(needle) == std::string::npos) {                                           \
      std::printf("  expected \"%s\" in: %s\n", needle, err.c_str());                      \
      CHECK(false);                                                                        \
    }                                                                                      \
  }

DAMAGED(fog_kinds_damaged_not_a_list, withValue(m.save(), {"seenKinds"}, num(1)), "battle.seenKinds")
DAMAGED(fog_kinds_damaged_wrong_team_count, withValue(m.save(), {"seenKinds"}, Json::makeArray()), "has 0 entries, expected 2")
DAMAGED(fog_kinds_damaged_team_not_a_list, withValue(m.save(), {"seenKinds", "#1"}, num(3)), "seenKinds[1]: not a list")
DAMAGED(fog_kinds_damaged_wrong_structure_count, withValue(m.save(), {"seenKinds", "#0"}, list({0})), "seenKinds[0]: has 1 entries")
DAMAGED(fog_kinds_damaged_not_a_number, withValue(m.save(), {"seenKinds", "#0", "#2"}, str("foundry")), "seenKinds[0][2]: not a structure kind number")
DAMAGED(fog_kinds_damaged_fraction, withValue(m.save(), {"seenKinds", "#1", "#2"}, num(0.5)), "seenKinds[1][2]: not a structure kind number")
DAMAGED(fog_kinds_damaged_negative, withValue(m.save(), {"seenKinds", "#1", "#2"}, num(-1)), "seenKinds[1][2]: not a structure kind number")
DAMAGED(fog_kinds_damaged_out_of_range, withValue(m.save(), {"seenKinds", "#1", "#2"}, num(99)), "seenKinds[1][2]: not a structure kind number")
DAMAGED(fog_kinds_damaged_unreachable_kind, withValue(m.save(), {"seenKinds", "#1", "#2"}, num((double)Keep)), "cannot be reached")
DAMAGED(fog_kinds_damaged_other_footprint, withValue(m.save(), {"seenKinds", "#1", "#2"}, num(3)), "does not have the size")
