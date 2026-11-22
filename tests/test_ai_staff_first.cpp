// Computer opponent: "staffFirst", "staffPick": "lastPlaced", "inflictOnlyOn": "casters" and a pull staff under "heals": "retreating"
// (docs/AI.md section 10 and section 9). Rules on the generated game of ai_scripted_fixture.h plus the staves, conditions and one more unit type
// written here. Blue "lord" never acts (the tests end its turn); red "a" and friends hold still so that only the staff user draws random numbers.
#include "ai_scripted_fixture.h"

using namespace ote;
using namespace scripted;

namespace {

// Staves (all reach 2 except the rescue's 3): mend heals 8, mend2 heals 5, sleepy / hush / dud afflict a foe (dud can never hit), restore cures everything, rescue pulls.
const char* kItems = R"({ "items": [
  { "id": "potion", "name": "Potion", "kind": "usable", "uses": 2, "heal": 10 },
  { "id": "mend", "name": "Mend", "kind": "staff", "group": "staff", "uses": 20, "range": 2, "target": "ally", "effects": [ { "heal": 8 } ] },
  { "id": "mend2", "name": "Mend 2", "kind": "staff", "group": "staff", "uses": 20, "range": 2, "target": "ally", "effects": [ { "heal": 5 } ] },
  { "id": "sleepy", "name": "Sleepy", "kind": "staff", "group": "staff", "uses": 20, "range": 2, "target": "foe", "hit": "100", "effects": [ { "inflict": { "condition": "sleep" } } ] },
  { "id": "hush", "name": "Hush", "kind": "staff", "group": "staff", "uses": 20, "range": 2, "target": "foe", "hit": "100", "effects": [ { "inflict": { "condition": "silence" } } ] },
  { "id": "dud", "name": "Dud", "kind": "staff", "group": "staff", "uses": 20, "range": 2, "target": "foe", "hit": "0", "effects": [ { "inflict": { "condition": "sleep" } } ] },
  { "id": "restore", "name": "Restore", "kind": "staff", "group": "staff", "uses": 20, "range": 2, "target": "ally", "effects": [ { "cure": "all" } ] },
  { "id": "rescue", "name": "Rescue", "kind": "staff", "group": "staff", "uses": 20, "range": 3, "target": "ally", "effects": [ { "pull": true } ] } ] })";
const char* kConditions = R"({ "conditions": [
  { "id": "sleep", "name": "Sleep", "tag": "Zz", "turns": 3, "blocks": [ { "when": "always", "do": [ { "cannotAct": true }, { "cannotMove": true } ] } ] },
  { "id": "silence", "name": "Silence", "tag": "Si", "turns": 3, "blocks": [ { "when": "always", "do": [ { "forbid": { "kinds": ["staff"], "groups": ["tome"] } } ] } ] },
  { "id": "curse", "name": "Curse", "tag": "Cu", "turns": 3, "blocks": [ { "when": "always", "do": [ { "stats": { "str": -1 } } ] } ] } ] })";
const char* kWeapons = R"({ "weapons": [ { "id": "sword", "name": "Sword", "group": "sword", "props": { "might": 5 } },
  { "id": "bow", "name": "Bow", "group": "bow", "range": [2, 2], "props": { "might": 4 } },
  { "id": "sword2", "name": "Sword 2", "group": "sword", "props": { "might": 5 } },
  { "id": "tome", "name": "Tome", "group": "tome", "range": [1, 2], "props": { "might": 4 } } ] })";
const char* kUnits = R"({ "units": [
  { "id": "soldier", "name": "Soldier", "glyph": "S", "moveType": "foot", "stats": { "hp": 20, "str": 6, "def": 2, "mov": 4 }, "weapons": ["sword"], "wields": ["staff"] },
  { "id": "archer", "name": "Archer", "glyph": "A", "moveType": "foot", "stats": { "hp": 16, "str": 5, "def": 1, "mov": 4 }, "weapons": ["bow"] },
  { "id": "twin", "name": "Twin", "glyph": "T", "moveType": "foot", "stats": { "hp": 20, "str": 6, "def": 2, "mov": 4 }, "weapons": ["sword", "sword2"] },
  { "id": "mage", "name": "Mage", "glyph": "M", "moveType": "foot", "stats": { "hp": 16, "str": 5, "def": 1, "mov": 4 }, "weapons": ["tome"], "wields": ["staff"] } ] })";

