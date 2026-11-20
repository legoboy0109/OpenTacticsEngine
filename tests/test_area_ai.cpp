// Docs/AI.md "Area attacks": the computer opponent with area weapons and line fire: it aims at an empty square that catches several
// enemies, counts a friend it would hit against the attack (also with attack weights), respects line blocking, and a whole game between
// two computer sides finishes, the same way every time.
#include "area_fixture.h"
#include "core/ai/ai.h"

using namespace ote;
using areatest::plain;
using areatest::Scene;
using areatest::U;

namespace {

// The events of the AI's steps while it is `side`'s turn (one turn at most).
void aiTurn(Scene& s, int side, int maxSteps = 40) {
  s.w->events.clear();
  for (int i = 0; i < maxSteps && !s.b().over && s.b().activeSide == side; ++i) aiTakeStep(s.b(), s.w->events);
}

int areaHits(const Scene& s) {
  int n = 0;
  for (const Event& e : s.w->events) n += e.type == EventType::Attack && (e.flags & kArea) && !(e.flags & kCounter);
  return n;
}

}  // namespace

TEST(ai_aims_an_area_weapon_at_an_empty_square_that_catches_two_enemies) {
  // The bomber's burst (the square and its four neighbours) catches both dummies only when aimed at the empty square between them.
  Scene s("ai_empty", plain(10, 5), {{"bomber", "blue", 0, 2, "b"}, {"dummy", "red", 4, 1, "a"}, {"dummy", "red", 4, 3, "c"}});
  aiTurn(s, 0);
  CHECK_EQ(areaHits(s), 2);
  CHECK(s.unit("a").hp < 12 && s.unit("c").hp < 12);
}

TEST(ai_counts_friendly_fire_against_an_area_attack) {
  // A tough enemy between two frail friends in a corridor: every shot that reaches it also kills a friend. The flaker waits instead.
  Scene s("ai_ff", {".........."}, {{"flaker", "blue", 0, 0, "f"}, {"frail", "blue", 4, 0, "x"}, {"tough", "red", 5, 0, "t"}, {"frail", "blue", 6, 0, "y"}}, false);
  aiTurn(s, 0);
  CHECK_EQ(areaHits(s), 0);
  CHECK(s.unit("x").alive && s.unit("y").alive);
  CHECK_EQ(s.unit("t").hp, 60);
}

TEST(ai_with_attack_weights_also_counts_friendly_fire_and_aims_at_squares) {
  auto weighted = [](const char* unit) {
    return [unit](skilltest::Copy& c) {
      c.swap("game.json", "\"rules\": { \"defeat\"", "\"rules\": { \"ai\": { \"weights\": { \"bold\": { \"dmg\": 3 } } }, \"defeat\"");
      c.swap("maps/field.json", std::string("\"id\": \"") + unit + "\"", std::string("\"id\": \"") + unit + "\", \"behaviour\": { \"weights\": \"bold\" }");
    };
  };
  Scene a("ai_w1", plain(10, 5), {{"bomber", "blue", 0, 2, "b"}, {"dummy", "red", 4, 1, "a"}, {"dummy", "red", 4, 3, "c"}}, false, weighted("b"));
  aiTurn(a, 0);
  CHECK_EQ(areaHits(a), 2);  // the empty square between them, by the weights too
  Scene f("ai_w2", {".........."}, {{"flaker", "blue", 0, 0, "f"}, {"frail", "blue", 4, 0, "x"}, {"tough", "red", 5, 0, "t"}, {"frail", "blue", 6, 0, "y"}}, false, weighted("f"));
  aiTurn(f, 0);
  CHECK_EQ(areaHits(f), 0);
  CHECK(f.unit("x").alive && f.unit("y").alive);
}

TEST(ai_does_not_shoot_through_a_friend_with_line_fire) {
  // The only enemy in reach is behind a friend on the same row: no rifle shot from where the row is blocked.
  Scene s("ai_block", {".........."}, {{"rifleman", "blue", 0, 0, "r"}, {"lancer", "blue", 2, 0, "ally"}, {"dummy", "red", 8, 0, "e"}});
  aiTurn(s, 0);
  CHECK_EQ(s.unit("e").hp, 12);  // the corridor is one square wide: the ally is always in the way
}

TEST(a_game_between_two_computer_sides_with_area_weapons_finishes_the_same_way_every_time) {
  auto play = [](std::string& hash, int& areaStrikes) {
    skilltest::World w("ai", ote_test::sourceDir() + "/tests/fixtures/area-fire");
    int steps = 0;
    areaStrikes = 0;
    while (!w.b().over && steps < 3000) {
      w.events.clear();
      aiTakeStep(w.b(), w.events);
      for (const Event& e : w.events) areaStrikes += e.type == EventType::Attack && (e.flags & kArea) && !(e.flags & kCounter);
      ++steps;
    }
    hash = savetest::stateText(w.b());
    return w.b().over;
  };
  std::string h1, h2;
  int s1 = 0, s2 = 0;
  CHECK(play(h1, s1));
  CHECK(play(h2, s2));
  CHECK_STR(h1, h2);
  CHECK(s1 > 0);  // the computer did use its area weapons
  CHECK_EQ(s1, s2);
}
