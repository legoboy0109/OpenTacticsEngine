// Which files get the table editor: an array of objects, or an object whose only member is such an array
// ({ "units": [ {...}, {...} ] } as in units.json, weapons.json, terrain.json, items.json ...). Everything else
// (game.json, maps) gets the generic tree view.
#pragma once
#include <optional>

#include "json_path.h"

namespace ote_native {

// Path of the array that the table edits (empty for a root array), or nothing when the file is not table-shaped.
std::optional<JsonPath> findTableArray(const ote::Json& root);

}  // namespace ote_native
