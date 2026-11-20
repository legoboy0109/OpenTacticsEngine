// Presentation of the shared storage: identical copies shown as ONE row with a count ("Potion  x3"). The storage itself
// (Battle's SideState::storage, a campaign's CarriedSide::storage, the saves) stays one entry per copy; only the lists
// the player sees are grouped, and each row remembers a real storage position so choosing it acts on one real copy.
//
// Identical = same item, same uses left and same drop flag (every per-copy field of ItemStack / ItemGrant).
// Row order: the game's item order (item index), then fuller copies first (unlimited, then most uses left), then the
// non-drop copies before the drop ones. `index` is the FIRST (lowest) position among the copies in the storage vector, so a
// row always maps to the same copy and the identical copies leave the vector in the same order whichever is taken.
#pragma once
#include <algorithm>
#include <vector>

namespace ote {

struct StorageRow {
  int item = 0;
  int uses = -1;
  bool drop = false;
  int count = 0;  // copies in this row
  int index = 0;  // storage position of the first of them
};

// T is ItemStack or ItemGrant (item, uses, drop).
template <class T>
std::vector<StorageRow> stackStorage(const std::vector<T>& storage) {
  std::vector<StorageRow> rows;
  for (size_t i = 0; i < storage.size(); ++i) {
    const T& s = storage[i];
    auto same = std::find_if(rows.begin(), rows.end(),
                             [&](const StorageRow& r) { return r.item == s.item && r.uses == s.uses && r.drop == s.drop; });
    if (same != rows.end()) {
      ++same->count;
      continue;
    }
    StorageRow r;
    r.item = s.item;
    r.uses = s.uses;
    r.drop = s.drop;
    r.count = 1;
    r.index = (int)i;
    rows.push_back(r);
  }
  std::stable_sort(rows.begin(), rows.end(), [](const StorageRow& a, const StorageRow& b) {
    if (a.item != b.item) return a.item < b.item;
    if (a.uses != b.uses) return a.uses < 0 || (b.uses >= 0 && a.uses > b.uses);  // unlimited first, then most uses left
    return !a.drop && b.drop;
  });
  return rows;
}

// The row that holds copies in this state (item, uses, drop), or -1: puts the cursor back on the row of a copy just moved.
inline int storageRowOf(const std::vector<StorageRow>& rows, int item, int uses, bool drop) {
  for (size_t i = 0; i < rows.size(); ++i)
    if (rows[i].item == item && rows[i].uses == uses && rows[i].drop == drop) return (int)i;
  return -1;
}

}  // namespace ote
