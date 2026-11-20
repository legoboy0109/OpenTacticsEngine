// Splits the text of an args file (UWP LocalState\args.txt, Xbox D:\args.txt; docs/XBOX.md, docs/XBOX_OG.md) into player
// arguments and environment settings. No platform code, so it is unit-tested (tests/test_launch_args.cpp).
#pragma once
#include <string>
#include <utility>
#include <vector>

namespace ote {

// Words are separated by spaces, tabs or line ends. A word NAME=value (NAME of letters, digits, underscore; not starting
// with "--") goes to `env`; everything else is returned as an argument. A line that starts with '#' is a comment.
// Text in double quotes keeps its spaces and loses the quotes.
std::vector<std::string> parseLaunchArgs(const std::string& text, std::vector<std::pair<std::string, std::string>>& env);

}  // namespace ote