const std::string kStaffFirst100 = R"({ "staffFirst": 100 })";

std::string items(const char* a, const char* b = nullptr) {
  return std::string("\"items\": [\"") + a + "\"" + (b ? std::string(", \"") + b + "\"" : "") + "]";
}
std::string holdWith(const std::string& extra = "") { return beh(R"({ "mode": "hold" })") + (extra.empty() ? "" : ", " + extra); }

struct TField {
  savetest::Rig rig;
  std::vector<Event> events;
  bool loaded = false;

  TField(const std::string& name, const std::string& units) {
    const std::string dir = SField::write(name, units, Options());
    writeTextFile(dir + "/data/items.json", kItems);
    writeTextFile(dir + "/data/conditions.json", kConditions);
    writeTextFile(dir + "/data/weapons.json", kWeapons);
    writeTextFile(dir + "/data/units.json", kUnits);
    loaded = rig.load(dir, "field");
    if (loaded) rig.battle.start(events);
  }
  Battle& b() { return rig.battle; }
  Unit& unit(const char* story) { return b().units[(size_t)b().unitByStoryId(story)]; }
  int hp(const char* who) { return unit(who).hp; }
  void hurt(const char* who, int hp, bool retreating = false) {
    unit(who).hp = hp;
    unit(who).retreating = retreating;
  }
  void give(const char* who, const char* condition) {
    ConditionState cs;
    cs.condition = b().db->findCondition(condition);
    cs.left = 3;
    unit(who).conditions.push_back(cs);
  }
  bool has(const char* who, const char* condition) { return b().hasCondition(unit(who), b().db->findCondition(condition)); }
  // Blue ends its turn; `roll` = what the battle's next random number will be (0..99) when red starts to act; then red plays its whole turn.
  bool redTurn(int* roll = nullptr) {
    events.clear();
    if (!b().apply(Action::endTurn(), events)) return false;
    if (roll) {
      Rng probe = b().rng;
      *roll = probe.range(100);
    }
    for (int i = 0; i < 80 && !b().over && b().activeSide == 1; ++i) aiTakeStep(b(), events);
    return b().over || b().activeSide == 0;
  }
  int uses() const {
    int n = 0;
    for (const Event& e : events) n += e.type == EventType::StaffUsed;
    return n;
  }
  int misses() const {
    int n = 0;
    for (const Event& e : events) n += e.type == EventType::StaffMissed;
    return n;
  }
  int x(const char* who) { return b().grid.x(unit(who).cell); }
  int y(const char* who) { return b().grid.y(unit(who).cell); }
};

// A staff user h at 6,2 (move 4, staff reach 2) holding two still allies: a (6,0) and b (6,4); the lord far away at 13,0 unless the test says.
std::string crew(const std::string& hExtra, const std::string& lord = "") {
  return (lord.empty() ? blue("lord", 13, 0) : lord) + ", " + red("h", 6, 2, hExtra) + ", " + red("a", 6, 0, holdWith()) + ", " + red("b", 6, 4, holdWith());
}

}  // namespace

TEST(ai_staff_first_uses_the_staff_before_its_usual_choice_which_would_have_been_an_attack) {
  // a is only scratched (19 of 20): the usual choice judges the heal not worth a use and attacks the lord, who stands within reach.
  const std::string lord = blue("lord", 9, 2);
  TField plain("aisf_plain", crew(items("mend"), lord));
  CHECK(plain.loaded);
  plain.hurt("a", 19);
  CHECK(plain.redTurn());
  CHECK_EQ(plain.uses(), 0);
  CHECK(plain.hp("lord") < 20);
  TField f("aisf_first", crew(items("mend") + ", " + beh(kStaffFirst100), lord));
  CHECK(f.loaded);
  f.hurt("a", 19);
  CHECK(f.redTurn());
  CHECK_EQ(f.uses(), 1);
  CHECK_EQ(f.hp("a"), 20);
  CHECK_EQ(f.hp("lord"), 20);  // it used the staff and was done: no attack
}

