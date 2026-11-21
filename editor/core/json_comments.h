// The engine's JSON reader accepts // and /* */ comments, but ote::Json does not keep them, so saving a file drops
// them. The editors warn before the first save of such a file; this finds out whether a file has any.
#pragma once
#include <string>

namespace ote_editor {

// True when `text` holds a // or /* */ comment outside of string literals.
bool jsonTextHasComments(const std::string& text);

}  // namespace ote_editor
