// The player's skills and unit details (docs/SRPG.md step U1), the parts that need no window: the rows of the Stats /
// Skills / Items pages, which pages a unit gets, how they draw into a small box, and the battle view playing
// SkillActivated, UnitHurt and SkillLearned (in the level-up box). The scene, the forecast and the panel are covered by
// tests/srpg_player_skills_test.py. Game: tests/fixtures/srpg-skills.
#include <algorithm>
#include <string>
#include <vector>

#include "battle/battle_view.h"
#include "battle/unit_info.h"
#include "battle/unit_info_rows.h"
#include "battle_fixture.h"
#include "fake_renderer.h"
#include "srpg_skills_fixture.h"

using namespace ote;
using skilltest::World;

namespace {

// Remembers what was drawn.
class Recorder : public ote_test::FakeRenderer {
 public:
  std::vector<std::string> texts;
  void drawText(int, int, const char* t, Color, int) override { texts.push_back(t); }
  bool has(const std::string& s) const {
    for (const std::string& t : texts)
      if (t == s) return true;
    return false;
  }
};

std::string titles(const UnitInfoPages& pages) {
  std::string out;
  for (const UnitInfoPage& p : pages) out += (out.empty() ? "" : ",") + p.title;
  return out;
}

Event ev(EventType t, int a, int b = 0, int c = -1) {
  Event e;
  e.type = t;
  e.a = a;
  e.b = b;
  e.c = c;
  return e;
}

}  // namespace

TEST(player_skills_rows_name_the_source_of_each_skill_in_judging_order) {
  World w;
  const Unit& eirik = w.unit("eirik");  // hero: steady_recovery (class), learned fast_learner light_step gifted, personal bold
  std::vector<SkillRow> rows = skillRows(w.b(), eirik);
  std::string got;
  for (const SkillRow& r : rows) got += r.name + ":" + skillSourceName(r.source) + " ";
  CHECK_STR(got, "Steady Recovery:Class Fast Learner:Learned Light Step:Learned Gifted:Learned Bold:Personal Blade Focus:Weapon ");
  // An accessory's skills are "Item"; a skill a unit has twice is named once, at its first place.
  Unit& lyra = w.unit("lyra");
  lyra.items.push_back({w.db().findItem("amulet"), -1, false});
  rows = skillRows(w.b(), lyra);
  got.clear();
  for (const SkillRow& r : rows) got += r.name + ":" + skillSourceName(r.source) + " ";
  CHECK(got.find("Tough:Item") != std::string::npos);
  CHECK(got.find("Lucky Charm:Item") != std::string::npos);
  CHECK(got.find("Steady Recovery:Class") != std::string::npos);
  // The description comes along (empty when the data has none).
  for (const SkillRow& r : skillRows(w.b(), eirik))
    if (r.name == "Bold") CHECK(r.description.empty());
}

TEST(player_skills_stat_rows_show_what_skills_and_items_add_in_colour) {
  World w;
  Unit& lyra = w.unit("lyra");
  std::vector<InfoRow> plain = statRows(w.b(), lyra);
  CHECK_STR(plain[0].label, "Class");
  for (const InfoRow& r : plain)
    if (r.label == "hp") CHECK(r.note.empty());  // (a weapon's skill adds to some others)
  lyra.items.push_back({w.db().findItem("amulet"), -1, false});  // hp +2, and the skills tough (+5 hp) and lucky charm (+1 def)
  std::vector<InfoRow> rows = statRows(w.b(), lyra);
  const InfoRow* hp = nullptr;
  const InfoRow* def = nullptr;
  for (const InfoRow& r : rows) {
    if (r.label == "hp") hp = &r;
    if (r.label == "def") def = &r;
  }
  CHECK(hp != nullptr && def != nullptr);
  if (hp && def) {
    CHECK_STR(hp->note, "(+7)");
    CHECK_STR(def->note, "(+1)");
    CHECK(hp->valueColor.g > hp->valueColor.r);  // green: more than the unit's own
    CHECK(hp->value.find('/') != std::string::npos);  // "current/maximum"
  }
}

TEST(player_skills_pages_exist_only_where_the_game_has_something_to_show) {
  World w;  // skills and items in this game
  CHECK_STR(titles(buildUnitInfoPages(w.b(), w.id("eirik"))), "Stats,Skills,Items");
  // A game without skills or inventory: the Stats page only.
  fixture::World plain({"...."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 3, 0, false}});
  CHECK(plain.begin());
  CHECK_STR(titles(buildUnitInfoPages(plain.battle, 0)), "Stats");
}