TEST(ai_staff_first_the_roll_is_the_battles_next_number_and_acts_when_roll_is_at_most_n) {
  int acted = 0, skipped = 0;
  for (unsigned seed = 1; seed <= 40; ++seed) {
    TField f("aisf_roll" + std::to_string(seed), crew(items("mend") + ", " + beh(R"({ "staffFirst": 50 })"), blue("lord", 9, 2)));
    CHECK(f.loaded);
    f.b().rng.seed(seed);
    f.hurt("a", 19);  // a scratch: only staffFirst would heal it (the usual choice attacks the lord)
    int roll = -1;
    CHECK(f.redTurn(&roll));
    const bool expect = roll <= 50;
    CHECK_EQ(f.uses(), expect ? 1 : 0);
    CHECK_EQ(f.hp("a"), expect ? 20 : 19);
    (expect ? acted : skipped)++;
  }
  CHECK(acted > 5 && skipped > 5);  // both outcomes happen: the check above is not vacuous
}

TEST(ai_staff_first_n_is_inclusive_100_always_and_0_acts_only_on_a_roll_of_0) {
  int zeroSeeds = 0, otherSeeds = 0;
  for (unsigned seed = 1; seed <= 600 && (zeroSeeds < 1 || otherSeeds < 3); ++seed) {
    TField probe("aisf_incl_p", crew(items("mend") + ", " + beh(R"({ "staffFirst": 0 })"), blue("lord", 9, 2)));
    TField all("aisf_incl_a", crew(items("mend") + ", " + beh(kStaffFirst100), blue("lord", 9, 2)));
    CHECK(probe.loaded && all.loaded);
    probe.b().rng.seed(seed);
    all.b().rng.seed(seed);
    probe.hurt("a", 19);
    all.hurt("a", 19);
    int roll = -1;
    CHECK(probe.redTurn(&roll));
    CHECK(all.redTurn());
    CHECK_EQ(all.uses(), 1);  // 100: every roll acts
    CHECK_EQ(probe.uses(), roll == 0 ? 1 : 0);
    (roll == 0 ? zeroSeeds : otherSeeds)++;
  }
  CHECK(zeroSeeds >= 1 && otherSeeds >= 3);
}

TEST(ai_staff_first_same_seed_same_battle_and_a_different_seed_may_differ) {
  const std::string units = crew(items("mend") + ", " + beh(R"({ "staffFirst": 50 })"), blue("lord", 9, 2));
  std::string first;
  bool varied = false;
  for (int round = 0; round < 2; ++round) {
    for (unsigned seed = 1; seed <= 12; ++seed) {
      TField f("aisf_det", units);
      CHECK(f.loaded);
      f.b().rng.seed(seed);
      f.hurt("a", 19);
      CHECK(f.redTurn());
      const std::string state = savetest::stateText(f.b());
      if (round == 0) first += state;
      else CHECK(first.find(state) != std::string::npos);
      varied = varied || state != savetest::stateText(f.b()) || f.hp("a") != 20;
    }
  }
  CHECK(varied);
}

TEST(ai_staff_first_draws_no_number_without_a_usable_staff_or_without_a_legal_target) {
  // Nobody hurt (no legal target), the lord out of reach: the unit stays put and the random generator is untouched.
  TField f("aisf_notarget", crew(items("mend") + ", " + beh(R"({ "mode": "hold", "staffFirst": 100 })")));
  CHECK(f.loaded);
  f.events.clear();
  CHECK(f.b().apply(Action::endTurn(), f.events));
  const Rng before = f.b().rng;
  for (int i = 0; i < 80 && f.b().activeSide == 1; ++i) aiTakeStep(f.b(), f.events);
  CHECK_EQ(f.b().rng.state, before.state);
  // No staff at all, though a hurt ally is there.
  TField g("aisf_nostaff", crew(beh(R"({ "mode": "hold", "staffFirst": 100 })")));
  g.hurt("a", 10);
  g.events.clear();
  CHECK(g.b().apply(Action::endTurn(), g.events));
  const Rng before2 = g.b().rng;
  for (int i = 0; i < 80 && g.b().activeSide == 1; ++i) aiTakeStep(g.b(), g.events);
  CHECK_EQ(g.b().rng.state, before2.state);
  CHECK_EQ(g.uses(), 0);
}

