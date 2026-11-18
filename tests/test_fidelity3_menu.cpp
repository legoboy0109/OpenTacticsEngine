// "Fidelity 3", item 1, the player's part: the menu line of an ability that spends its item shows the uses left (the smaller of the item's and the
// ability's own per-battle uses).
#include "battle/ability_text.h"
#include "fidelity3_fixture.h"

using namespace ote;

namespace {

std::string extra(f3::World& w, int zap) {
  std::vector<AbilityOption> opts;
  w.b().abilityOptions(w.id("hero"), w.unit("hero").cell, opts);
  for (const AbilityOption& o : opts)
    if (o.ability == zap) return abilityMenuExtra(w.b(), w.unit("hero"), o);
  return "(not offered)";
}

}  // namespace

TEST(fid3_menu_shows_the_uses_the_item_has_left) {
  f3::World w(f3::make("menu_item"));
  const int zap = w.ability("zap");
  w.unit("pal").hp = 10;  // a heal needs somebody hurt, or the menu says "nothing to target"
  CHECK_STR(extra(w, zap), "3 left");
  CHECK(w.run(Action::useAbility(w.id("hero"), zap, w.id("pal"))));
  CHECK_STR(extra(w, zap), "2 left");
}

TEST(fid3_menu_shows_the_smaller_of_the_item_and_the_battle_limit) {
  f3::Opts o;
  o.abilities = f3::abilities(",\"uses\":1");
  f3::World w(f3::make("menu_both", o));
  w.unit("pal").hp = 10;
  CHECK_STR(extra(w, w.ability("zap")), "1 left");  // the battle allows one, the wand has three
}

TEST(fid3_menu_an_ability_without_the_key_is_unchanged) {
  f3::World w(f3::make("menu_plain"));
  std::vector<AbilityOption> opts;
  w.b().abilityOptions(w.id("hero"), w.unit("hero").cell, opts);
  for (const AbilityOption& o : opts)
    if (o.ability != w.ability("zap")) CHECK_EQ(o.itemUses, -2);
}
