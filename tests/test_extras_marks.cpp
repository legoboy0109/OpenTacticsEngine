// A link between two strike blocks (docs/SRPG.md "Linking two strike blocks"): `{ "mark": "name" }` in a strike / struck block and
// `{ "marked": "name" }` (with "is": false) in the same unit's other blocks of the same fight.
// Fixture tests/fixtures/srpg-skillblocks2 (carried iron swords, 100 % hit, one strike): "thief" (blue rogue, str 5, def 2, 0,1) attacking "mark"
// (red guard, def 4, hp 30, 1,1) with an iron sword (might 3) does 5 + 3 - 4 = 4.
#include <functional>
#include <memory>

#include "srpg_skills_fixture.h"

using namespace ote;
using skilltest::Copy;
using skilltest::World;
using skilltest::saysIn;

namespace {

const char* kFix = "srpg-skillblocks2";

// The fixture's own conditions (its abilities cure some of them) plus these.
std::string conditions(const std::string& list) {
  std::string base;
  readTextFile(ote_test::sourceDir() + "/tests/fixtures/" + kFix + "/data/conditions.json", base);
  base = base.substr(base.find('[') + 1, base.rfind(']') - base.find('[') - 1);
  return "{ \"conditions\": [" + base + "," + list + "] }";
}

// A fading condition: `ward` (4 turns) gives +1 defence for each turn it has left; `might` gives +1 strength per turn left; `torch` gives
// +1 vision per turn left; `bane` does not fade (-2 strength).
const char* kFading = R"(
  { "id": "ward", "name": "Ward", "tag": "Wd", "turns": 4, "scale": "turnsLeft", "blocks": [ { "when": "always", "do": [ { "stats": { "def": 1 } } ] } ] },
  { "id": "might", "name": "Might", "tag": "Mi", "turns": 4, "scale": "turnsLeft", "blocks": [ { "when": "always", "do": [ { "stats": { "str": 1 } } ] } ] },
  { "id": "torch", "name": "Torch", "tag": "To", "turns": 4, "scale": "turnsLeft", "blocks": [ { "when": "always", "do": [ { "vision": 1 } ] } ] },
  { "id": "bane", "name": "Bane", "tag": "Ba", "turns": 4, "blocks": [ { "when": "always", "do": [ { "stats": { "str": -2 } } ] } ] })";

struct Game {
  Copy c;
  std::unique_ptr<World> w;
  Game(const char* name, const std::string& skills, const std::vector<std::pair<const char*, const char*>>& learn,
       const std::function<void(Copy&)>& tweak = nullptr)
      : c(name, kFix) {
    c.skills(skills);
    c.put("data/conditions.json", conditions(kFading));
    if (tweak) tweak(c);
    w.reset(new World("field", c.dir));
    for (const auto& l : learn) w->unit(l.first).learned.push_back(w->skill(l.second));
  }
  void give(const char* unit, const char* cond, int left) {
    ConditionState cs;
    cs.condition = w->db().findCondition(cond);
    cs.left = left;
    w->unit(unit).conditions.push_back(cs);
  }
  int weapon(const char* unit) { return w->b().restingWeapon(w->unit(unit)); }
  Forecast forecast(const char* from, const char* to) {
    Forecast f;
    CHECK(w->b().forecast(w->id(from), w->id(to), weapon(from), w->unit(from).cell, f));
    return f;
  }
  std::vector<int> strikes(const char* from) {
    std::vector<int> out;
    for (const Event& e : w->events)
      if (e.type == EventType::Attack && e.a == w->id(from)) out.push_back(e.d);
    return out;
  }
  bool fight(const char* from, const char* to) { return w->apply(Action::attack(w->id(from), w->id(to), weapon(from))); }
  void twoRounds() { CHECK(w->apply(Action::endTurn()) && w->apply(Action::endTurn())); }  // blue, red: blue's next turn starts
};

void twoStrikes(Copy& c) { c.swap("game.json", "\"strikes\": \"1\"", "\"strikes\": \"2\""); }

