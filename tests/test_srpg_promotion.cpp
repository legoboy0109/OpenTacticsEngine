// Step P1 of docs/SRPG.md "Promotion", in a battle: each way to promote (item, offer, automatic, story), the choice
// between options, what a promotion changes, level reset / keep and its override, refusals, saves, the roster.
#include <algorithm>
#include <functional>

#include "core/battle/carried.h"
#include "core/battle/save.h"
#include "srpg_promo_fixture.h"

using namespace ote;
using promotest::Game;
using savetest::Rig;

namespace {

// The fixture game loaded into a battle (blue's first turn is under way).
struct World {
  Game game;
  Rig rig;
  std::vector<Event> events;
  explicit World(const std::string& name, const std::function<void(Game&)>& tweak = nullptr) : game(name) {
    if (tweak) tweak(game);
    CHECK(rig.load(game.dir, "field"));
    rig.battle.start(events);
  }
  Battle& b() { return rig.battle; }
  int id(const char* story) { return b().unitByStoryId(story); }
  Unit& unit(const char* story) { return b().units[(size_t)id(story)]; }
  int type(const char* name) { return rig.db.findUnitType(name); }
  int item(const char* name) { return rig.db.findItem(name); }
  int stat(const char* story, const char* s) { return unit(story).stats[(size_t)rig.db.findStat(s)]; }
  Cell at(int x, int y) { return b().grid.at(x, y); }
  // Appends an item to the unit's inventory; returns its slot.
  int give(const char* story, const char* name, int uses = 1) {
    ItemStack s;
    s.item = item(name);
    s.uses = uses;
    unit(story).items.push_back(s);
    return (int)unit(story).items.size() - 1;
  }
  int slotOf(const char* story, const char* name) {
    const Unit& u = unit(story);
    for (size_t i = 0; i < u.items.size(); ++i)
      if (u.items[i].item == item(name)) return (int)i;
    return -1;
  }
  // Runs an action that must be legal; the events it made are put in `events`.
  bool run(const Action& a) {
    events.clear();
    std::string why;
    bool ok = b().apply(a, events, &why);
    if (!ok) std::printf("  refused: %s\n", why.c_str());
    return ok;
  }
  // The refusal of an action ("" = it is legal); nothing changes.
  std::string why(const Action& a) {
    std::string w;
    if (b().validate(a, &w)) return "";
    return w.empty() ? "(no reason)" : w;
  }
  const Event* find(EventType t) const {
    for (const Event& e : events)
      if (e.type == t) return &e;
    return nullptr;
  }
  int count(EventType t) const {
    int n = 0;
    for (const Event& e : events) n += e.type == t;
    return n;
  }
};

void swapRule(Game& g, const char* from, const char* to) { g.swap("game.json", from, to); }

}  // namespace

// ---- by item ----

TEST(promo_item_promotes_the_user_and_resets_the_level) {
  World w("promo_item");
  Unit& rec = w.unit("rec");
  rec.hp = 4;
  rec.xp = 40;
  const int writ = w.slotOf("rec", "writ");
  std::vector<int> options;
  w.b().promotionOptions(w.id("rec"), writ, options);
  CHECK_EQ(options.size(), (size_t)2);  // captain and scout, in listed order
  CHECK_EQ(options[0], w.type("captain"));
  CHECK_EQ(options[1], w.type("scout"));

  CHECK(w.run(Action::useItem(w.id("rec"), writ, w.id("rec"))));  // -1: the first that fits
  const Unit& u = w.unit("rec");
  CHECK_EQ(u.type, w.type("captain"));
  CHECK_EQ(u.level, 1);  // rules.promotion default: reset
  CHECK_EQ(u.xp, 0);
  CHECK_EQ(w.stat("rec", "hp"), 16);  // the base difference: 16-10, 7-4, 4-2, 4-4
  CHECK_EQ(w.stat("rec", "atk"), 7);
  CHECK_EQ(w.stat("rec", "def"), 4);
  CHECK_EQ(w.stat("rec", "mov"), 4);
  CHECK_EQ(u.hp, 10);  // 4 + the +6 maximum
  CHECK(u.done);       // an item ends the turn
  CHECK_EQ(u.items.size(), (size_t)2);  // the writ is used up, sword and bow stay
  CHECK_STR(u.name, "Recruit");        // the unit keeps its name and identity
  CHECK_STR(u.storyId, "rec");
  CHECK(u.leader);
  const Event* e = w.find(EventType::UnitPromoted);
  CHECK(e != nullptr);
  if (e) {
    CHECK_EQ(e->a, w.id("rec"));
    CHECK_EQ(e->b, w.type("recruit"));
    CHECK_EQ(e->c, w.type("captain"));
    CHECK_EQ(e->d, 1);
    CHECK_EQ(e->data.size(), (size_t)4);
    CHECK_EQ(e->data[0], 6);
    CHECK_EQ(e->data[1], 3);
    CHECK_EQ(e->data[2], 2);
    CHECK_EQ(e->data[3], 0);
  }
  CHECK(w.find(EventType::ItemUsed) != nullptr);
  CHECK(w.find(EventType::ItemBroke) != nullptr);
}

