#include "script_input.h"

#include <cstdlib>
#include <cstring>

namespace ote {
namespace {

bool toInt(const std::string& s, int& out) {
  if (s.empty()) return false;
  char* end = nullptr;
  long v = strtol(s.c_str(), &end, 10);
  if (*end) return false;
  out = (int)v;
  return true;
}

struct Name { const char* name; Button button; };
const Name kButtons[] = {
    {"up", Button::Up},     {"down", Button::Down},   {"left", Button::Left},
    {"right", Button::Right}, {"confirm", Button::Confirm}, {"cancel", Button::Cancel},
    {"menu", Button::Menu}, {"next", Button::Next},   {"prev", Button::Prev},
    {"turnleft", Button::TurnLeft}, {"turnright", Button::TurnRight},
};

}  // namespace

bool ScriptInput::parse(const std::string& text, int interval, std::string& err) {
  interval_ = interval < 1 ? 1 : interval;
  tokens_.clear();
  size_t pos = 0;
  while (pos <= text.size()) {
    size_t comma = text.find(',', pos);
    std::string tok = text.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
    pos = comma == std::string::npos ? text.size() + 1 : comma + 1;
    while (!tok.empty() && tok[0] == ' ') tok.erase(0, 1);
    while (!tok.empty() && tok.back() == ' ') tok.pop_back();
    if (tok.empty()) continue;

    Token t;
    bool found = false;
    for (const Name& n : kButtons)
      if (tok == n.name) { t.kind = Token::Press; t.button = n.button; found = true; }
    if (!found && tok == "wait") { t.kind = Token::Wait; t.count = 1; found = true; }
    if (!found && tok.compare(0, 5, "wait:") == 0) {
      found = toInt(tok.substr(5), t.count) && t.count >= 0;
      t.kind = Token::Wait;
    }
    if (!found && tok.compare(0, 4, "tap:") == 0) {
      size_t colon = tok.find(':', 4);
      found = colon != std::string::npos && toInt(tok.substr(4, colon - 4), t.x) &&
              toInt(tok.substr(colon + 1), t.y);
      t.kind = Token::Tap;
    }
    if (!found) {
      err = "bad --input token '" + tok + "'";
      return false;
    }
    tokens_.push_back(t);
  }
  return true;
}

void ScriptInput::tick(std::vector<InputEvent>& out) {
  if (tokens_.empty() || finished_) return;
  if (settle_ >= 0) {  // script exhausted: count down the settle time
    if (--settle_ < 0) finished_ = true;
    return;
  }
  if (--wait_ > 0) return;
  const Token& t = tokens_[next_++];
  wait_ = interval_;
  InputEvent ev;
  if (t.kind == Token::Press) {
    ev.button = t.button;
    ev.type = InputEvent::ButtonDown;
    out.push_back(ev);
    ev.type = InputEvent::ButtonUp;
    out.push_back(ev);
  } else if (t.kind == Token::Tap) {
    ev.x = t.x;
    ev.y = t.y;
    ev.type = InputEvent::PointerDown;
    out.push_back(ev);
    ev.type = InputEvent::PointerUp;
    out.push_back(ev);
  } else {
    wait_ = interval_ * t.count;
  }
  if (next_ >= tokens_.size()) settle_ = wait_ + 30;
}

}  // namespace ote