TEST(ai_staff_first_off_draws_no_number_either_so_a_game_without_the_key_is_unchanged) {
  // The same staff and the same scratched ally without the key: the roll of an eligible unit is the only draw staffFirst adds, and there is none.
  TField f("aisf_off", crew(items("mend") + ", " + beh(R"({ "mode": "hold" })")));
  CHECK(f.loaded);
  f.hurt("a", 10);  // really hurt: the usual choice heals it
  f.events.clear();
  CHECK(f.b().apply(Action::endTurn(), f.events));
  const Rng before = f.b().rng;
  for (int i = 0; i < 80 && f.b().activeSide == 1; ++i) aiTakeStep(f.b(), f.events);
  CHECK_EQ(f.uses(), 1);
  CHECK_EQ(f.b().rng.state, before.state);  // (the heal never misses and draws nothing)
}

TEST(ai_staff_first_a_silenced_unit_cannot_and_a_staff_the_rules_forbid_is_not_tried) {
  TField f("aisf_silenced", crew(items("mend") + ", " + beh(kStaffFirst100), blue("lord", 9, 2)));
  CHECK(f.loaded);
  f.hurt("a", 19);
  f.give("h", "silence");
  CHECK(f.redTurn());
  CHECK_EQ(f.uses(), 0);
  CHECK_EQ(f.hp("a"), 19);
  CHECK(f.hp("lord") < 20);  // it chose as usual: it attacked
}

TEST(ai_staff_first_staves_are_tried_in_the_order_carried) {
  TField f("aisf_order1", crew(items("mend", "mend2") + ", " + beh(kStaffFirst100)));
  f.hurt("a", 6);
  CHECK(f.redTurn());
  CHECK_EQ(f.hp("a"), 14);  // mend (8) first
  TField g("aisf_order2", crew(items("mend2", "mend") + ", " + beh(kStaffFirst100)));
  g.hurt("a", 6);
  CHECK(g.redTurn());
  CHECK_EQ(g.hp("a"), 11);  // mend2 (5) first
}

TEST(ai_staff_first_the_usual_choice_still_uses_a_staff_by_score_when_the_roll_fails) {
  // staffFirst 0 almost never acts, but a nearly dead ally is healed by the usual scoring all the same.
  for (unsigned seed = 1; seed <= 10; ++seed) {
    TField f("aisf_fallback" + std::to_string(seed), crew(items("mend") + ", " + beh(R"({ "mode": "hold", "staffFirst": 0 })")));
    f.b().rng.seed(seed);
    f.hurt("a", 4);
    CHECK(f.redTurn());
    CHECK_EQ(f.hp("a"), 12);
  }
}

TEST(ai_staff_pick_last_placed_heal_target_wins_over_the_most_wounded) {
  // a (placed first) is nearly dead, b (placed later) barely hurt: "best" heals a, "lastPlaced" heals b.
  TField best("aisp_heal_best", crew(items("mend")));
  best.hurt("a", 2);
  best.hurt("b", 14);
  CHECK(best.redTurn());
  CHECK(best.hp("a") > 2);
  CHECK_EQ(best.hp("b"), 14);
  TField last("aisp_heal_last", crew(items("mend") + ", " + beh(R"({ "staffPick": "lastPlaced" })")));
  last.hurt("a", 2);
  last.hurt("b", 14);
  CHECK(last.redTurn());
  CHECK_EQ(last.hp("a"), 2);
  CHECK_EQ(last.hp("b"), 20);
}

