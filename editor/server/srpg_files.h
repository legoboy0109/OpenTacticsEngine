// The optional strategy-RPG data files (docs/SRPG.md "Editor"): data/characters.json, skills.json, abilities.json, conditions.json
// (each `{ "<key>": [...] }`) and data/supports.json (one object). Kept apart from projects.cpp / project_write.cpp so
// those files only gain one call each.
#pragma once
#include <string>
#include <vector>

#include "config.h"

namespace ote_editor {

// GET: sets "characters", "skills", "abilities" (the list, or null when the file is absent) and "supports" (the
// file's object, or null) on `doc`. A file that cannot be read is null plus a problem, like the other sections.
void readSrpgSections(const fs::path& dir, ote::Json& doc, std::vector<Problem>& probs);

// PUT, step 1: checks the types of the four sections in `body`; returns an error message, or "" when all are fine.
std::string checkSrpgBody(const ote::Json& body);

// PUT, step 2: what the four sections of `body` ask for. A non-empty list / an object is a write of the file; an
// EMPTY list removes the file (a game without the file and one with an empty list are the same game: opening a
// screen must not change the data hash); `supports: false` removes data/supports.json. null / absent = untouched.
struct SrpgFileOps {
  struct Write { std::string rel, bytes; };
  std::vector<Write> writes;
  std::vector<std::string> removals;  // project-relative; only removed if the file exists
};
SrpgFileOps planSrpgFiles(const ote::Json& body);

// True for the project-relative names of the four files (a map may not use them as its file).
bool isSrpgFile(const std::string& rel);

}  // namespace ote_editor
