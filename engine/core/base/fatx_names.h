// What the original Xbox's hard-disk file system (FATX) accepts as a name (docs/XBOX_OG.md "File name limits").
// Pure string rules, no file access, so they are unit-tested (tests/test_fatx_names.cpp) and used by the Xbox build
// script's packaging check (tools/xbox-og/check_names.py repeats the same rules) and by the player at run time.
//
// The rules: a name is 1..42 characters; the characters are ASCII letters, digits, space and ! # $ % & ' ( ) - . @ [ ] ^ _ ` { } ~
// (the set the open-source FATX tools accept, e.g. fatx 1.17 `nchar`); names are compared without regard to case but the
// case is kept. No other punctuation (no + , ; = " * / : < > ? | \), no non-ASCII bytes, no control characters.
#pragma once
#include <string>
#include <vector>

namespace ote {

constexpr size_t kFatxMaxName = 42;
// The kernel's object path limit is about 250 characters; names deeper than this are refused by the build check.
constexpr size_t kFatxMaxPath = 240;

// True for one character a FATX name may contain.
bool fatxCharOk(unsigned char c);

// "" when `name` (one path component, no slash) is fine; else the reason ("42 characters at most (this has 47)",
// "contains '+', which FATX does not allow", ...). "." and ".." and the empty name are not names.
std::string fatxNameProblem(const std::string& name);

// Problems of a whole relative path with '/' or '\' separators: every component's problem, a too long total, plus (given
// `siblings`, the other names in the same folder) nothing; see fatxCaseClashes for that. Each entry is "<path>: <reason>".
std::vector<std::string> fatxPathProblems(const std::string& path);

// Pairs among `names` (names in one folder) that differ only by case: FATX would treat them as the same file.
std::vector<std::string> fatxCaseClashes(const std::vector<std::string>& names);

// A name that is safe on FATX and stable: unchanged when already fine, else every character that is not allowed
// (and '~') becomes "~XX" (two upper-case hex digits of the byte), and a name still over 42 characters is cut and ends
// in "~" plus eight hex digits of a hash of the whole original, so two long names do not collide. Never empty.
std::string fatxSafeName(const std::string& name);

}  // namespace ote
