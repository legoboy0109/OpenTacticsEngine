// The comment scan moved to editor/core (json_comments.h) so ote-mcp shares it; this keeps the old name for the native editor.
#pragma once
#include "json_comments.h"

namespace ote_native {
using ote_editor::jsonTextHasComments;
}  // namespace ote_native
