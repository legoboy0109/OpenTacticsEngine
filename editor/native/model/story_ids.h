// Id lists that are computed from a whole map file instead of read from one key: the ids a story may name a unit by, and the
// teams of the sides. Pure functions on the JSON tree (no widgets), so forms and tests share them.
#pragma once
#include <QStringList>
#include <string>
#include <vector>

#include "core/base/json.h"

namespace ote_native {

class IdSource;

// "@storyUnits": the ids events may name units by: the map's units (their `id`, or their `character`'s id) and the units its
// events add (addUnit, at any nesting depth). Mirrors the loader (collectAddedUnitIds), which also accepts any character id
// in some places; those come from data/characters.json and are added by the caller.
QStringList storyUnitIds(const ote::Json& map);
// "@teams": the team names of the sides (a side without `team` is its own team), then the side ids: a command that names a team
// accepts a side id for that side's team too.
QStringList teamIds(const ote::Json& map);

// Every id source of a node, resolved against `doc` (the file being edited) and `ids` (the other files of the project):
// "@sides", "@units" ... (a list of the file itself), "@storyUnits", "@teams", "keys:@changes" (the entry names of an object of
// the file) and everything IdSource knows. The one resolver forms, tests and the conformance samples share.
QStringList resolveSources(const ote::Json& doc, IdSource* ids, const std::vector<std::string>& sources);

}  // namespace ote_native
