// Id sources: "which ids does this reference point at?", answered from the project's documents. Qt-free; the native editor
// answers from its open (possibly edited) documents, ote-mcp from the files of the game folder.
//
// A source is "data/units.json" (the ids of its table rows), "game.json#damageTypes" (the list under that key path; items
// are objects with an "id", or plain strings), "tags:FILE" / "field:KEY:FILE" (the free words a key of the rows holds),
// "keys:FILE#path" (the entry names of an object), and the sources of the file being edited: "@sides" (a list of it),
// "keys:@changes", and the computed "@storyUnits" and "@teams". See docs/NATIVE_EDITOR.md "Id sources".
#pragma once
#include <functional>
#include <string>
#include <vector>

#include "core/base/json.h"

namespace ote_editor {

// The parsed document of a project-relative file, or nullptr when the file does not exist / is unreadable.
using DocLookup = std::function<const ote::Json*(const std::string& rel)>;

// The file a source names ("keys:game.json#a.b" -> "game.json"); "" for the sources of the edited file ("@...").
std::string sourceFile(const std::string& source);

// The array a table file edits: a root array of objects, or the only member of an object when it is one. nullptr otherwise
// (an empty array says nothing about its columns).
const ote::Json* tableArray(const ote::Json& root);

// Ids of every source, in order, without duplicates. `doc` is the file being edited (for "@..." sources). A source whose
// file is missing contributes nothing.
std::vector<std::string> resolveIdSources(const ote::Json& doc, const DocLookup& docs, const std::vector<std::string>& sources);

// "@storyUnits": the ids events may name units by: the map's units (their `id`, or their `character`'s id) and the units its
// events add (addUnit, at any nesting depth). Mirrors the loader (collectAddedUnitIds), which also accepts any character id
// in some places; those come from data/characters.json and are added by the caller.
std::vector<std::string> storyUnitIds(const ote::Json& map);
// "@teams": the team names of the sides (a side without `team` is its own team), then the side ids: a command that names a team
// accepts a side id for that side's team too.
std::vector<std::string> teamIds(const ote::Json& map);

}  // namespace ote_editor