TEST(ai_staff_pick_last_placed_afflicting_staff_picks_a_hostile_unit_with_no_condition_that_it_can_hit) {
  // Foes e1 < e2 < e3 (placed in that order), all within reach of the sleep staff.
  const std::string foes = blue("lord", 13, 0) + ", " + blue("e1", 7, 2) + ", " + blue("e2", 6, 3) + ", " + blue("e3", 5, 2);
  auto field = [&](const char* name, const std::string& extra) { return TField(name, foes + ", " + red("h", 6, 2, items("sleepy") + (extra.empty() ? "" : ", " + extra)) + ", " + red("a", 6, 0, holdWith())); };
  TField last = field("aisp_inf_last", beh(R"({ "mode": "hold", "staffPick": "lastPlaced" })"));
  CHECK(last.loaded);
  CHECK(last.redTurn());
  CHECK(last.has("e3", "sleep") && !last.has("e1", "sleep") && !last.has("e2", "sleep"));
  // e3 already has a (different) condition: any condition counts, so e2 is next.
  TField cond = field("aisp_inf_cond", beh(R"({ "mode": "hold", "staffPick": "lastPlaced" })"));
  cond.give("e3", "curse");
  CHECK(cond.redTurn());
  CHECK(cond.has("e2", "sleep") && !cond.has("e3", "sleep") && !cond.has("e1", "sleep"));
  // The default ("best") still afflicts the unit with a condition like any other (value ties go to the lowest id), so the option is what changes it.
  TField def = field("aisp_inf_best", beh(R"({ "mode": "hold" })"));
  CHECK(def.redTurn());
  CHECK(def.uses() == 1 && !def.has("e3", "sleep"));
}

TEST(ai_staff_pick_last_placed_never_uses_a_staff_that_cannot_hit_and_staff_first_then_does_not_either) {
  const std::string foes = blue("lord", 7, 2);
  // A staff whose hit chance is 0: staffFirst with the default pick tries it (a legal target), "lastPlaced" leaves it alone.
  TField any("aisp_dud_any", foes + ", " + red("h", 6, 2, items("dud") + ", " + beh(R"({ "mode": "hold", "staffFirst": 100 })")));
  CHECK(any.loaded);
  CHECK(any.redTurn());
  CHECK_EQ(any.misses(), 1);
  TField last("aisp_dud_last", foes + ", " + red("h", 6, 2, items("dud") + ", " + beh(R"({ "mode": "hold", "staffFirst": 100, "staffPick": "lastPlaced" })")));
  CHECK(last.redTurn());
  CHECK_EQ(last.uses() + last.misses(), 0);
}

TEST(ai_staff_pick_last_placed_cure_staff_goes_to_the_last_placed_ally_but_never_the_user) {
  TField best("aisp_cure_best", crew(items("restore")));
  best.give("a", "curse");
  best.give("b", "curse");
  best.give("h", "curse");
  CHECK(best.redTurn());
  CHECK(!best.has("a", "curse") && best.has("b", "curse"));  // by worth, ties to the lowest id
  TField last("aisp_cure_last", crew(items("restore") + ", " + beh(R"({ "staffPick": "lastPlaced" })")));
  last.give("a", "curse");
  last.give("b", "curse");
  last.give("h", "curse");
  CHECK(last.redTurn());
  CHECK(last.has("a", "curse") && !last.has("b", "curse") && last.has("h", "curse"));
}

