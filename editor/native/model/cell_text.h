// Text <-> JSON value conversion for table cells, tree values and pasted text. The rule that matters: an edit keeps the
// type of the value it replaces (a number stays a number; text typed into a number cell is refused with a message).
#pragma once
#include <QString>

#include "core/base/json.h"

namespace ote_native {

// Display and copy text: strings as they are, numbers in their shortest exact form, true/false, null, and objects/arrays
// as compact JSON.
QString jsonToCellText(const ote::Json& value);

struct CellParse {
  bool ok = false;
  ote::Json value;
  QString error;  // plain-language reason when !ok
};

// Parse `text` for a cell whose current value is `current` (nullptr = the key is absent). `columnHint` is the type most
// of the column has; it decides an absent cell's type.
CellParse parseCellText(const QString& text, const ote::Json* current, ote::Json::Type columnHint);


// A fresh value of the given type: "", 0, false, [], {} (Null stays Null). Used for the defaults of a new row.
ote::Json defaultValueFor(ote::Json::Type t);

}  // namespace ote_native
