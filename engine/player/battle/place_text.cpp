// The wording logic of place_text.h. (Named like a unit-test-friendly file: no scene, no drawing.)
#include "battle/place_text.h"

namespace ote {

std::string placeReasonText(const std::string& why) {
  if (why.find("needs a key") != std::string::npos) return PlaceWords::kNeedsKey;
  if (why.find("carry more") != std::string::npos) return PlaceWords::kPackFull;
  if (why.find("locked") != std::string::npos) return PlaceWords::kLockedShort;
  return why;
}

// "door" is the default word (the strings are then exactly PlaceWords::kBreak, kPickDoor and kNotBreakable).
std::string placeBreakLabel(const std::string& word) { return word.empty() ? std::string("Break") : "Break " + word; }
std::string placePickBreakHint(const std::string& word) { return "Pick " + (word.empty() ? std::string("something") : "a " + word) + " to break"; }
std::string placeNotBreakableText(const std::string& word) { return word.empty() ? std::string("Cannot be broken") : "Not a " + word + " you can break"; }

std::string placeBreakWord(const Battle& b, const std::vector<PlaceAttack>& list) {
  std::string word;
  bool first = true;
  for (const PlaceAttack& a : list) {
    const PlaceDef& p = b.placeOf(a.structure);
    const std::string w = p.word.empty() ? std::string("door") : p.word;
    if (first) word = w;
    else if (w != word) return "";
    first = false;
  }
  return word;
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