TEST(ai_inflict_only_on_casters_silences_only_a_foe_that_carries_a_staff_or_a_tome) {
  // Foes: s (a plain soldier with a sword, no staff), m (a mage with a tome).
  auto field = [&](const char* name, const std::string& foes, const std::string& extra) {
    return TField(name, blue("lord", 13, 0) + ", " + foes + ", " + red("h", 6, 2, items("hush") + (extra.empty() ? "" : ", " + extra)) + ", " + red("a", 6, 0, holdWith()));
  };
  const std::string only = R"({ "mode": "hold", "inflictOnlyOn": "casters" })";
  TField none("aiio_none", blue("lord", 13, 0) + ", " + blue("s", 7, 2) + ", " + red("h", 6, 2, items("hush") + ", " + beh(only)) + ", " + red("a", 6, 0, holdWith()));
  CHECK(none.loaded);
  CHECK(none.redTurn());
  CHECK_EQ(none.uses(), 0);  // only a swordsman in reach: nothing to silence
  TField plain("aiio_plain", blue("lord", 13, 0) + ", " + blue("s", 7, 2) + ", " + red("h", 6, 2, items("hush") + ", " + beh(R"({ "mode": "hold" })")) + ", " + red("a", 6, 0, holdWith()));
  CHECK(plain.redTurn());
  CHECK_EQ(plain.uses(), 1);  // by default it silences him anyway
  // With a soldier and a mage both in reach it silences the mage (staffFirst: the scoring's fear of being outnumbered there does not enter).
  TField mage = field("aiio_mage", blue("s", 7, 2) + ", " + blue("m", 5, 2, "mage"), beh(R"({ "mode": "hold", "staffFirst": 100, "inflictOnlyOn": "casters" })"));
  CHECK(mage.loaded);
  CHECK(mage.redTurn());
  CHECK(mage.has("m", "silence") && !mage.has("s", "silence"));
  // A soldier that carries a staff he can use counts too; one whose staff is already silenced does not (it has a condition, but a silenced unit loses nothing more).
  TField staffer("aiio_staffer", blue("lord", 13, 0) + ", " + blue("s", 7, 2, "soldier") + ", " + red("h", 6, 2, items("hush") + ", " + beh(only)) + ", " + red("a", 6, 0, holdWith()));
  staffer.unit("s").items.push_back(ItemStack{staffer.b().db->findItem("mend"), 5});
  CHECK(staffer.redTurn());
  CHECK(staffer.has("s", "silence"));
  // A staff that does not stop staves (sleep) is not narrowed by the option.
  TField sleeper("aiio_sleep", blue("lord", 13, 0) + ", " + blue("s", 7, 2) + ", " + red("h", 6, 2, items("sleepy") + ", " + beh(only)) + ", " + red("a", 6, 0, holdWith()));
  CHECK(sleeper.redTurn());
  CHECK(sleeper.has("s", "sleep"));
}

TEST(ai_heals_retreating_extends_to_a_pull_staff_a_rescue_that_serves_only_retreating_hurt_allies) {
  // a is 3 squares away (the staff's reach): pulled next to the user when it is retreating and hurt, with "heals": "retreating".
  TField f("aihp_pull", blue("lord", 13, 0) + ", " + red("h", 6, 2, items("rescue") + ", " + beh(R"({ "mode": "hold", "heals": "retreating" })")) + ", " + red("a", 9, 2, holdWith()));
  CHECK(f.loaded);
  f.hurt("a", 8, true);
  CHECK(f.redTurn());
  CHECK_EQ(f.uses(), 1);
  CHECK(std::abs(f.x("a") - f.x("h")) + std::abs(f.y("a") - f.y("h")) == 1);  // beside the user now
  // Not retreating, or not hurt: left alone (and without the option the computer ignores a pull staff at all).
  TField calm("aihp_pull_calm", blue("lord", 13, 0) + ", " + red("h", 6, 2, items("rescue") + ", " + beh(R"({ "mode": "hold", "heals": "retreating" })")) + ", " + red("a", 9, 2, holdWith()));
  calm.hurt("a", 8, false);
  CHECK(calm.redTurn());
  CHECK_EQ(calm.uses(), 0);
  TField off("aihp_pull_off", blue("lord", 13, 0) + ", " + red("h", 6, 2, items("rescue") + ", " + beh(R"({ "mode": "hold" })")) + ", " + red("a", 9, 2, holdWith()));
  off.hurt("a", 8, true);
  CHECK(off.redTurn());
  CHECK_EQ(off.uses(), 0);
  CHECK_EQ(off.x("a"), 9);
}

