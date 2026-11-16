// The player's part of docs/SRPG.md step U3 (abilities and summons): the words, the preview of where units end up
// (compared with what the core really does, on the square grid and on both hexagon layouts, in every direction), the
// battle view's AbilityUsed, and the Abilities page of Unit details (with a renderer that records what is drawn).
// The menu and the targeting mode need a window; tests/srpg_player_abilities_test.py plays them.
#include "abilities_fixture.h"
#include "battle/ability_preview.h"
#include "battle/ability_text.h"
#include "battle/battle_view.h"
#include "battle/unit_info.h"
#include "fake_renderer.h"

using namespace ote;
using namespace abtest;

namespace {

// The target (unit 1, red) stands next to the user (unit 0, blue) in direction `d`; `extra` units follow.
struct Pair {
  Grid g;
  XY u, t;
  AbWorld w;
  int ab = -1;
  Pair(Kind k, int d, const AbilityDef& def, bool fog = false, const std::vector<UnitSpec>& extra = {})
      : g(gridOf(k, 9, 9)), u{4, 4}, t(step(g, u, d)), w(k, fog, units(u, t, extra)) {
    ab = w.give("soldier", def);
    CHECK(w.begin());
  }
  static std::vector<UnitSpec> units(XY user, XY target, std::vector<UnitSpec> extra) {
    std::vector<UnitSpec> out = {{"soldier", 0, user.x, user.y, false}, {"soldier", 1, target.x, target.y, false}};
    out.insert(out.end(), extra.begin(), extra.end());
    return out;
  }
};

// Where the preview says a unit ends (its last path square), kNoCell = it does not move.
Cell previewEnd(const Battle& b, int user, int ab, int target, int unit) {
  for (const AbilityMovePreview& m : previewAbilityMoves(b, user, ab, target))
    if (m.unit == unit && !m.path.empty()) return m.path.back();
  return kNoCell;
}

// A renderer that records the text it is asked to draw (with the 6 px per character of the fake one).
class RecordingRenderer : public ote_test::FakeRenderer {
 public:
  struct Line { int x, y; std::string text; };
  void drawText(int x, int y, const char* text, ote::Color, int) override { lines.push_back({x, y, text}); }
  bool has(const std::string& part) const {
    for (const Line& l : lines)
      if (l.text.find(part) != std::string::npos) return true;
    return false;
  }
  std::vector<Line> lines;
};

}  // namespace

TEST(abilities_player_the_effects_are_described_in_plain_words) {
  AbWorld w(Square, false, {{"soldier", 0, 1, 1, false}});
  const Database& db = w.db;
  AbilityDef d = ability("x", AbilityTarget::Enemy, AbilityEffect::Push, 1);
  CHECK(abilityEffectText(db, d) == std::string("Pushes it 1 square away"));
  d.amount = 3;
  CHECK(abilityEffectText(db, d) == std::string("Pushes it 3 squares away"));
  CHECK(abilityEffectText(db, ability("x", AbilityTarget::Enemy, AbilityEffect::Pull)) == std::string("Pulls it next to you"));
  CHECK(abilityEffectText(db, ability("x", AbilityTarget::Ally, AbilityEffect::Swap)) == std::string("Swaps places with it"));
  CHECK(abilityEffectText(db, ability("x", AbilityTarget::Enemy, AbilityEffect::MoveBehind)) == std::string("Moves it behind you"));
  CHECK(abilityEffectText(db, ability("x", AbilityTarget::Ally, AbilityEffect::Heal, 10)) == std::string("Heals 10 HP"));
  CHECK(abilityEffectText(db, ability("x", AbilityTarget::Self, AbilityEffect::Heal, kHealFull)) == std::string("Heals all HP"));
  AbilityDef s = AbWorld::summon("call", db.findUnitType("soldier"), 2);
  s.summon.lifetime = 3;
  s.summon.level = 4;
  CHECK(abilityEffectText(db, s) == std::string("Calls a ") + db.unitTypes[(size_t)db.findUnitType("soldier")].name +
                                         " (level 4, up to 2 at a time, lasts 3 turns, waits a turn)");
  CHECK(abilityTargetText(d) == std::string("an enemy"));
  CHECK(abilityTargetText(ability("x", AbilityTarget::Cell, AbilityEffect::Summon)) == std::string("a square"));
  CHECK(abilityRangeText(d) == std::string("range 1"));
  d.maxRange = 3;
  CHECK(abilityRangeText(d) == std::string("range 1-3"));
  CHECK(abilityRangeText(ability("x", AbilityTarget::Self, AbilityEffect::Heal, 1)) == std::string());
  d.free = true;
  d.uses = 2;
  d.cooldown = 1;
  d.afterMove = false;
  CHECK(abilityCostText(d) == std::string("free"));
  CHECK(abilityLimitText(d) == std::string("2 uses per battle, ready again after 1 turn, only before moving"));
  CHECK(abilityShortReason("can only be used before moving") == std::string("before moving"));
  CHECK(abilityShortReason("already has 2 summoned Hounds") == std::string("limit reached"));
  CHECK(abilityShortReason("ready in 2 turns") == std::string("ready in 2 turns"));
}

