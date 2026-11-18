// "Fidelity 3", item 1: an ability that spends its item ("spendsItem", docs/SRPG.md "An ability that spends its item").
#include "fidelity3_fixture.h"

using namespace ote;

namespace {

int itemsCarried(f3::World& w, const char* unit) { return (int)w.unit(unit).items.size(); }

// The wand's uses (-1 = the hero no longer carries one).
int wandUses(f3::World& w) {
  const int wand = w.db().findItem("wand");
  for (const ItemStack& s : w.unit("hero").items)
    if (s.item == wand) return s.uses;
  return -1;
}

}  // namespace

TEST(fid3_spends_item_loads_and_is_a_plain_flag) {
  Database db;
  std::string err;
  CHECK(loadGame(f3::make("sp_load"), db, err));
  CHECK(db.abilities[(size_t)db.findAbility("zap")].spendsItem);
  CHECK(!db.abilities[(size_t)db.findAbility("inspire")].spendsItem);
}

TEST(fid3_spends_item_refusals) {
  // No item gives it.
  f3::Opts none;
  none.items = std::string(f3::kItems);
  none.items.replace(none.items.find(",\"skills\":[\"zapper\"]"), 20, ",\"bonus\":{\"atk\":1}");
  CHECK(f3::says(f3::error(f3::make("sp_none", none)), "no item gives this ability"));
  // An item that never wears out.
  f3::Opts unlimited;
  unlimited.items = std::string(f3::kItems);
  const size_t at = unlimited.items.find("\"uses\":3,\"skills\"");
  unlimited.items.replace(at, 9, "");
  CHECK(f3::says(f3::error(f3::make("sp_unlim", unlimited)), "unlimited uses"));
  // A unit type listing it itself.
  f3::Opts listed;
  listed.units = "{\"id\":\"knight\",\"name\":\"Knight\",\"moveType\":\"foot\",\"stats\":{\"hp\":30,\"atk\":8,\"def\":2,\"mov\":4},\"wields\":[\"blade\"],"
                 "\"items\":[\"iron_sword\",\"wand\"],\"abilities\":[\"zap\"]},"
                 "{\"id\":\"brigand\",\"name\":\"Brigand\",\"moveType\":\"foot\",\"stats\":{\"hp\":20,\"atk\":5,\"def\":1,\"mov\":3},\"wields\":[\"blade\"],\"items\":[\"old_axe\"]},"
                 "{\"id\":\"hound\",\"name\":\"Hound\",\"moveType\":\"foot\",\"stats\":{\"hp\":8,\"atk\":3,\"def\":1,\"mov\":4},\"wields\":[\"blade\"],\"items\":[\"old_axe\"]}";
  CHECK(f3::says(f3::error(f3::make("sp_listed", listed)), "lists the ability itself"));
  // A flag that is not true / false.
  f3::Opts bad;
  bad.abilities = "{\"id\":\"zap\",\"target\":\"ally\",\"effect\":{\"heal\":5},\"spendsItem\":3}";
  CHECK(f3::says(f3::error(f3::make("sp_bad", bad)), "spendsItem"));
}

TEST(fid3_spends_item_uses_up_the_wand_and_the_ability_goes) {
  f3::World w(f3::make("sp_use"));
  const int hero = w.id("hero"), pal = w.id("pal"), zap = w.ability("zap");
  CHECK_EQ(wandUses(w), 3);
  std::vector<AbilityOption> opts;
  w.b().abilityOptions(hero, w.unit("hero").cell, opts);
  bool offered = false;
  for (const AbilityOption& o : opts)
    if (o.ability == zap) { offered = true; CHECK_EQ(o.itemUses, 3); }
  CHECK(offered);
  for (int left = 2; left >= 0; --left) {
    w.unit("pal").hp = 10;
    w.unit("hero").done = false;
    CHECK_STR(w.why(Action::useAbility(hero, zap, pal)), "");
    CHECK(w.run(Action::useAbility(hero, zap, pal)));
    CHECK_EQ(w.count(EventType::ItemBroke), left == 0 ? 1 : 0);
    CHECK_EQ(wandUses(w), left == 0 ? -1 : left);
    w.round();
  }
  // The wand is gone, and so is the ability.
  CHECK_EQ(itemsCarried(w, "hero"), 1);
  std::vector<int> have;
  w.b().abilitiesOf(w.unit("hero"), have);
  for (int a : have) CHECK(a != zap);
  w.unit("pal").hp = 10;
  CHECK(!w.why(Action::useAbility(hero, zap, pal)).empty());
  CHECK_EQ(w.unit("hero").abilityUses.size(), (size_t)1);  // used three times, counted as any ability
  CHECK_EQ(w.unit("hero").abilityUses[0].used, 3);
}

TEST(fid3_spends_item_with_per_battle_uses_both_apply) {
  f3::Opts o;
  o.abilities = f3::abilities(",\"uses\":2");
  f3::World w(f3::make("sp_both", o));
  const int hero = w.id("hero"), pal = w.id("pal"), zap = w.ability("zap");
  for (int i = 0; i < 2; ++i) {
    w.unit("pal").hp = 10;
    CHECK(w.run(Action::useAbility(hero, zap, pal)));
    w.round();
  }
  CHECK_EQ(wandUses(w), 1);  // the wand has one use left but the battle limit is reached
  w.unit("pal").hp = 10;
  CHECK(f3::says(w.why(Action::useAbility(hero, zap, pal)), "no uses left"));
  CHECK_EQ(wandUses(w), 1);
}

TEST(fid3_spends_item_save_round_trip_keeps_the_uses) {
  f3::World w(f3::make("sp_save"));
  w.unit("pal").hp = 10;
  CHECK(w.run(Action::useAbility(w.id("hero"), w.ability("zap"), w.id("pal"))));
  CHECK_EQ(wandUses(w), 2);
  Json j;
  saveBattleState(w.b(), j);
  f3::World again(f3::make("sp_save"), false);
  std::string err;
  CHECK(restoreBattleState(again.b(), j, err));
  CHECK_EQ(wandUses(again), 2);
  CHECK_STR(savetest::stateText(again.b()), savetest::stateText(w.b()));
}

TEST(fid3_a_game_without_the_key_is_unchanged) {
  // The fidelity 2 fixture (no spendsItem anywhere) loads, plays and saves as before.
  f2::World w(f2::make("sp_plain"));
  CHECK(savetest::stateText(w.b()).find("spendsItem") == std::string::npos);
}