TEST(player_skills_pages_draw_inside_a_small_box_and_cut_with_more) {
  World w;
  Unit& eirik = w.unit("eirik");
  UnitInfoPages pages = buildUnitInfoPages(w.b(), w.id("eirik"));
  Recorder r;
  // Tall box: every skill is named. 6 skills.
  pages[1].draw(r, 0, 0, 200, 200, 8);
  CHECK(r.has("Steady Recovery") && r.has("Bold") && r.has("Blade Focus"));
  CHECK(!r.has("+1 more"));
  // A box two skills high: the rest are summed up.
  Recorder small;
  pages[1].draw(small, 0, 0, 200, 40, 8);
  bool more = false;
  for (const std::string& t : small.texts) more |= t.size() > 6 && t.compare(t.size() - 5, 5, " more") == 0;
  CHECK(more);
  CHECK(!small.has("Blade Focus"));
  // The stats page puts many rows into columns instead of dropping them.
  Recorder tight;
  pages[0].draw(tight, 0, 0, 240, 30, 8);
  CHECK(tight.has("Class"));
  CHECK(tight.has("mov"));
  // A unit without skills or items says so.
  eirik.learned.clear();
  fixture::World plain({"...."}, {{"soldier", 0, 0, 0, false}, {"soldier", 1, 3, 0, false}});
  CHECK(plain.begin());
  Recorder none;
  drawItemRows(none, 0, 0, 100, 40, 8, {});
  drawSkillRows(none, 0, 0, 100, 40, 8, {});
  CHECK(none.has("No items") && none.has("No skills"));
}

TEST(player_skills_view_shows_a_skill_name_and_the_hurt) {
  World w;
  BattleView view;
  view.init(w.b(), 24, true);
  const int eirik = w.id("eirik"), gorm = w.id("gorm"), heavy = w.skill("heavy_blow");
  const int hpBefore = view.units[(size_t)gorm].hp;
  view.push({ev(EventType::SkillActivated, eirik, heavy, gorm), ev(EventType::UnitHurt, gorm, 3, eirik)});
  bool label = false, hurt = false;
  int lowest = hpBefore;
  for (int i = 0; i < 200 && (i == 0 || view.busy()); ++i) {  // play it out, watching the floating texts
    view.update(0.05, w.b());
    lowest = std::min(lowest, view.units[(size_t)gorm].hp);
    for (const FloatText& f : view.floats) {
      label |= f.text == "Heavy Blow";
      hurt |= f.text == "-3";
    }
  }
  CHECK(label);
  CHECK(hurt);
  CHECK(!view.busy());
  CHECK(lowest < hpBefore);  // its shown hit points ran down (the view re-reads the rules when the queue ends)
}

TEST(player_skills_view_lists_a_learned_skill_in_the_level_up_box) {
  World w;
  BattleView view;
  view.init(w.b(), 24, false);  // not autoplay: the box waits for Confirm
  const int eirik = w.id("eirik");
  Event up = ev(EventType::LevelUp, eirik, 4);
  up.data.assign(w.db().stats.size(), 0);
  view.push({up, ev(EventType::SkillLearned, eirik, w.skill("heavy_blow")), ev(EventType::SkillLearned, eirik, w.skill("tough")),
             ev(EventType::SkillLearned, w.id("lyra"), w.skill("bold"))});
  view.update(0.1, w.b());
  CHECK(view.levelUp.active);
  std::string lines;
  for (const std::string& l : view.levelUp.lines) lines += l + "|";
  CHECK(lines.find("New skill: Heavy Blow") != std::string::npos);
  CHECK(lines.find("New skill: Tough") != std::string::npos);
  CHECK(lines.find("Bold") == std::string::npos);  // another unit's: not in Eirik's box
  view.skip(w.b());                                 // Confirm closes the box
  bool learned = false;  // Lyra's entry plays on its own (a label over her)
  for (int i = 0; i < 200 && (i == 0 || view.busy()); ++i) {
    view.update(0.05, w.b());
    for (const FloatText& f : view.floats) learned |= f.text == "Learned Bold";
  }
  CHECK(learned);
  CHECK(!view.busy());
}
