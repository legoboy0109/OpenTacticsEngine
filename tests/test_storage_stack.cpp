// The shared storage's lists stack identical copies into one row with a count (engine/player/battle/storage_stack.h) and
// the row maps back to one real copy; the count is never cut when a name has to be shortened (ui/text.h fitNameCount).
#include "battle/storage_stack.h"
#include "core/battle/battle.h"
#include "fake_renderer.h"
#include "test.h"
#include "ui/text.h"

using namespace ote;
using ote_test::FakeRenderer;

namespace {
ItemGrant g(int item, int uses, bool drop = false) {
  ItemGrant x;
  x.item = item;
  x.uses = uses;
  x.drop = drop;
  return x;
}
}  // namespace

TEST(storage_stack_identical_copies_are_one_row_with_a_count) {
  std::vector<ItemGrant> s = {g(0, 3), g(0, 3), g(1, -1), g(0, 3), g(1, -1)};
  std::vector<StorageRow> r = stackStorage(s);
  CHECK_EQ((int)r.size(), 2);
  CHECK_EQ(r[0].item, 0);
  CHECK_EQ(r[0].count, 3);
  CHECK_EQ(r[1].item, 1);
  CHECK_EQ(r[1].count, 2);
  CHECK_EQ((int)stackStorage(std::vector<ItemGrant>()).size(), 0);
}

TEST(storage_stack_state_makes_its_own_row_ordered_by_item_then_fuller_first) {
  // A half-used copy, a drop copy and an unlimited one are each their own row.
  std::vector<ItemGrant> s = {g(2, 1), g(0, 1), g(0, 3), g(0, 3, true), g(0, -1), g(0, 3), g(2, 5)};
  std::vector<StorageRow> r = stackStorage(s);
  CHECK_EQ((int)r.size(), 6);
  // item 0: unlimited, 3 (x2), 3 drop, 1; then item 2: 5, 1
  CHECK(r[0].item == 0 && r[0].uses == -1);
  CHECK(r[1].item == 0 && r[1].uses == 3 && !r[1].drop && r[1].count == 2);
  CHECK(r[2].item == 0 && r[2].uses == 3 && r[2].drop && r[2].count == 1);
  CHECK(r[3].item == 0 && r[3].uses == 1);
  CHECK(r[4].item == 2 && r[4].uses == 5);
  CHECK(r[5].item == 2 && r[5].uses == 1);
}

TEST(storage_stack_a_row_maps_to_the_first_matching_copy) {
  std::vector<ItemGrant> s = {g(1, 2), g(0, 3), g(1, 2), g(0, 3), g(1, 1)};
  std::vector<StorageRow> r = stackStorage(s);
  CHECK_EQ((int)r.size(), 3);
  CHECK_EQ(r[0].index, 1);  // item 0, 3 uses
  CHECK_EQ(r[1].index, 0);  // item 1, 2 uses
  CHECK_EQ(r[2].index, 4);  // item 1, 1 use
  // Taking one copy leaves the same rows with the count down; the last copy removes the row.
  s.erase(s.begin() + r[1].index);
  r = stackStorage(s);
  CHECK_EQ((int)r.size(), 3);
  CHECK(r[1].item == 1 && r[1].count == 1);
  s.erase(s.begin() + r[1].index);
  r = stackStorage(s);
  CHECK_EQ((int)r.size(), 2);
  // Giving one back merges into the matching row.
  s.push_back(g(0, 3));
  r = stackStorage(s);
  CHECK(r[0].item == 0 && r[0].count == 3 && storageRowOf(r, 0, 3, false) == 0);
  CHECK_EQ(storageRowOf(r, 7, 1, false), -1);
}

TEST(storage_stack_works_on_battle_item_stacks_too) {
  std::vector<ItemStack> s(3);
  s[0].item = 4;
  s[1].item = 4;
  s[2].item = 4;
  s[2].uses = 2;
  std::vector<StorageRow> r = stackStorage(s);
  CHECK_EQ((int)r.size(), 2);
  CHECK(r[0].uses == -1 && r[0].count == 2 && r[1].uses == 2);
}

TEST(storage_stack_the_count_fits_the_row_by_shortening_the_name) {
  FakeRenderer r;
  CHECK(fitNameCount(r, "Vulnerary", 3, 400) == "Vulnerary  x3");
  CHECK(fitNameCount(r, "Vulnerary", 1, 400) == "Vulnerary");  // a single copy shows no count
  for (int w = 48; w < 140; w += 7) {
    const std::string t = fitNameCount(r, "Silver Lance of the North", 12, w);
    CHECK(textW(r, t) <= w);
    CHECK(t.size() >= 4 && t.substr(t.size() - 4) == " x12");  // the count stays whole
  }
  // No room even for ".. x12": the count goes rather than overflowing.
  CHECK(textW(r, fitNameCount(r, "Potion", 12, 6)) <= 6);
}
