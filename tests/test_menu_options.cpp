// Menu options of the player (docs/SRPG.md "Menu options"): the two game.json keys and their refusals, the player's override, which
// action-menu entries "hide" leaves out, and the weapon list of "attackWeaponChoice": "pick" (its rows, their order, the targets of
// one weapon, which row the list starts on and when an Equip order comes first).
#include <cstring>

#include "battle/menu_hide.h"
#include "battle/weapon_choice.h"
#include "fidelity3_fixture.h"
#include "options/effective.h"
#include "options/option_defs.h"
#include "srpg_staves_fixture.h"

using namespace ote;

namespace {

const char* kVersion = "\"formatVersion\": 1,";

// The ranks fixture's duel map with the sword fighter also wielding bows: kell (1,1) carries bronze sword, short bow, steel sword,
// war sword (rank B: he cannot wield it), a draught; g1 (red) stands next to him at 2,1, g2 two squares away at 3,1.
// Sword reach 1 -> g1; short bow reach 2 -> g2.
void duelTweak(rankstest::Copy& c) {
  c.swap("data/units.json", "\"wields\": [\"sword\"], \"ranks\": { \"sword\": \"e\" }", "\"wields\": [\"sword\", \"bow\"], \"ranks\": { \"sword\": \"e\", \"bow\": \"e\" }");
  c.swap("data/characters.json", "\"items\": [\"bronze_sword\", \"steel_sword\", \"war_sword\", \"might_draught\"]",
         "\"items\": [\"bronze_sword\", \"short_bow\", \"steel_sword\", \"war_sword\", \"might_draught\"]");
  c.swap("maps/duel.json", "\"x\": 5, \"y\": 2", "\"x\": 3, \"y\": 1");
}

struct Duel : rankstest::World {
  Duel(const char* name) : World(name, "duel", duelTweak) {}
  std::vector<AttackOption> options() {
    std::vector<AttackOption> o;
    b().attackOptions(id("kell"), unit("kell").cell, o);
    return o;
  }
  std::vector<WeaponRow> rows() { return weaponRows(b(), id("kell"), unit("kell").cell, options()); }
};

}  // namespace

// ---- the two game.json keys

TEST(menu_options_game_keys_default_to_today) {
  rankstest::Copy c("menu_keys_default", "srpg-staves");
  Database db;
  std::string err;
  CHECK(loadGame(c.dir, db, err));
  CHECK(!db.menuHideUnavailable);
  CHECK(!db.attackWeaponPick);
}

TEST(menu_options_game_keys_are_read) {
  rankstest::Copy c("menu_keys_read", "srpg-staves");
  c.swap("game.json", kVersion, std::string(kVersion) + " \"menuUnavailable\": \"hide\", \"attackWeaponChoice\": \"pick\",");
  Database db;
  std::string err;
  CHECK(loadGame(c.dir, db, err));
  CHECK(db.menuHideUnavailable);
  CHECK(db.attackWeaponPick);
  rankstest::Copy g("menu_keys_explicit", "srpg-staves");  // the default words written out are the defaults
  g.swap("game.json", kVersion, std::string(kVersion) + " \"menuUnavailable\": \"grey\", \"attackWeaponChoice\": \"equipped\",");
  Database d2;
  CHECK(loadGame(g.dir, d2, err));
  CHECK(!d2.menuHideUnavailable && !d2.attackWeaponPick);
}

TEST(menu_options_game_keys_refuse_other_words) {
  rankstest::Copy a("menu_keys_bad1", "srpg-staves");
  a.swap("game.json", kVersion, std::string(kVersion) + " \"menuUnavailable\": \"gray\",");
  CHECK(rankstest::saysIn(a.gameError(), "\"menuUnavailable\" must be \"grey\""));
  rankstest::Copy b("menu_keys_bad2", "srpg-staves");
  b.swap("game.json", kVersion, std::string(kVersion) + " \"attackWeaponChoice\": \"ask\",");
  CHECK(rankstest::saysIn(b.gameError(), "\"attackWeaponChoice\" must be \"equipped\""));
  rankstest::Copy n("menu_keys_bad3", "srpg-staves");  // not a word at all
  n.swap("game.json", kVersion, std::string(kVersion) + " \"menuUnavailable\": true,");
  CHECK(!n.gameError().empty());
}

