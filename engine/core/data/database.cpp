#include "core/data/database.h"

namespace ote {

template <class T>
static int findById(const std::vector<T>& v, const std::string& id) {
  for (size_t i = 0; i < v.size(); ++i)
    if (v[i].id == id) return static_cast<int>(i);
  return -1;
}

int Database::findStat(const std::string& id) const { return findById(stats, id); }
int Database::findResource(const std::string& id) const { return findById(resources, id); }
int Database::findMoveType(const std::string& id) const { return findById(moveTypes, id); }
int Database::findTerrain(const std::string& id) const { return findById(terrains, id); }
int Database::findWeapon(const std::string& id) const { return findById(weapons, id); }
int Database::findUnitType(const std::string& id) const { return findById(unitTypes, id); }
int Database::findMap(const std::string& id) const { return findById(maps, id); }
int Database::findStyle(const std::string& id) const { return findById(styles, id); }

}  // namespace ote
