// The player's supports (docs/SRPG.md step U5), the parts that need no window: the words, the rows of the
// Supports page and of the camp, the Supports page itself, and the view's label and banner. The menu entry, the
// camp scene and the real flow are covered by tests/srpg_player_supports_test.py.
#include "battle/battle_view.h"
#include "battle/support_text.h"
#include "battle/unit_info.h"
#include "core/battle/support_rules.h"
#include "fake_renderer.h"
#include "srpg_supports_fixture.h"

using namespace suptest;

namespace {

// A fake renderer that remembers the text drawn.
struct TextRenderer : ote_test::FakeRenderer {
  mutable std::vector<std::string> drawn;
  void drawText(int, int, const char* t, ote::Color, int) override { drawn.push_back(t); }
  bool has(const std::string& s) const {
    for (const std::string& d : drawn)
      if (d.find(s) != std::string::npos) return true;
    return false;
  }
};

const Lv kLevel1{10, bonus(10, 5), {"eirik: You never miss.", "lyra: I miss plenty."}};
const Lv kLevel2{30, bonus(20, 10), {}};

// eirik and lyra side by side, a pair with two levels (the first has a conversation) and "levelNames" C, B.
void setUpPair(SupWorld& w, int& pair) {
  w.points(5);
  w.db.supports.rules.levelNames = {"C", "B"};
  pair = w.pair("eirik", "lyra", {kLevel1, kLevel2});
}

}  // namespace

TEST(supports_player_words) {
  int p = 0;
  SupWorld w = trio();
  setUpPair(w, p);
  CHECK(w.begin());
  CHECK(supportPairName(w.db, p) == std::string("eirik and lyra"));
  CHECK(supportMenuLabel("Lyra") == std::string("Support: Lyra"));
  CHECK(supportBannerText(w.db, p, 2) == std::string("eirik and lyra: support level B"));
  CHECK(supportPairName(w.db, 9) == std::string("?"));
}

TEST(supports_player_rows_show_level_bar_and_ready) {
  int p = 0;
  SupWorld w = trio();
  setUpPair(w, p);
  CHECK(w.begin());
  w.battle.supports.points[0] = 4;
  std::vector<SupportRow> rows = supportRows(w.db, w.battle.supports, w.db.findCharacter("lyra"));
  CHECK_EQ(rows.size(), (size_t)1);
  CHECK(rows[0].partner == std::string("eirik"));
  CHECK(supportLevelText(rows[0]) == std::string("No support yet"));
  CHECK(!rows[0].ready);
  CHECK(supportBarFraction(rows[0]) > 0.39f && supportBarFraction(rows[0]) < 0.41f);  // 4 of 10
  w.battle.supports.points[0] = 12;  // level 1 has a conversation: ready, the level is not gained yet
  rows = supportRows(w.db, w.battle.supports, w.db.findCharacter("lyra"));
  CHECK(rows[0].ready);
  CHECK_EQ(rows[0].level, 0);
  w.battle.supports.points[0] = 20;  // level 1 played: the bar runs from 10 to 30
  w.battle.supports.level[0] = 1;
  rows = supportRows(w.db, w.battle.supports, w.db.findCharacter("eirik"));
  CHECK(supportLevelText(rows[0]) == std::string("Level C"));
  CHECK(supportBarFraction(rows[0]) > 0.49f && supportBarFraction(rows[0]) < 0.51f);
  CHECK(!rows[0].ready);
  w.battle.supports.points[0] = 50;  // level 2 has no conversation, so it would be gained at once; the state
  w.battle.supports.level[0] = 2;    // says it was: the top
  rows = supportRows(w.db, w.battle.supports, w.db.findCharacter("eirik"));
  CHECK(rows[0].top);
  CHECK(supportBarFraction(rows[0]) == 1.0f);
  CHECK(supportRows(w.db, w.battle.supports, w.db.findCharacter("gorm")).empty());  // in no pair
  SupportState bad;
  CHECK(supportRows(w.db, bad, 0).empty());  // a state that does not fit the game
}