TEST(menu_options_game_info_for_the_launcher) {
  rankstest::Copy c("menu_keys_info", "srpg-staves");
  c.swap("game.json", kVersion, std::string(kVersion) + " \"menuUnavailable\": \"hide\",");
  GameInfo info;
  std::string err;
  CHECK(loadGameInfo(c.dir, info, err));
  CHECK(info.menuHide);
}

// ---- the player's own choice

namespace {
bool hides(const LookLayer& args, const LookLayer& perGame, const LookLayer& global, bool gamePresent, bool gameHides) {
  GameLook g;
  g.present = gamePresent;
  g.menuHide = gameHides;
  g.styleIds = {"default"};
  return resolveLook("assets", args, perGame, global, g).menuHide;
}
LookLayer layer(const char* v) {
  LookLayer l;
  l.menuUnavailable = v;
  return l;
}
}  // namespace

TEST(menu_options_player_choice_beats_the_game) {
  CHECK(!hides(LookLayer(), LookLayer(), LookLayer(), true, false));  // existing games: greyed, as today
  CHECK(hides(LookLayer(), LookLayer(), LookLayer(), true, true));    // the maker opted in
  CHECK(!hides(LookLayer(), LookLayer(), LookLayer(), false, true));  // the launcher has no game
  CHECK(hides(LookLayer(), LookLayer(), layer("hide"), true, false));
  CHECK(!hides(LookLayer(), LookLayer(), layer("grey"), true, true));
  CHECK(hides(LookLayer(), layer("hide"), layer("grey"), true, false));  // this game's override beats the global one
  CHECK(!hides(LookLayer(), layer("grey"), layer("hide"), true, true));
  CHECK(hides(layer("hide"), layer("grey"), layer("grey"), true, false));  // the command line first
  CHECK(hides(layer("nonsense"), LookLayer(), LookLayer(), true, true));   // an unknown word is "unset"
}

TEST(menu_options_option_row_and_settings_file) {
  const OptionDef* def = nullptr;
  for (const OptionDef& d : optionDefs())
    if (!strcmp(d.key, "menuUnavailable")) def = &d;
  CHECK(def != nullptr);
  if (!def) return;
  OptionContext ctx;
  std::vector<OptionChoice> global = def->choices(ctx);
  CHECK_EQ((int)global.size(), 3);
  CHECK(global[0].value.isNull() && global[0].label == "Game default");
  CHECK(global[1].label == "Greyed out" && global[1].value.asString() == "grey");
  CHECK(global[2].label == "Hidden" && global[2].value.asString() == "hide");
  GameEntry game;
  ctx.game = &game;
  CHECK(def->choices(ctx)[0].label == "Use global");
  Settings s;
  s.setGlobal("menuUnavailable", Json::makeString("hide"));
  s.setOverride("g", "menuUnavailable", Json::makeString("grey"));
  CHECK(globalLayer(s).menuUnavailable == "hide");
  CHECK(overrideLayer(s, "g").menuUnavailable == "grey");
  CHECK(overrideLayer(s, "other").menuUnavailable.empty());
}

// ---- which entries "hide" leaves out

TEST(menu_options_staff_entry_hides_only_when_nobody_is_in_reach_for_every_staff) {
  stavestest::Clinic w("menu_staff_hide");  // wen: mend, rally, high (rank B: she is C), kin, chorus; everybody unhurt
  std::vector<StaffOption> opts;
  w.b().staffOptions(w.id("wen"), w.unit("wen").cell, opts);
  CHECK_EQ((int)opts.size(), 5);
  for (const StaffOption& o : opts) CHECK(!o.ok);
  int noTarget = 0;
  for (const StaffOption& o : opts) noTarget += o.noTarget;
  CHECK(noTarget >= 3);  // mend, rally, chorus: nobody needs healing
  CHECK(!w.option("wen", "high_staff").noTarget);  // a rank reason is not "nobody there"
  CHECK(!staffEntryHidden(false, opts));  // the default keeps today's greyed entry
  CHECK(!staffEntryHidden(true, opts));   // the high staff's rank reason keeps it greyed (the player should know)
  w.hurt("tarn", 5);
  w.b().staffOptions(w.id("wen"), w.unit("wen").cell, opts);
  CHECK(!staffEntryHidden(true, opts));   // somebody to heal: usable
}