TEST(abilities_player_menu_text_is_the_reason_or_the_uses_left) {
  AbilityDef d = ability("shove", AbilityTarget::Enemy, AbilityEffect::Push, 1);
  d.uses = 2;
  d.cooldown = 2;
  Pair p(Square, 0, d);
  const Unit& user = p.w.un(0);
  AbilityOption o;
  o.ability = p.ab;
  o.ok = true;
  CHECK(abilityMenuExtra(p.w.battle, user, o) == std::string("2 left"));
  CHECK(abilityStatusText(p.w.battle, user, p.ab) == std::string("2 of 2 uses left"));
  CHECK(p.w.use(0, p.ab, 1));
  CHECK(abilityMenuExtra(p.w.battle, user, o) == std::string("1 left"));
  CHECK(abilityStatusText(p.w.battle, user, p.ab) == std::string("1 of 2 uses left, ready in 2 turns"));
  o.ok = false;
  o.why = "ready in 2 turns";
  CHECK(abilityMenuExtra(p.w.battle, user, o) == std::string("ready in 2 turns"));
  // An unlimited ability that can be used says nothing on the right.
  Pair q(Square, 0, ability("shove", AbilityTarget::Enemy, AbilityEffect::Push, 1));
  o.ok = true;
  o.ability = q.ab;
  CHECK(abilityMenuExtra(q.w.battle, q.w.un(0), o) == std::string());
}

TEST(abilities_player_preview_matches_what_the_core_does_everywhere) {
  const AbilityEffect effects[] = {AbilityEffect::Push, AbilityEffect::Pull, AbilityEffect::Swap, AbilityEffect::MoveBehind};
  for (Kind k : kKinds) {
    const int dirs = gridOf(k, 9, 9).directionCount();
    for (AbilityEffect e : effects)
      for (int d = 0; d < dirs; ++d) {
        Pair p(k, d, ability("m", AbilityTarget::Enemy, e, e == AbilityEffect::Push ? 2 : 0));
        const Cell userFrom = p.w.cellOf(0), targetFrom = p.w.cellOf(1);
        const Cell wantTarget = previewEnd(p.w.battle, 0, p.ab, 1, 1), wantUser = previewEnd(p.w.battle, 0, p.ab, 1, 0);
        CHECK(p.w.use(0, p.ab, 1));
        const Cell gotTarget = p.w.cellOf(1), gotUser = p.w.cellOf(0);
        // The preview names an end square for every unit that really moved, and the same one.
        CHECK_EQ(wantTarget == kNoCell ? targetFrom : wantTarget, gotTarget);
        CHECK_EQ(wantUser == kNoCell ? userFrom : wantUser, gotUser);
        if (e != AbilityEffect::Push) CHECK(gotTarget != targetFrom);
      }
  }
}

TEST(abilities_player_preview_stops_before_a_unit_in_the_way_and_ignores_hidden_ones) {
  const AbilityDef push = ability("shove", AbilityTarget::Enemy, AbilityEffect::Push, 3);
  {  // a unit two squares behind the target: a push of 3 goes one square (the user at 4,4, the target at 5,4)
    Pair p(Square, 1, push, false, {{"soldier", 0, 7, 4, false}});  // direction 1 = east
    const AbilityMovePreview m = previewAbilityMoves(p.w.battle, 0, p.ab, 1).at(0);
    CHECK_EQ((int)m.path.size(), 1);
    CHECK_EQ(m.path.back(), p.w.at(6, 4));
  }
  {  // the same in fog, the blocker is a hidden enemy in the forest: the player cannot know, so the preview goes through
    AbWorld w(Square, true, {{"blind", 0, 1, 1, false}, {"blind", 1, 2, 1, false}, {"blind", 1, 4, 1, false}});
    w.forestHides();
    int ab = w.give("blind", push);
    CHECK(w.begin());
    // (the real result is checked by the core's own tests; here: nothing about the hidden unit leaks into the path)
    std::vector<AbilityMovePreview> m = previewAbilityMoves(w.battle, 0, ab, 1);
    CHECK(!m.empty());
    for (Cell c : m[0].path) CHECK(w.battle.unitAt(c) < 0 || !w.battle.visibleTo(0, c));
  }
  // heal and summon move nobody
  Pair h(Square, 0, ability("mend", AbilityTarget::Enemy, AbilityEffect::Heal, 5));
  CHECK(previewAbilityMoves(h.w.battle, 0, h.ab, 1).empty());
}