std::string skill(const std::string& id, const std::string& blocks) { return "{ \"id\": \"" + id + "\", \"blocks\": [" + blocks + "] }"; }

}  // namespace

// ---- a link between two strike blocks ----

// Block 1: the first strike, `chance`; it does +3 damage and marks. Block 2: the second strike, only when marked, no chance: +5 damage.
std::string linked(const char* chance, const char* extraBlocks = "") {
  return skill("link",
               std::string("{ \"when\": \"strike\", \"if\": [ { \"strike\": \"first\" } ], \"chance\": ") + chance +
                   ", \"do\": [ { \"damage\": 3 }, { \"mark\": \"hit\" } ] },"
                   "{ \"when\": \"strike\", \"if\": [ { \"strike\": 2 }, { \"marked\": \"hit\" } ], \"do\": [ { \"damage\": 5 } ] }" + extraBlocks);
}

TEST(marks_one_roll_activates_both_strikes) {
  Game g("ex_mark_both", linked("100"), {{"thief", "link"}}, twoStrikes);
  CHECK(g.fight("thief", "mark"));
  const std::vector<int> s = g.strikes("thief");
  CHECK_EQ(s.size(), (size_t)2);
  if (s.size() == 2) {
    CHECK_EQ(s[0], 4 + 3);  // block 1
    CHECK_EQ(s[1], 4 + 5);  // block 2, because block 1 marked
  }
}
TEST(marks_a_failed_roll_gives_neither_and_the_mark_does_not_outlive_the_fight) {
  Game g("ex_mark_none", linked("0"), {{"thief", "link"}}, twoStrikes);
  CHECK(g.fight("thief", "mark"));
  const std::vector<int> s = g.strikes("thief");
  CHECK_EQ(s.size(), (size_t)2);
  if (s.size() == 2) CHECK(s[0] == 4 && s[1] == 4);
  // A mark is gone when the fight is over: the roll fires in round 1 and misses in round 2 (its chance is a formula of the turn), and the second
  // fight's second strike gets nothing although the first fight marked.
  Game h("ex_mark_gone", linked("\"turn == 1 and 100 or 0\""), {{"thief", "link"}}, twoStrikes);
  CHECK(h.fight("thief", "mark"));
  CHECK_EQ(h.strikes("thief")[1], 4 + 5);
  h.twoRounds();
  CHECK(h.fight("thief", "mark"));
  CHECK_EQ(h.strikes("thief")[0], 4);
  CHECK_EQ(h.strikes("thief")[1], 4);  // not marked in this fight
}
TEST(marks_is_false_and_afterCombat_blocks_read_marks_too) {
  const std::string extra =
      ", { \"when\": \"strike\", \"if\": [ { \"strike\": 2 }, { \"marked\": \"hit\", \"is\": false } ], \"do\": [ { \"damage\": 1 } ] },"
      "{ \"when\": \"afterCombat\", \"if\": [ { \"marked\": \"hit\" } ], \"do\": [ { \"hurt\": 2 } ] }";
  Game a("ex_mark_f1", linked("100", extra.c_str()), {{"thief", "link"}}, twoStrikes);
  CHECK(a.fight("thief", "mark"));
  CHECK_EQ(a.strikes("thief")[1], 4 + 5);                 // marked: the `is false` block does not fire
  CHECK_EQ(a.w->unit("mark").hp, 30 - 7 - 9 - 2);         // the afterCombat block fired: 2 more
  Game b("ex_mark_f2", linked("0", extra.c_str()), {{"thief", "link"}}, twoStrikes);
  CHECK(b.fight("thief", "mark"));
  CHECK_EQ(b.strikes("thief")[1], 4 + 1);                 // not marked: the `is false` block fires
  CHECK_EQ(b.w->unit("mark").hp, 30 - 4 - 5);             // and no afterCombat damage
}
TEST(marks_the_forecast_shows_the_chance_note_and_not_the_linked_block) {
  Game g("ex_mark_fc", linked("40"), {{"thief", "link"}}, twoStrikes);
  const Forecast f = g.forecast("thief", "mark");
  CHECK_EQ(f.attack.damage, 4);  // a chance effect is not in the numbers; the linked block depends on it, so it is not either
  bool note = false;
  for (const ForecastNote& n : f.notes) note = note || (n.unit == g.w->id("thief") && n.chance == 40);
  CHECK(note);
  for (const StrikeNumbers& n : f.attack.each) CHECK_EQ(n.damage, 4);
}
TEST(marks_belong_to_the_owner_and_a_struck_block_can_set_one) {
  // The mark guard's struck block sets "hurt" when it is struck; its own strike (the counter) reads it: only the owner's marks count.
  const std::string guardSkill = skill("guardlink",
      "{ \"when\": \"struck\", \"do\": [ { \"mark\": \"hurt\" } ] },"
      "{ \"when\": \"strike\", \"if\": [ { \"marked\": \"hurt\" } ], \"do\": [ { \"damage\": 10 } ] }");
  Game g("ex_mark_owner", guardSkill, {{"mark", "guardlink"}});
  CHECK(g.fight("thief", "mark"));
  int counter = -1;
  for (const Event& e : g.w->events)
    if (e.type == EventType::Attack && e.a == g.w->id("mark")) counter = e.d;
  CHECK_EQ(counter, 5 + 3 - 2 + 10);  // the guard's counter: str 5 + iron sword 3 - the thief's def 2, plus the 10 its mark gave
  Game h("ex_mark_owner2", guardSkill, {{"thief", "guardlink"}});
  CHECK(h.fight("thief", "mark"));
  CHECK_EQ(h.strikes("thief")[0], 4);  // the thief owns the skill, but nothing struck the thief yet: no mark
}
TEST(marks_loader_refusals) {
  auto err = [](const char* name, const std::string& skills) {
    Copy c(name, kFix);
    c.skills(skills);
    return c.gameError();
  };
  CHECK(saysIn(err("ex_mn1", skill("x", R"({ "when": "combat", "if": [ { "marked": "a" } ], "do": [ { "hit": 5 } ] })")),
               "\"marked\" can only be asked in \"strike\", \"struck\", \"defeat\" and \"afterCombat\" blocks"));
  CHECK(saysIn(err("ex_mn2", skill("x", R"({ "when": "combat", "do": [ { "mark": "a" } ] })")), "\"mark\" cannot be used in a \"combat\" block (it belongs to: strike, struck)"));
  CHECK(saysIn(err("ex_mn3", skill("x", R"({ "when": "strike", "do": [ { "mark": 3 } ] })")), "\"mark\" must be the name of a mark"));
  CHECK(saysIn(err("ex_mn4", skill("x", R"({ "when": "strike", "do": [ { "mark": "" } ] })")), "\"mark\" must be the name of a mark"));
  CHECK(saysIn(err("ex_mn5", skill("x", R"({ "when": "strike", "if": [ { "marked": "" } ], "do": [ { "hit": 1 } ] })")), "\"marked\" must be the name of a mark"));
  CHECK(saysIn(err("ex_mn6", skill("x", R"({ "when": "strike", "if": [ { "marked": "a", "is": "no" } ], "do": [ { "hit": 1 } ] })")), "\"is\""));
  CHECK(saysIn(err("ex_mn7", skill("x", R"({ "when": "turnStart", "if": [ { "marked": "a" } ], "do": [ { "heal": 1 } ] })")), "\"marked\" can only be asked"));
  CHECK_STR(err("ex_mn8", skill("x", R"({ "when": "strike", "if": [ { "marked": "a", "is": false } ], "do": [ { "mark": "b" } ] })")), "");
  CHECK_STR(err("ex_mn9", skill("x", R"({ "when": "defeat", "if": [ { "marked": "a" } ], "do": [ { "heal": 1 } ] })")), "");
}
