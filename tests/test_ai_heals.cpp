// Computer opponent: "heals": "retreating" (docs/AI.md "Healers that serve retreating allies"). Rules on the generated game of
// ai_scripted_fixture.h plus two staves written here: mend (heals an ally in reach 2 by 8) and rest (heals its user by 8).
#include "ai_scripted_fixture.h"

using namespace ote;
using namespace scripted;

namespace {

const char* kHealer = R"("items": ["mend"])";
const char* kRetreating = R"({ "heals": "retreating" })";

// The scripted fixture with the two staves in items.json. Blue "lord" never acts (the tests end its turn).
struct HField {
  savetest::Rig rig;
  std::vector<Event> events;
  bool loaded = false;

  HField(const std::string& name, const std::string& units) {
    const std::string dir = SField::write(name, units, Options());
    writeTextFile(dir + "/data/items.json", R"({ "items": [
      { "id": "potion", "name": "Potion", "kind": "usable", "uses": 2, "heal": 10 },
      { "id": "mend", "name": "Mend", "kind": "staff", "group": "staff", "uses": 20, "range": 2, "target": "ally", "effects": [ { "heal": 8 } ] },
      { "id": "rest", "name": "Rest", "kind": "staff", "group": "staff", "uses": 20, "target": "self", "effects": [ { "heal": 8 } ] } ] })");
    // The soldiers may use staves (a unit type's "wields").
    std::string types = unitsJson();
    types.replace(types.find("\"weapons\": [\"sword\"] }"), 22, "\"weapons\": [\"sword\"], \"wields\": [\"staff\"] }");
    writeTextFile(dir + "/data/units.json", types);
    loaded = rig.load(dir, "field");
    if (loaded) rig.battle.start(events);
  }
  Battle& b() { return rig.battle; }
  Unit& unit(const char* story) { return b().units[(size_t)b().unitByStoryId(story)]; }
  int hp(const char* who) { return unit(who).hp; }
  int x(const char* who) { return b().grid.x(unit(who).cell); }
  int y(const char* who) { return b().grid.y(unit(who).cell); }
  // Hurts `who` to `hp`, in the retreat state or not (the flag is what a unit with a "retreat" block keeps; the staff rule reads only it).
  void hurt(const char* who, int hp, bool retreating) {
    unit(who).hp = hp;
    unit(who).retreating = retreating;
  }
  // Blue ends its turn, then red plays its whole turn.
  bool redTurn() {
    events.clear();
    if (!b().apply(Action::endTurn(), events)) return false;
    for (int i = 0; i < 80 && !b().over && b().activeSide == 1; ++i) aiTakeStep(b(), events);
    return b().over || b().activeSide == 0;
  }
  int staffUses() const {
    int n = 0;
    for (const Event& e : events) n += e.type == EventType::StaffUsed;
    return n;
  }
};

// A healer h at 10,2 (staff reach 2) with three red allies around it: a (10,0), b (10,4), c (9,2). Ids: lord 0, h 1, a 2, b 3, c 4.
std::string team(const std::string& healerExtra) {
  return blue("lord", 0, 2) + ", " + red("h", 10, 2, healerExtra) + ", " + red("a", 10, 0) + ", " + red("b", 10, 4) + ", " + red("c", 9, 2);
}

}  // namespace

TEST(ai_heals_default_heals_any_hurt_ally_and_the_key_anyone_is_the_same) {
  HField f("aih_default", team(kHealer));
  CHECK(f.loaded);
  f.hurt("a", 6, false);  // not retreating: today's healer still helps it
  CHECK(f.redTurn());
  CHECK(f.hp("a") > 6);
  CHECK_EQ(f.staffUses(), 1);
  HField g("aih_anyone", team(std::string(kHealer) + R"(, "behaviour": { "heals": "anyone" })"));
  CHECK(g.loaded);
  CHECK(g.unit("h").behaviour.isDefault());  // "anyone" is the default: nothing is stored
  g.hurt("a", 6, false);
  CHECK(g.redTurn());
  CHECK(g.hp("a") > 6);
}

TEST(ai_heals_retreating_heals_only_a_retreating_hurt_ally) {
  HField f("aih_only", team(std::string(kHealer) + ", " + beh(kRetreating)));
  CHECK(f.loaded);
  f.hurt("a", 6, false);  // hurt but not retreating: left alone
  f.hurt("b", 6, true);   // retreating and hurt: healed
  CHECK(f.redTurn());
  CHECK_EQ(f.hp("a"), 6);
  CHECK(f.hp("b") > 6);
  CHECK_EQ(f.staffUses(), 1);
  // A retreating ally at full HP is not a target either.
  HField g("aih_full", team(std::string(kHealer) + ", " + beh(kRetreating)));
  g.hurt("a", 20, true);
  CHECK(g.redTurn());
  CHECK_EQ(g.staffUses(), 0);
}