TEST(abilities_player_view_shows_the_name_and_says_blocked) {
  Pair p(Square, 1, ability("shove", AbilityTarget::Enemy, AbilityEffect::Push, 1));
  BattleView view;
  view.init(p.w.battle, 24, true);
  Event e;
  e.type = EventType::AbilityUsed;
  e.a = 0;
  e.b = p.ab;
  e.c = 1;
  e.d = -1;
  view.push({e});
  view.update(0.0, p.w.battle);
  CHECK(view.busy());  // it takes a moment, unlike the no-op of U0
  CHECK_EQ((int)view.floats.size(), 1);
  CHECK(view.floats[0].text == std::string("shove"));
  view.update(1.0, p.w.battle);
  CHECK(!view.busy());
  // A blocked one also names the unit in the way, over its square.
  view.floats.clear();
  e.flags = kBlocked;
  e.data = {1};
  view.push({e});
  view.update(0.0, p.w.battle);
  CHECK_EQ((int)view.floats.size(), 2);
  CHECK(view.floats[1].text == std::string(AbilityWords::kBlocked));
  view.update(1.0, p.w.battle);
  CHECK(!view.busy());
  // An event with an ability that does not exist ends at once and shows nothing.
  view.floats.clear();
  e.b = 99;
  e.flags = 0;
  view.push({e});
  view.update(0.0, p.w.battle);
  CHECK(!view.busy());
  CHECK(view.floats.empty());
}

TEST(abilities_player_page_lists_the_abilities_and_fits_the_box) {
  AbilityDef shove = ability("shove", AbilityTarget::Enemy, AbilityEffect::Push, 2);
  shove.name = "Shove";
  shove.description = "Knocks a neighbour back.";
  shove.uses = 2;
  Pair p(Square, 0, shove);
  AbilityDef rest = ability("rest", AbilityTarget::Self, AbilityEffect::Heal, kHealFull);
  rest.name = "Rest";
  rest.free = true;
  rest.cooldown = 2;
  p.w.give("soldier", rest);  // the pages are built from the type's list when the details open
  UnitInfoView v;
  UnitInfoPages only;
  addAbilitiesPage(only, p.w.battle, 0);
  CHECK_EQ((int)only.size(), 1);  // a unit with no ability gets none (below)
  v.openPages(0, only);  // the page alone: the other steps' pages stand before it in the real details
  RecordingRenderer r;
  v.draw(r, p.w.battle, 8);
  CHECK(r.has("Abilities"));
  CHECK(r.has("Shove"));
  CHECK(r.has("Pushes it 2 squares away"));
  CHECK(r.has("Use on an enemy, range 1; ends the turn"));
  CHECK(r.has("2 uses per battle"));
  CHECK(r.has("Knocks a neighbour back."));
  // Nothing is drawn past the right edge of the box (6 px per character in the fake) or the screen.
  for (const RecordingRenderer::Line& l : r.lines) CHECK(l.x + (int)l.text.size() * 6 <= r.width() - 4);
  // The page of a unit with no abilities does not exist.
  p.w.db.unitTypes[(size_t)p.w.db.findUnitType("soldier")].abilities.clear();
  UnitInfoPages pages;
  addAbilitiesPage(pages, p.w.battle, 1);
  CHECK(pages.empty());
}

TEST(abilities_player_page_cuts_a_long_list_with_a_more_line) {
  AbWorld w(Square, false, {{"soldier", 0, 1, 1, false}});
  for (int i = 0; i < 8; ++i) {
    AbilityDef d = ability(("a" + std::to_string(i)).c_str(), AbilityTarget::Self, AbilityEffect::Heal, 5);
    d.name = "Ability " + std::to_string(i);
    d.description = "Some words about it that need a line or two on a small screen.";
    w.give("soldier", d);
  }
  CHECK(w.begin());
  UnitInfoPages pages;
  addAbilitiesPage(pages, w.battle, 0);
  CHECK_EQ((int)pages.size(), 1);
  RecordingRenderer r;
  pages[0].draw(r, 0, 0, 200, 100, 8);
  CHECK(r.has("Ability 0"));
  CHECK(r.has(AbilityWords::kMore));
  for (const RecordingRenderer::Line& l : r.lines) CHECK(l.y + 8 <= 100);  // never below the box
}
