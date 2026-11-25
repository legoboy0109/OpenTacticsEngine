// "{name:<id>}" in story text (docs/EVENTS.md "Naming a character"): the current name of a character, which the player may have
// typed in (askName). The loaders check the tokens' shape; the player replaces them when a line is shown, so a name entered after a
// scene was run still shows in the lines behind the entry.
#pragma once
#include <functional>
#include <string>

namespace ote {

// False with a reason when a "{name:" in `text` is not followed by a story id and "}". Any other "{" is plain text.
bool nameTokensOk(const std::string& text, std::string& why);
// `text` with every "{name:<id>}" replaced by nameOf(id). Text without tokens comes back unchanged.
std::string expandNameTokens(const std::string& text, const std::function<std::string(const std::string&)>& nameOf);
// True when `name` may be a character's entered name: 1..kMaxNameLetters letters (UTF-8 characters), no control characters and
// no "{" or "}" (a name must never make a token).
bool validEnteredName(const std::string& name);

// The longest name a player may type (askName "max" is 1..this); the default "max".
constexpr int kMaxNameLetters = 24;
constexpr int kDefaultNameLetters = 12;

}  // namespace ote
