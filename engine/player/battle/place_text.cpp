// The wording logic of place_text.h. (Named like a unit-test-friendly file: no scene, no drawing.)
#include "battle/place_text.h"

namespace ote {

std::string placeReasonText(const std::string& why) {
  if (why.find("needs a key") != std::string::npos) return PlaceWords::kNeedsKey;
  if (why.find("carry more") != std::string::npos) return PlaceWords::kPackFull;
  if (why.find("locked") != std::string::npos) return PlaceWords::kLockedShort;
  return why;
}

std::vector<PlaceRow> placeInfoRows(const Battle& b, int s, int kind, bool known) {
  std::vector<PlaceRow> rows;
  if (s < 0 || s >= (int)b.structures.size() || kind < 0 || (size_t)kind >= b.db->structures.size()) return rows;
  const StructureState& st = b.structures[(size_t)s];
  const PlaceDef& p = b.db->structures[(size_t)kind].place;
  const PlaceDef& placed = b.db->structures[(size_t)st.mapType].place;
  if (p.isDoor() || p.isChest()) {
    // The lock is only known for what the viewer sees now; out of sight it is a shut door, nothing more.
    rows.push_back({PlaceWords::kStateRow, known && st.locked ? PlaceWords::kStLocked : PlaceWords::kStShut});
    if (p.isDoor() && known && p.hp > 0 && st.hp >= 0) rows.push_back({PlaceWords::kStHp, std::to_string(st.hp) + "/" + std::to_string(p.hp)});
    return rows;
  }
  if (p.isVisit()) {
    if (known && st.used && p.repeat) rows.push_back({PlaceWords::kStateRow, PlaceWords::kStVisited});
    return rows;
  }
  // A kind that was a door / chest / place and has become something else.
  if ((placed.isDoor() || placed.isChest()) && kind == placed.to) rows.push_back({PlaceWords::kStateRow, PlaceWords::kStOpen});
  else if (placed.isVisit() && kind == placed.destroyedTo) rows.push_back({PlaceWords::kStateRow, PlaceWords::kStRuined});
  else if (placed.isVisit() && kind == placed.to) rows.push_back({PlaceWords::kStateRow, PlaceWords::kStVisited});
  else if (placed.isDoor() && kind != st.mapType) rows.push_back({PlaceWords::kStateRow, PlaceWords::kStOpen});
  return rows;
}

}  // namespace ote
