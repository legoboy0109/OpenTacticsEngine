// Which columns a table gets from a schema. Scalar keys are columns; an object marked "expand" gives one column per
// member and a map marked "expand" one per id of its key source (units' stats: one column per stat of the game), named
// "stats.hp". Lists and other nested things only get a column when a row has them (then they are JSON cells, as before).
#pragma once
#include <vector>

#include "json_table_model.h"

namespace ote_native {

std::vector<ColumnSpec> planColumns(const SchemaNode& rows, IdSource* ids);

}  // namespace ote_native
