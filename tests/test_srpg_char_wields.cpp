// A character's own weapon groups (docs/SRPG.md section 7, characters.json "wields"): they only add to the class's, and everything
// goes through the one wield test. Game: tests/fixtures/srpg-locks (mara is an archer, bow only) and srpg-staves (brae, a knight).
#include <algorithm>

#include "battle/rank_text.h"
#include "srpg_staves_fixture.h"

using namespace ote;
using namespace rankstest;

namespace {
struct Wields : World {
  Wields(const char* name, const std::string& maraKeys)
      : World(name, "duel", [&](Copy& c) { c.swap("data/characters.json", "\"type\": \"archer\",", "\"type\": \"archer\", " + maraKeys + ","); },
              "srpg-locks") {}
  std::string refusal(const char* story, const char* weapon) {
    std::string why;
    return b().canWieldNow(unit(story), this->weapon(weapon), &why) ? "" : why;
  }
};
}  // namespace

TEST(char_wields_adds_a_group_to_the_class) {
  Wields w("cw_add", "\"wields\": [\"sword\"]");
  CHECK_EQ((int)w.rig.db.characters[(size_t)w.rig.db.findCharacter("mara")].wields.size(), 1);
  CHECK_STR(w.refusal("mara", "bronze_sword"), "");       // her class (archer) has bows only
  CHECK_STR(w.refusal("mara", "short_bow"), "");          // the class's own still works
  CHECK_STR(w.refusal("mara", "trio_blade"), "");         // on its character list, and now the class side passes
  CHECK_STR(w.refusal("mara", "dawn_blade"), "Only Kell can wield this");  // locks still apply (the class side no longer says "cannot")
  CHECK_STR(w.refusal("mara", "steel_sword"), "Needs rank D");              // so do ranks
  // A weapon in no group she has is still out; so is another unit of her class (a generic archer would need the key too).
  CHECK_STR(w.refusal("kell", "short_bow"), "the unit cannot wield that");
  // The key adds to the class and removes nothing; without it she is as before.
  Wields none("cw_none", "\"description\": \"x\"");
  CHECK_STR(none.refusal("mara", "bronze_sword"), "the unit cannot wield that");
}

TEST(char_wields_equip_attack_and_the_player_follow) {
  Wields w("cw_follow", "\"wields\": [\"sword\"]");
  Unit& mara = w.unit("mara");
  std::vector<int> weapons;
  w.b().weaponsOf(mara, weapons);
  CHECK(std::find(weapons.begin(), weapons.end(), w.weapon("short_bow")) != weapons.end());
  CHECK_STR(w.why(Action::equip(mara.id, w.slotOf("mara", "short_bow"))), "that is already equipped");
  mara.items.push_back(ItemStack());
  mara.items.back().item = w.item("bronze_sword");
  CHECK_STR(w.why(Action::equip(mara.id, (int)mara.items.size() - 1)), "");
  CHECK_STR(weaponBlockReason(w.b(), mara, mara.items[1]), "Only Kell can wield this");  // the dawn blade: a lock, shown greyed
}

TEST(char_wields_loader_refusals_and_the_empty_key) {
  Copy a("cw_l1", "srpg-locks");
  a.swap("data/characters.json", "\"type\": \"archer\",", "\"type\": \"archer\", \"wields\": \"sword\",");
  CHECK(saysIn(a.gameError(), "character \"mara\": \"wields\" must be a list"));
  Copy b("cw_l2", "srpg-locks");
  b.swap("data/characters.json", "\"type\": \"archer\",", "\"type\": \"archer\", \"wields\": [4],");
  CHECK(saysIn(b.gameError(), "character \"mara\": \"wields\" entry 1 must be a string"));
  Copy c("cw_l3", "srpg-locks");
  c.swap("data/characters.json", "\"type\": \"archer\",", "\"type\": \"archer\", \"wields\": [],");
  CHECK_STR(c.gameError(), "");
}

TEST(char_wields_also_for_staves) {
  stavestest::Clinic w("cw_staff", "clinic");
  std::string why;
  CHECK(!w.b().canWieldStaff(w.unit("brae"), w.item("mend_staff"), &why));  // a knight: no staves
  CHECK_STR(why, "the unit cannot wield that");
  stavestest::Clinic v("cw_staff2", "clinic", [](Copy& c) {
    c.swap("data/characters.json", "\"type\": \"knight\", \"level\": 2", "\"type\": \"knight\", \"level\": 2, \"wields\": [\"staff\"]");
  });
  CHECK(v.b().canWieldStaff(v.unit("brae"), v.item("mend_staff"), &why));
  CHECK(!v.b().canWieldStaff(v.unit("dax"), v.item("mend_staff"), &why));  // a generic fighter did not get it
}
