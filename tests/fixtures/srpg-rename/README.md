# srpg-rename

A small game in which the character `hero` (named "Aldric") is spelled in every kind of place that can name a character: the
editor's rename of a character's id (`editor/web/src/model/characterRename.ts`) must find all of them.
`editor/web/tests/unit/characterRename.test.ts` renames `hero` and checks that nothing still holds the old id;
`tests/editor_api_rename_checks.py` loads the game, and the same game with `"hero"` replaced by `"captain"`, in the real loader.
The id `hero` is written only as the quoted string `"hero"` and only where it is a reference (no name, text or other id contains it),
so a plain text replacement of `"hero"` is the expected result of the rename.