TEST(menu_options_staff_entry_hidden_when_every_staff_has_nobody) {
  stavestest::Clinic w("menu_staff_hide2", "clinic", [](rankstest::Copy& c) {
    c.swap("data/characters.json", "\"items\": [\"mend_staff\", \"rally_staff\", \"high_staff\", \"kin_staff\", \"chorus_staff\"]",
           "\"items\": [\"mend_staff\", \"rally_staff\"]");
  });
  std::vector<StaffOption> opts;
  w.b().staffOptions(w.id("wen"), w.unit("wen").cell, opts);
  CHECK_EQ((int)opts.size(), 2);
  CHECK(staffEntryHidden(true, opts));
  CHECK(!staffEntryHidden(false, opts));
  CHECK(!staffEntryHidden(true, std::vector<StaffOption>()));  // no staves: no entry anyway
  w.hurt("ola", 3);  // ola is at 2,1, next to wen
  w.b().staffOptions(w.id("wen"), w.unit("wen").cell, opts);
  CHECK(!staffEntryHidden(true, opts));
}

TEST(menu_options_ability_with_no_target_hides_but_a_used_up_one_stays_greyed) {
  f3::World w(f3::make("menu_ability_hide"));
  const int zap = w.ability("zap");
  auto option = [&]() {
    std::vector<AbilityOption> opts;
    w.b().abilityOptions(w.id("hero"), w.unit("hero").cell, opts);
    for (const AbilityOption& o : opts)
      if (o.ability == zap) return o;
    return AbilityOption();
  };
  AbilityOption o = option();
  CHECK(!o.ok && o.noTarget);  // nobody is hurt: "nothing to target"
  CHECK(abilityEntryHidden(true, o));
  CHECK(!abilityEntryHidden(false, o));
  w.unit("pal").hp = 10;
  o = option();
  CHECK(o.ok && !o.noTarget);
  CHECK(!abilityEntryHidden(true, o));
  // Out of uses: stays greyed with its reason even with nobody to use it on.
  f3::Opts opts;
  opts.abilities = f3::abilities(",\"uses\":1");
  f3::World u(f3::make("menu_ability_used", opts));
  u.unit("pal").hp = 10;
  CHECK(u.run(Action::useAbility(u.id("hero"), u.ability("zap"), u.id("pal"))));
  u.unit("pal").hp = u.b().maxHp(u.unit("pal"));
  std::vector<AbilityOption> list;
  u.b().abilityOptions(u.id("hero"), u.unit("hero").cell, list);
  bool found = false;
  for (const AbilityOption& a : list)
    if (a.ability == u.ability("zap")) {
      found = true;
      CHECK(!a.ok && !a.noTarget);
      CHECK(!abilityEntryHidden(true, a));
    }
  CHECK(found);
}

TEST(menu_options_drop_with_no_room_hides_and_other_entries_are_judged_by_their_flag) {
  RescueEntry drop;
  drop.label = "Drop";
  drop.enabled = false;
  drop.extra = "no room";
  drop.noTarget = true;
  CHECK(rescueEntryHidden(true, drop));
  CHECK(!rescueEntryHidden(false, drop));
  drop.enabled = true;  // there is room
  CHECK(!rescueEntryHidden(true, drop));
  PlaceOption key;
  key.ok = false;
  key.noTarget = true;  // locked, no key
  CHECK(placeEntryHidden(true, key));
  CHECK(!placeEntryHidden(false, key));
  PlaceOption full;
  full.ok = false;  // pack full: the player should know
  CHECK(!placeEntryHidden(true, full));
  PlaceOption fine;
  fine.ok = true;
  CHECK(!placeEntryHidden(true, fine));
}

// ---- the weapon list