TEST(promo_the_player_chooses_between_options) {
  World w("promo_choice");
  w.unit("rec").hp = 4;
  Action a = Action::useItem(w.id("rec"), w.slotOf("rec", "writ"), w.id("rec"));
  a.unitType = w.type("scout");
  CHECK(w.run(a));
  const Unit& u = w.unit("rec");
  CHECK_EQ(u.type, w.type("scout"));
  CHECK_EQ(u.level, 3);  // this entry's "levelAfter": "keep"
  // "gains" is the whole change: hp +3 and mov +1, nothing else (not the base difference).
  CHECK_EQ(w.stat("rec", "hp"), 13);
  CHECK_EQ(w.stat("rec", "atk"), 4);
  CHECK_EQ(w.stat("rec", "def"), 2);
  CHECK_EQ(w.stat("rec", "mov"), 5);
  CHECK_EQ(u.hp, 7);  // 4 + 3
}

TEST(promo_weapons_stay_but_may_not_be_wieldable) {
  World w("promo_weapons");
  Unit& rec = w.unit("rec");
  CHECK_EQ(w.b().equippedSlot(rec), 0);  // the sword
  Action a = Action::useItem(w.id("rec"), w.slotOf("rec", "writ"), w.id("rec"));
  a.unitType = w.type("scout");  // a scout wields bows only
  CHECK(w.run(a));
  const Unit& u = w.unit("rec");
  CHECK_EQ(u.items.size(), (size_t)2);
  CHECK_EQ(u.items[0].item, w.item("blade"));  // still carried
  std::vector<int> weapons;
  w.b().weaponsOf(u, weapons);
  CHECK_EQ(weapons.size(), (size_t)1);
  CHECK_EQ(weapons[0], w.rig.db.findWeapon("bow"));
  CHECK_EQ(w.b().equippedSlot(u), 1);  // the first wieldable weapon is in hand: the longbow
}

TEST(promo_item_refusals) {
  World w("promo_item_refusals");
  const int rec2 = w.id("rec2"), rec = w.id("rec");
  // Level 1 is below fromLevel 3.
  CHECK_STR(w.why(Action::useItem(rec2, w.slotOf("rec2", "writ"), rec2)), "that item cannot promote this unit now");
  // A type the item cannot bring.
  Action a = Action::useItem(rec, w.slotOf("rec", "writ"), rec);
  a.unitType = w.type("rider");
  CHECK_STR(w.why(a), "that promotion is not on offer");
  a.unitType = w.type("scout");
  CHECK_STR(w.why(a), "");
  // An item that is not a promotion item does nothing for a unit at full strength.
  int potion = w.give("rec", "potion");
  CHECK_STR(w.why(Action::useItem(rec, potion, rec)), "the item would do nothing there");
  // A promotion item used on somebody else: not offered (the item is for the one who carries it).
  CHECK_STR(w.why(Action::useItem(rec, w.slotOf("rec", "writ"), rec2)), "the item would do nothing there");
  // Nothing was changed by asking.
  CHECK_EQ(w.unit("rec").type, w.type("recruit"));
  CHECK(!w.unit("rec").done);
}