TEST(ai_staff_options_load_validate_save_and_round_trip) {
  Options o;
  const std::string lord = blue("lord", 0, 2) + ", ";
  CHECK_STR(SField::error("aisf_ok", lord + red("r", 8, 2, beh(R"({ "staffFirst": 40, "staffPick": "lastPlaced", "inflictOnlyOn": "casters" })")), o), "");
  CHECK_STR(SField::error("aisf_ok0", lord + red("r", 8, 2, beh(R"({ "staffFirst": 0, "staffPick": "best", "inflictOnlyOn": "anyone" })")), o), "");
  CHECK(says(SField::error("aisf_b1", lord + red("r", 8, 2, beh(R"({ "staffFirst": 101 })")), o), "\"staffFirst\" must be a whole number from 0 to 100"));
  CHECK(says(SField::error("aisf_b2", lord + red("r", 8, 2, beh(R"({ "staffFirst": -1 })")), o), "\"staffFirst\" must be a whole number from 0 to 100"));
  CHECK(says(SField::error("aisf_b3", lord + red("r", 8, 2, beh(R"({ "staffFirst": 2.5 })")), o), "\"staffFirst\" must be a whole number"));
  CHECK(says(SField::error("aisf_b4", lord + red("r", 8, 2, beh(R"({ "staffFirst": "often" })")), o), "\"staffFirst\" must be a whole number"));
  CHECK(says(SField::error("aisf_b5", lord + red("r", 8, 2, beh(R"({ "staffPick": "first" })")), o), "\"staffPick\" must be \"best\""));
  CHECK(says(SField::error("aisf_b6", lord + red("r", 8, 2, beh(R"({ "inflictOnlyOn": "mages" })")), o), "\"inflictOnlyOn\" must be \"anyone\""));
  CHECK(says(SField::error("aisf_b7", lord + red("r", 8, 2, beh(R"({ "staffpick": "best" })")), o), "staffPick"));  // a near miss names the key
  // A unit of a person's side takes none.
  CHECK(!SField::error("aisf_b8", "{ \"type\": \"soldier\", \"id\": \"lord\", \"side\": \"blue\", \"x\": 0, \"y\": 2, \"behaviour\": { \"staffFirst\": 5 } }", o).empty());
  // Saved only when set (a staffFirst of 0 is set).
  TField f("aisf_save", crew(items("mend") + ", " + beh(R"({ "staffFirst": 0, "staffPick": "lastPlaced", "inflictOnlyOn": "casters" })")));
  CHECK(f.loaded);
  const std::string text = savetest::stateText(f.b());
  CHECK(text.find("\"staffFirst\"") != std::string::npos);
  CHECK(text.find("\"staffPick\"") != std::string::npos);
  CHECK(text.find("\"inflictOnlyOn\"") != std::string::npos);
  TField plain("aisf_save_plain", crew(items("mend")));
  const std::string t2 = savetest::stateText(plain.b());
  CHECK(t2.find("staffFirst") == std::string::npos && t2.find("staffPick") == std::string::npos && t2.find("inflictOnlyOn") == std::string::npos);
  AiBehaviour back;
  std::string err;
  CHECK(parseBehaviour(behaviourToJson(f.unit("h").behaviour), f.rig.db, back, err));
  CHECK_EQ(back.staffFirst, 0);
  CHECK(back.staffLastPlaced && back.inflictCasters);
  AiBehaviour none;
  Json def;
  CHECK(Json::parse(R"({ "staffPick": "best", "inflictOnlyOn": "anyone" })", def, err));
  CHECK(parseBehaviour(def, f.rig.db, none, err));
  CHECK(none.isDefault());  // the defaults store nothing
  // A game that saves mid-battle gives back the same decisions: restore and play the same seed again.
  TField g("aisf_restore", crew(items("mend") + ", " + beh(R"({ "staffFirst": 50 })"), blue("lord", 9, 2)));
  g.hurt("a", 19);
  const std::string before = savetest::stateText(g.b());
  CHECK(g.redTurn());
  CHECK(savetest::stateText(g.b()) != before);
}
