// The art memory budget's rule (battle/art_budget.h): least recently used goes first, nothing used this frame goes.
#include "battle/art_budget.h"
#include "test.h"

using namespace ote;

TEST(art_budget_counts_bytes) {
  ArtBudget b;
  b.reset(4, 100);
  CHECK_EQ(b.total(), 0u);
  b.setBytes(0, 30);
  b.setBytes(2, 50);
  CHECK_EQ(b.total(), 80u);
  b.setBytes(0, 40);  // replaced, not added
  CHECK_EQ(b.total(), 90u);
  b.setBytes(2, 0);
  CHECK_EQ(b.total(), 40u);
  CHECK_EQ(b.bytes(0), 40u);
  CHECK(b.pickVictims().empty());  // within budget: nothing to give back
}

TEST(art_budget_evicts_least_recently_used_first) {
  ArtBudget b;
  b.reset(4, 100);
  for (size_t i = 0; i < 4; ++i) {
    b.use(i);
    b.setBytes(i, 40);  // 160 held, 100 allowed
  }
  b.beginFrame();
  b.use(1);  // used again: 0, 2, 3 are older, in that order
  b.beginFrame();
  std::vector<int> v = b.pickVictims();
  CHECK_EQ(v.size(), 2u);  // 160 -> 120 -> 80
  CHECK_EQ(v[0], 0);
  CHECK_EQ(v[1], 2);
  CHECK_EQ(b.total(), 80u);
  CHECK_EQ(b.bytes(0), 0u);  // the victims are zeroed here: the caller frees what they held
  CHECK_EQ(b.bytes(1), 40u);
  CHECK(b.pickVictims().empty());
}

TEST(art_budget_never_evicts_what_the_frame_uses) {
  ArtBudget b;
  b.reset(3, 50);
  for (size_t i = 0; i < 3; ++i) {
    b.use(i);
    b.setBytes(i, 40);
  }
  // All three used in this frame: over budget, but nothing may go (a texture handed out stays valid until the frame ends).
  CHECK(b.pickVictims().empty());
  CHECK_EQ(b.total(), 120u);
  b.beginFrame();
  b.use(2);  // only the last is used in the new frame
  std::vector<int> v = b.pickVictims();
  CHECK_EQ(v.size(), 2u);
  CHECK_EQ(b.bytes(2), 40u);
  CHECK_EQ(b.total(), 40u);
}

TEST(art_budget_no_limit_and_drop_all) {
  ArtBudget b;
  b.reset(2, 0);  // 0 = no limit
  b.setBytes(0, 1000000);
  b.setBytes(1, 1000000);
  b.beginFrame();
  CHECK(b.pickVictims().empty());
  b.use(1);
  std::vector<int> v = b.pickVictims(0);  // everything not used this frame (low memory)
  CHECK_EQ(v.size(), 1u);
  CHECK_EQ(v[0], 0);
  CHECK_EQ(b.total(), 1000000u);
}

TEST(art_budget_reports_evictions_rarely) {
  ArtBudget b;
  b.reset(1, 10);
  uint64_t n = 1, bytes = 100;
  CHECK(b.noteEvicted(0.0, 5.0, n, bytes));  // the first goes out at once
  CHECK_EQ(n, 1u);
  CHECK_EQ(bytes, 100u);
  n = 2;
  bytes = 50;
  CHECK(!b.noteEvicted(1.0, 5.0, n, bytes));  // too soon: kept for later
  n = 1;
  bytes = 25;
  CHECK(!b.noteEvicted(4.9, 5.0, n, bytes));
  n = 1;
  bytes = 25;
  CHECK(b.noteEvicted(5.0, 5.0, n, bytes));  // everything since the last report
  CHECK_EQ(n, 4u);
  CHECK_EQ(bytes, 100u);
}
