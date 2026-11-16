// Step U2 of docs/SRPG.md, the pure part of the player's promotion UI: what a promotion would give, as text
// (battle/promotion_text.*), for the fixture game of the promotion tests. The scene, the screen and the animation are
// covered by tests/srpg_player_promotion_test.py (they need a window).
#include "battle/battle_view.h"
#include "battle/promotion_text.h"
#include "battle_fixture.h"
#include "core/battle/save.h"
#include "srpg_promo_fixture.h"

using namespace ote;
using promotest::Game;
using savetest::Rig;

namespace {

struct World {
  Game game;
  Rig rig;
  std::vector<Event> events;
  explicit World(const std::string& name) : game(name) {
    CHECK(rig.load(game.dir, "field"));
    rig.battle.start(events);
  }
  Battle& b() { return rig.battle; }
  int id(const char* story) { return b().unitByStoryId(story); }
  int type(const char* name) { return rig.db.findUnitType(name); }
};

}  // namespace

TEST(promo_text_an_offer_shows_the_base_difference_and_the_level_reset) {
  World w("promo_text_offer");
  PromotionPreview p = previewPromotion(w.b(), w.id("rec"), -1, w.type("captain"));
  CHECK_EQ(p.to, w.type("captain"));
  CHECK(p.name == std::string("Captain"));
  // recruit 10/4/2/4 -> captain 16/7/4/4: mov does not change and is left out.
  CHECK_EQ(p.changes.size(), (size_t)3);
  CHECK(p.changes[0] == std::string("+6 hp"));
  CHECK(p.changes[1] == std::string("+3 atk"));
  CHECK(p.changes[2] == std::string("+2 def"));
  CHECK(p.level == std::string("Back to level 1"));  // rec is level 3 and the game's default is "reset"
  CHECK(p.cannotWield.empty());                          // a captain wields blades like a recruit
}

TEST(promo_text_listed_gains_the_level_override_and_what_can_no_longer_be_wielded) {
  World w("promo_text_gains");
  PromotionPreview p = previewPromotion(w.b(), w.id("rec"), 1, w.type("scout"));  // slot 1: the writ
  CHECK_EQ(p.changes.size(), (size_t)2);  // the entry's own gains, not the base difference
  CHECK(p.changes[0] == std::string("+3 hp"));
  CHECK(p.changes[1] == std::string("+1 mov"));
  CHECK(p.level.empty());                  // "levelAfter": "keep" on this entry
  CHECK_EQ(p.cannotWield.size(), (size_t)1);
  CHECK(p.cannotWield[0] == std::string("Blade"));  // the bow it carries it could not wield as a recruit either
}

TEST(promo_text_a_pair_without_an_entry_uses_the_base_difference) {
  World w("promo_text_free");
  PromotionPreview p = previewPromotion(w.b(), w.id("cad"), -1, w.type("recruit"));  // (a story may do this)
  CHECK(joinChanges(p.changes) == std::string("+2 hp, +1 atk, +1 def, +1 mov"));
  CHECK(p.level.empty());  // cad is level 1 already
  PromotionPreview back = previewPromotion(w.b(), w.id("rec"), -1, w.type("recruit"));
  CHECK_EQ(back.to, w.type("recruit"));
  CHECK(back.changes.empty());
  CHECK(joinChanges(back.changes) == std::string("No stat changes"));
}

TEST(promo_text_the_question_and_the_banner) {
  World w("promo_text_words");
  PromotionPreview p = previewPromotion(w.b(), w.id("rec"), 1, w.type("scout"));  // slot 1: the writ
  CHECK(promotionQuestion(w.b(), w.id("rec"), p) == std::string("Promote Recruit to Scout?\n+3 hp, +1 mov"));
  CHECK(promotionBannerText("Kell", "Captain") == std::string("Kell is now a Captain"));
  CHECK(promotionBannerText("Lyra", "Archer") == std::string("Lyra is now an Archer"));
  CHECK(promotionBannerText("Lyra", "owl") == std::string("Lyra is now an owl"));
}

TEST(promo_text_bad_arguments_give_an_empty_preview) {
  World w("promo_text_bad");
  CHECK_EQ(previewPromotion(w.b(), -1, -1, 0).to, -1);
  CHECK_EQ(previewPromotion(w.b(), w.id("rec"), -1, 999).to, -1);
}

TEST(promo_view_shows_the_old_type_then_flashes_swaps_banners_and_ends_with_the_box) {
  fixture::World w({"...."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 3, 0, false}});
  CHECK(w.begin());
  BattleView view;
  view.init(w.battle, 24, true);  // autoplay: the stat box goes by itself
  Event e;
  e.type = EventType::UnitPromoted;
  e.a = 0;
  e.b = 0;  // the old type, drawn until the swap (the rules already hold the new one)
  e.c = 0;
  e.d = 1;
  e.data.assign(w.battle.units[0].stats.size(), 0);
  e.data[0] = 3;
  view.push({e});
  CHECK_EQ(view.units[0].type, 0);  // marked at once, before the entry plays
  view.update(0.0, w.battle);
  CHECK(view.busy());
  view.update(0.1, w.battle);
  CHECK(view.units[0].flash > 0 && view.units[0].type == 0);  // flashing, still the old picture
  view.update(0.25, w.battle);
  CHECK_EQ(view.units[0].type, -1);  // past the swap: the unit's own (new) type
  CHECK(view.banner.empty());
  view.update(0.25, w.battle);       // t = 0.6: the banner
  CHECK(view.banner.find(" is now a") != std::string::npos);
  CHECK(!view.levelUp.active);
  view.update(1.0, w.battle);        // t = 1.6: the stat box with the change
  CHECK(view.banner.empty() && view.levelUp.active);
  CHECK_EQ(view.levelUp.lines.size(), (size_t)1);
  view.update(1.3, w.battle);        // autoplay: it closes by itself
  CHECK(!view.busy() && !view.levelUp.active);
  CHECK_EQ(view.units[0].type, -1);
}

TEST(promo_view_skip_finishes_the_entry_and_a_bad_event_takes_no_time) {
  fixture::World w({"...."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 3, 0, false}});
  CHECK(w.begin());
  BattleView view;
  view.init(w.battle, 24, false);  // not autoplay: the box waits for Confirm
  Event e;
  e.type = EventType::UnitPromoted;
  e.a = 0;
  e.b = 0;
  e.c = 0;
  e.d = 1;
  view.push({e});
  view.update(0.0, w.battle);
  view.update(2.0, w.battle);
  CHECK(view.busy() && view.levelUp.active);  // waiting for the player
  CHECK(view.levelUp.lines.size() == 1);      // (no data: "No stat changes")
  view.skip(w.battle);
  view.update(0.0, w.battle);
  CHECK(!view.busy() && !view.levelUp.active && view.banner.empty() && view.units[0].type == -1);
  e.c = 999;  // not a unit type: nothing to show
  view.push({e});
  view.update(0.0, w.battle);
  CHECK(!view.busy());
}