TEST(promo_a_promotion_item_with_a_heal_heals_first) {
  World w("promo_elixir");
  w.unit("rec").hp = 4;
  const int slot = w.give("rec", "elixir");
  CHECK(w.run(Action::useItem(w.id("rec"), slot, w.id("rec"))));
  CHECK_EQ(w.unit("rec").type, w.type("captain"));
  CHECK_EQ(w.unit("rec").hp, 15);  // 4 + 5 healed, then the +6 maximum
  CHECK(w.find(EventType::UnitHealed) != nullptr);
  // A unit that cannot promote still drinks it (the heal is the point).
  w.events.clear();
  World v("promo_elixir_heal");
  v.unit("rec2").hp = 3;
  const int s2 = v.give("rec2", "elixir");
  CHECK(v.run(Action::useItem(v.id("rec2"), s2, v.id("rec2"))));
  CHECK_EQ(v.unit("rec2").type, v.type("recruit"));
  CHECK_EQ(v.unit("rec2").hp, 8);
  CHECK(v.find(EventType::UnitPromoted) == nullptr);
  CHECK(v.unit("rec2").done);
}

// ---- offered at a level ----

TEST(promo_an_offered_promotion_is_an_action_that_ends_the_turn) {
  World w("promo_offer");
  std::vector<int> options;
  w.b().promotionOptions(w.id("rec"), -1, options);
  CHECK_EQ(options.size(), (size_t)1);  // only the "offer" entry; the scout entry is "off"
  CHECK_EQ(options[0], w.type("captain"));
  w.b().promotionOptions(w.id("rec2"), -1, options);  // level 1
  CHECK(options.empty());
  w.b().promotionOptions(w.id("cad"), -1, options);   // "auto" entries are not offered
  CHECK(options.empty());

  Action bad = Action::promote(w.id("rec"), w.type("scout"));
  CHECK_STR(w.why(bad), "that promotion is not on offer");
  CHECK_STR(w.why(Action::promote(w.id("rec2"))), "that unit has no promotion on offer");

  CHECK(w.run(Action::promote(w.id("rec"))));
  CHECK_EQ(w.unit("rec").type, w.type("captain"));
  CHECK_EQ(w.unit("rec").level, 1);
  CHECK(w.unit("rec").done);
  CHECK_EQ(w.count(EventType::UnitPromoted), 1);
  CHECK_STR(w.why(Action::promote(w.id("rec"))), "that unit has already acted");
}

TEST(promo_the_offer_can_wait_until_the_unit_has_moved) {
  World w("promo_offer_stand");
  // pwn stands on rock; a rider (wheels) cannot.
  const int pwn = w.id("pwn");
  std::vector<int> options;
  w.b().promotionOptions(pwn, -1, options);
  CHECK(options.empty());
  CHECK_STR(w.why(Action::promote(pwn)), "a Rider could not stand on this square");
  // The writ too.
  const int slot = w.give("pwn", "writ");
  CHECK_STR(w.why(Action::useItem(pwn, slot, pwn)), "a Rider could not stand on this square");
  CHECK(w.run(Action::move(pwn, w.at(2, 0))));
  w.b().promotionOptions(pwn, -1, options);
  CHECK_EQ(options.size(), (size_t)1);
  CHECK(w.run(Action::promote(pwn)));
  CHECK_EQ(w.unit("pwn").type, w.type("rider"));
  CHECK_EQ(w.unit("pwn").cell, w.at(2, 0));  // it did not move
}

// ---- automatic ----

TEST(promo_automatic_right_after_the_level_up) {
  World w("promo_auto");
  Unit& cad = w.unit("cad");
  cad.xp = 95;
  const int sword = w.rig.db.findWeapon("sword");
  CHECK(w.run(Action::attack(w.id("cad"), w.b().occupant[w.at(2, 1)], sword)));
  const Unit& u = w.unit("cad");
  CHECK_EQ(u.type, w.type("captain"));
  CHECK_EQ(u.level, 1);
  CHECK_EQ(u.xp, 0);
  // LevelUp first, then the promotion.
  int up = -1, pr = -1;
  for (size_t i = 0; i < w.events.size(); ++i) {
    if (w.events[i].type == EventType::LevelUp) up = (int)i;
    if (w.events[i].type == EventType::UnitPromoted) pr = (int)i;
  }
  CHECK(up >= 0 && pr > up);
  CHECK(u.done);  // the attack ended the turn, the promotion did not change that
}

