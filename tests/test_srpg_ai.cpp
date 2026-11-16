// Step AI1 of docs/SRPG.md "AI": what the computer opponent does with promotion, abilities, move again and rescue, and
// that whole battles on every strategy-RPG fixture end (or at least never stall) and repeat exactly per seed.
#include <functional>

#include "abilities_summons_fixture.h"
#include "core/ai/ai.h"
#include "rescue_fixture.h"
#include "srpg_promo_fixture.h"

using namespace ote;
using savetest::Rig;

namespace {

// The side whose turn it is plays one whole turn with the AI. Fails the test if the turn does not end.
bool aiTurn(Battle& b, std::vector<Event>& events) {
  const long limit = 20L * (long)b.units.size() + 100;
  long steps = 0;
  while (aiTakeStep(b, events))
    if (++steps > limit) return false;
  return true;
}

int count(const std::vector<Event>& events, EventType t) {
  int n = 0;
  for (const Event& e : events) n += e.type == t;
  return n;
}

// The fixture promotion game with one map: `rec` (level 3, can promote by offer) and a grunt at (gx, gy).
struct PromoWorld {
  promotest::Game game;
  Rig rig;
  std::vector<Event> events;
  PromoWorld(int gx, int gy) : game("ai_promo") {
    game.put("maps/field.json", R"({ "name": "Field", "grid": { "type": "square", "width": 8, "height": 3 },
      "legend": { ".": "plain" }, "tiles": ["........", "........", "........"],
      "sides": [ { "id": "blue", "controller": "human" }, { "id": "red", "controller": "ai" } ],
      "units": [ { "id": "rec", "type": "recruit", "side": "blue", "x": 0, "y": 0, "level": 3, "leader": true },
                 { "type": "grunt", "side": "red", "x": )" + std::to_string(gx) + ", \"y\": " + std::to_string(gy) + " } ] }");
    CHECK(rig.load(game.dir, "field"));
    rig.battle.start(events);
    events.clear();
  }
};

}  // namespace

TEST(ai_takes_an_offered_promotion_when_it_has_nothing_better) {
  PromoWorld w(7, 2);
  Battle& b = w.rig.battle;
  const int rec = b.unitByStoryId("rec");
  std::vector<int> options;
  b.promotionOptions(rec, -1, options);
  CHECK(!options.empty());
  CHECK(aiTurn(b, w.events));
  CHECK_EQ(count(w.events, EventType::UnitPromoted), 1);
  CHECK_EQ(b.units[(size_t)rec].type, options[0]);  // the first one offered
  CHECK_EQ(b.activeSide, 1);
}

TEST(ai_prefers_an_attack_to_a_promotion) {
  PromoWorld w(1, 0);  // a grunt next to the recruit
  Battle& b = w.rig.battle;
  CHECK(aiTurn(b, w.events));
  CHECK_EQ(count(w.events, EventType::UnitPromoted), 0);
  CHECK(count(w.events, EventType::Attack) > 0);
}

TEST(ai_heals_a_wounded_neighbour_with_a_heal_ability) {
  abtest::AbWorld w(abtest::Square, false, {{"soldier", 0, 2, 2, false}, {"soldier", 0, 3, 2, false}, {"soldier", 1, 8, 4, false}},
                    fogtest::field(9, 5), {"a", "b", "foe"});
  const int mend = w.give("soldier", abtest::ability("mend", AbilityTarget::Ally, AbilityEffect::Heal, 5));
  CHECK(w.begin());
  w.mut(1).hp = 3;  // of 10
  w.mut(1).done = true;  // it stays where it is (otherwise it may well walk off before the healer plans)
  std::vector<Event> ev;
  CHECK(aiTurn(w.battle, ev));
  bool healed = false;
  for (const Event& e : ev) healed |= e.type == EventType::AbilityUsed && e.b == mend && e.c == 1;
  CHECK(healed);
  CHECK(w.un(1).hp > 3);
}

TEST(ai_does_not_heal_a_healthy_unit_or_use_other_abilities) {
  abtest::AbWorld w(abtest::Square, false, {{"soldier", 0, 2, 2, false}, {"soldier", 0, 3, 2, false}, {"soldier", 1, 3, 3, false}},
                    fogtest::field(9, 5), {"a", "b", "foe"});
  w.give("soldier", abtest::ability("mend", AbilityTarget::Ally, AbilityEffect::Heal, 5));
  w.give("soldier", abtest::ability("shove", AbilityTarget::Enemy, AbilityEffect::Push, 1));
  w.give("soldier", abtest::ability("swap", AbilityTarget::Ally, AbilityEffect::Swap));
  CHECK(w.begin());
  std::vector<Event> ev;
  for (int round = 0; round < 6 && !w.battle.over; ++round) {
    CHECK(aiTurn(w.battle, ev));
    CHECK(aiTurn(w.battle, ev));
  }
  CHECK_EQ(count(ev, EventType::AbilityUsed), 0);  // nobody was ever below half HP at its turn's start
}

TEST(ai_summons_next_to_the_enemy_and_never_beyond_the_limit) {
  abtest::Camp c;
  c.setup();
  Battle& b = c.w.battle;
  std::vector<Event> ev;
  CHECK(aiTurn(b, ev));  // blue: the lord walks toward the foe and calls a hound
  const int hound = c.summonOf(0);
  CHECK(hound >= 0);
  const Unit& lord = b.units[0];
  // Of every square the call could take from where the lord now stands, the hound is on the one nearest the foe.
  std::vector<int> us;
  std::vector<Cell> cells;
  int best = 1 << 20;
  for (Cell s : {b.units[(size_t)hound].cell}) best = b.grid.distance(s, b.units[1].cell);
  for (int dx = -2; dx <= 2; ++dx)
    for (int dy = -2; dy <= 2; ++dy) {
      int x = b.grid.x(lord.cell) + dx, y = b.grid.y(lord.cell) + dy;
      if (!b.grid.valid(b.grid.at(x, y)) || b.grid.distance(lord.cell, b.grid.at(x, y)) > 2) continue;
      if (b.occupant[(size_t)b.grid.at(x, y)] >= 0 && b.occupant[(size_t)b.grid.at(x, y)] != hound) continue;
      CHECK(b.grid.distance(b.grid.at(x, y), b.units[1].cell) >= best);
    }
  // Many rounds later there is still one living hound of this ability per summoner, and no turn ever stalls.
  for (int round = 0; round < 12 && !b.over; ++round) {
    CHECK(aiTurn(b, ev));
    CHECK(aiTurn(b, ev));
    CHECK(b.summonCount(0, c.call) <= 1);
    CHECK(b.summonCount(1, c.call) <= 1);
  }
}

TEST(ai_never_rescues_gives_takes_or_pairs_up) {
  for (RescueMode mode : {RescueMode::Carry, RescueMode::PairUp}) {
    rescuetest::RescueWorld w = rescuetest::line(mode);
    CHECK(w.begin());
    std::vector<Event> ev;
    for (int round = 0; round < 15 && !w.battle.over; ++round) {
      CHECK(aiTurn(w.battle, ev));
      CHECK(aiTurn(w.battle, ev));
    }
    CHECK_EQ(count(ev, EventType::UnitRescued), 0);
    CHECK_EQ(count(ev, EventType::UnitDropped), 0);
    CHECK_EQ(count(ev, EventType::PairSwitched), 0);
    for (const Unit& u : w.battle.units) CHECK(u.carrying < 0 && u.carriedBy < 0);
  }
}

TEST(ai_with_move_again_uses_what_is_left_and_the_turn_ends) {
  Rig rig;
  CHECK(rig.load(ote_test::sourceDir() + "/tests/fixtures/srpg-skills", "field"));
  Battle& b = rig.battle;
  const int again = rig.db.findSkill("strike_and_move");
  CHECK(again >= 0);
  for (Unit& u : b.units) u.learned.push_back(again);
  std::vector<Event> ev;
  b.start(ev);
  // Blue's lord has used its action in place and still has its whole move.
  b.units[0].acted = true;
  b.units[0].moveSpent = 0;
  ev.clear();
  CHECK(aiTurn(b, ev));
  CHECK(b.activeSide == 1);  // blue's turn ended: the acted unit moved or waited, it never acted again
  for (const Event& e : ev)
    if (e.type == EventType::Attack) CHECK(e.a != 0);
}

namespace {

// Two AI-played sides on one map for up to `rounds` rounds; the text of everything that happened.
std::string simulate(const std::string& dir, const std::string& map, uint32_t seed, int rounds, bool& stalled) {
  Rig rig;
  stalled = false;
  if (!rig.load(dir, map, seed)) {
    stalled = true;
    return "";
  }
  std::vector<Event> ev;
  rig.battle.start(ev);
  while (!rig.battle.over && rig.battle.round <= rounds)
    if (!aiTurn(rig.battle, ev)) {
      stalled = true;
      break;
    }
  std::string out;
  for (const Event& e : ev)
    out += std::to_string((int)e.type) + ":" + std::to_string(e.a) + "," + std::to_string(e.b) + "," + std::to_string(e.c) + "," +
           std::to_string(e.d) + ";";
  return out + savetest::stateText(rig.battle);
}

}  // namespace

TEST(ai_200_turn_sims_on_every_strategy_rpg_fixture_never_stall_and_repeat_per_seed) {
  struct Case { const char* game; const char* map; };
  const Case cases[] = {{"srpg-skills", "field"}, {"srpg-skills", "woods"}, {"srpg-chars", "field"},
                        {"srpg-chars", "plain"}, {"srpg-abilities", "field"}};
  for (const Case& c : cases)
    for (uint32_t seed = 1; seed <= 5; ++seed) {
      bool s1, s2;
      const std::string dir = ote_test::sourceDir() + "/tests/fixtures/" + c.game;
      const std::string a = simulate(dir, c.map, seed, 200, s1), b = simulate(dir, c.map, seed, 200, s2);
      CHECK(!s1 && !s2);
      CHECK(a == b);
    }
  for (uint32_t seed = 1; seed <= 5; ++seed) {  // the promotion game, with enemies near and far
    promotest::Game g("ai_promo_sim");
    bool s1, s2;
    const std::string a = simulate(g.dir, "field", seed, 200, s1), b = simulate(g.dir, "field", seed, 200, s2);
    CHECK(!s1 && !s2);
    CHECK(a == b);
  }
}
