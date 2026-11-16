// The player's weapon ranks and stat caps (docs/SRPG.md section 7), the parts that need no window: the rows of the
// Stats and Items pages, the words of the item screens, the level-up box at a cap and the "Sword rank C" label. The
// scene is covered by tests/srpg_player_ranks_test.py. Game: tests/fixtures/srpg-ranks (the duel map).
#include <string>

#include "battle/battle_view.h"
#include "battle/rank_text.h"
#include "battle/unit_info.h"
#include "battle/unit_info_rows.h"
#include "srpg_ranks_fixture.h"

using namespace ote;
using namespace rankstest;

namespace {
std::string rowText(const std::vector<InfoRow>& rows, const std::string& label) {
  for (const InfoRow& r : rows)
    if (r.label == label) return r.value + "|" + r.note + "|" + r.cap;
  return "(no row " + label + ")";
}
}  // namespace

TEST(player_ranks_stats_page_shows_ranks_and_caps) {
  World w("player_ranks_rows", "duel");
  std::vector<InfoRow> rows = statRows(w.b(), w.unit("kell"));
  CHECK_STR(rowText(rows, "Sword rank"), "D|4/7|");  // experience 4, C needs 7
  CHECK_STR(rowText(rows, "Bow rank"), "E|0/3|");
  CHECK_STR(rowText(rows, "Strength"), "6||cap 10");
  CHECK_STR(rowText(rows, "Defense"), "3||");  // no cap anywhere
  w.unit("kell").stats[(size_t)w.stat("str")] = 10;
  w.unit("kell").weaponXp[(size_t)w.group("sword")] = 12;
  rows = statRows(w.b(), w.unit("kell"));
  CHECK_STR(rowText(rows, "Strength"), "10||MAX");
  CHECK_STR(rowText(rows, "Sword rank"), "B|MAX|");
  // Fixed growth: no progress shown.
  World f("player_ranks_fixed", "duel", [](Copy& c) { c.swap("data/ranks.json", "\"growth\": \"use\"", "\"growth\": \"fixed\""); });
  CHECK_STR(rowText(statRows(f.b(), f.unit("kell")), "Sword rank"), "D||");
}

TEST(player_ranks_items_page_greys_a_weapon_the_rank_does_not_allow) {
  World w("player_ranks_items", "duel");
  std::vector<ItemRow> rows = itemRows(w.b(), w.unit("kell"));
  CHECK_EQ((int)rows.size(), 4);
  CHECK_STR(rows[0].blocked, "");
  CHECK_STR(rows[1].blocked, "");            // steel sword: rank D is enough
  CHECK_STR(rows[2].blocked, "Needs rank B");  // war sword
  CHECK_STR(rows[2].detail, "Needs rank B");   // the reason replaces the description
  CHECK_STR(rows[3].blocked, "");            // a draught is not a weapon
  CHECK(rows[0].equipped);
  CHECK_STR(weaponBlockReason(w.b(), w.unit("mara"), w.unit("mara").items[1]), "Needs rank C");  // her long bow
}

TEST(player_ranks_a_game_without_ranks_adds_nothing) {
  savetest::Rig rig;
  CHECK(rig.loadDemo("demo-srpg", "bridge"));
  rig.start();
  for (const Unit& u : rig.battle.units) {
    for (const InfoRow& r : statRows(rig.battle, u)) CHECK(r.cap.empty() && r.label.find(" rank") == std::string::npos);
    for (const ItemStack& s : u.items) CHECK_STR(weaponBlockReason(rig.battle, u, s), "");
  }
}

TEST(player_ranks_level_up_box_marks_a_stat_at_its_cap) {
  World w("player_ranks_box", "duel");
  BattleView view;
  view.init(w.b(), 24, false);
  const int kell = w.id("kell");
  w.unit("kell").stats[(size_t)w.stat("str")] = 10;  // at the cap
  Event up;
  up.type = EventType::LevelUp;
  up.a = kell;
  up.b = 3;
  up.data.assign(w.rig.db.stats.size(), 0);
  up.data[(size_t)w.stat("hp")] = 2;
  view.push({up});
  view.update(0.1, w.b());
  CHECK(view.levelUp.active);
  std::string lines;
  for (const std::string& l : view.levelUp.lines) lines += l + "|";
  CHECK_STR(lines, "+2 HP|Strength: max|");
  view.skip(w.b());
}

TEST(player_ranks_rank_up_floats_over_the_unit) {
  World w("player_ranks_float", "duel");
  BattleView view;
  view.init(w.b(), 24, false);
  Event e;
  e.type = EventType::RankUp;
  e.a = w.id("kell");
  e.b = w.group("sword");
  e.c = 2;
  e.d = 1;
  CHECK_STR(rankUpText(w.b(), e), "Sword rank C");
  view.push({e});
  bool seen = false;
  for (int i = 0; i < 100 && (i == 0 || view.busy()); ++i) {
    view.update(0.05, w.b());
    for (const FloatText& f : view.floats) seen |= f.text == "Sword rank C";
  }
  CHECK(seen);
  CHECK(!view.busy());
}