TEST(promo_automatic_is_tried_again_at_the_start_of_the_next_turn) {
  World w("promo_auto_retry");
  const int drn = w.id("drn");
  w.unit("drn").level = 2;  // meets fromLevel 2, but stands on rock where a rider cannot
  CHECK(w.run(Action::endTurn()));  // red's turn begins
  CHECK_EQ(w.unit("drn").type, w.type("drone"));
  CHECK(w.run(Action::endTurn()));  // blue again: still on rock
  CHECK_EQ(w.unit("drn").type, w.type("drone"));
  CHECK(w.run(Action::move(drn, w.at(4, 0))));  // off the rock: nothing happens until the turn starts
  CHECK_EQ(w.unit("drn").type, w.type("drone"));
  CHECK(w.run(Action::endTurn()));
  CHECK_EQ(w.unit("drn").type, w.type("drone"));  // the other side's turn start leaves it alone
  CHECK(w.run(Action::endTurn()));
  CHECK_EQ(w.unit("drn").type, w.type("rider"));
  CHECK_EQ(w.unit("drn").level, 1);
  CHECK(w.find(EventType::UnitPromoted) != nullptr);
}

// ---- level after promotion ----

TEST(promo_rules_promotion_keep_and_the_per_entry_override) {
  World w("promo_keep", [](Game& g) {
    swapRule(g, "\"counterAttacks\": true,", "\"counterAttacks\": true, \"promotion\": { \"levelAfter\": \"keep\" },");
    g.swap("data/units.json", R"("levelAfter":"keep")", R"("levelAfter":"reset")");
  });
  CHECK(w.rig.db.rules.promotion.keepLevel);
  w.unit("rec").xp = 33;
  CHECK(w.run(Action::promote(w.id("rec"))));  // the entry has no override: the game's "keep"
  CHECK_EQ(w.unit("rec").level, 3);
  CHECK_EQ(w.unit("rec").xp, 33);

  World v("promo_keep_override", [](Game& g) {
    swapRule(g, "\"counterAttacks\": true,", "\"counterAttacks\": true, \"promotion\": { \"levelAfter\": \"keep\" },");
    g.swap("data/units.json", R"("levelAfter":"keep")", R"("levelAfter":"reset")");
  });
  v.unit("rec").xp = 33;
  Action a = Action::useItem(v.id("rec"), v.slotOf("rec", "writ"), v.id("rec"));
  a.unitType = v.type("scout");  // the entry says "reset" although the game keeps
  CHECK(v.run(a));
  CHECK_EQ(v.unit("rec").level, 1);
  CHECK_EQ(v.unit("rec").xp, 0);
}

// ---- stats ----

TEST(promo_a_stat_stops_at_zero_and_hit_points_stay_in_range) {
  // A promotion with a negative gain through the story (the base difference captain -> cadet is negative).
  World w("promo_negative", [](Game& g) {
    g.put("maps/field.json",
          promotest::map(R"({"id":"ev","when":{"battle":"start"},"do":[{"promote":"cad","to":"recruit"},{"promote":"rec","to":"cadet"}]})"));
  });
  // cad: cadet -> recruit is +2 hp +1 atk +1 def +1 mov, hp rises with it.
  CHECK_EQ(w.stat("cad", "hp"), 10);
  CHECK_EQ(w.unit("cad").hp, 10);
  // rec: recruit -> cadet is -2 hp: the hit points follow the maximum down.
  CHECK_EQ(w.stat("rec", "hp"), 8);
  CHECK_EQ(w.unit("rec").hp, 8);
  CHECK_EQ(w.stat("rec", "atk"), 3);
}

TEST(promo_gains_may_lower_a_stat_but_never_below_zero) {
  World w("promo_gains_floor", [](Game& g) {
    g.swap("data/units.json", R"("gains":{"hp":3,"mov":1})", R"("gains":{"def":-9,"hp":-1})");
  });
  Action a = Action::useItem(w.id("rec"), w.slotOf("rec", "writ"), w.id("rec"));
  a.unitType = w.type("scout");
  CHECK(w.run(a));
  CHECK_EQ(w.stat("rec", "def"), 0);
  CHECK_EQ(w.stat("rec", "hp"), 9);
  CHECK_EQ(w.unit("rec").hp, 9);
}

// ---- story ----

