// Step K2 of docs/SRPG.md "Promotion" with real skills (P1 could not test this before skills existed): a promotion
// keeps what the unit learned and teaches, through learnUpTo, the new type's `learns` entries at or below the level
// the unit has after the promotion, without events.
#include "srpg_promo_fixture.h"

using namespace ote;
using promotest::Game;

namespace {

std::string always(const char* id, const char* stat) {
  return std::string("{ \"id\": \"") + id + "\", \"blocks\": [ { \"when\": \"always\", \"do\": [ { \"stats\": { \"" + stat +
         "\": 1 } } ] } ] }";
}

struct PromoWorld {
  Game game;
  savetest::Rig rig;
  std::vector<Event> events;
  PromoWorld() : game("promo_skills") {
    game.put("data/skills.json", "{ \"skills\": [" + always("tough", "def") + "," + always("keen", "atk") + "," + always("calm", "def") +
                                     "," + always("old", "mov") + "] }");
    // The recruit learns "tough" at 2; the captain and the scout learn at 2, 3 (and the scout at 5).
    game.swap("data/units.json", "\"id\":\"recruit\",\"name\":\"Recruit\",",
              "\"id\":\"recruit\",\"name\":\"Recruit\",\"learns\":[{\"level\":2,\"skill\":\"tough\"}],");
    game.swap("data/units.json", "\"id\":\"captain\",\"name\":\"Captain\",",
              "\"id\":\"captain\",\"name\":\"Captain\",\"learns\":[{\"level\":3,\"skill\":\"calm\"},{\"level\":2,\"skill\":\"keen\"}],");
    game.swap("data/units.json", "\"id\":\"scout\",\"name\":\"Scout\",",
              "\"id\":\"scout\",\"name\":\"Scout\",\"learns\":[{\"level\":5,\"skill\":\"old\"},{\"level\":3,\"skill\":\"calm\"},"
              "{\"level\":2,\"skill\":\"keen\"}],");
    CHECK(rig.load(game.dir, "field"));
    rig.battle.start(events);
  }
  Battle& b() { return rig.battle; }
  Unit& rec() { return b().units[(size_t)b().unitByStoryId("rec")]; }
  std::string learned() {
    std::string out;
    for (int s : rec().learned) out += (out.empty() ? "" : " ") + rig.db.skills[(size_t)s].id;
    return out;
  }
  bool promote(const char* to) {
    ItemStack writ;
    writ.item = rig.db.findItem("writ");
    rec().items.push_back(writ);
    Action a = Action::useItem(rec().id, (int)rec().items.size() - 1, rec().id);
    a.unitType = rig.db.findUnitType(to);
    events.clear();
    return b().apply(a, events);
  }
};

}  // namespace

TEST(skills_promotion_teaches_the_new_types_entries_at_or_below_the_new_level) {
  PromoWorld w;
  CHECK_STR(w.learned(), "tough");  // level 3 at the start: the recruit's own entry
  // The scout entry keeps the level (3): keen (2) and calm (3) are learned, "old" (5) is not; tough stays.
  CHECK(w.promote("scout"));
  CHECK_EQ(w.rec().level, 3);
  CHECK_STR(w.learned(), "tough keen calm");
  for (const Event& e : w.events) CHECK(e.type != EventType::SkillLearned);  // silent, like a unit being made
  CHECK_EQ(w.b().stat(w.rec(), w.rig.db.findStat("atk")), 4 + 1);  // the new skills count at once
}

TEST(skills_promotion_that_resets_the_level_teaches_only_what_level_1_has) {
  PromoWorld w;
  CHECK(w.promote("captain"));  // "reset": level 1 again
  CHECK_EQ(w.rec().level, 1);
  CHECK_STR(w.learned(), "tough");  // keen needs level 2 and calm 3; what was learned before is kept
}
