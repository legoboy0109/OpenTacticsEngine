#include "core/data/database.h"

namespace ote {

template <class T>
static int findById(const std::vector<T>& v, const std::string& id) {
  for (size_t i = 0; i < v.size(); ++i)
    if (v[i].id == id) return static_cast<int>(i);
  return -1;
}

bool parseSavingMode(const std::string& text, SavingMode& out) {
  if (text == "any") out = SavingMode::Any;
  else if (text == "continue") out = SavingMode::Continue;
  else if (text == "off") out = SavingMode::Off;
  else return false;
  return true;
}

int Database::findStat(const std::string& id) const { return findById(stats, id); }
int Database::findResource(const std::string& id) const { return findById(resources, id); }
int Database::findMoveType(const std::string& id) const { return findById(moveTypes, id); }
int Database::findTerrain(const std::string& id) const { return findById(terrains, id); }
int Database::findStructure(const std::string& id) const { return findById(structures, id); }
int Database::findWeapon(const std::string& id) const { return findById(weapons, id); }
int Database::findUnitType(const std::string& id) const { return findById(unitTypes, id); }
int Database::findMap(const std::string& id) const { return findById(maps, id); }
int Database::findStyle(const std::string& id) const { return findById(styles, id); }

int enterCostOf(const Database& db, int terrain, int structure, int moveType) {
  if (structure >= 0) {
    const std::vector<int>& mc = db.structures[(size_t)structure].moveCost;
    if ((size_t)moveType < mc.size() && mc[(size_t)moveType] >= 0) return mc[(size_t)moveType];
  }
  return db.terrains[(size_t)terrain].moveCost[(size_t)moveType];
}

const Props& squareNumbers(const Database& db, int terrain, int structure, Props& scratch) {
  const Props& base = db.terrains[(size_t)terrain].props;
  if (structure < 0 || db.structures[(size_t)structure].props.empty()) return base;
  scratch = base;
  for (const NamedNumber& n : db.structures[(size_t)structure].props) {
    bool found = false;
    for (NamedNumber& m : scratch)
      if (m.key == n.key) { m.value = n.value; found = true; break; }
    if (!found) scratch.push_back(n);
  }
  return scratch;
}

}  // namespace ote