TEST(menu_options_weapon_rows_follow_the_inventory_and_leave_out_what_cannot_be_wielded_or_reached) {
  Duel w("menu_weapon_rows");
  const std::vector<WeaponRow> rows = w.rows();
  CHECK_EQ((int)rows.size(), 3);  // the war sword (rank B) is not wieldable: not offered
  if (rows.size() != 3) return;
  CHECK_STR(rows[0].name, "Bronze Sword");
  CHECK_STR(rows[1].name, "Short Bow");
  CHECK_STR(rows[2].name, "Steel Sword");
  CHECK_EQ(rows[0].slot, 0);
  CHECK_EQ(rows[1].slot, 1);
  CHECK_EQ(rows[2].slot, 2);
  CHECK(rows[0].equipped && !rows[1].equipped && !rows[2].equipped);
  CHECK_EQ(rows[0].targets, 1);
  CHECK_EQ(rows[1].targets, 1);
  CHECK(rows[0].stats.find("Dmg ") == 0);
  CHECK(rows[0].stats.find("Range 1") != std::string::npos);
  CHECK(rows[1].stats.find("Range 2") != std::string::npos);
  CHECK(rows[0].uses.empty());  // no use limit on these items
  CHECK(!rows[0].mapWeapon && !rows[1].mapWeapon);
}

TEST(menu_options_one_weapons_targets_are_its_own) {
  Duel w("menu_weapon_targets");
  const std::vector<AttackOption> all = w.options();
  const std::vector<WeaponRow> rows = w.rows();
  if (rows.size() != 3) return;
  const std::vector<AttackOption> sword = optionsOfWeapon(all, rows[0].weapon);
  const std::vector<AttackOption> bow = optionsOfWeapon(all, rows[1].weapon);
  CHECK_EQ((int)sword.size(), 1);
  CHECK_EQ((int)bow.size(), 1);
  if (sword.empty() || bow.empty()) return;
  CHECK_EQ(sword[0].target, w.id("g1"));
  CHECK_EQ(bow[0].target, w.id("g2"));  // the targets differ from the equipped weapon's
  CHECK((int)optionsOfWeapon(all, -5).size() == 0);
}

TEST(menu_options_the_list_starts_on_the_weapon_in_hand_or_one_that_reaches_the_tapped_enemy) {
  Duel w("menu_weapon_start");
  const std::vector<AttackOption> all = w.options();
  const std::vector<WeaponRow> rows = w.rows();
  CHECK_EQ(startRow(rows, all, -1), 0);                 // the equipped bronze sword
  CHECK_EQ(startRow(rows, all, w.id("g1")), 0);         // it reaches the tapped enemy
  CHECK_EQ(startRow(rows, all, w.id("g2")), 1);         // it does not: the first weapon that does (the bow)
  CHECK_EQ(startRow(std::vector<WeaponRow>(), all, -1), 0);
}

TEST(menu_options_equip_comes_first_only_for_a_weapon_not_in_hand_and_the_orders_match_equipping_by_hand) {
  Duel w("menu_weapon_equip");
  const std::vector<WeaponRow> rows = w.rows();
  if (rows.size() != 3) return;
  const int kell = w.id("kell");
  CHECK(!needsEquip(w.b(), kell, rows[0]));
  CHECK(needsEquip(w.b(), kell, rows[1]));
  CHECK(needsEquip(w.b(), kell, rows[2]));
  // What the player submits for "Short Bow": the Equip of its slot, then the Attack; the same two orders a hand-made equip would give.
  CHECK(w.run(Action::equip(kell, rows[1].slot)));
  CHECK_EQ(w.count(EventType::ItemEquipped), 1);
  CHECK_EQ(w.b().equippedSlot(w.unit("kell")), 0);
  CHECK_EQ(w.unit("kell").items[0].item, w.item("short_bow"));
  CHECK(w.run(Action::attack(kell, w.id("g2"), rows[1].weapon)));
  // The list asked again now: the bow is the one in hand, so no Equip is needed for it.
  const std::vector<WeaponRow> after = w.rows();
  for (const WeaponRow& r : after)
    if (r.weapon == rows[1].weapon) {
      CHECK(r.equipped);
      CHECK(!needsEquip(w.b(), kell, r));
    }
}

TEST(menu_options_nothing_in_reach_gives_an_empty_list) {
  Duel w("menu_weapon_none");
  CHECK((int)weaponRows(w.b(), w.id("mara"), w.unit("mara").cell, std::vector<AttackOption>()).size() == 0);
}