TEST(promo_story_command_changes_class_freely_and_the_trigger_fires) {
  World w("promo_story", [](Game& g) {
    g.put("maps/field.json", promotest::map(R"(
      {"id":"go","when":{"battle":"start"},"do":[
         {"promote":"rec","to":"rider"}, {"promote":"cad"}, {"promote":"pwn","to":"rider"},
         {"removeUnit":"rec2"}, {"promote":"rec2","to":"captain"} ]},
      {"id":"any","when":{"promoted":{}},"repeat":true,"do":[{"say":"promoted"}]},
      {"id":"cap","when":{"promoted":{"type":"captain"}},"do":[{"say":"a captain"}]}
    )"));
  });
  // rec: any type, not in its promotions; no entry for the pair, so the base difference and the game's reset.
  CHECK_EQ(w.unit("rec").type, w.type("rider"));
  CHECK_EQ(w.unit("rec").level, 1);
  CHECK_EQ(w.stat("rec", "hp"), 14);
  CHECK_EQ(w.unit("rec").hp, 14);  // 10 + 4
  CHECK_EQ(w.unit("rec").cell, w.at(0, 0));
  // cad: the only promotion, with the entry's rules (reset).
  CHECK_EQ(w.unit("cad").type, w.type("captain"));
  CHECK_EQ(w.unit("cad").level, 1);
  CHECK_EQ(w.stat("cad", "atk"), 7);
  // pwn is on rock: a rider cannot stand there; nothing happens.
  CHECK_EQ(w.unit("pwn").type, w.type("pawn"));
  // rec2 was removed first: nothing happens, nothing breaks.
  CHECK(!w.unit("rec2").alive);
  CHECK(w.unit("rec2").type == w.type("recruit"));
  int promoted = 0, says = 0;
  for (const Event& e : w.events) {
    promoted += e.type == EventType::UnitPromoted;
    says += e.type == EventType::Dialogue;
  }
  CHECK_EQ(promoted, 2);
  // "any" fires once per promotion (it repeats), "cap" only for the captain: 2 + 1.
  CHECK_EQ(says, 3);
}

TEST(promo_the_trigger_filter_picks_the_unit) {
  World w("promo_trigger", [](Game& g) {
    g.put("maps/field.json", promotest::map(R"(
      {"id":"go","when":{"battle":"start"},"do":[ {"promote":"cad"} ]},
      {"id":"mine","when":{"promoted":"rec"},"do":[{"say":"rec"}]},
      {"id":"cadet","when":{"promoted":"cad"},"do":[{"say":"cad"}]}
    )"));
  });
  int says = 0;
  for (const Event& e : w.events) says += e.type == EventType::Dialogue;
  CHECK_EQ(says, 1);  // only the cadet's event
}

// ---- saves and the roster ----

TEST(promo_a_promoted_unit_saves_and_restores) {
  World w("promo_save");
  w.unit("rec").hp = 4;
  Action a = Action::useItem(w.id("rec"), w.slotOf("rec", "writ"), w.id("rec"));
  a.unitType = w.type("scout");
  CHECK(w.run(a));
  const std::string text = savetest::stateText(w.b());
  Rig fresh;
  CHECK(fresh.load(w.game.dir, "field"));
  std::string err;
  Json saved;
  saveBattleState(w.b(), saved);
  CHECK(restoreBattleState(fresh.battle, saved, err));
  CHECK_STR(savetest::stateText(fresh.battle), text);
  const Unit& u = fresh.battle.units[(size_t)fresh.battle.unitByStoryId("rec")];
  CHECK_EQ(u.type, w.type("scout"));
  CHECK_EQ(u.level, 3);
  CHECK_EQ(u.hp, 7);
  CHECK_EQ(fresh.battle.stat(u, fresh.db.findStat("mov")), 5);
}

TEST(promo_the_roster_carries_the_new_type) {
  World w("promo_roster");
  CHECK(w.run(Action::promote(w.id("rec"))));
  const Unit& u = w.unit("rec");
  RosterUnit r;
  r.id = u.storyId;
  r.type = u.type;
  r.name = u.name;
  r.level = u.level;
  r.xp = u.xp;
  r.stats = u.stats;
  r.side = "blue";
  auto roster = std::make_shared<Roster>();
  roster->push_back(r);
  World next("promo_roster_next");
  next.b().setStartingRoster(roster);
  CHECK_EQ(next.unit("rec").type, next.type("captain"));
  CHECK_EQ(next.b().stat(next.unit("rec"), next.rig.db.findStat("atk")), 7);
  CHECK_EQ(next.unit("rec").hp, 16);  // a chapter begins at full strength
}