TEST(supports_player_camp_rows_name_the_level_each_conversation_unlocks) {
  int p = 0;
  SupWorld w = trio();
  setUpPair(w, p);
  CHECK(w.begin());
  std::vector<CampRow> rows = campRows(w.db, w.battle.supports, {p, 7});  // 7 is not a pair: left out
  CHECK_EQ(rows.size(), (size_t)1);
  CHECK_EQ(rows[0].pair, p);
  CHECK(rows[0].label == std::string("eirik and lyra"));
  CHECK(rows[0].extra == std::string("Level C"));
  CHECK(campRows(w.db, w.battle.supports, {}).empty());
}

TEST(supports_player_page_exists_only_for_a_unit_in_a_pair) {
  int p = 0;
  SupWorld w = trio();
  setUpPair(w, p);
  CHECK(w.begin());
  w.battle.supports.points[0] = 12;
  ote::UnitInfoPages pages;
  ote::addSupportsPage(pages, w.battle, w.unitOf("lyra"));
  CHECK_EQ(pages.size(), (size_t)1);
  CHECK(pages[0].title == std::string("Supports"));
  TextRenderer r;
  pages[0].draw(r, 0, 0, 300, 120, 10);
  CHECK(r.has("eirik"));
  CHECK(r.has("No support yet"));
  CHECK(r.has("Conversation ready"));
  ote::UnitInfoPages none;
  ote::addSupportsPage(none, w.battle, w.unitOf("gorm"));  // a character in no pair
  CHECK(none.empty());
  ote::UnitInfoPages plain;
  fixture::World g({"...."}, {{"soldier", 0, 0, 0, false}});
  CHECK(g.begin());
  ote::addSupportsPage(plain, g.battle, 0);  // a game without supports
  CHECK(plain.empty());
}

TEST(supports_player_page_that_does_not_fit_the_height_says_how_many_more) {
  SupWorld w = SupWorld({{"soldier", 0, 1, 1, false}, {"soldier", 0, 2, 1, false}, {"soldier", 0, 0, 2, false},
                         {"soldier", 0, 3, 1, false}},
                        {"eirik", "lyra", "gorm", "bram"});
  w.pair("eirik", "lyra", {kLevel2});
  w.pair("eirik", "gorm", {kLevel2});
  w.pair("eirik", "bram", {kLevel2});
  CHECK(w.begin());
  ote::UnitInfoPages pages;
  ote::addSupportsPage(pages, w.battle, w.unitOf("eirik"));
  CHECK_EQ(pages.size(), (size_t)1);
  TextRenderer r;
  pages[0].draw(r, 0, 0, 300, 50, 10);  // room for two blocks of 28 pixels
  CHECK(r.has("more"));
}

TEST(supports_player_view_labels_a_ready_pair_and_banners_a_level) {
  int p = 0;
  SupWorld w = trio();
  setUpPair(w, p);
  CHECK(w.begin());
  ote::BattleView view;
  view.init(w.battle, 24, true);
  ote::Event ready;
  ready.type = ote::EventType::SupportPoints;
  ready.a = w.unitOf("eirik");
  ready.b = w.unitOf("lyra");
  ready.c = p;
  ready.d = 12;
  ready.flags = ote::kReady;
  view.push({ready});
  view.update(0.0, w.battle);
  CHECK_EQ(view.floats.size(), (size_t)2);  // one over each unit
  if (view.floats.size() == 2) CHECK(view.floats[0].text == std::string("Support!"));
  view.update(1.0, w.battle);
  CHECK(!view.busy());

  view.floats.clear();
  ote::Event plain = ready;  // a plain gain shows nothing
  plain.flags = 0;
  view.push({plain});
  view.update(0.0, w.battle);
  CHECK(view.floats.empty());
  CHECK(!view.busy());

  ote::Event reached;
  reached.type = ote::EventType::SupportReached;
  reached.a = ready.a;
  reached.b = ready.b;
  reached.c = p;
  reached.d = 1;
  view.push({reached});
  view.update(0.0, w.battle);
  CHECK(view.banner == std::string("eirik and lyra: support level C"));
  view.update(5.0, w.battle);
  CHECK(view.banner.empty());
  CHECK(!view.busy());
}
