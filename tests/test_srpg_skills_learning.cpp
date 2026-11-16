// Step K1 of docs/SRPG.md "Skills", in a battle: growth bonuses, and learning (creation, level up, roster, story) with
// the story's teachSkill / forgetSkill / hasSkill.
#include <memory>

#include "core/battle/carried.h"
#include "srpg_skills_fixture.h"

using namespace ote;
using skilltest::Copy;
using skilltest::World;

TEST(skills_growth_bonuses_add_to_the_type_and_the_character) {
  World w;
  const int atk = w.rig.db.findStat("atk"), skl = w.rig.db.findStat("skl"), hp = w.rig.db.findStat("hp");
  const Unit& e = w.unit("eirik");  // hero growths hp 100, atk 40; gifted (learned at 3) +10 atk, +20 skl
  CHECK_EQ(w.b().growthOf(e, atk), 50);
  CHECK_EQ(w.b().growthOf(e, skl), 20);
  CHECK_EQ(w.b().growthOf(e, hp), 100);
  CHECK_EQ(w.b().growthOf(w.unit("lyra"), atk), 40);  // no skill, no bonus
  CHECK_EQ(w.b().growthOf(w.unit("lyra"), skl), 0);
  // A negative total counts as 0.
  Copy c("skills_growth_negative");
  c.skills(R"({ "id": "slump", "blocks": [ { "when": "always", "do": [ { "growths": { "atk": -100 } } ] } ] })");
  c.swap("data/characters.json", "{ \"id\": \"gorm\", \"name\": \"Gorm\", \"type\": \"grunt\" }",
         "{ \"id\": \"gorm\", \"name\": \"Gorm\", \"type\": \"grunt\", \"skills\": [\"slump\"] }");
  World v("field", c.dir);
  CHECK_EQ(v.b().growthOf(v.unit("gorm"), v.rig.db.findStat("atk")), 0);
  CHECK_EQ(v.b().growthOf(v.unit("gorm"), v.rig.db.findStat("hp")), 50);
}

TEST(skills_learning_at_creation_level_up_and_through_a_roster) {
  // Level up: lyra (level 1) earns 100 xp per hit and learns fast_learner at level 2, light_step at 3.
  Copy c("skills_learning");
  c.swap("game.json", "\"xpHit\": \"30\"", "\"xpHit\": \"100\"");
  c.swap("data/units.json", "\"skills\": [\"steady_recovery\"]", "\"skills\": []");
  World w("field", c.dir);
  CHECK_STR(w.learnedIds(w.unit("lyra")), "");
  CHECK_STR(w.learnedIds(w.unit("eirik")), "fast_learner light_step gifted");
  // Eirik (level 3) hits gorm: level 4 earns heavy_blow (his character's), once, after the LevelUp event.
  CHECK(w.apply(Action::move(w.id("eirik"), w.at(4, 0))));
  CHECK(w.apply(Action::attack(w.id("eirik"), w.id("gorm"), w.db().findWeapon("sword"))));
  CHECK_EQ(w.count(EventType::LevelUp), 1);
  CHECK_EQ(w.count(EventType::SkillLearned), 1);
  size_t up = 0, learned = 0;
  for (size_t i = 0; i < w.events.size(); ++i) {
    if (w.events[i].type == EventType::LevelUp) up = i;
    if (w.events[i].type == EventType::SkillLearned) {
      learned = i;
      CHECK_EQ(w.events[i].a, w.id("eirik"));
      CHECK_EQ(w.events[i].b, w.skill("heavy_blow"));
    }
  }
  CHECK(learned > up);
  CHECK_STR(w.learnedIds(w.unit("eirik")), "fast_learner light_step gifted heavy_blow");

  // A roster: what it carries, then what the level adds (lyra at level 3 gets fast_learner and light_step).
  World r("field", c.dir, false);
  RosterUnit ru;
  ru.id = "lyra";
  ru.type = r.db().findUnitType("hero");
  ru.name = "Lyra";
  ru.level = 3;
  ru.stats = {20, 6, 4, 3, 4};
  ru.skills = {r.skill("tough")};
  auto roster = std::make_shared<Roster>();
  roster->push_back(ru);
  r.b().setStartingRoster(roster);
  std::vector<Event> ev;
  r.b().start(ev);
  CHECK_STR(r.learnedIds(r.unit("lyra")), "tough fast_learner light_step");
  CHECK_EQ(r.b().maxHp(r.unit("lyra")), 25);  // tough counted before the hit points were set
  CHECK_EQ(r.unit("lyra").hp, 25);
}

TEST(skills_story_teach_forget_and_has_skill) {
  World w("story");  // lyra is taught heavy_blow when the battle starts; the events then read it back
  Unit& l = w.unit("lyra");
  CHECK_STR(w.learnedIds(l), "heavy_blow");
  CHECK_EQ(w.count(EventType::SkillLearned), 1);
  const int hasHeavy = w.db().findSwitch("hasHeavy"), both = w.db().findSwitch("bothHave");
  CHECK_EQ(w.b().storyState.switches[(size_t)hasHeavy], 1);
  CHECK_EQ(w.b().storyState.switches[(size_t)both], 0);  // nobody here has "bold"

  Copy c("skills_story_forget");
  const std::string head = R"({ "name": "Story", "grid": { "type": "square", "width": 6, "height": 3 }, "legend": { ".": "plain" },
    "tiles": ["......", "......", "......"], "sides": [ { "id": "blue", "controller": "human" }, { "id": "red", "controller": "ai" } ],
    "units": [ { "character": "lyra", "side": "blue", "x": 0, "y": 0 }, { "character": "gorm", "side": "red", "x": 5, "y": 2 } ],
    "events": )";
  // Taught twice = learned once and one event; forgotten again; a skill from another source stays.
  c.put("maps/story.json", head + R"([
    { "id": "a", "when": { "battle": "start" }, "do": [ { "teachSkill": "tough", "to": "lyra" }, { "teachSkill": "tough", "to": { "side": "blue" } },
        { "teachSkill": "bold", "to": { "team": "red" } } ] },
    { "id": "b", "when": { "battle": "start" }, "do": [ { "forgetSkill": "steady_recovery", "from": "lyra" }, { "forgetSkill": "bold", "from": {} } ] },
    { "id": "c", "when": { "battle": "start" }, "if": [ { "hasSkill": "steady_recovery", "unit": "lyra" } ], "do": [ { "setSwitch": "hasHeavy", "to": true } ] },
    { "id": "d", "when": { "battle": "start" }, "if": [ { "hasSkill": "bold" } ], "do": [ { "setSwitch": "bothHave", "to": true } ] } ] })");
  World v("story", c.dir);
  CHECK_STR(v.learnedIds(v.unit("lyra")), "tough");
  CHECK_EQ(v.count(EventType::SkillLearned), 2);  // lyra's tough once, gorm's bold; the second tough taught nothing
  CHECK_STR(v.learnedIds(v.unit("gorm")), "");    // bold was forgotten again
  CHECK_EQ(v.b().storyState.switches[(size_t)hasHeavy], 1);  // the type's skill stays after forgetSkill
  CHECK_EQ(v.b().storyState.switches[(size_t)both], 0);
  // A taught skill that raises the maximum raises the hit points too; forgetting it cuts them back.
  CHECK_EQ(v.b().maxHp(v.unit("lyra")), 25);
  CHECK_EQ(v.unit("lyra").hp, 25);
}
