#include "script_input.h"

#include <cstdlib>
#include <cstring>

#include "ui/input_hints.h"

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
    {"tiltup", Button::TiltUp}, {"tiltdown", Button::TiltDown}, {"zoomin", Button::ZoomIn}, {"zoomout", Button::ZoomOut},
    {"skip", Button::Skip}, {"ranges", Button::Ranges},
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
    struct LifeName { const char* name; Lifecycle life; };
    static const LifeName kLife[] = {{"background", Lifecycle::WillEnterBackground}, {"foreground", Lifecycle::DidEnterForeground},
                                     {"lowmemory", Lifecycle::LowMemory}, {"terminate", Lifecycle::Terminating}};
    for (const LifeName& n : kLife)
      if (tok == n.name) { t.kind = Token::Life; t.life = n.life; found = true; }
    if (!found && tok == "wait") { t.kind = Token::Wait; t.count = 1; found = true; }
    if (!found && tok.compare(0, 5, "wait:") == 0) {
      found = toInt(tok.substr(5), t.count) && t.count >= 0;
      t.kind = Token::Wait;
    }
    if (!found && tok.compare(0, 7, "device:") == 0) {
      found = parseInputDevice(tok.substr(7), t.device);
      t.kind = Token::Device;
    }
    if (!found && tok.compare(0, 4, "tap:") == 0) {
      size_t colon = tok.find(':', 4);
      found = colon != std::string::npos && toInt(tok.substr(4, colon - 4), t.x) &&
              toInt(tok.substr(colon + 1), t.y);
      t.kind = Token::Tap;
    }
    if (!found && tok.compare(0, 5, "move:") == 0) {
      size_t colon = tok.find(':', 5);
      found = colon != std::string::npos && toInt(tok.substr(5, colon - 5), t.x) &&
              toInt(tok.substr(colon + 1), t.y);
      t.kind = Token::Move;
    }
    if (!found && tok == "leave") { t.kind = Token::Leave; found = true; }
    if (!found) {
      err = "bad --input token '" + tok + "'";
      return false;
    }
    tokens_.push_back(t);
  }
  return true;
}

void ScriptInput::tick(std::vector<InputEvent>& out, std::vector<Lifecycle>& notices) {
  if (tokens_.empty() || finished_) return;
  if (settle_ >= 0) {  // script exhausted: count down the settle time
    if (--settle_ < 0) finished_ = true;
    return;
  }
  if (--wait_ > 0) return;
  // `device:` words take no time: apply them, then the token after them fires in this same frame.
  while (next_ < tokens_.size() && tokens_[next_].kind == Token::Device) {
    device_ = tokens_[next_++].device;
    deviceChanged_ = true;
  }
  if (next_ >= tokens_.size()) { settle_ = 30; return; }
  const Token& t = tokens_[next_++];
  wait_ = interval_;
  InputEvent ev;
  ev.device = device_;
  if (t.kind == Token::Press) {
    ev.button = t.button;
    ev.type = InputEvent::ButtonDown;
    out.push_back(ev);
    ev.type = InputEvent::ButtonUp;
    out.push_back(ev);
  } else if (t.kind == Token::Life) {
    notices.push_back(t.life);
  } else if (t.kind == Token::Tap) {
    ev.x = t.x;
    ev.y = t.y;
    ev.type = InputEvent::PointerDown;
    out.push_back(ev);
    ev.type = InputEvent::PointerUp;
    out.push_back(ev);
  } else if (t.kind == Token::Move || t.kind == Token::Leave) {
    ev.type = InputEvent::PointerMove;
    ev.x = t.kind == Token::Move ? t.x : kPointerGone;
    ev.y = t.kind == Token::Move ? t.y : kPointerGone;
    out.push_back(ev);
  } else {
    wait_ = interval_ * t.count;
  }
  if (next_ >= tokens_.size()) settle_ = wait_ + 30;
}

}  // namespace ote
