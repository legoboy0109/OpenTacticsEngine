#include "battle/menu_hide.h"

namespace ote {

bool staffEntryHidden(bool hide, const std::vector<StaffOption>& opts) {
  if (!hide || opts.empty()) return false;
  for (const StaffOption& o : opts)
    if (o.ok || !o.noTarget) return false;
  return true;
}

}  // namespace ote