TEST(ai_heals_retreating_never_uses_a_healing_staff_on_the_healer_itself) {
  const std::string selfStaff = R"("items": ["rest"])";
  HField f("aih_self", team(selfStaff + ", " + beh(kRetreating)));
  CHECK(f.loaded);
  f.hurt("h", 6, true);  // the healer is hurt and in the retreat state itself: a staff that only heals its user is not used
  CHECK(f.redTurn());
  CHECK_EQ(f.staffUses(), 0);
  CHECK_EQ(f.hp("h"), 6);
  // The same staff with the default behaviour is used (so the check above is about the option).
  HField g("aih_self_default", team(selfStaff));
  g.hurt("h", 6, true);
  CHECK(g.redTurn());
  CHECK_EQ(g.staffUses(), 1);
  CHECK(g.hp("h") > 6);
}

TEST(ai_heals_retreating_the_unit_placed_last_wins_not_the_most_wounded) {
  HField f("aih_last", team(std::string(kHealer) + ", " + beh(kRetreating)));
  CHECK(f.loaded);
  f.hurt("a", 2, true);    // placed first, nearly dead
  f.hurt("b", 14, true);   // placed later, barely hurt: it wins
  f.hurt("c", 12, false);  // the highest id, but not retreating: never a candidate
  CHECK(f.redTurn());
  CHECK_EQ(f.hp("a"), 2);
  CHECK_EQ(f.hp("b"), 20);  // 14 + 8, capped at the 20 maximum
  CHECK_EQ(f.hp("c"), 12);
  // The default healer picks by worth instead: the nearly dead one.
  HField g("aih_last_default", team(kHealer));
  g.hurt("a", 2, true);
  g.hurt("b", 14, true);
  CHECK(g.redTurn());
  CHECK(g.hp("a") > 2);
  CHECK_EQ(g.hp("b"), 14);
}

TEST(ai_heals_retreating_a_last_unit_out_of_reach_is_not_a_candidate) {
  // d is placed last and retreating, but at 13,4 it is beyond anything the healer (move 4, reach 2) can get to; the blue lord is far too.
  HField f("aih_reach", blue("lord", 13, 0) + ", " + red("h", 6, 2, std::string(kHealer) + ", " + beh(kRetreating)) + ", " + red("a", 6, 0) + ", " + red("d", 13, 4));
  CHECK(f.loaded);
  f.hurt("a", 6, true);
  f.hurt("d", 6, true);
  CHECK(f.redTurn());
  CHECK(f.hp("a") > 6);
  CHECK_EQ(f.hp("d"), 6);
}

TEST(ai_heals_retreating_with_nobody_to_heal_the_unit_does_what_it_would_otherwise) {
  // Nobody is retreating (one ally is merely hurt): the unit acts exactly as a plain healer does on a map where nobody needs healing.
  const std::string line = blue("lord", 3, 2) + ", " + red("a", 10, 0) + ", ";
  HField f("aih_none", line + red("h", 6, 2, std::string(kHealer) + ", " + beh(kRetreating)));
  HField g("aih_none_plain", line + red("h", 6, 2, kHealer));
  CHECK(f.loaded && g.loaded);
  f.hurt("a", 6, false);
  g.hurt("a", 20, false);
  CHECK(f.redTurn());
  CHECK(g.redTurn());
  CHECK_EQ(f.staffUses(), 0);
  CHECK_EQ(f.hp("a"), 6);
  CHECK_EQ(f.x("h"), g.x("h"));
  CHECK_EQ(f.y("h"), g.y("h"));
  CHECK_EQ(f.hp("lord"), g.hp("lord"));  // it attacked the lord the same way
  CHECK(f.hp("lord") < 20);
}

TEST(ai_heals_loads_validates_and_round_trips) {
  Options o;
  CHECK_STR(SField::error("aih_ok", blue("lord", 0, 2) + ", " + red("r", 8, 2, beh(R"({ "heals": "retreating" })")), o), "");
  CHECK(says(SField::error("aih_bad1", blue("lord", 0, 2) + ", " + red("r", 8, 2, beh(R"({ "heals": "everyone" })")), o), "\"heals\" must be \"anyone\""));
  CHECK(says(SField::error("aih_bad2", blue("lord", 0, 2) + ", " + red("r", 8, 2, beh(R"({ "heals": true })")), o), "\"heals\" must be \"anyone\""));
  CHECK(says(SField::error("aih_bad3", blue("lord", 0, 2) + ", " + red("r", 8, 2, beh(R"({ "speed": 1 })")), o), "ignores, heals"));
  // Saved only when set.
  HField f("aih_save", team(std::string(kHealer) + ", " + beh(kRetreating)));
  CHECK(f.loaded);
  CHECK(savetest::stateText(f.b()).find("\"heals\"") != std::string::npos);
  HField plain("aih_save_plain", team(kHealer));
  CHECK(savetest::stateText(plain.b()).find("heals") == std::string::npos);
  AiBehaviour back;
  std::string err;
  CHECK(parseBehaviour(behaviourToJson(f.unit("h").behaviour), f.rig.db, back, err));
  CHECK(back.healsRetreating);
}
