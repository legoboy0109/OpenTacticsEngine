// The player's words for doors, chests and visiting (docs/SRPG.md section 10 "Player"): the short greyed reasons and the state
// rows of the info panel, including what a viewer must NOT learn about a structure out of sight. Fixture: tests/fixtures/srpg-doors.
#include "battle/place_text.h"
#include "srpg_doors_fixture.h"

using namespace ote;
using namespace doorstest;

namespace {
std::string rowsText(const std::vector<PlaceRow>& rows) {
  std::string t;
  for (const PlaceRow& r : rows) t += r.label + "=" + r.value + ";";
  return t;
}
}  // namespace

TEST(player_doors_reasons_are_short) {
  CHECK(placeReasonText("it is locked and needs a key") == "Needs a key");
  CHECK(placeReasonText("the unit cannot carry more: drop something first") == "Pack full");
  CHECK(placeReasonText("it is locked") == "Locked");
  CHECK(placeReasonText("something else") == "something else");
}

TEST(player_doors_info_rows_and_fog_secrecy) {
  Doors w("pd_rows", "gatehouse");
  const int gate = w.place("g1");
  const int kind = w.st("g1").type;
  CHECK(rowsText(placeInfoRows(w.b(), gate, kind, true)) == "State=Locked;HP=20/20;");
  w.st("g1").hp = 7;
  CHECK(rowsText(placeInfoRows(w.b(), gate, kind, true)) == "State=Locked;HP=7/20;");
  // Out of sight: only a shut door, no lock, no hit points.
  CHECK(rowsText(placeInfoRows(w.b(), gate, kind, false)) == "State=Shut;");
  // Open, visited and ruined kinds say so.
  const int open = w.db().findStructure("gate_open");
  CHECK(rowsText(placeInfoRows(w.b(), gate, open, true)) == "State=Open;");
  Doors h("pd_rows2", "hall");
  const int v4 = h.place("v4");
  CHECK(rowsText(placeInfoRows(h.b(), v4, h.db().findStructure("hamlet_ruin"), true)) == "State=Ruined;");
  CHECK(rowsText(placeInfoRows(h.b(), v4, h.db().findStructure("hamlet_shut"), true)) == "State=Visited;");
  CHECK(rowsText(placeInfoRows(h.b(), v4, h.st("v4").type, true)).empty());  // a fresh hamlet: nothing to say
}
