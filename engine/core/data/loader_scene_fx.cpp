// Reading the scene presentation commands (docs/EVENTS.md "Scene presentation"); see loader_scene_fx.h.
#include "core/data/loader_scene_fx.h"

#include <cstdlib>

#include "core/data/events.h"  // isStoryId

namespace ote {

namespace {

std::string quoted(const std::string& s) { return "\"" + s + "\""; }

constexpr const char* kKeys[] = {"background", "fade", "wait", "shake", "leave", "clearStage", "weather", "enter", "move"};

// "#rrggbb" or "#rgb".
bool parseColor(const std::string& s, uint32_t& out) {
  if (s.size() != 7 && s.size() != 4) return false;
  if (s[0] != '#') return false;
  uint32_t v = 0;
  for (size_t i = 1; i < s.size(); ++i) {
    const char ch = s[i];
    int d;
    if (ch >= '0' && ch <= '9') d = ch - '0';
    else if (ch >= 'a' && ch <= 'f') d = 10 + ch - 'a';
    else if (ch >= 'A' && ch <= 'F') d = 10 + ch - 'A';
    else return false;
    v = s.size() == 7 ? (v << 4) | (uint32_t)d : (v << 8) | (uint32_t)(d * 17);
  }
  out = v;
  return true;
}

// "position": one of the seven slot names.
bool readPosition(const LoadCtx& c, const Json& e, const std::string& at, int& out) {
  const Json& p = e["position"];
  for (int i = 1; i < 8; ++i)
    if (p.isString() && p.asString() == positionName(i)) {
      out = i;
      return true;
    }
  return c.fail(at, "\"position\" must be \"left\", \"right\" or \"center\" (or \"farLeft\", \"midLeft\", \"midRight\", \"farRight\")");
}

}  // namespace

bool SceneFxReader::isKey(const std::string& key) {
  for (const char* k : kKeys)
    if (key == k) return true;
  return false;
}

bool SceneFxReader::isCommandIn(const Json& e, const char* key) {
  if (!e.has(key)) return false;
  const std::string k = key;
  if (k == "fade") return !e.has("playMusic") && !e.has("stopMusic") && !e.has("playSound");  // (playSound: its own "unknown key" message)
  if (k == "wait") return !e.has("focus") && !e.has("moveUnit");
  return true;
}

bool SceneFxReader::onlyKeys(const Json& o, std::initializer_list<const char*> keys, const std::string& at) const {
  for (const Json::Member& m : o.members()) {
    bool known = false;
    for (const char* k : keys) known = known || m.first == k;
    if (known) continue;
    std::string list;
    for (const char* k : keys) list += (list.empty() ? "" : ", ") + quoted(k);
    return c.fail(at, "unknown key " + quoted(m.first) + " (expected " + list + ")");
  }
  return true;
}

bool SceneFxReader::ms(const Json& o, const char* key, const std::string& at, int lo, int hi, int& out) const {
  if (!o.has(key)) return true;
  if (!LoadCtx::isWhole(o[key])) return c.fail(at, quoted(key) + " must be a whole number of milliseconds");
  const int v = o[key].asInt();
  if (v < lo || v > hi)
    return c.fail(at, quoted(key) + " is milliseconds, between " + std::to_string(lo) + " and " + std::to_string(hi));
  out = v;
  return true;
}

bool SceneFxReader::command(const Json& e, const std::string& key, const std::string& at, SceneFx& out) const {
  using K = SceneFx::Kind;
  const Json& v = e[key.c_str()];
  if (key == "background") {
    out.kind = K::Background;
    if (!onlyKeys(e, {"background", "transition", "ms"}, at)) return false;
    if (v.isNull()) {
      out.text = -1;  // back to the map (a map event) / to nothing (a campaign scene)
    } else {
      if (!v.isString()) return c.fail(at, "\"background\" must name a background of data/graphics.json \"backgrounds\", or be null to clear it");
      if (db.graphics.findBackground(v.asString()) < 0)
        return c.fail(at, "\"background\" names unknown background " + quoted(v.asString()) +
                              (db.graphics.backgrounds.empty() ? " (this game has no backgrounds, data/graphics.json \"backgrounds\")"
                                                               : " (data/graphics.json \"backgrounds\")"));
      out.text = text(v.asString());
    }
    std::string tr = "cut";
    if (!c.str(e, "transition", at, tr)) return false;
    if (tr != "cut" && tr != "fade") return c.fail(at, "\"transition\" must be \"cut\" or \"fade\"");
    out.fadeTransition = tr == "fade";
    out.ms = kDefaultFadeMs;
    return ms(e, "ms", at, 0, kMaxSceneMs, out.ms);
  }
  if (key == "fade") {
    out.kind = K::Fade;
    if (!onlyKeys(e, {"fade", "ms", "color"}, at)) return false;
    if (!v.isString() || (v.asString() != "out" && v.asString() != "in")) return c.fail(at, "\"fade\" must be \"out\" or \"in\"");
    out.out = v.asString() == "out";
    out.ms = kDefaultFadeMs;
    if (!ms(e, "ms", at, 0, kMaxSceneMs, out.ms)) return false;
    std::string col = "#000000";
    if (!c.str(e, "color", at, col)) return false;
    if (!parseColor(col, out.color)) return c.fail(at, "\"color\" must be a colour like \"#000000\"");
    return true;
  }
  if (key == "wait") {
    out.kind = K::Wait;
    if (!onlyKeys(e, {"wait"}, at)) return false;
    if (!LoadCtx::isWhole(v) || v.asInt() < 0 || v.asInt() > kMaxWaitMs)
      return c.fail(at, "\"wait\" is milliseconds, a whole number between 0 and " + std::to_string(kMaxWaitMs));
    out.ms = v.asInt();
    return true;
  }
  if (key == "shake") {
    out.kind = K::Shake;
    if (!onlyKeys(e, {"shake"}, at)) return false;
    if (!v.isObject()) return c.fail(at, "\"shake\" must be { \"ms\": 400, \"strength\": 3 }");
    if (!onlyKeys(v, {"ms", "strength"}, at + ": \"shake\"")) return false;
    out.ms = 400;
    if (!ms(v, "ms", at + ": \"shake\"", 0, kMaxSceneMs, out.ms)) return false;
    if (v.has("strength")) {
      if (!LoadCtx::isWhole(v["strength"]) || v["strength"].asInt() < 1 || v["strength"].asInt() > kMaxShakeStrength)
        return c.fail(at, "\"strength\" is pixels, a whole number between 1 and " + std::to_string(kMaxShakeStrength));
      out.strength = v["strength"].asInt();
    }
    return true;
  }
  if (key == "leave") {
    out.kind = K::Leave;
    if (!onlyKeys(e, {"leave", "transition", "ms"}, at)) return false;
    if (!v.isString() || v.asString().empty()) return c.fail(at, "\"leave\" must name the speaker that leaves the stage (the id or name a say line used)");
    out.text = text(v.asString());
    return stageTransition(e, at, out);
  }
  if (key == "clearStage") {
    out.kind = K::Leave;
    if (!onlyKeys(e, {"clearStage", "transition", "ms"}, at)) return false;
    if (!v.isBool() || !v.asBool()) return c.fail(at, "write { \"clearStage\": true }");
    out.text = -1;
    return stageTransition(e, at, out);
  }
  if (key == "enter" || key == "move") {
    const bool enter = key == "enter";
    out.kind = enter ? K::Enter : K::Move;
    if (enter ? !onlyKeys(e, {"enter", "position", "portrait", "transition", "ms"}, at)
              : !onlyKeys(e, {"move", "position", "transition", "ms"}, at))
      return false;
    if (!v.isString() || v.asString().empty())
      return c.fail(at, quoted(key) + " must name the speaker " + (enter ? "that comes on stage" : "that moves") + " (the id or name a say line uses)");
    out.text = text(v.asString());
    if (!e.has("position")) return c.fail(at, quoted(key) + " needs a \"position\" (farLeft, left, midLeft, center, midRight, right or farRight)");
    if (!readPosition(c, e, at, out.position)) return false;
    if (enter && e.has("portrait")) {
      std::string pid;
      if (!c.str(e, "portrait", at, pid)) return false;
      out.portrait = text(pid);
    }
    // A move slides (400 ms) unless it is told to cut; an enter / leave cuts unless it is told to fade.
    if (!enter) {
      std::string tr = "fade";
      if (!c.str(e, "transition", at, tr)) return false;
      if (tr != "cut" && tr != "fade") return c.fail(at, "\"transition\" must be \"cut\" (jump there) or \"fade\" (slide, the default)");
      out.fadeTransition = tr == "fade";
      out.ms = kDefaultSlideMs;
      return ms(e, "ms", at, 0, kMaxSceneMs, out.ms);
    }
    return stageTransition(e, at, out);
  }
  // weather
  out.kind = K::Weather;
  if (!mapEvent)
    return c.fail(at, "\"weather\" is for map events only: a campaign scene has no map to put weather over");
  if (!onlyKeys(e, {"weather"}, at)) return false;
  if (!v.isString() || (v.asString() != "rain" && v.asString() != "snow" && v.asString() != "none"))
    return c.fail(at, "\"weather\" must be \"rain\", \"snow\" or \"none\"");
  out.text = text(v.asString());
  return true;
}

// "transition" ("cut" default, or "fade") and "ms" of enter / leave / clearStage.
bool SceneFxReader::stageTransition(const Json& e, const std::string& at, SceneFx& out) const {
  std::string tr = "cut";
  if (!c.str(e, "transition", at, tr)) return false;
  if (tr != "cut" && tr != "fade") return c.fail(at, "\"transition\" must be \"cut\" or \"fade\"");
  out.fadeTransition = tr == "fade";
  out.ms = kDefaultFadeMs;
  return ms(e, "ms", at, 0, kMaxSceneMs, out.ms);
}

bool SceneFxReader::sayExtras(const Json& e, const std::string& at, SceneFx& out) const {
  if (e.has("position")) {
    if (!readPosition(c, e, at, out.position)) return false;
  }
  if (e.has("auto")) {
    out.ms = 0;
    if (!ms(e, "auto", at, 1, kMaxAutoMs, out.ms)) return false;
  }
  return true;
}

bool SceneFxReader::focusExtras(const Json& e, const std::string& at, SceneFx& out) const {
  if (!ms(e, "ms", at, 0, kMaxSceneMs, out.ms)) return false;
  if (e.has("speed")) {  // the scroll takes as long as its distance needs at this speed (docs/EVENTS.md "The camera")
    if (e.has("ms")) return c.fail(at, "give \"ms\" (how long the scroll takes) or \"speed\" (how fast it goes), not both");
    if (!e["speed"].isNumber() || e["speed"].asNumber() < kMinFocusSpeed || e["speed"].asNumber() > kMaxFocusSpeed)
      return c.fail(at, "\"speed\" is squares per second, from 0.1 to 100");
    out.speedMilli = (int)(e["speed"].asNumber() * 1000 + 0.5);
  }
  if (e.has("wait")) {
    if (!e["wait"].isBool()) return c.fail(at, "\"wait\" must be true or false");
    out.wait = e["wait"].asBool() ? 1 : 0;
  }
  return true;
}

bool SceneFxReader::moveExtras(const Json& e, const std::string& at, SceneFx& out) const {
  if (e.has("walk")) {
    if (!e["walk"].isBool()) return c.fail(at, "\"walk\" must be true or false");
    out.walk = e["walk"].asBool();
  }
  if (e.has("wait")) {
    if (!e["wait"].isBool()) return c.fail(at, "\"wait\" must be true or false");
    if (!out.walk) return c.fail(at, "\"wait\" only means something with \"walk\": true (a move that is not walked does not wait)");
    out.wait = e["wait"].asBool() ? 1 : 0;
  }
  return true;
}

}  // namespace ote
